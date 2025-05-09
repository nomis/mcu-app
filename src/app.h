/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022-2025  Simon Arlott
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

#include <initializer_list>
#include <memory>
#include <mutex>
#include <vector>

#ifndef ENV_NATIVE
# include <uuid/syslog.h>
# include <uuid/telnet.h>
#endif

#include "console.h"
#include "ddns.h"
#include "network.h"

#ifndef APP_CONSOLE_PIN
# define APP_CONSOLE_PIN -1
#endif

namespace app {

class AppShell;

class App {
private:
	static constexpr unsigned long SERIAL_CONSOLE_BAUD_RATE = 115200;
	static constexpr auto& serial_console_ = Serial;
	static constexpr int CONSOLE_PIN = APP_CONSOLE_PIN;

#if defined(ARDUINO_ESP8266_WEMOS_D1MINI) || defined(ESP8266_WEMOS_D1MINI)
#elif defined(ARDUINO_LOLIN_S2_MINI)
#elif defined(ARDUINO_LOLIN_S3)
#elif defined(ARDUINO_ESP_S3_DEVKITC)
#elif defined(ARDUINO_ESP_S3_DEVKITM)
#elif defined(ENV_NATIVE)
#else
# error "Unknown board"
#endif

public:
	static std::recursive_mutex& file_mutex() { return file_mutex_; }

	~App() = default;
	virtual void init();
	virtual void start();
	virtual void loop();
	void exception(const __FlashStringHelper *where);

#ifndef ENV_NATIVE
	void config_syslog();
#endif
#ifdef ARDUINO_ARCH_ESP8266
	void config_ota();
#endif

#ifndef ENV_NATIVE
	Network network_;
# ifdef ARDUINO_ARCH_ESP32
	DynamicDNS ddns_;
# endif
#endif

protected:
	static uuid::log::Logger logger_;

	App();

	bool local_console_enabled() { return CONSOLE_PIN >= 0 && local_console_; }
#ifndef ARDUINO_ARCH_ESP8266
	inline const std::string& app_hash() const { return app_hash_; }
#endif

private:
	static std::recursive_mutex file_mutex_;

	void shell_prompt();

#ifndef ENV_NATIVE
	uuid::syslog::SyslogService syslog_;
	uuid::telnet::TelnetService telnet_;
#endif
	std::shared_ptr<AppShell> shell_;
	bool local_console_;
#ifdef ARDUINO_ARCH_ESP8266
	bool ota_running_ = false;
#else
	std::string app_hash_;
#endif
};

} // namespace app
