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

#include "console.h"

#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP8266
# include <ESP8266WiFi.h>
#else
# include <WiFi.h>
#endif
#include <time.h>

#ifdef ARDUINO_ARCH_ESP32
# include <esp_https_ota.h>
# include <esp_ota_ops.h>
# include <rom/rtc.h>
#endif

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <uuid/console.h>
#include <uuid/log.h>

#include "app.h"
#include "config.h"
#include "console_stream.h"
#include "fs.h"
#include "network.h"

using ::uuid::flash_string_vector;
using ::uuid::console::Commands;
using ::uuid::console::Shell;
using LogLevel = ::uuid::log::Level;
using LogFacility = ::uuid::log::Facility;

#define MAKE_PSTR(string_name, string_literal) static const char __pstr__##string_name[] __attribute__((__aligned__(sizeof(int)))) PROGMEM = string_literal;
#define MAKE_PSTR_WORD(string_name) MAKE_PSTR(string_name, #string_name)
#define F_(string_name) FPSTR(__pstr__##string_name)

namespace app {

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wunused-const-variable"
#if !defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(bad)
#endif
MAKE_PSTR_WORD(connect)
MAKE_PSTR_WORD(console)
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(disabled)
#endif
MAKE_PSTR_WORD(disconnect)
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(enabled)
#endif
MAKE_PSTR_WORD(exit)
#if !defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(good)
#endif
MAKE_PSTR_WORD(help)
MAKE_PSTR_WORD(host)
MAKE_PSTR_WORD(hostname)
MAKE_PSTR_WORD(level)
MAKE_PSTR_WORD(log)
MAKE_PSTR_WORD(logout)
MAKE_PSTR_WORD(mark)
MAKE_PSTR_WORD(memory)
MAKE_PSTR_WORD(mkfs)
MAKE_PSTR_WORD(network)
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(off)
MAKE_PSTR_WORD(on)
#endif
MAKE_PSTR_WORD(ota)
MAKE_PSTR_WORD(passwd)
MAKE_PSTR_WORD(password)
MAKE_PSTR_WORD(reconnect)
MAKE_PSTR_WORD(restart)
MAKE_PSTR_WORD(scan)
MAKE_PSTR_WORD(set)
MAKE_PSTR_WORD(show)
MAKE_PSTR_WORD(ssid)
MAKE_PSTR_WORD(status)
MAKE_PSTR_WORD(su)
MAKE_PSTR_WORD(sync)
MAKE_PSTR_WORD(syslog)
MAKE_PSTR_WORD(system)
MAKE_PSTR_WORD(umount)
#if !defined(ARDUINO_ARCH_ESP8266) && defined(OTA_URL)
MAKE_PSTR_WORD(update)
#endif
MAKE_PSTR_WORD(uptime)
MAKE_PSTR_WORD(version)
MAKE_PSTR_WORD(wifi)
MAKE_PSTR(asterisks, "********")
MAKE_PSTR(host_is_fmt, "Host = %s")
MAKE_PSTR(invalid_log_level, "Invalid log level")
MAKE_PSTR(ip_address_optional, "[IP address]")
MAKE_PSTR(log_level_is_fmt, "Log level = %s")
MAKE_PSTR(log_level_optional, "[level]")
MAKE_PSTR(mark_interval_is_fmt, "Mark interval = %lus");
MAKE_PSTR(name_mandatory, "<name>")
MAKE_PSTR(name_optional, "[name]")
MAKE_PSTR(new_password_prompt1, "Enter new password: ")
MAKE_PSTR(new_password_prompt2, "Retype new password: ")
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR(ota_enabled_fmt, "OTA %S");
MAKE_PSTR(ota_password_fmt, "OTA Password = %S");
#endif
MAKE_PSTR(password_prompt, "Password: ")
MAKE_PSTR(seconds_optional, "[seconds]")
MAKE_PSTR(unset, "<unset>")
MAKE_PSTR(wifi_ssid_fmt, "WiFi SSID = %s");
MAKE_PSTR(wifi_password_fmt, "WiFi Password = %S");
#pragma GCC diagnostic pop

static constexpr unsigned long INVALID_PASSWORD_DELAY_MS = 3000;

static inline AppShell &to_shell(Shell &shell) {
	return dynamic_cast<AppShell&>(shell);
}

static inline App &to_app(Shell &shell) {
	return to_shell(shell).app_;
}

#define NO_ARGUMENTS std::vector<std::string>{}

static void setup_builtin_commands(std::shared_ptr<Commands> &commands) {
	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(console), F_(log)}, flash_string_vector{F_(log_level_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		if (!arguments.empty()) {
			uuid::log::Level level;

			if (uuid::log::parse_level_lowercase(arguments[0], level)) {
				shell.log_level(level);
			} else {
				shell.printfln(F_(invalid_log_level));
				return;
			}
		}
		shell.printfln(F_(log_level_is_fmt), uuid::log::format_level_uppercase(shell.log_level()));
	},
	[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) -> std::vector<std::string> {
		return uuid::log::levels_lowercase();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(exit)}, AppShell::main_exit_function);

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(help)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.print_all_available_commands();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(logout)}, AppShell::main_logout_function);

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(mkfs)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		if (FS.begin()) {
			shell.logger().warning("Formatting filesystem");
			if (FS.format()) {
				auto msg = F("Formatted filesystem");
				shell.logger().warning(msg);
				shell.println(msg);
			} else {
				auto msg = F("Error formatting filesystem");
				shell.logger().emerg(msg);
				shell.println(msg);
			}
		} else {
			auto msg = F("Unable to mount filesystem");
			shell.logger().alert(msg);
			shell.println(msg);
		}
	});

