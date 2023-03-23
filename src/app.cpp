/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022-2023  Simon Arlott
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

#include "app/app.h"

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
# include <ArduinoOTA.h>
#endif

#ifdef ARDUINO_ARCH_ESP32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wswitch-enum"
# include <esp_crt_bundle.h>
# pragma GCC diagnostic pop
# include <esp_ota_ops.h>
# include <rom/rtc.h>
# include <rom/spi_flash.h>
#endif

#ifdef ENV_NATIVE
# include <stdlib.h>
# include <sys/types.h>
# include <time.h>
# include <unistd.h>
#endif

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include <uuid/common.h>
#include <uuid/console.h>
#include <uuid/log.h>
#ifndef ENV_NATIVE
# include <uuid/syslog.h>
# include <uuid/telnet.h>
#endif

#include "app/config.h"
#include "app/console.h"
#include "app/console_stream.h"
#include "app/fs.h"
#include "app/network.h"
#include "app/util.h"

#ifndef APP_NAME
# error "APP_NAME not defined"
#endif
#ifndef APP_VERSION
# error "APP_VERSION not defined"
#endif

#ifdef ARDUINO_ARCH_ESP32
static_assert(uuid::thread_safe, "uuid-common must be thread-safe");
static_assert(uuid::log::thread_safe, "uuid-log must be thread-safe");
static_assert(uuid::syslog::thread_safe, "uuid-syslog must be thread-safe");
static_assert(uuid::console::thread_safe, "uuid-console must be thread-safe");

extern const uint8_t x509_crt_bundle_start[] asm("_binary_app_pio_certs_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_app_pio_certs_x509_crt_bundle_end");

# if !CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#  error "Need CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE for OTA"
# endif
#endif

#ifndef PSTR_ALIGN
# define PSTR_ALIGN 4
#endif

static const char __pstr__logger_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = APP_NAME;

namespace app {

uuid::log::Logger App::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::KERN};

App::App()
#ifndef ENV_NATIVE
		: telnet_([this] (Stream &stream, const IPAddress &addr, uint16_t port) -> std::shared_ptr<uuid::console::Shell> {
			return std::make_shared<app::AppConsole>(*this, stream, addr, port);
		})
#endif
	{

}

void App::init() {
#ifdef ENV_NATIVE
	shell_ = std::make_shared<AppConsole>(*this, serial_console_, true);
	shell_->start();
	shell_->log_level(uuid::log::Level::TRACE);
#else
	syslog_.start();
	syslog_.maximum_log_messages(100);
#endif

	logger_.info(F("System startup (" APP_NAME " " APP_VERSION ")"));

	if (FS_begin(true)) {
		logger_.debug(F("Mounted filesystem"));
	} else {
		logger_.emerg(F("Unable to mount filesystem"));
	}
}

