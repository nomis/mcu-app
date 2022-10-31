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

#include <initializer_list>
#include <memory>
#include <vector>

#include <uuid/syslog.h>
#include <uuid/telnet.h>

#include "console.h"
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
#else
# error "Unknown board"
#endif

public:
	~App() = default;
	virtual void init();
	virtual void start();
	virtual void loop();

	void config_syslog();
#ifdef ARDUINO_ARCH_ESP8266
	void config_ota();
#endif

	Network network_;

protected:
	static uuid::log::Logger logger_;

	App();

	bool local_console_enabled() { return CONSOLE_PIN >= 0 && local_console_; }

private:
	void shell_prompt();

	uuid::syslog::SyslogService syslog_;
	uuid::telnet::TelnetService telnet_;
	std::shared_ptr<AppShell> shell_;
	bool local_console_;
#ifdef ARDUINO_ARCH_ESP8266
	bool ota_running_ = false;
#endif
};

} // namespace app