#if !defined(ARDUINO_ARCH_ESP8266)
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(ota), F_(bad)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
		if (err) {
			shell.printfln(F("Rollback failed: %d"), err);
		}
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(ota), F_(good)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
		if (err) {
			shell.printfln(F("Commit failed: %d"), err);
		}
	});

# ifdef OTA_URL
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(ota), F_(update)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		static const char *root_ca =
			"-----BEGIN CERTIFICATE-----\n"
			"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\n"
			"-----END CERTIFICATE-----\n";
		esp_http_client_config_t http_config{};
		esp_https_ota_config_t ota_config{};
		esp_https_ota_handle_t handle{};

		http_config.url = OTA_URL;
		http_config.cert_pem = root_ca;
		http_config.buffer_size_tx = 512;
		ota_config.http_config = &http_config;

		esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
		if (err) {
			shell.printfln(F("OTA failed: %d"), err);
			return;
		}

		const int size = esp_https_ota_get_image_size(handle);
		uint64_t last_update_ms = uuid::get_uptime_ms();
		int last_progress = -1;
		shell.printfln(F("OTA size: %d"), size);

		shell.block_with([http_config, ota_config, handle, size, last_update_ms, last_progress]
				(Shell &shell, bool stop) mutable -> bool {
			if (stop) {
				esp_https_ota_abort(handle);
				shell.printfln(F("OTA aborted"));
				return true;
			}

			int err = esp_https_ota_perform(handle);
			int count = esp_https_ota_get_image_len_read(handle);
			int progress = (count * 100) / size;
			uint64_t now_ms = uuid::get_uptime_ms();

			if (err == ESP_OK || (now_ms - last_update_ms >= 1000 && progress != last_progress)) {
				shell.printfln(F("OTA progress: %3d%% (%d)"), progress, count);
				last_progress = progress;
				last_update_ms = now_ms;
			}

			if (err == ESP_OK) {
				err = esp_https_ota_finish(handle);
				if (err) {
					shell.printfln(F("OTA failed: %d"), err);
				} else {
					shell.printfln(F("OTA finished"));
				}

				return true;
			} else if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
				shell.printfln(F("OTA perform failed: %d"), err);
				esp_https_ota_abort(handle);
			}

			return false;
		});
	});
