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

#ifdef ARDUINO_ARCH_ESP32
# include <esp_ota_ops.h>
# include <rom/rtc.h>
#endif

#include <string>

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

#ifdef ARDUINO_ARCH_ESP32
std::string reset_reason_string(RESET_REASON reason);
std::string wakeup_cause_string(WAKEUP_REASON cause);
#endif
#if !defined(ENV_NATIVE) && !defined(ARDUINO_ARCH_ESP8266)
const __FlashStringHelper *ota_state_string(esp_ota_img_states_t state);
#endif

} // namespace app
