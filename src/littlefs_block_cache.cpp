/*
 * mcu-app - Microcontroller application framework
 * Copyright 2023,2025  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

struct lfs_config;
typedef uint32_t lfs_block_t;
typedef uint32_t lfs_off_t;
typedef uint32_t lfs_size_t;

extern "C" int __real_littlefs_api_read(const struct lfs_config *c,
	lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);

namespace app {

namespace filesystem_cache {

#if defined(ARDUINO_LOLIN_S2_MINI)
static constexpr size_t FILESYSTEM_BLOCK_SIZE = 4096;
static constexpr size_t FILESYSTEM_SIZE = 2 * 1024 * 1024;
static constexpr size_t FILESYSTEM_CACHE_SIZE = 512 * 1024;
#elif defined(ARDUINO_LOLIN_S3)
static constexpr size_t FILESYSTEM_BLOCK_SIZE = 4096;
static constexpr size_t FILESYSTEM_SIZE = 8 * 1024 * 1024;
static constexpr size_t FILESYSTEM_CACHE_SIZE = 2 * 1024 * 1024;
#elif defined(ARDUINO_ESP_S3_DEVKITM)
static constexpr size_t FILESYSTEM_BLOCK_SIZE = 4096;
static constexpr size_t FILESYSTEM_SIZE = 8 * 1024 * 1024;
static constexpr size_t FILESYSTEM_CACHE_SIZE = 2 * 1024 * 1024;
#elif defined(ARDUINO_ESP_S3_DEVKITC)
static constexpr size_t FILESYSTEM_BLOCK_SIZE = 4096;
static constexpr size_t FILESYSTEM_SIZE = 8 * 1024 * 1024;
static constexpr size_t FILESYSTEM_CACHE_SIZE = 2 * 1024 * 1024;
#endif

static constexpr size_t FILESYSTEM_BLOCKS = FILESYSTEM_SIZE / FILESYSTEM_BLOCK_SIZE;
static constexpr size_t FILESYSTEM_CACHE_BLOCKS = FILESYSTEM_CACHE_SIZE / FILESYSTEM_BLOCK_SIZE;
static uint8_t* cache = nullptr;
static uint16_t* block_index = nullptr;
static uint16_t* cache_index = nullptr;
static int used_cache_size = 0;

static void init() {
	if (!cache) {
		cache = reinterpret_cast<uint8_t*>(::heap_caps_malloc(FILESYSTEM_CACHE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

		block_index = reinterpret_cast<uint16_t*>(::heap_caps_malloc(FILESYSTEM_BLOCKS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
		memset(block_index, 0xFF, FILESYSTEM_BLOCKS * sizeof(uint16_t));

		cache_index = reinterpret_cast<uint16_t*>(::heap_caps_malloc(FILESYSTEM_CACHE_BLOCKS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
		memset(block_index, 0xFF, FILESYSTEM_CACHE_BLOCKS * sizeof(uint16_t));
	}
}

static int read(const struct lfs_config *c,
		lfs_block_t block, lfs_off_t off, uint8_t *buffer, lfs_size_t size) {
	init();

	while (off >= FILESYSTEM_BLOCK_SIZE) {
		off -= FILESYSTEM_BLOCK_SIZE;
		block++;
	}

	while (size > 0) {
		size_t available = std::min(FILESYSTEM_BLOCK_SIZE - off, size);

		if (block >= FILESYSTEM_BLOCKS)
			return __real_littlefs_api_read(c, block, off, buffer, size);

		if (block_index[block] == UINT16_MAX) {
			if (used_cache_size >= FILESYSTEM_CACHE_BLOCKS) {
				uint16_t pos = rand() % FILESYSTEM_CACHE_BLOCKS;

				if (cache_index[pos] != UINT16_MAX) {
					block_index[cache_index[pos]] = UINT16_MAX;
				}
				block_index[block] = pos;
				cache_index[block_index[block]] = block;
			} else {
				block_index[block] = used_cache_size;
				cache_index[block_index[block]] = block;
				used_cache_size++;
			}

			int ret = __real_littlefs_api_read(c, block, 0,
				cache + block_index[block] * FILESYSTEM_BLOCK_SIZE,
				FILESYSTEM_BLOCK_SIZE);

			if (ret) {
				block_index[block] = UINT16_MAX;
				cache_index[block_index[block]] = block;
				return ret;
			}
		}

		std::memcpy(buffer, cache + block_index[block] * FILESYSTEM_BLOCK_SIZE + off, available);
		buffer += available;
		size -= available;
		off = 0;
		block++;
	}

	return 0;
}

static void evict(lfs_block_t block, lfs_off_t off, lfs_size_t size) {
	if (used_cache_size == 0)
		return;

	while (off >= FILESYSTEM_BLOCK_SIZE) {
		off -= FILESYSTEM_BLOCK_SIZE;
		block++;
	}

	while (size > 0) {
		size_t available = std::min(FILESYSTEM_BLOCK_SIZE - off, size);

		if (block >= FILESYSTEM_BLOCKS)
			return;

		if (block_index[block] != UINT16_MAX) {
			cache_index[block_index[block]] = UINT16_MAX;
			block_index[block] = UINT16_MAX;
		}

		size -= available;
		off = 0;
		block++;
	}
}

} // namespace filesystem_cache

} // namespace app

extern "C" {

int __wrap_littlefs_api_read(const struct lfs_config *c, lfs_block_t block,
		lfs_off_t off, void *buffer, lfs_size_t size) {
	return app::filesystem_cache::read(c, block, off,
		reinterpret_cast<uint8_t*>(buffer), size);
}

int __real_littlefs_api_prog(const struct lfs_config *c, lfs_block_t block,
		lfs_off_t off, const void *buffer, lfs_size_t size);

int __wrap_littlefs_api_prog(const struct lfs_config *c, lfs_block_t block,
		lfs_off_t off, const void *buffer, lfs_size_t size) {
	app::filesystem_cache::evict(block, off, size);
	return __real_littlefs_api_prog(c, block, off, buffer, size);
}

int __real_littlefs_api_erase(const struct lfs_config *c, lfs_block_t block);

int __wrap_littlefs_api_erase(const struct lfs_config *c, lfs_block_t block) {
	app::filesystem_cache::evict(block, 0,
		app::filesystem_cache::FILESYSTEM_BLOCK_SIZE);
	return __real_littlefs_api_erase(c, block);
}

}

#endif