# endif
#endif

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(passwd)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.enter_password(F_(new_password_prompt1),
				[] (Shell &shell, bool completed, const std::string &password1) {
			if (completed) {
				shell.enter_password(F_(new_password_prompt2),
						[password1] (Shell &shell, bool completed, const std::string &password2) {
					if (completed) {
						if (password1 == password2) {
							Config config;
							config.admin_password(password2);
							config.commit();
							shell.println(F("Admin password updated"));
						} else {
							shell.println(F("Passwords do not match"));
						}
					}
				});
			}
		});
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(restart)},
		[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) {
			ESP.restart();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(set)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		if (shell.has_flags(CommandFlags::ADMIN | CommandFlags::LOCAL)) {
			shell.printfln(F_(wifi_ssid_fmt), config.wifi_ssid().empty() ? uuid::read_flash_string(F_(unset)).c_str() : config.wifi_ssid().c_str());
			shell.printfln(F_(wifi_password_fmt), config.wifi_password().empty() ? F_(unset) : F_(asterisks));
		}
#if defined(ARDUINO_ARCH_ESP8266)
		if (shell.has_flags(CommandFlags::ADMIN)) {
			shell.printfln(F_(ota_enabled_fmt), config.ota_enabled() ? F_(enabled) : F_(disabled));
		}
		if (shell.has_flags(CommandFlags::ADMIN | CommandFlags::LOCAL)) {
			shell.printfln(F_(ota_password_fmt), config.ota_password().empty() ? F_(unset) : F_(asterisks));
		}
#endif
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(hostname)}, flash_string_vector{F_(name_optional)},
			[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments) {
		Config config;

		if (arguments.empty()) {
			config.hostname("");
		} else {
			config.hostname(arguments.front());
		}
		config.commit();

		to_app(shell).config_syslog();
	});

#if defined(ARDUINO_ARCH_ESP8266)
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(ota), F_(off)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		config.ota_enabled(false);
		config.commit();
		to_app(shell).config_ota();
		shell.printfln(F("OTA disabled"));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(ota), F_(on)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		config.ota_enabled(true);
		config.commit();
		to_app(shell).config_ota();
		shell.printfln(F("OTA enabled"));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(ota), F_(password)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.enter_password(F_(new_password_prompt1), [] (Shell &shell, bool completed, const std::string &password1) {
				if (completed) {
					shell.enter_password(F_(new_password_prompt2), [password1] (Shell &shell, bool completed, const std::string &password2) {
						if (completed) {
							if (password1 == password2) {
								Config config;
								config.ota_password(password2);
								config.commit();
								to_app(shell).config_ota();
								shell.println(F("OTA password updated"));
							} else {
								shell.println(F("Passwords do not match"));
							}
						}
					});
				}
			});
	});
