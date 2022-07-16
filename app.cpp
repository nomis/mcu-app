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

#include "app.h"

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
# include <ArduinoOTA.h>
#endif

#ifdef ARDUINO_ARCH_ESP32
# include <esp_ota_ops.h>
# include <rom/rtc.h>
#endif

#include <initializer_list>
#include <memory>
#include <vector>

#include <uuid/common.h>
#include <uuid/console.h>
#include <uuid/log.h>
#include <uuid/modbus.h>
#include <uuid/syslog.h>
#include <uuid/telnet.h>

#include "config.h"
#include "console.h"
#include "console_stream.h"
#include "network.h"

#ifndef APP_NAME
# error "APP_NAME not defined"
#endif
#ifndef APP_VERSION
# error "APP_VERSION not defined"
#endif

static const char __pstr__logger_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = APP_NAME;

namespace app {

uuid::log::Logger App::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::KERN};

App::App() : telnet_([this] (Stream &stream, const IPAddress &addr, uint16_t port) -> std::shared_ptr<uuid::console::Shell> {
		return std::make_shared<app::AppStreamConsole>(*this, stream, addr, port);
	}) {

}

void App::start() {
	syslog_.start();
	syslog_.maximum_log_messages(100);

	if (CONSOLE_PIN >= 0) {
		pinMode(CONSOLE_PIN, INPUT_PULLUP);
		delay(1);
		local_console_ = digitalRead(CONSOLE_PIN) == LOW;
		pinMode(CONSOLE_PIN, INPUT);
	} else {
		local_console_ = true;
	}

	logger_.info(F("System startup (" APP_NAME " " APP_VERSION ")"));
#if defined(ARDUINO_ARCH_ESP8266)
	logger_.info(F("Reset: %s"), ESP.getResetInfo().c_str());
#elif defined(ARDUINO_ARCH_ESP32)
	logger_.info(F("Reset: %u/%u"), rtc_get_reset_reason(0), rtc_get_reset_reason(1));
	logger_.info(F("Wake: %u"), rtc_get_wakeup_cause());
#else
# error "unknown arch"
#endif

#if !defined(ARDUINO_ARCH_ESP8266)
	const esp_partition_t *part = esp_ota_get_running_partition();
	const esp_app_desc_t* desc = esp_ota_get_app_description();
	esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

	if (part != nullptr) {
		if (esp_ota_get_state_partition(part, &state)) {
			state = ESP_OTA_IMG_UNDEFINED;
		}
	}

	logger_.info(F("OTA partition: %s %d"), part ? part->label : nullptr, state);
	if (desc != nullptr) {
		logger_.info(F("App build: %s %s"), desc->date, desc->time);
	}
#endif

	Config config;
	if (config.wifi_ssid().empty()) {
		local_console_ = true;
	}

	if (CONSOLE_PIN >= 0) {
		logger_.info(F("Local console %S"), local_console_ ? F("enabled") : F("disabled"));
	}

	if (local_console_) {
		serial_console_.begin(SERIAL_CONSOLE_BAUD_RATE);
		serial_console_.println();
		serial_console_.println(F(APP_NAME " " APP_VERSION));
	}

	network_.start();
	config_syslog();
#if defined(ARDUINO_ARCH_ESP8266)
	config_ota();
#endif
	telnet_.default_write_timeout(1000);
	telnet_.start();

	if (local_console_) {
		shell_prompt();
	}
}

void App::loop() {
	uuid::loop();
	syslog_.loop();
	telnet_.loop();
	uuid::console::Shell::loop_all();

#if defined(ARDUINO_ARCH_ESP8266)
	if (ota_running_) {
		ArduinoOTA.handle();
	}
#endif

	if (local_console_) {
		if (shell_) {
			if (!shell_->running()) {
				shell_.reset();
				shell_prompt();
			}
		} else {
			int c = serial_console_.read();
			if (c == '\x03' || c == '\x0C') {
				shell_ = std::make_shared<AppStreamConsole>(*this, serial_console_, c == '\x0C');
				shell_->start();
			}
		}
	}
}

void App::shell_prompt() {
	serial_console_.println();
	serial_console_.println(F("Press ^C to activate this console"));
}

void App::config_syslog() {
	Config config;
	IPAddress addr;

	if (!addr.fromString(config.syslog_host().c_str())) {
		addr = (uint32_t)0;
	}

	syslog_.hostname(config.hostname());
	syslog_.log_level(config.syslog_level());
	syslog_.mark_interval(config.syslog_mark_interval());
	syslog_.destination(addr);
}

#if defined(ARDUINO_ARCH_ESP8266)
void App::config_ota() {
	Config config;

	if (ota_running_) {
		ESP.restart();
		return;
	}

	if (config.ota_enabled() && !config.ota_password().empty()) {
		ArduinoOTA.setPassword(config.ota_password().c_str());
		ArduinoOTA.onStart([this] () {
			logger_.notice("OTA start");

			Config config;
			config.umount();

			while (syslog_.current_log_messages()) {
				syslog_.loop();
			}
		});
		ArduinoOTA.onEnd([this] () {
			logger_.notice("OTA end");

			Config config;
			config.commit();

			while (syslog_.current_log_messages()) {
				syslog_.loop();
			}
		});
		ArduinoOTA.onError([this] (ota_error_t error) {
			if (error == OTA_END_ERROR) {
				logger_.notice("OTA error");
				Config config;
				config.commit();

				while (syslog_.current_log_messages()) {
					syslog_.loop();
				}
			}
		});
		ArduinoOTA.begin(false);
		logger_.info("OTA enabled");
		ota_running_ = true;
	} else if (ota_running_) {
		logger_.info("OTA disabled");
		ota_running_ = false;
	}
}
#endif

} // namespace app
