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

#ifndef ENV_NATIVE
#include "app/ddns.h"

#include <Arduino.h>
#include <StreamString.h>

#include <esp_http_client.h>
#include <esp_pthread.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#include <esp_crt_bundle.h>
#pragma GCC diagnostic pop

#if defined(ARDUINO_ARCH_ESP8266)
# include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
# include <WiFi.h>
#else
# error "Unknown arch"
#endif

#include <thread>

#include <CBOR.h>
#include <CBOR_parsing.h>
#include <CBOR_streams.h>

#include <uuid/common.h>
#include <uuid/log.h>

#include "app/config.h"
#include "app/util.h"

#ifndef PSTR_ALIGN
# define PSTR_ALIGN 4
#endif

namespace cbor = qindesign::cbor;

static const char __pstr__logger_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "ddns";

namespace app {

uuid::log::Logger DynamicDNS::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::DAEMON};

void DynamicDNS::loop() {
	if (!running_) {
		if (thread_.joinable()) {
			thread_.join();
		}

		if (current_address_ != WiFi.localIP()) {
			current_address_ = WiFi.localIP();
		}

		if (current_address_ != 0 && current_address_ != remote_address_) {
			auto now = uuid::get_uptime_ms();

			if (!last_attempt_ || now - last_attempt_ > RETRY_INTERVAL) {
				Config config;

				url_ = config.ddns_url();
				password_ = config.ddns_password();

				if (!url_.empty() && !password_.empty()) {
					try {
						auto cfg = esp_pthread_get_default_config();
						cfg.stack_size = TASK_STACK_SIZE;
						cfg.prio = uxTaskPriorityGet(nullptr);
						esp_pthread_set_cfg(&cfg);

						running_ = true;
						thread_ = std::thread{[this] {
							try {
								this->run();
							} catch (...) {
								logger_.emerg("Thread exception");
							}
							last_attempt_ = uuid::get_uptime_ms();
							running_ = false;
						}};
					} catch (...) {
						logger_.emerg("Out of memory");
						last_attempt_ = now;
					}
				} else {
					last_attempt_ = now;
				}
			}
		}
	}
}

void DynamicDNS::run() {
	auto ip = uuid::printable_to_string(current_address_);

	logger_.debug("Updating... IP %s", ip.c_str());

	std::unique_ptr<struct esp_http_client,HandleDeleter> handle_;
	esp_err_t err;
	int ret;

	{
		esp_http_client_config_t config{};

		config.crt_bundle_attach = arduino_esp_crt_bundle_attach;
		config.disable_auto_redirect = true;
		config.url = url_.c_str();

		handle_ = std::unique_ptr<struct esp_http_client,HandleDeleter>{esp_http_client_init(&config)};
		if (!handle_) {
			logger_.err(F("URL %s invalid"), url_.c_str());
			return;
		}
	}

	{
		auto mac_address = WiFi.macAddress();

		mac_address.replace(":", "");

		StreamString buffer;
		cbor::Writer writer{buffer};

		writer.beginMap(3);
		write_text(writer, "hostname");
		write_text(writer, mac_address.c_str());
		write_text(writer, "password");
		write_text(writer, password_);
		write_text(writer, "ip4");
		write_text(writer, ip);

		err = esp_http_client_open(handle_.get(), buffer.length());
		if (err != ESP_OK) {
			logger_.debug(F("POST open failed: %d"), err);
			return;
		}

		int sent = esp_http_client_write(handle_.get(), buffer.begin(), buffer.length());
		if (sent != buffer.length()) {
			logger_.debug(F("POST write failed: %d/%d"), sent, buffer.length());
			return;
		}
	}

	ret = esp_http_client_fetch_headers(handle_.get());
	if (ret < 0) {
		logger_.debug(F("Headers for POST failed: %d"), ret);
		return;
	}

	int status_code = esp_http_client_get_status_code(handle_.get());

	logger_.log(status_code != 200 ? uuid::log::Level::DEBUG : uuid::log::Level::TRACE,
		F("Status code %ld for POST"), status_code);
	if (status_code != 200)
		return;

	/* :( */
	StreamString buffer;

	buffer.reserve(256);
	while (buffer.length() < 256)
		buffer.write(0xF6);

	int received = esp_http_client_read_response(handle_.get(), buffer.begin(), buffer.length());
	if (received < 0) {
		logger_.debug(F("POST read failed: %d"), received);
		return;
	}

	logger_.trace(F("Received %d bytes"), received);

	cbor::Reader reader{buffer};
	uint64_t length;
	bool indefinite;

	if (!cbor::expectArray(reader, &length, &indefinite) || indefinite) {
		logger_.trace(F("Response does not contain a definite length array"));
		return;
	}

	if (length < 1) {
		logger_.trace(F("Response does not contain a result"));
		return;
	}

	bool success;

	if (!cbor::expectBoolean(reader, &success)) {
		logger_.trace(F("Result is not a boolean"));
		return;
	}

	if (success) {
		logger_.info("Updated IP %s", ip.c_str());
		remote_address_ = current_address_;
	} else {
		std::string message;

		if (!read_text(reader, message)) {
			logger_.trace(F("Message is not a string"));
			return;
		}

		logger_.err(F("Error: %s"), message.c_str());
	}
}

} // namespace app
#endif