#endif

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(wifi), F_(ssid)}, flash_string_vector{F_(name_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		config.wifi_ssid(arguments.front());
		config.commit();
		shell.printfln(F_(wifi_ssid_fmt), config.wifi_ssid().empty() ? uuid::read_flash_string(F_(unset)).c_str() : config.wifi_ssid().c_str());
	},
	[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) -> std::vector<std::string> {
		Config config;
		return std::vector<std::string>{config.wifi_ssid()};
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(wifi), F_(password)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.enter_password(F_(new_password_prompt1), [] (Shell &shell, bool completed, const std::string &password1) {
				if (completed) {
					shell.enter_password(F_(new_password_prompt2), [password1] (Shell &shell, bool completed, const std::string &password2) {
						if (completed) {
							if (password1 == password2) {
								Config config;
								config.wifi_password(password2);
								config.commit();
								shell.println(F("WiFi password updated"));
							} else {
								shell.println(F("Passwords do not match"));
							}
						}
					});
				}
			});
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		const std::string show = uuid::read_flash_string(F("show"));
		bool first = true;

		for (auto &command : shell.available_commands()) {
			if (command.arguments().empty() && command.name().size() > 1 && command.name()[0] == show) {
				if (!first) {
					shell.println();
				} else {
					first = false;
				}
				std::vector<std::string> no_arguments;
				command.function()(shell, no_arguments);
			}
		}
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(memory)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
#if defined(ARDUINO_ARCH_ESP8266)
		shell.printfln(F("Free heap:                %lu bytes"), (unsigned long)ESP.getFreeHeap());
		shell.printfln(F("Maximum free block size:  %lu bytes"), (unsigned long)ESP.getMaxFreeBlockSize());
		shell.printfln(F("Heap fragmentation:       %u%%"), ESP.getHeapFragmentation());
		shell.printfln(F("Free continuations stack: %lu bytes"), (unsigned long)ESP.getFreeContStack());
#elif defined(ARDUINO_ARCH_ESP32)
		shell.printfln(F("Heap size:                %lu bytes"), (unsigned long)ESP.getHeapSize());
		shell.printfln(F("Free heap:                %lu bytes"), (unsigned long)ESP.getFreeHeap());
		shell.printfln(F("Minimum free heap:        %lu bytes"), (unsigned long)ESP.getMinFreeHeap());
		shell.printfln(F("Maximum heap block size:  %lu bytes"), (unsigned long)ESP.getMaxAllocHeap());
		shell.println();
		shell.printfln(F("PSRAM size:                %lu bytes"), (unsigned long)ESP.getPsramSize());
		shell.printfln(F("Free PSRAM:                %lu bytes"), (unsigned long)ESP.getFreePsram());
		shell.printfln(F("Minimum free PSRAM:        %lu bytes"), (unsigned long)ESP.getMinFreePsram());
		shell.printfln(F("Maximum PSRAM block size:  %lu bytes"), (unsigned long)ESP.getMaxAllocPsram());
#else
# error "Unknown arch"
#endif
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(network)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		to_app(shell).network_.print_status(shell);
	});

#if !defined(ARDUINO_ARCH_ESP8266)
	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(ota)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		const esp_partition_t *current = esp_ota_get_running_partition();
		const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
		const esp_partition_t *boot = esp_ota_get_boot_partition();
		const esp_partition_t *part = current;

		for (int i = 0; i < esp_ota_get_app_partition_count(); i++, part = esp_ota_get_next_update_partition(part)) {
			esp_app_desc_t desc;
			esp_ota_img_states_t state;

			if (esp_ota_get_state_partition(part, &state)) {
				state = ESP_OTA_IMG_UNDEFINED;
			}

			shell.printf(F("OTA partition %u: %s"), i, part->label);
			if (part == current) {
				shell.print(F(" [current]"));
			}
			if (part == next) {
				shell.print(F(" [next]"));
			}
			if (part == boot) {
				shell.print(F(" [boot]"));
			}
			switch (state) {
			case ESP_OTA_IMG_NEW:
				shell.print(F(" new"));
				break;
			case ESP_OTA_IMG_PENDING_VERIFY:
				shell.print(F(" pending-verify"));
				break;
			case ESP_OTA_IMG_VALID:
				shell.print(F(" valid"));
				break;
			case ESP_OTA_IMG_INVALID:
				shell.print(F(" invalid"));
				break;
			case ESP_OTA_IMG_ABORTED:
				shell.print(F(" aborted"));
				break;
			case ESP_OTA_IMG_UNDEFINED:
				shell.print(F(" undefined"));
				break;
			}
			shell.println();

			if (!esp_ota_get_partition_description(part, &desc)) {
				shell.printfln(F("    Name:      %s"), desc.project_name);
				shell.printfln(F("    Version:   %s"), desc.version);
				shell.printfln(F("    Timestamp: %s %s"), desc.date, desc.time);
				shell.print(F("    Hash:      "));
				for (int i = 0; i < sizeof(desc.app_elf_sha256); i++) {
					shell.printf(F("%02x"), desc.app_elf_sha256[i]);
				}
				shell.println();
			}
		}
	});
