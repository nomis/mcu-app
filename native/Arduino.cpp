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
#ifdef ENV_NATIVE

#include <Arduino.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <list>
#include <string>
#include <vector>

NativeConsole Serial;

static unsigned long start_millis;
static unsigned long start_micros;
static struct termios tm_orig;

static void fix_termios(void) {
	tcsetattr(STDIN_FILENO, TCSANOW, &tm_orig);
}

static void signal_handler(int num) {
	raise(SIGQUIT);
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
	struct timespec ts;
	struct termios tm_new;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	start_millis = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	start_micros = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;

	tcgetattr(STDIN_FILENO, &tm_orig);
	tm_new = tm_orig;
	cfmakeraw(&tm_new);
	tm_new.c_lflag |= ISIG;
	tm_new.c_cc[VINTR] = _POSIX_VDISABLE;
	tm_new.c_cc[VQUIT] = _POSIX_VDISABLE;
	tm_new.c_cc[VSUSP] = tm_orig.c_cc[VSUSP];
	signal(SIGTSTP, signal_handler);
	atexit(fix_termios);
	tcsetattr(STDIN_FILENO, TCSANOW, &tm_new);

	setup();
	while (1) {
		loop();
	}
	return 0;
}

unsigned long millis() {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000) - start_millis;
}

void delay(unsigned long millis) {
	struct timespec ts = {
		.tv_sec = (long)millis / 1000,
		.tv_nsec = ((long)millis % 1000) * 1000000,
	};

	nanosleep(&ts, NULL);
}

unsigned long micros() {
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (unsigned long)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000) - start_micros;
}

void delayMicroseconds(unsigned long micros) {
	struct timespec ts = {
		.tv_sec = (long)micros / 1000000,
		.tv_nsec = ((long)micros % 1000000) * 1000,
	};

	nanosleep(&ts, NULL);
}

void yield(void) {
	sched_yield();
}

int snprintf_P(char *str, size_t size, const char *format, ...) {
	va_list ap;

	va_start(ap, format);
	int ret = vsnprintf_P(str, size, format, ap);
	va_end(ap);

	return ret;
}

int vsnprintf_P(char *str, size_t size, const char *format, va_list ap) {
	std::string native_format;

	char previous = 0;
	for (size_t i = 0; i < strlen(format); i++) {
		char c = format[i];

		// This would be a lot easier if the ESP8266 platform
		// simply read all strings with 32-bit accesses instead
		// of repurposing %S (wchar_t).
		if (previous == '%' && c == 'S') {
			c = 's';
		}

		native_format += c;
		previous = c;
	}

	return vsnprintf(str, size, native_format.c_str(), ap);
}

#endif
