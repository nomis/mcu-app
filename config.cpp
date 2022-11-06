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

#include "config.h"

#include <Arduino.h>
#ifndef ENV_NATIVE
# include <IPAddress.h>
#endif

#include <cmath>
#include <string>
#include <vector>

#include <uuid/common.h>
#include <uuid/log.h>
#include <ArduinoJson.hpp>

#include "app.h"
#include "fs.h"

#ifndef PSTR_ALIGN
# define PSTR_ALIGN 4
#endif

#define MAKE_PSTR(string_name, string_literal) static const char __pstr__##string_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = string_literal;

namespace app {
#ifdef ARDUINO_ARCH_ESP8266
# define MCU_APP_INTERNAL_CONFIG_DATA_OTA \
		MCU_APP_CONFIG_PRIMITIVE(bool, "", ota_enabled, "", true) \
		MCU_APP_CONFIG_SIMPLE(std::string, "", ota_password, "", "")
#else
# define MCU_APP_INTERNAL_CONFIG_DATA_OTA
#endif

#define MCU_APP_INTERNAL_CONFIG_DATA \
		MCU_APP_CONFIG_SIMPLE(std::string, "", admin_password, "", "") \
		MCU_APP_CONFIG_SIMPLE(std::string, "", hostname, "", "") \
		MCU_APP_CONFIG_SIMPLE(std::string, "", wifi_ssid, "", "") \
		MCU_APP_CONFIG_SIMPLE(std::string, "", wifi_password, "", "") \
		MCU_APP_CONFIG_CUSTOM(std::string, "", syslog_host, "", "") \
		MCU_APP_CONFIG_ENUM(uuid::log::Level, "", syslog_level, "", uuid::log::Level::OFF) \
		MCU_APP_CONFIG_PRIMITIVE(unsigned long, "", syslog_mark_interval, "", 0) \
		MCU_APP_INTERNAL_CONFIG_DATA_OTA \
		MCU_APP_CONFIG_DATA

/* Create member data and flash strings */
#define MCU_APP_CONFIG_SIMPLE MCU_APP_CONFIG_GENERIC
#define MCU_APP_CONFIG_CUSTOM MCU_APP_CONFIG_GENERIC
#define MCU_APP_CONFIG_PRIMITIVE MCU_APP_CONFIG_GENERIC
#define MCU_APP_CONFIG_ENUM MCU_APP_CONFIG_GENERIC
#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name##_; \
		MAKE_PSTR(__name, __key_prefix #__name __key_suffix)
MCU_APP_INTERNAL_CONFIG_DATA
#undef MCU_APP_CONFIG_GENERIC
#undef MCU_APP_CONFIG_ENUM

void Config::read_config(const ArduinoJson::JsonDocument &doc) {
#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__name(doc[FPSTR(__pstr__##__name)] | __read_default, ##__VA_ARGS__);
#define MCU_APP_CONFIG_ENUM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__name(static_cast<__type>(doc[FPSTR(__pstr__##__name)] | static_cast<int>(__read_default)), ##__VA_ARGS__);
	MCU_APP_INTERNAL_CONFIG_DATA
#undef MCU_APP_CONFIG_GENERIC
#undef MCU_APP_CONFIG_ENUM
}

void Config::write_config(ArduinoJson::JsonDocument &doc) {
#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		doc[FPSTR(__pstr__##__name)] = __name();
#define MCU_APP_CONFIG_ENUM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		doc[FPSTR(__pstr__##__name)] = static_cast<int>(__name());
	MCU_APP_INTERNAL_CONFIG_DATA
#undef MCU_APP_CONFIG_GENERIC
#undef MCU_APP_CONFIG_PRIMITIVE
#undef MCU_APP_CONFIG_ENUM
}

#undef MCU_APP_CONFIG_SIMPLE
#undef MCU_APP_CONFIG_PRIMITIVE
#undef MCU_APP_CONFIG_CUSTOM

/* Create getters/setters for simple config items */
#define MCU_APP_CONFIG_SIMPLE(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name() const { \
			return __name##_; \
		} \
		void Config::__name(const __type &__name) { \
			__name##_ = __name; \
		}
/* Create getters/setters for primitive config items */
#define MCU_APP_CONFIG_ENUM MCU_APP_CONFIG_PRIMITIVE
#define MCU_APP_CONFIG_PRIMITIVE(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name() const { \
			return __name##_; \
		} \
		void Config::__name(__type __name) { \
			__name##_ = __name; \
		}