#endif

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(system)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
#if defined(ARDUINO_ARCH_ESP8266)
		shell.printfln(F("Chip ID:       0x%08x"), ESP.getChipId());
		shell.printfln(F("SDK version:   %s"), ESP.getSdkVersion());
		shell.printfln(F("Core version:  %s"), ESP.getCoreVersion().c_str());
		shell.printfln(F("Full version:  %s"), ESP.getFullVersion().c_str());
		shell.printfln(F("Boot version:  %u"), ESP.getBootVersion());
		shell.printfln(F("Boot mode:     %u"), ESP.getBootMode());
		shell.printfln(F("CPU frequency: %u MHz"), ESP.getCpuFreqMHz());
		shell.printfln(F("Flash chip:    0x%08X (%u bytes)"), ESP.getFlashChipId(), ESP.getFlashChipRealSize());
		shell.printfln(F("Reset reason:  %s"), ESP.getResetReason().c_str());
		shell.printfln(F("Reset info:    %s"), ESP.getResetInfo().c_str());
#elif defined(ARDUINO_ARCH_ESP32)
		shell.printfln(F("Chip model:    %s"), ESP.getChipModel());
		shell.printfln(F("Chip revision: 0x%02x"), ESP.getChipRevision());
		shell.printfln(F("Chip cores:    %u"), ESP.getChipCores());
		shell.printfln(F("SDK version:   %s"), ESP.getSdkVersion());
		shell.printfln(F("CPU frequency: %u MHz"), ESP.getCpuFreqMHz());
		shell.printfln(F("Flash chip:    %u Hz (%u bytes)"), ESP.getFlashChipSpeed(), ESP.getFlashChipSize());
		shell.printfln(F("PSRAM size:    %u bytes"), ESP.getPsramSize());
		shell.printfln(F("Reset reason:  %u/%u"), rtc_get_reset_reason(0), rtc_get_reset_reason(1));
		shell.printfln(F("Wake cause:    %u"), rtc_get_wakeup_cause());
#else
# error "Unknown arch"
#endif
		shell.printfln(F("Sketch size:   %u bytes (%u bytes free)"), ESP.getSketchSize(), ESP.getFreeSketchSpace());

#if defined(ARDUINO_ARCH_ESP8266)
		FSInfo info;
		if (FS.info(info)) {
			shell.printfln(F("FS size:       %zu bytes (block size %zu bytes, page size %zu bytes)"), info.totalBytes, info.blockSize, info.pageSize);
			shell.printfln(F("FS used:       %zu bytes (%.2f%%)"), info.usedBytes, (float)info.usedBytes / (float)info.totalBytes);
		}
#elif defined(ARDUINO_ARCH_ESP32)
		shell.printfln(F("FS size:       %zu bytes"), FS.totalBytes());
		shell.printfln(F("FS used:       %zu bytes (%.2f%%)"), FS.usedBytes(), (float)FS.usedBytes() / (float)FS.totalBytes());
