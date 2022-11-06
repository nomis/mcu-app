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

#ifndef ARDUINO_H_
#define ARDUINO_H_

#include <sys/select.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include <Print.h>
#include <Printable.h>
#include <Stream.h>
#include <WString.h>

#define PROGMEM
#define PGM_P const char *
#define PSTR(s) (__extension__({static const char __c[] = (s); &__c[0];}))

#define IRAM_ATTR

#define printf_P printf
#define strlen_P strlen
#define strncpy_P strncpy
#define strcmp_P strcmp

#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
static inline void *heap_caps_malloc(size_t size, uint32_t caps) { return malloc(size); }
static inline void *heap_caps_realloc(void *ptr, size_t size, uint32_t caps) { return realloc(ptr, size); }

int snprintf_P(char *str, size_t size, const char *format, ...);
int vsnprintf_P(char *str, size_t size, const char *format, va_list ap);

#define pgm_read_byte(addr) (*reinterpret_cast<const char *>(addr))

typedef bool boolean;

class NativeConsole: public Stream {
public:
	void begin(unsigned long baud __attribute__((unused))) {

	}

	int available() override {
		if (peek_ != -1)
			return 1;

		struct timeval timeout;
		fd_set rfds;

		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);

		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;

		return ::select(STDIN_FILENO + 1, &rfds, NULL, NULL, &timeout) > 0 ? 1 : 0;
	}

	int read() override {
		if (peek_ != -1) {
			uint8_t c = peek_;
			peek_ = -1;
			return c;
		}

		if (available() > 0) {
			uint8_t c;
			int ret = ::read(STDIN_FILENO, &c, 1);

			if (ret == 0) {
				return '\x04';
			} else if (ret == 1) {
				return c;
			} else {
				exit(1);
			}
		}

		return -1;
	}

	int peek() override {
		if (peek_ == -1)
			peek_ = read();

		return peek_;
	}

	size_t write(uint8_t c) override {
		if (::write(STDOUT_FILENO, &c, 1) == 1) {
			return 1;
		} else {
			exit(1);
		}
	}

	size_t write(const uint8_t *buffer, size_t size) {
		if (::write(STDOUT_FILENO, buffer, size) == (ssize_t)size) {
			return size;
		} else {
			exit(1);
		}
	}

private:
	int peek_ = -1;
};

extern NativeConsole Serial;

unsigned long millis();
unsigned long micros();

void delay(unsigned long millis);
void delayMicroseconds(unsigned long micros);

void yield(void);

void setup(void);
void loop(void);

#endif