/* Create getters for config items with custom setters */
#define MCU_APP_CONFIG_CUSTOM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__type Config::__name() const { \
			return __name##_; \
		}

MCU_APP_INTERNAL_CONFIG_DATA

#undef MCU_APP_CONFIG_SIMPLE
#undef MCU_APP_CONFIG_PRIMITIVE
#undef MCU_APP_CONFIG_CUSTOM
#undef MCU_APP_CONFIG_ENUM

static const char __pstr__config_filename[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "/config.msgpack";
static const char __pstr__config_backup_filename[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "/config.msgpack~";

static const char __pstr__logger_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "config";
uuid::log::Logger Config::logger_{FPSTR(__pstr__logger_name), uuid::log::Facility::DAEMON};

bool Config::mounted_ = false;
bool Config::unavailable_ = false;
bool Config::loaded_ = false;

Config::Config(bool mount) {
	if (!unavailable_ && !mounted_ && mount) {
		logger_.info(F("Mounting filesystem"));
		if (FS_begin(true)) {
			logger_.info(F("Mounted filesystem"));
			mounted_ = true;
		} else {
			logger_.alert(F("Unable to mount filesystem"));
			unavailable_ = true;
		}
	}

	if (mounted_ && !loaded_) {
		if (read_config(uuid::read_flash_string(FPSTR(__pstr__config_filename)))
				|| read_config(uuid::read_flash_string(FPSTR(__pstr__config_backup_filename)))) {
			loaded_ = true;
		}
	}

	if (!loaded_) {
		if (mount) {
			logger_.err(F("Config failure, using defaults"));
			read_config(ArduinoJson::StaticJsonDocument<0>());
			loaded_ = true;
		} else {
			logger_.crit(F("Config accessed before load"));
		}
	}
}

void Config::syslog_host(const std::string &syslog_host) {
#ifndef ENV_NATIVE
	IPAddress addr;

	if (addr.fromString(syslog_host.c_str())) {
		syslog_host_= syslog_host;
	} else {
		syslog_host_.clear();
	}
#endif
}

void Config::commit() {
	if (!unavailable_ && !mounted_) {
		logger_.info(F("Mounting filesystem"));
		if (FS_begin(true)) {
			logger_.info(F("Mounted filesystem"));
			mounted_ = true;
		} else {
			logger_.alert(F("Unable to mount filesystem"));
			unavailable_ = true;
		}
	}

	if (mounted_) {
		std::string filename = uuid::read_flash_string(FPSTR(__pstr__config_filename));
		std::string backup_filename = uuid::read_flash_string(FPSTR(__pstr__config_backup_filename));

		if (write_config(filename)) {
			if (read_config(filename, false)) {
				write_config(backup_filename);
			}
		}
	}
}

void Config::umount() {
	if (mounted_) {
		logger_.info(F("Unmounting filesystem"));
		FS.end();
		logger_.info(F("Unmounted filesystem"));
		mounted_ = false;
	}
}

bool Config::read_config(const std::string &filename, bool load) {
	logger_.info(F("Reading config file %s"), filename.c_str());
	File file = FS.open(filename.c_str(), "r");
	if (file) {
		ArduinoJson::DynamicJsonDocument doc(BUFFER_SIZE);

		auto error = ArduinoJson::deserializeMsgPack(doc, file);
		if (error) {
			logger_.err(F("Failed to parse config file %s: %s"), filename.c_str(), error.c_str());
			return false;
		} else {
			if (load) {
				logger_.info(F("Loading config from file %s"), filename.c_str());
				read_config(doc);
			}
			return true;
		}
	} else {
		logger_.err(F("Config file %s does not exist"), filename.c_str());
		return false;
	}
}

bool Config::write_config(const std::string &filename) {
	logger_.info(F("Writing config file %s"), filename.c_str());
	File file = FS.open(filename.c_str(), "w");
	if (file) {
		ArduinoJson::DynamicJsonDocument doc(BUFFER_SIZE);

		write_config(doc);

		ArduinoJson::serializeMsgPack(doc, file);

		if (file.getWriteError()) {
			logger_.alert(F("Failed to write config file %s: %u"), filename.c_str(), file.getWriteError());
			return false;
		} else {
			return true;
		}
	} else {
		logger_.alert(F("Unable to open config file %s for writing"), filename.c_str());
		return false;
	}
}

} // namespace app