#else
# error "Unknown arch"
#endif
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(uptime)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.print(F("Uptime: "));
		shell.print(uuid::log::format_timestamp_ms(uuid::get_uptime_ms(), 3));
		shell.println();

		struct timeval tv;
		// time() does not return UTC on the ESP8266: https://github.com/esp8266/Arduino/issues/4637
		if (gettimeofday(&tv, nullptr) == 0) {
			struct tm tm;

			tm.tm_year = 0;
			gmtime_r(&tv.tv_sec, &tm);

			if (tm.tm_year != 0) {
				shell.printfln(F("Time: %04u-%02u-%02uT%02u:%02u:%02u.%06luZ"),
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
						tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned long)tv.tv_usec);
			}
		}
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(version)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		shell.println(F("Version: " APP_VERSION));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(su)},
			[=] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		auto become_admin = [] (Shell &shell) {
			shell.logger().log(LogLevel::NOTICE, LogFacility::AUTH, F("Admin session opened on console %s"), dynamic_cast<AppShell&>(shell).console_name().c_str());
			shell.add_flags(CommandFlags::ADMIN);
		};

		if (shell.has_flags(CommandFlags::LOCAL)) {
			become_admin(shell);
		} else {
			shell.enter_password(F_(password_prompt), [=] (Shell &shell, bool completed, const std::string &password) {
				if (completed) {
					uint64_t now = uuid::get_uptime_ms();

					if (!password.empty() && password == Config().admin_password()) {
						become_admin(shell);
					} else {
						shell.delay_until(now + INVALID_PASSWORD_DELAY_MS, [] (Shell &shell) {
							shell.logger().log(LogLevel::NOTICE, LogFacility::AUTH, F("Invalid admin password on console %s"), dynamic_cast<AppShell&>(shell).console_name().c_str());
							shell.println(F("su: incorrect password"));
						});
					}
				}
			});
		}
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(sync)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		auto msg = F("Unable to mount filesystem");
		if (FS.begin()) {
			FS.end();
			if (!FS.begin()) {
				shell.logger().alert(msg);
			}
		} else {
			shell.logger().alert(msg);
		}
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(syslog), F_(host)}, flash_string_vector{F_(ip_address_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			config.syslog_host(arguments[0]);
			config.commit();
		}
		auto host = config.syslog_host();
		shell.printfln(F_(host_is_fmt), !host.empty() ? host.c_str() : uuid::read_flash_string(F_(unset)).c_str());
		to_app(shell).config_syslog();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(syslog), F_(level)}, flash_string_vector{F_(log_level_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			uuid::log::Level level;

			if (uuid::log::parse_level_lowercase(arguments[0], level)) {
				config.syslog_level(level);
				config.commit();
				to_app(shell).config_syslog();
			} else {
				shell.printfln(F_(invalid_log_level));
				return;
			}
		}
		shell.printfln(F_(log_level_is_fmt), uuid::log::format_level_uppercase(config.syslog_level()));
	},
	[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) -> std::vector<std::string> {
		return uuid::log::levels_lowercase();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(syslog), F_(mark)}, flash_string_vector{F_(seconds_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			config.syslog_mark_interval(String(arguments[0].c_str()).toInt());
			config.commit();
		}
		shell.printfln(F_(mark_interval_is_fmt), config.syslog_mark_interval());

		to_app(shell).config_syslog();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(umount)},
			[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) {
		Config config;
		config.umount();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(wifi), F_(connect)},
			[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) {
		to_app(shell).network_.connect();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(wifi), F_(disconnect)},
			[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) {
		to_app(shell).network_.disconnect();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(wifi), F_(reconnect)},
			[] (Shell &shell __attribute__((unused)), const std::vector<std::string> &arguments __attribute__((unused))) {
		to_app(shell).network_.reconnect();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(wifi), F_(scan)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		to_app(shell).network_.scan(shell);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(wifi), F_(status)},
			[] (Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
		to_app(shell).network_.print_status(shell);
	});
}

__attribute__((weak)) void setup_commands(std::shared_ptr<Commands> &commands __attribute__((unused))) {}

std::shared_ptr<Commands> AppShell::commands_ = [] {
	std::shared_ptr<Commands> commands = std::make_shared<Commands>();
	setup_builtin_commands(commands);
	setup_commands(commands);
	return commands;
} ();

AppShell::AppShell(App &app) : Shell(), app_(app) {

}

void AppShell::started() {
	logger().log(LogLevel::INFO, LogFacility::CONSOLE,
		F("User session opened on console %s"), console_name().c_str());
}

void AppShell::stopped() {
	if (has_flags(CommandFlags::ADMIN)) {
		logger().log(LogLevel::INFO, LogFacility::AUTH,
			F("Admin session closed on console %s"), console_name().c_str());
	}
	logger().log(LogLevel::INFO, LogFacility::CONSOLE,
		F("User session closed on console %s"), console_name().c_str());
}

void AppShell::display_banner() {
	printfln(F(APP_NAME " " APP_VERSION));
	println();
}

