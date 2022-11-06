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

#include <string>

#include <uuid/log.h>

#include "json.h"

namespace app {

class Config {
public:
	Config(bool mount = true);
	~Config() = default;

	std::string admin_password() const;
	void admin_password(const std::string &admin_password);

	std::string hostname() const;
	void hostname(const std::string &hostname);

	std::string wifi_ssid() const;
	void wifi_ssid(const std::string &wifi_ssid);

	std::string wifi_password() const;
	void wifi_password(const std::string &wifi_password);

	std::string syslog_host() const;
	void syslog_host(const std::string &syslog_host);

	uuid::log::Level syslog_level() const;
	void syslog_level(uuid::log::Level syslog_level);

	unsigned long syslog_mark_interval() const;
	void syslog_mark_interval(unsigned long syslog_mark_interval);

#if defined(ARDUINO_ARCH_ESP8266)
	bool ota_enabled() const;
	void ota_enabled(bool ota_enabled);

	std::string ota_password() const;
	void ota_password(const std::string &ota_password);
#endif

	void commit();
	void umount();

private:
	static constexpr size_t BUFFER_SIZE = 4096;

	static uuid::log::Logger logger_;

	static bool mounted_;
	static bool unavailable_;
	static bool loaded_;

	static std::string admin_password_;
	static std::string hostname_;
	static std::string wifi_password_;
	static std::string wifi_ssid_;
	static std::string syslog_host_;
	static uuid::log::Level syslog_level_;
	static unsigned long syslog_mark_interval_;
#if defined(ARDUINO_ARCH_ESP8266)
	static bool ota_enabled_;
	static std::string ota_password_;
#endif

	bool read_config(const std::string &filename, bool load = true);
	void read_config(const app::JsonDocument &doc);
	bool write_config(const std::string &filename);
	void write_config(app::JsonDocument &doc);

#if __has_include("../config_class.h")
# include "../config_class.h"
#else
# define MCU_APP_CONFIG_DATA
#endif
};

} // namespace app
