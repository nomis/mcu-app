/*
 * mcu-app - Microcontroller application framework
 * Copyright 2023  Simon Arlott
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

#include <Arduino.h>
#include <esp_http_client.h>
#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif

#include <atomic>
#include <memory>
#include <thread>

#include <uuid/log.h>

namespace app {

class DynamicDNS {
public:
	void loop();

private:
	class HandleDeleter {
	public:
		void operator()(esp_http_client_handle_t handle) {
			esp_http_client_cleanup(handle);
		}
	};

	static constexpr uint64_t RETRY_INTERVAL = 60 * 1000;
	static constexpr size_t TASK_STACK_SIZE = 4 * 1024;

	static uuid::log::Logger logger_;

	void run();

	IPAddress current_address_{0, 0, 0, 0};
	IPAddress remote_address_{0, 0, 0, 0};
	uint64_t last_attempt_{0};
	std::string url_;
	std::string password_;
	std::thread thread_;
	bool running_{false};
};

} // namespace app
#endif
