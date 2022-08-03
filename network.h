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
#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif

#include <uuid/console.h>
#include <uuid/log.h>

namespace app {

class Network {
public:
	void start();
	void connect();
	void reconnect();
	void disconnect();
	void scan(uuid::console::Shell &shell);
	void print_status(uuid::console::Shell &shell);

private:
	static uuid::log::Logger logger_;

#if defined(ARDUINO_ARCH_ESP8266)
	void sta_mode_connected(const WiFiEventStationModeConnected &event);
	void sta_mode_disconnected(const WiFiEventStationModeDisconnected &event);
	void sta_mode_got_ip(const WiFiEventStationModeGotIP &event);
	void sta_mode_dhcp_timeout();
#elif defined(ARDUINO_ARCH_ESP32)
	void sta_mode_connected(arduino_event_id_t event, arduino_event_info_t info);
	void sta_mode_disconnected(arduino_event_id_t event, arduino_event_info_t info);
	void sta_mode_got_ip(arduino_event_id_t event, arduino_event_info_t info);

# ifndef MANUAL_NTP
#  if !defined(CONFIG_LWIP_DHCP_GET_NTP_SRV) || CONFIG_LWIP_DHCP_GET_NTP_SRV != 1
#   define MANUAL_NTP
#  endif
# endif
# ifdef MANUAL_NTP
	void configure_ntp();
# endif

#else
# error "Unknown arch"
#endif
};

} // namespace app
