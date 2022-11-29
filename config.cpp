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

#include <CBOR.h>
#include <CBOR_parsing.h>
#include <CBOR_streams.h>

#include <uuid/common.h>
#include <uuid/log.h>

#include "app.h"
#include "fs.h"

#ifndef PSTR_ALIGN
# define PSTR_ALIGN 4
#endif

#define MAKE_PSTR(string_name, string_literal) static const char __pstr__##string_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = string_literal;

namespace cbor = qindesign::cbor;

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
#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) + 1
static constexpr const size_t config_num_keys = (0 MCU_APP_INTERNAL_CONFIG_DATA);
#undef MCU_APP_CONFIG_GENERIC

void Config::read_config_defaults() {
#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		__name(__read_default, ##__VA_ARGS__);

	MCU_APP_INTERNAL_CONFIG_DATA

#undef MCU_APP_CONFIG_GENERIC
}

static bool read_map_value(cbor::Reader &reader, std::string &value) {
	uint64_t length;
	bool indefinite;

	if (!cbor::expectText(reader, &length, &indefinite) || indefinite)
		return false;

	std::vector<char> data(length + 1);

	if (cbor::readFully(reader, reinterpret_cast<uint8_t*>(data.data()), length) != length)
		return false;

	value = {data.data()};
	return true;
}

static bool read_map_value(cbor::Reader &reader, long &value) {
	int64_t value64;

	if (!cbor::expectInt(reader, &value64))
		return false;

	value = value64;
	return true;
}

static bool read_map_value(cbor::Reader &reader, unsigned long &value) {
	uint64_t value64;

	if (!cbor::expectUnsignedInt(reader, &value64))
		return false;

	value = value64;
	return true;
}

static bool read_map_key(cbor::Reader &reader, std::string &value) {
	return read_map_value(reader, value);
}

bool Config::read_config(cbor::Reader &reader) {
	uint64_t length;
	bool indefinite;

	if (!cbor::expectMap(reader, &length, &indefinite) || indefinite)
		return false;

	while (length-- > 0) {
		std::string key;

		if (!read_map_key(reader, key))
			return false;

#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		} else if (key == uuid::read_flash_string(FPSTR(__pstr__##__name))) { \
			__type value; \
			\
			if (!read_map_value(reader, value)) \
				return false; \
			\
			__name(value, ##__VA_ARGS__);

#undef MCU_APP_CONFIG_ENUM
#define MCU_APP_CONFIG_ENUM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		} else if (key == uuid::read_flash_string(FPSTR(__pstr__##__name))) { \
			long value; \
			\
			if (!read_map_value(reader, value)) \
				return false; \
			\
			__name(static_cast<__type>(value), ##__VA_ARGS__);

		if (false) {
		MCU_APP_INTERNAL_CONFIG_DATA
		} else if (!reader.isWellFormed()) {
			return false;
		}
	}

#undef MCU_APP_CONFIG_GENERIC
#undef MCU_APP_CONFIG_ENUM
#define MCU_APP_CONFIG_ENUM MCU_APP_CONFIG_GENERIC

	return true;
}

static void write_map_value(cbor::Writer &writer, const std::string &value) {
	writer.beginText(value.length());
	writer.writeBytes(reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
}

static void write_map_value(cbor::Writer &writer, long value) {
	writer.writeInt(value);
}

static void write_map_value(cbor::Writer &writer, unsigned long value) {
	writer.writeInt(value);
}

static void write_map_key(cbor::Writer &writer, const __FlashStringHelper *key) {
	write_map_value(writer, uuid::read_flash_string(key));
}

void Config::write_config(cbor::Writer &writer) {
	std::string key;

	writer.beginMap(config_num_keys);

#define MCU_APP_CONFIG_GENERIC(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		write_map_key(writer, FPSTR(__pstr__##__name)); \
		write_map_value(writer, __name());

#undef MCU_APP_CONFIG_ENUM
#define MCU_APP_CONFIG_ENUM(__type, __key_prefix, __name, __key_suffix, __read_default, ...) \
		write_map_key(writer, FPSTR(__pstr__##__name)); \
		write_map_value(writer, static_cast<long>(__name()));

	MCU_APP_INTERNAL_CONFIG_DATA

#undef MCU_APP_CONFIG_GENERIC
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

static const char __pstr__config_filename[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "/config.cbor";
static const char __pstr__config_backup_filename[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = "/config.cbor~";

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
			read_config_defaults();
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
	auto file = FS.open(filename.c_str());
	if (file) {
		cbor::Reader reader{file};

		if (!cbor::expectValue(reader, cbor::DataType::kTag, cbor::kSelfDescribeTag)
				|| !reader.isWellFormed()) {
			logger_.err(F("Failed to parse config file %s"), filename.c_str());
			return false;
		} else {
			if (load) {
				logger_.info(F("Loading config from file %s"), filename.c_str());
				file.seek(0);

				if (!cbor::expectValue(reader, cbor::DataType::kTag, cbor::kSelfDescribeTag))
					return false;

				return read_config(reader);
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
	const char mode[2] = {'w', '\0'};
	auto file = FS.open(filename.c_str(), mode);
	if (file) {
		cbor::Writer writer{file};

		writer.writeTag(cbor::kSelfDescribeTag);
		write_config(writer);

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