void App::start() {
	init();

#ifndef ENV_NATIVE
	if (CONSOLE_PIN >= 0) {
		pinMode(CONSOLE_PIN, INPUT_PULLUP);
		delay(1);
		local_console_ = digitalRead(CONSOLE_PIN) == LOW;
		pinMode(CONSOLE_PIN, INPUT);
	} else {
		local_console_ = true;
	}
#endif

#if defined(ARDUINO_ARCH_ESP8266)
	logger_.info(F("Reset: %s"), ESP.getResetInfo().c_str());
#elif defined(ARDUINO_ARCH_ESP32)
	logger_.info(F("Reset: %u/%u (%s/%s)"),
		rtc_get_reset_reason(0), rtc_get_reset_reason(1),
		reset_reason_string(rtc_get_reset_reason(0)).c_str(),
		reset_reason_string(rtc_get_reset_reason(1)).c_str());
	logger_.info(F("Wake: %u (%s)"), rtc_get_wakeup_cause(),
		wakeup_cause_string(rtc_get_wakeup_cause()).c_str());
#elif defined(ENV_NATIVE)
#else
# error "unknown arch"
#endif

#if !defined(ENV_NATIVE) && !defined(ARDUINO_ARCH_ESP8266)
	const esp_partition_t *part = esp_ota_get_running_partition();
	const esp_app_desc_t* desc = esp_ota_get_app_description();
	esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

	if (part != nullptr) {
		if (esp_ota_get_state_partition(part, &state)) {
			state = ESP_OTA_IMG_UNDEFINED;
		}
	}

	logger_.info(F("OTA partition: %s %d (%S)"), part ? part->label : nullptr, state, ota_state_string(state));
	if (desc != nullptr) {
		logger_.info(F("App build: %s %s"), desc->date, desc->time);
		app_hash_ = hex_string(desc->app_elf_sha256, sizeof(desc->app_elf_sha256));
		logger_.info(F("App hash: %s"), app_hash_.c_str());
	}
#endif

#ifdef ENV_NATIVE
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	app_hash_ = std::to_string(getuid()) + "-" + std::to_string(getgid())
		+ "-" + std::to_string(getpid()) + "-" + std::to_string(now.tv_sec)
		+ "." + std::to_string(now.tv_nsec);
#endif

#ifndef ENV_NATIVE
# if defined(ARDUINO_ARCH_ESP8266)
	uint32_t flash_chip_id = ESP.getFlashChipId();
# elif defined(ARDUINO_ARCH_ESP32)
	uint32_t flash_chip_id = g_rom_flashchip.device_id;
# else
#  error "unknown arch"
#endif
	if ((flash_chip_id & 0xFF0000) == 0x200000) {
		// https://github.com/espressif/esp-idf/issues/7994
		logger_.warning(F("Flash chip %08x may be vulnerable to issue espressif/esp-idf#7994"), flash_chip_id);
		// xmc_check_lock_sr(true);
	} else {
		logger_.trace(F("Flash chip %08x ok"), flash_chip_id);
	}
#endif

	Config config;
	if (config.wifi_ssid().empty()) {
		local_console_ = true;
	}

#ifndef ENV_NATIVE
	if (CONSOLE_PIN >= 0) {
		logger_.info(F("Local console %S"), local_console_ ? F("enabled") : F("disabled"));
	}

	if (local_console_) {
		serial_console_.begin(SERIAL_CONSOLE_BAUD_RATE);
		serial_console_.println();
		serial_console_.println(F(APP_NAME " " APP_VERSION));
	}

#if defined(ARDUINO_ARCH_ESP32)
	uint16_t num_certs = 0;

	if (x509_crt_bundle_end - x509_crt_bundle_start >= 2) {
		num_certs = (x509_crt_bundle_start[0] << 8) | x509_crt_bundle_start[1];

		arduino_esp_crt_bundle_set(x509_crt_bundle_start);
	}

	if (num_certs > 0) {
		logger_.trace(F("Configured %u CA certificates"), num_certs);
	} else {
		logger_.crit(F("No CA certificates"));
	}
#endif

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
#endif
}

void App::loop() {
	uuid::loop();
#ifndef ENV_NATIVE
	syslog_.loop();
	telnet_.loop();
#endif
	uuid::console::Shell::loop_all();

#if defined(ARDUINO_ARCH_ESP8266)
	if (ota_running_) {
		ArduinoOTA.handle();
	}
#endif

#ifdef ENV_NATIVE
	if (!shell_->running()) {
		::exit(0);
	}
#else
	if (local_console_) {
		if (shell_) {
			if (!shell_->running()) {
				shell_.reset();
				shell_prompt();
			}
		} else {
			int c = serial_console_.read();
			if (c == '\x03' || c == '\x0C') {
				shell_ = std::make_shared<AppConsole>(*this, serial_console_, c == '\x0C');
				shell_->start();
			}
		}
	}
#endif
}

void App::exception(const __FlashStringHelper *where) {
	uint64_t uptime = uuid::get_uptime_ms();

	serial_console_.begin(SERIAL_CONSOLE_BAUD_RATE);

	while (1) {
		serial_console_.printf(
			uuid::read_flash_string(F("%s Exception in %S at %s (" APP_NAME " " APP_VERSION ")\r\n")).c_str(),
			uuid::log::format_timestamp_ms(uuid::get_uptime_ms()).c_str(),
			where, uuid::log::format_timestamp_ms(uptime).c_str());
		delay(1000);
		::yield();
	}
}

void App::shell_prompt() {
	serial_console_.println();
	serial_console_.println(F("Press ^C to activate this console"));
}

#ifndef ENV_NATIVE
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
#endif

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
