/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022-2023  Simon Arlott
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

#ifdef ARDUINO_ARCH_ESP32
# include <esp_ota_ops.h>
# include <rom/rtc.h>
#endif

#if __has_include(<mutex>)
# include <mutex>
#endif
#include <cstring>
#include <string>

#ifndef MCU_APP_STD_MUTEX_AVAILABLE
# if __has_include(<mutex>) && (!defined(_GLIBCXX_MUTEX) || defined(_GLIBCXX_HAS_GTHREADS))
#  define MCU_APP_STD_MUTEX_AVAILABLE 1
# else
#  define MCU_APP_STD_MUTEX_AVAILABLE 0
# endif
#endif

#if defined(ARDUINO_ARCH_ESP32) || CONFIG_STD_MUTEX_AVAILABLE
# define MCU_APP_THREAD_SAFE 1
#else
# define MCU_APP_THREAD_SAFE 0
#endif

#include <CBOR.h>
#include <CBOR_parsing.h>
#include <CBOR_streams.h>

namespace app {

class HexPrintable: public Printable {
public:
	HexPrintable(const uint8_t *buf, size_t len);

	size_t printTo(Print &print) const override;

private:
	const uint8_t *buf_;
	const size_t len_;
};

std::string hex_string(const uint8_t *buf, size_t len);

template<typename T, size_t size>
static inline std::string null_terminated_string(T(&data)[size]) {
	T *found = reinterpret_cast<T*>(std::memchr(&data[0], '\0', size));
	return std::string{&data[0], found ? (found - &data[0]) : size};
};

std::string normalise_filename(const std::string &filename);
std::string base_filename(const std::string &filename);

void write_text(qindesign::cbor::Writer &writer, const std::string &text);
void write_text(qindesign::cbor::Writer &writer, const char *text);
void write_text(qindesign::cbor::Writer &writer, const __FlashStringHelper *text);
bool read_text(qindesign::cbor::Reader &reader, std::string &text, size_t max_length = 256);

bool expect_float(qindesign::cbor::Reader &reader, float &value);

#ifdef ARDUINO_ARCH_ESP32
std::string reset_reason_string(RESET_REASON reason);
std::string wakeup_cause_string(WAKEUP_REASON cause);
#endif
#if !defined(ENV_NATIVE) && !defined(ARDUINO_ARCH_ESP8266)
const __FlashStringHelper *ota_state_string(esp_ota_img_states_t state);
#endif

} // namespace app
