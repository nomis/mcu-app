/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022  Simon Arlott
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

#pragma once

#include <Arduino.h>
#include <ArduinoJson.hpp>

namespace app {

#if defined(ARDUINO_ARCH_ESP8266)
using JsonDocument = ArduinoJson::DynamicJsonDocument JsonDocument;
#elif defined(ARDUINO_ARCH_ESP32) || defined(ENV_NATIVE)

class SPIRAMAllocator {
public:
	void *allocate(size_t size) {
		return ::heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	}

	void deallocate(void *ptr) {
		::free(ptr);
	}

	void *reallocate(void *ptr, size_t new_size) {
		return ::heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	}
};

using JsonDocument = ArduinoJson::BasicJsonDocument<SPIRAMAllocator>;
#else
# error "Unknown arch"
#endif

} // namespace app
