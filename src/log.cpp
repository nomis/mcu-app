/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022,2024  Simon Arlott
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

#include <Arduino.h>

#include <cstdarg>

#include <uuid/log.h>

#ifndef PSTR_ALIGN
# define PSTR_ALIGN 4
#endif

static const char __pstr__logger_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "espressif";

static uuid::log::Logger logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::KERN};

extern "C" {

int ets_printf(const char *format, ...) {
	std::vector<char> text(256);
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vsnprintf(text.data(), text.size(), format, ap);
	va_end(ap);

	while (ret > 0) {
		if (text[ret - 1] == '\r' || text[ret - 1] == '\n') {
			text[ret - 1] = '\0';
			ret--;
		} else {
			break;
		}
	}

	if (ret > 0) {
		logger_.logp(uuid::log::Level::NOTICE, text.data());
	}

	return ret;
}

}
