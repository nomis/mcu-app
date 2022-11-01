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
#ifndef ENV_NATIVE

#include <FS.h>
#include <LittleFS.h>

namespace app {

static constexpr auto &FS = LittleFS;

inline bool FS_begin(bool formatOnFail) {
	bool ret;
#if defined(ARDUINO_ARCH_ESP8266)
	ret = FS.begin();
	if (!ret && formatOnFail) {
		if (FS.format()) {
			ret = FS.begin();
		}
	}
#elif defined(ARDUINO_ARCH_ESP32)
	ret = FS.begin(formatOnFail);
#else
# error "Unknown arch"
#endif
	return ret;
}

} // namespace app
#endif