std::string AppShell::hostname_text() {
	Config config{false};

	std::string hostname = config.hostname();

	if (hostname.empty()) {
#ifdef ARDUINO_ARCH_ESP8266
		hostname.resize(20, '\0');

		::snprintf_P(&hostname[0], hostname.capacity() + 1, PSTR("esp-%08x"), ESP.getChipId());
#else
		hostname = uuid::read_flash_string(F("esp-"));
		String mac = WiFi.macAddress();
		mac.replace(F(":"), F(""));
		hostname.append(mac.c_str());
#endif
	}

	return hostname;
}

std::string AppShell::prompt_suffix() {
	if (has_flags(CommandFlags::ADMIN)) {
		return std::string{'#'};
	} else {
		return std::string{'$'};
	}
}

void AppShell::end_of_transmission() {
	if (context() != ShellContext::MAIN || has_flags(CommandFlags::ADMIN)) {
		invoke_command(uuid::read_flash_string(F_(exit)));
	} else {
		invoke_command(uuid::read_flash_string(F_(logout)));
	}
}

void AppShell::main_exit_function(Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
	if (shell.has_flags(CommandFlags::ADMIN)) {
		AppShell::main_exit_admin_function(shell, NO_ARGUMENTS);
	} else {
		AppShell::main_exit_user_function(shell, NO_ARGUMENTS);
	}
}

void AppShell::main_logout_function(Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
	if (shell.has_flags(CommandFlags::ADMIN)) {
		AppShell::main_exit_admin_function(shell, NO_ARGUMENTS);
	}
	AppShell::main_exit_user_function(shell, NO_ARGUMENTS);
};

void AppShell::main_exit_user_function(Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
	shell.stop();
};

void AppShell::main_exit_admin_function(Shell &shell, const std::vector<std::string> &arguments __attribute__((unused))) {
	shell.logger().log(LogLevel::INFO, LogFacility::AUTH, "Admin session closed on console %s", dynamic_cast<AppShell&>(shell).console_name().c_str());
	shell.remove_flags(CommandFlags::ADMIN);
};

std::vector<bool> AppStreamConsole::ptys_;

AppStreamConsole::AppStreamConsole(App &app, Stream &stream, bool local)
		: uuid::console::Shell(commands_, ShellContext::MAIN,
			local ? (CommandFlags::USER | CommandFlags::LOCAL) : CommandFlags::USER),
		  uuid::console::StreamConsole(stream),
		  APP_SHELL_TYPE(app),
		  name_(uuid::read_flash_string(F("ttyS0"))),
		  pty_(std::numeric_limits<size_t>::max()),
		  addr_(),
		  port_(0) {

}

AppStreamConsole::AppStreamConsole(App &app, Stream &stream, const IPAddress &addr, uint16_t port)
		: uuid::console::Shell(commands_, ShellContext::MAIN, CommandFlags::USER),
		  uuid::console::StreamConsole(stream),
		  APP_SHELL_TYPE(app),
		  addr_(addr),
		  port_(port) {
	std::array<char, 16> text;

	pty_ = 0;
	while (pty_ < ptys_.size() && ptys_[pty_])
		pty_++;
	if (pty_ == ptys_.size()) {
		ptys_.push_back(true);
	} else {
		ptys_[pty_] = true;
	}

	snprintf_P(text.data(), text.size(), PSTR("pty%u"), pty_);
	name_ = text.data();

	logger().info(F("Allocated console %s for connection from [%s]:%u"),
		name_.c_str(), uuid::printable_to_string(addr_).c_str(), port_);
}

AppStreamConsole::~AppStreamConsole() {
	if (pty_ != SIZE_MAX) {
		logger().info(F("Shutdown console %s for connection from [%s]:%u"),
			name_.c_str(), uuid::printable_to_string(addr_).c_str(), port_);

		ptys_[pty_] = false;
		ptys_.shrink_to_fit();
	}
}

std::string AppStreamConsole::console_name() {
	return name_;
}

} // namespace app
