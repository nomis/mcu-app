/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022-2024  Simon Arlott
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

#include "app/console.h"

#include <Arduino.h>
#include <time.h>

#ifdef ARDUINO_ARCH_ESP32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wswitch-enum"
# include <esp_crt_bundle.h>
# pragma GCC diagnostic pop
# include <esp_https_ota.h>
# include <esp_ota_ops.h>
# include <rom/rtc.h>
#endif

#ifdef ENV_NATIVE
# include <sys/time.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <uuid/console.h>
#include <uuid/log.h>

#include "app/app.h"
#include "app/config.h"
#include "app/console_stream.h"
#include "app/fs.h"
#include "app/network.h"
#include "app/util.h"

#ifndef PSTR_ALIGN
# define PSTR_ALIGN 4
#endif

#if defined(ARDUINO_ARCH_ESP32)
# define CONSOLE_FILESYSTEM_SUPPORTED 1
#else
# define CONSOLE_FILESYSTEM_SUPPORTED 0
#endif

using ::uuid::flash_string_vector;
using ::uuid::console::Commands;
using ::uuid::console::Shell;
using LogLevel = ::uuid::log::Level;
using LogFacility = ::uuid::log::Facility;

#define MAKE_PSTR(string_name, string_literal) static const char __pstr__##string_name[] __attribute__((__aligned__(PSTR_ALIGN))) PROGMEM = string_literal;
#define MAKE_PSTR_WORD(string_name) MAKE_PSTR(string_name, #string_name)
#define F_(string_name) FPSTR(__pstr__##string_name)

namespace app {

#pragma GCC diagnostic push
#ifndef ENV_NATIVE
# pragma GCC diagnostic error "-Wunused-const-variable"
#endif
#if !defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(bad)
#endif
MAKE_PSTR_WORD(connect)
MAKE_PSTR_WORD(console)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(cp)
#endif
MAKE_PSTR_WORD(ddns)
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(disabled)
#endif
MAKE_PSTR_WORD(disconnect)
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(enabled)
#endif
MAKE_PSTR_WORD(exit)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(fs)
#endif
#if !defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(good)
#endif
MAKE_PSTR_WORD(help)
MAKE_PSTR_WORD(host)
MAKE_PSTR_WORD(hostname)
MAKE_PSTR_WORD(level)
MAKE_PSTR_WORD(log)
MAKE_PSTR_WORD(logout)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(ls)
#endif
MAKE_PSTR_WORD(mark)
MAKE_PSTR_WORD(memory)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(mkdir)
#endif
MAKE_PSTR_WORD(mkfs)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(mv)
#endif
MAKE_PSTR_WORD(network)
#if defined(ARDUINO_ARCH_ESP8266)
MAKE_PSTR_WORD(off)
MAKE_PSTR_WORD(on)
#endif
MAKE_PSTR_WORD(ota)
MAKE_PSTR_WORD(passwd)
MAKE_PSTR_WORD(password)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(read)
#endif
MAKE_PSTR_WORD(reboot)
MAKE_PSTR_WORD(reconnect)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(rm)
MAKE_PSTR_WORD(rmdir)
#endif
MAKE_PSTR_WORD(scan)
MAKE_PSTR_WORD(set)
MAKE_PSTR_WORD(show)
MAKE_PSTR_WORD(ssid)
MAKE_PSTR_WORD(status)
MAKE_PSTR_WORD(su)
MAKE_PSTR_WORD(syslog)
MAKE_PSTR_WORD(system)
#if !defined(ARDUINO_ARCH_ESP8266) && defined(OTA_URL)
MAKE_PSTR_WORD(update)
#endif
MAKE_PSTR_WORD(uptime)
MAKE_PSTR_WORD(url)
MAKE_PSTR_WORD(version)
MAKE_PSTR_WORD(wifi)
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR_WORD(write)
#endif
MAKE_PSTR(asterisks, "********")
MAKE_PSTR(ddns_url_fmt, "DDNS URL = %s");
MAKE_PSTR(ddns_password_fmt, "DDNS Password = %S");
#if CONSOLE_FILESYSTEM_SUPPORTED
MAKE_PSTR(filename_mandatory, "<filename>")
MAKE_PSTR(filename_optional, "[filename]")
#endif
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
MAKE_PSTR(url_mandatory, "<url>")
MAKE_PSTR(wifi_ssid_fmt, "WiFi SSID = %s");
MAKE_PSTR(wifi_password_fmt, "WiFi Password = %S");
#pragma GCC diagnostic pop

static constexpr unsigned long INVALID_PASSWORD_DELAY_MS = 3000;

static inline AppShell &to_shell(Shell &shell) {
	return static_cast<AppShell&>(shell);
}

static inline App &to_app(Shell &shell) {
	return to_shell(shell).app_;
}

#define NO_ARGUMENTS std::vector<std::string>{}

#if CONSOLE_FILESYSTEM_SUPPORTED

static char encode_base64(uint8_t value) {
	if (value < 26) {
		return 'A' + value;
	} else if (value < 52) {
		return 'a' + (value - 26);
	} else if (value < 62) {
		return '0' + (value - 52);
	} else if (value == 62) {
		return '+';
	} else {
		return '/';
	}
}

static int8_t decode_base64(char value) {
	if (value >= 'A' && value <= 'Z') {
		return value - 'A';
	} else if (value >= 'a' && value <= 'z') {
		return 26 + (value - 'a');
	} else if (value >= '0' && value <= '9') {
		return 52 + (value - '0');
	} else if (value == '+') {
		return 62;
	} else if (value == '/') {
		return 63;
	} else if (value == '=') {
		return 64;
	} else {
		return -1;
	}
}

static void list_file(Shell &shell, fs::File &file) {
	std::string path = file.path();
	struct tm tm;
	time_t mtime = file.getLastWrite();

	if (file.isDirectory() && (path.empty() || path.back() != '/'))
		path.push_back('/');

	tm.tm_year = 0;
	gmtime_r(&mtime, &tm);

	if (tm.tm_year != 0) {
		shell.printfln(F("%c %7zu %04u-%02u-%02u %02u:%02u:%02u %s"),
			file.isDirectory() ? 'd' : '-', file.size(),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			path.c_str());
	} else {
		shell.printfln(F("%c %7zu [%10ld] %s"),
			file.isDirectory() ? 'd' : '-', file.size(),
			mtime, (unsigned long)file.getLastWrite(), path.c_str());
	}
}

static bool fs_allowed(Shell &shell, const std::string &filename) {
	if (shell.has_any_flags(CommandFlags::LOCAL)) {
		return true;
	} else {
		std::string path = normalise_filename(filename);

		return path.find(uuid::read_flash_string(F("/config.")), 0) != 0;
	}
}

static bool fs_valid_file(Shell &shell, const std::string &filename, bool allow_dir = false) {
	std::lock_guard lock{App::file_mutex()};
	const char mode[2] = { 'r', '\0' };
	auto file = FS.open(filename.c_str(), mode);

	if (!file) {
		shell.printfln(F("%s: file not found"), filename.c_str());
		return false;
	}

	if (file.isDirectory() && !allow_dir) {
		shell.printfln(F("%s: is a directory"), filename.c_str());
		return false;
	}

	if (!fs_allowed(shell, filename)) {
		shell.printfln(F("%s: access denied"), filename.c_str());
		return false;
	}

	return true;
}

static bool fs_valid_dir(Shell &shell, const std::string &dirname, bool must_exist = true) {
	std::lock_guard lock{App::file_mutex()};
	auto dir = FS.open(dirname.c_str());

	if (dir) {
		if (!dir.isDirectory()) {
			shell.printfln(F("%s: is not a directory"), dirname.c_str());
			return false;
		}
	} else if (must_exist) {
		shell.printfln(F("%s: directory not found"), dirname.c_str());
		return false;
	}

	if (!fs_allowed(shell, dirname)) {
		shell.printfln(F("%s: access denied"), dirname.c_str());
		return false;
	}

	return true;
}

static bool fs_valid_mv_cp(Shell &shell, const std::string &from_filename, std::string &to_filename, bool allow_dir = false) {
	std::lock_guard lock{App::file_mutex()};

	if (!fs_valid_file(shell, from_filename, allow_dir))
		return false;

	if (!fs_allowed(shell, to_filename)) {
		shell.printfln(F("%s: access denied"), to_filename.c_str());
		return false;
	}

	if (!to_filename.empty()) {
		auto file = FS.open(to_filename.c_str());
		if (file.isDirectory()) {
			if (to_filename.back() != '/')
				to_filename.push_back('/');

			to_filename.append(base_filename(from_filename));

			if (!fs_allowed(shell, to_filename)) {
				shell.printfln(F("%s: access denied"), to_filename.c_str());
				return false;
			}

			auto file2 = FS.open(to_filename.c_str());
			if (file2.isDirectory()) {
				shell.printfln(F("%s: is a directory"), to_filename.c_str());
				return false;
			}
		}
	}

	return true;
}

static std::vector<std::string> fs_autocomplete(Shell &shell,
		const std::vector<std::string> &current_arguments,
		const std::string &next_argument) {
	std::string path = next_argument.empty() ? std::string{{'/'}} : next_argument;
	std::vector<std::string> files;
	std::lock_guard lock{App::file_mutex()};

retry:
	auto dir = FS.open(path.c_str());
	if (dir) {
		if (dir.isDirectory()) {
			path = dir.path();
			if (path.empty() || path.back() != '/')
				path.push_back('/');
			files.push_back(path);

			while (1) {
				auto name = dir.getNextFileName();
				if (name.length() > 0) {
					files.emplace_back(name.c_str());
				} else {
					break;
				}
			}
		} else {
			files.emplace_back(dir.path());
		}
	} else if (path.length() > 1 && path.back() != '/') {
		while (path.length() > 1 && path.back() != '/') {
			path.pop_back();
			if (path.back() == '/')
				goto retry;
		}
	}

	std::sort(files.begin(), files.end());
	return files;
}

#endif

static void console_log_level(Shell &shell, const std::vector<std::string> &arguments) {
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
}

static std::vector<std::string> log_level_autocomplete(Shell &shell,
		const std::vector<std::string> &current_arguments,
		const std::string &next_argument) {
	return uuid::log::levels_lowercase();
}

static void setup_builtin_commands(std::shared_ptr<Commands> &commands) {
	for (unsigned int context = ShellContext::MAIN; context < ShellContext::END; context++) {
		commands->add_command(context, CommandFlags::USER, {F_(console), F_(log)}, {F_(log_level_optional)}, console_log_level, log_level_autocomplete);
		commands->add_command(context, CommandFlags::USER, {F_(exit)},
			context == ShellContext::MAIN ? AppShell::main_exit_function : AppShell::generic_exit_context_function);
		commands->add_command(context, CommandFlags::USER, {F_(help)}, AppShell::main_help_function);
		commands->add_command(context, CommandFlags::USER, {F_(logout)}, AppShell::main_logout_function);
	}

#if CONSOLE_FILESYSTEM_SUPPORTED
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(fs)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		shell.enter_context(ShellContext::FILESYSTEM);
	});
#endif

#ifndef ENV_NATIVE
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(mkfs)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		std::lock_guard lock{App::file_mutex()};
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
	});
#endif

#if !defined(ENV_NATIVE) && !defined(ARDUINO_ARCH_ESP8266)
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(ota), F_(bad)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
		if (err) {
			shell.printfln(F("Rollback failed: %d"), err);
		}
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(ota), F_(good)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
		if (err) {
			shell.printfln(F("Commit failed: %d"), err);
		}
	});

# ifdef OTA_URL
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(ota), F_(update)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		esp_http_client_config_t http_config{};
		esp_https_ota_config_t ota_config{};
		esp_https_ota_handle_t handle{};

		http_config.buffer_size = 4096;
		http_config.crt_bundle_attach = arduino_esp_crt_bundle_attach;
		http_config.keep_alive_enable = true;
		http_config.disable_auto_redirect = true;
		http_config.url = OTA_URL;
		ota_config.http_config = &http_config;

		esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
		if (err) {
			shell.printfln(F("OTA failed: %d"), err);
			return;
		}

		const int size = esp_https_ota_get_image_size(handle);
		uint64_t start_ms = uuid::get_uptime_ms();
		uint64_t last_update_ms = start_ms;
		int last_progress = -1;
		shell.printfln(F("OTA size: %d"), size);

		shell.block_with([http_config, ota_config, handle, size, start_ms, last_update_ms, last_progress]
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
					shell.printfln(F("OTA finished (%ums)"),
						uuid::get_uptime_ms() - start_ms);
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
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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

#ifndef ENV_NATIVE
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(reboot)},
		[] (Shell &shell, const std::vector<std::string> &arguments) {
			ESP.restart();
	});
#endif

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(set)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_shell(shell).set_command(shell);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(ddns), F_(url)}, flash_string_vector{F_(url_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		config.ddns_url(arguments.front());
		config.commit();
		shell.printfln(F_(ddns_url_fmt), config.ddns_url().empty() ? uuid::read_flash_string(F_(unset)).c_str() : config.ddns_url().c_str());
	},
	[] (Shell &shell, const std::vector<std::string> &current_arguments,
			const std::string &next_argument) -> std::vector<std::string> {
		Config config;
		return {config.ddns_url()};
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(ddns), F_(password)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		shell.enter_password(F_(new_password_prompt1), [] (Shell &shell, bool completed, const std::string &password1) {
				if (completed) {
					shell.enter_password(F_(new_password_prompt2), [password1] (Shell &shell, bool completed, const std::string &password2) {
						if (completed) {
							if (password1 == password2) {
								Config config;
								config.ddns_password(password2);
								config.commit();
								shell.println(F("DDNS password updated"));
							} else {
								shell.println(F("Passwords do not match"));
							}
						}
					});
				}
			});
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(hostname)}, flash_string_vector{F_(name_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;

		if (arguments.empty()) {
			config.hostname("");
		} else {
			config.hostname(arguments.front());
		}
		config.commit();
#ifndef ENV_NATIVE
		to_app(shell).config_syslog();
#endif
	});

#if defined(ARDUINO_ARCH_ESP8266)
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(set), F_(ota), F_(off)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		config.ota_enabled(false);
		config.commit();
		to_app(shell).config_ota();
		shell.printfln(F("OTA disabled"));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(ota), F_(on)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		config.ota_enabled(true);
		config.commit();
		to_app(shell).config_ota();
		shell.printfln(F("OTA enabled"));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(ota), F_(password)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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
	[] (Shell &shell, const std::vector<std::string> &current_arguments,
			const std::string &next_argument) -> std::vector<std::string> {
		Config config;
		return {config.wifi_ssid()};
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(set), F_(wifi), F_(password)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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

#ifndef ENV_NATIVE
	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(memory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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
		shell.printfln(F("PSRAM size:               %lu bytes"), (unsigned long)ESP.getPsramSize());
		shell.printfln(F("Free PSRAM:               %lu bytes"), (unsigned long)ESP.getFreePsram());
		shell.printfln(F("Minimum free PSRAM:       %lu bytes"), (unsigned long)ESP.getMinFreePsram());
		shell.printfln(F("Maximum PSRAM block size: %lu bytes"), (unsigned long)ESP.getMaxAllocPsram());
#else
# error "Unknown arch"
#endif
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(network)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_app(shell).network_.print_status(shell);
	});

#if !defined(ARDUINO_ARCH_ESP8266)
	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(ota)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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
			shell.print(' ');
			shell.println(ota_state_string(state));

			if (!esp_ota_get_partition_description(part, &desc)) {
				shell.printfln(F("    Name:      %s"), null_terminated_string(desc.project_name).c_str());
				shell.printfln(F("    Version:   %s"), null_terminated_string(desc.version).c_str());
				shell.printfln(F("    Timestamp: %s %s"),
					null_terminated_string(desc.date).c_str(),
					null_terminated_string(desc.time).c_str());
				shell.print(F("    Hash:      "));
				shell.println(HexPrintable(desc.app_elf_sha256, sizeof(desc.app_elf_sha256)));
			}
		}
	});
#endif

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(system)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
#if defined(ARDUINO_ARCH_ESP8266)
		if (0) {
			shell.printfln(F("Chip ID:       0x%08x"), ESP.getChipId());
			shell.printfln(F("SDK version:   %s"), ESP.getSdkVersion());
			shell.printfln(F("Core version:  %s"), ESP.getCoreVersion().c_str());
			shell.printfln(F("Full version:  %s"), ESP.getFullVersion().c_str());
			shell.printfln(F("Boot version:  %u"), ESP.getBootVersion());
			shell.printfln(F("Boot mode:     %u"), ESP.getBootMode());
			shell.printfln(F("CPU frequency: %u MHz"), ESP.getCpuFreqMHz());
			shell.printfln(F("Flash chip:    0x%08X (%u bytes)"), ESP.getFlashChipId(), ESP.getFlashChipRealSize());
		}
		shell.printfln(F("Reset reason:  %s"), ESP.getResetReason().c_str());
		shell.printfln(F("Reset info:    %s"), ESP.getResetInfo().c_str());
#elif defined(ARDUINO_ARCH_ESP32)
		if (0) {
			shell.printfln(F("Chip model:    %s"), ESP.getChipModel());
			shell.printfln(F("Chip revision: 0x%02x"), ESP.getChipRevision());
			shell.printfln(F("Chip cores:    %u"), ESP.getChipCores());
			shell.printfln(F("SDK version:   %s"), ESP.getSdkVersion());
			shell.printfln(F("CPU frequency: %u MHz"), ESP.getCpuFreqMHz());
			shell.printfln(F("Flash chip:    %u Hz (%u bytes)"), ESP.getFlashChipSpeed(), ESP.getFlashChipSize());
			shell.printfln(F("PSRAM size:    %u bytes"), ESP.getPsramSize());
		}
		shell.printfln(F("Reset reason:  %u/%u"), rtc_get_reset_reason(0), rtc_get_reset_reason(1));
		shell.printfln(F("Wake cause:    %u"), rtc_get_wakeup_cause());
#else
# error "Unknown arch"
#endif
		if (0) {
			shell.printfln(F("Sketch size:   %u bytes (%u bytes free)"), ESP.getSketchSize(), ESP.getFreeSketchSpace());
		}

#if defined(ARDUINO_ARCH_ESP8266)
		std::lock_guard lock{App::file_mutex()};
		FSInfo info;
		if (FS.info(info)) {
			shell.printfln(F("FS size:       %zu bytes (block size %zu bytes, page size %zu bytes)"), info.totalBytes, info.blockSize, info.pageSize);
			shell.printfln(F("FS used:       %zu bytes (%.2f%%)"), info.usedBytes, (float)info.usedBytes / (float)info.totalBytes * 100);
		}
#elif defined(ARDUINO_ARCH_ESP32)
		shell.printfln(F("FS size:       %zu bytes"), FS.totalBytes());
		shell.printfln(F("FS used:       %zu bytes (%.2f%%)"), FS.usedBytes(), (float)FS.usedBytes() / (float)FS.totalBytes() * 100);
#else
# error "Unknown arch"
#endif
	});
#endif

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(show), F_(uptime)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
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
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		shell.println(F("Version: " APP_VERSION));
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::USER, flash_string_vector{F_(su)},
			[=] (Shell &shell, const std::vector<std::string> &arguments) {
		auto become_admin = [] (Shell &shell) {
			shell.logger().log(LogLevel::NOTICE, LogFacility::AUTH, F("Admin session opened on console %s"), to_shell(shell).console_name().c_str());
			shell.add_flags(CommandFlags::ADMIN);
		};

		if (shell.has_flags(CommandFlags::ADMIN)) {
			return;
		} else if (shell.has_flags(CommandFlags::LOCAL)) {
			become_admin(shell);
		} else {
			shell.enter_password(F_(password_prompt), [=] (Shell &shell, bool completed, const std::string &password) {
				if (completed) {
					uint64_t now = uuid::get_uptime_ms();

					if (!password.empty() && password == Config().admin_password()) {
						become_admin(shell);
					} else {
						shell.delay_until(now + INVALID_PASSWORD_DELAY_MS, [] (Shell &shell) {
							shell.logger().log(LogLevel::NOTICE, LogFacility::AUTH, F("Invalid admin password on console %s"), to_shell(shell).console_name().c_str());
							shell.println(F("su: incorrect password"));
						});
					}
				}
			});
		}
	});

#ifndef ENV_NATIVE
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
	[] (Shell &shell, const std::vector<std::string> &current_arguments,
			const std::string &next_argument) -> std::vector<std::string> {
		return uuid::log::levels_lowercase();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(syslog), F_(mark)}, flash_string_vector{F_(seconds_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		Config config;
		if (!arguments.empty()) {
			config.syslog_mark_interval(std::atol(arguments[0].c_str()));
			config.commit();
		}
		shell.printfln(F_(mark_interval_is_fmt), config.syslog_mark_interval());

		to_app(shell).config_syslog();
	});
#endif

#ifndef ENV_NATIVE
	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(wifi), F_(connect)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_app(shell).network_.connect();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN | CommandFlags::LOCAL, flash_string_vector{F_(wifi), F_(disconnect)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_app(shell).network_.disconnect();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(wifi), F_(reconnect)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_app(shell).network_.reconnect();
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(wifi), F_(scan)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_app(shell).network_.scan(shell);
	});

	commands->add_command(ShellContext::MAIN, CommandFlags::ADMIN, flash_string_vector{F_(wifi), F_(status)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		to_app(shell).network_.print_status(shell);
	});
#endif

#if CONSOLE_FILESYSTEM_SUPPORTED
	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(ls)}, flash_string_vector{F_(filename_optional)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto dirname = arguments.empty() ? uuid::read_flash_string(F("/")) : arguments[0];
		std::lock_guard lock{App::file_mutex()};
		const char mode[2] = { 'r', '\0' };
		auto dir = FS.open(dirname.c_str(), mode);
		if (dir) {
			for (auto &filename : fs_autocomplete(shell, {}, dirname)) {
				auto file = FS.open(filename.c_str());
				list_file(shell, file);
			}
		} else {
			shell.printfln(F("%s: file not found"), dirname.c_str());
		}
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(mv)},
				flash_string_vector{F_(filename_mandatory), F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto &from_filename = arguments[0];
		auto to_filename = arguments[1];
		std::lock_guard lock{App::file_mutex()};

		if (!fs_valid_mv_cp(shell, from_filename, to_filename, true))
			return;

		if (!FS.rename(from_filename.c_str(), to_filename.c_str())) {
			if (!FS.open(from_filename.c_str())) {
				shell.printfln(F("%s: error"), from_filename.c_str());
			} else {
				shell.printfln(F("%s: error"), to_filename.c_str());
			}
		}
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(cp)},
				flash_string_vector{F_(filename_mandatory), F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto &from_filename = arguments[0];
		auto to_filename = arguments[1];
		std::lock_guard lock{App::file_mutex()};

		if (!fs_valid_mv_cp(shell, from_filename, to_filename))
			return;

		const char read_mode[2] = { 'r', '\0' };
		auto from_file = FS.open(from_filename.c_str(), read_mode);
		const char write_mode[2] = { 'w', '\0' };
		auto to_file = FS.open(to_filename.c_str(), write_mode, true);

		if (!to_file) {
			shell.printfln(F("%s: open error"), to_filename.c_str());
			return;
		}

		uint8_t buf[1024];
		size_t len;

		do {
			len = from_file.read(buf, sizeof(buf));
			if (len > 0) {
				if (to_file.write(buf, len) != len) {
					shell.printfln(F("%s: write error"), to_filename.c_str());
					return;
				}
			}
		} while (len > 0);
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(rm)}, flash_string_vector{F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto &filename = arguments[0];
		std::lock_guard lock{App::file_mutex()};

		if (!fs_valid_file(shell, filename))
			return;

		if (!FS.remove(filename.c_str())) {
			shell.printfln(F("%s: error"), filename.c_str());
			return;
		}
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(mkdir)}, flash_string_vector{F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto &dirname = arguments[0];
		std::lock_guard lock{App::file_mutex()};

		if (!fs_valid_dir(shell, dirname, false))
			return;

		if (!FS.mkdir(dirname.c_str())) {
			shell.printfln(F("%s: error"), dirname.c_str());
			return;
		}
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(rmdir)}, flash_string_vector{F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto &dirname = arguments[0];
		std::lock_guard lock{App::file_mutex()};

		if (!fs_valid_dir(shell, dirname))
			return;

		if (!FS.rmdir(dirname.c_str())) {
			shell.printfln(F("%s: error"), dirname.c_str());
			return;
		}
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(read)}, flash_string_vector{F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto &filename = arguments[0];
		std::lock_guard lock{App::file_mutex()};

		uint8_t buf[58];
		const char mode[2] = { 'r', '\0' };
		auto file = FS.open(filename.c_str(), mode);

		if (file) {
			if (file.isDirectory()) {
				shell.printfln(F("%s: is a directory"), filename.c_str());
				return;
			}

			if (!fs_allowed(shell, filename)) {
				shell.printfln(F("%s: access denied"), filename.c_str());
				return;
			}

			bool newline = true;
			size_t total = 0;
			size_t len;

			do {
				len = file.read(buf, sizeof(buf) - 1);

				if (len > 0) {
					size_t pos = 0;
					size_t available = len;
					bool end = false;

					buf[len] = 0;

					while (available > 0) {
						newline = false;
						shell.print(encode_base64(buf[pos] >> 2));
						shell.print(encode_base64(((buf[pos] & 0x3) << 4) | (buf[pos + 1] >> 4)));
						if (available >= 2) {
							shell.print(encode_base64(((buf[pos + 1] & 0xF) << 2) | (buf[pos + 2] >> 6)));
						} else {
							shell.print('=');
						}
						if (available >= 3) {
							shell.print(encode_base64( buf[pos + 2] & 0x3F));
						} else {
							shell.print('=');
							end = true;
							break;
						}
						pos += 3;
						available -= 3;
					}

					if (end || len == sizeof(buf) - 1) {
						shell.println();
						newline = true;
					}

					total += len;
				}
			} while (len == sizeof(buf) - 1);

			if (!newline)
				shell.println();

			shell.printfln(F("%s: read %zu"), filename.c_str(), total);
		} else {
			shell.printfln(F("%s: file not found"), filename.c_str());
		}
	}, fs_autocomplete);

	commands->add_command(ShellContext::FILESYSTEM, CommandFlags::ADMIN, flash_string_vector{F_(write)}, flash_string_vector{F_(filename_mandatory)},
			[] (Shell &shell, const std::vector<std::string> &arguments) {
		auto filename = arguments[0];
		std::lock_guard lock{App::file_mutex()};

		{
			const char mode[2] = { 'r', '\0' };
			auto file = FS.open(filename.c_str(), mode);

			if (file && file.isDirectory()) {
				shell.printfln(F("%s: is a directory"), filename.c_str());
				return;
			}
		}

		if (!fs_allowed(shell, filename)) {
			shell.printfln(F("%s: access denied"), filename.c_str());
			return;
		}

		std::vector<uint8_t> data;
		std::array<uint8_t,4> buf{};
		size_t len = 0;
		size_t padding = 0;
		bool newline = true;

		shell.block_with([filename, data, buf, len, padding, newline] (Shell &shell, bool stop) mutable -> bool {
			if (stop)
				return stop;

			int c = shell.read();

			if (c == -1)
				return stop;

			int8_t val = decode_base64(c);

			if (val >= 0) {
				shell.write(c);
				newline = false;
			} else if (c == '\r') {
				shell.println();
				newline = true;
			} else if (c == '\x03' || c == '\x1C') {
				shell.println();
				shell.println(F("Interrupted"));
				return true;
			}

			if (val == 64) {
				padding++;

				if (padding > 2) {
					if (!newline)
						shell.println();

					shell.println(F("Data error: too much padding"));
					return true;
				}
			} else if (val >= 0) {
				if (padding > 0) {
					if (!newline)
						shell.println();

					shell.println(F("Data error: content after padding"));
					return true;
				}

				buf[len] = val;
				len++;
			}

			if (len + padding >= 4) {
				if (len == 1) {
					if (!newline)
						shell.println();

					shell.println(F("Data error: incomplete byte"));
					return true;
				}

				if (len >= 2)
					data.push_back((char)(buf[0] << 2) | (buf[1] >> 4));

				if (len >= 3)
					data.push_back((char)(buf[1] << 4) | (buf[2] >> 2));

				if (len >= 4)
					data.push_back((char)(buf[2] << 6) | buf[3]);

				len = 0;
				padding = 0;
			}

			if (c == '\x04') {
				if (!newline)
					shell.println();

				if (len + padding > 0) {
					shell.println(F("Data error: incomplete sequence"));
				} else {
					std::lock_guard lock{App::file_mutex()};
					const char mode[2] = { 'w', '\0' };
					auto file = FS.open(filename.c_str(), mode, true);

					if (file) {
						if (file.write(data.data(), data.size()) != data.size()) {
							shell.printfln(F("%s: write error"), filename.c_str());
						} else {
							shell.printfln(F("%s: write %zu"), filename.c_str(), data.size());
						}
						file.close();
					} else {
						shell.printfln(F("%s: unable to open for writing"), filename.c_str());
					}
				}

				return true;
			}

			return stop;
		});
	}, fs_autocomplete);
#endif
}

__attribute__((weak)) void setup_commands(std::shared_ptr<Commands> &commands) {}

std::shared_ptr<Commands> AppShell::commands_ = [] {
	std::shared_ptr<Commands> commands = std::make_shared<Commands>();
	setup_builtin_commands(commands);
	setup_commands(commands);
	return commands;
} ();

AppShell::AppShell(App &app, Stream &stream, unsigned int context, unsigned int flags)
	: Shell(stream, commands_, context, flags), app_(app) {

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
#if defined(ARDUINO_ARCH_ESP8266)
		hostname.resize(20, '\0');

		::snprintf_P(&hostname[0], hostname.capacity() + 1, PSTR("esp-%08x"), ESP.getChipId());
#elif defined(ENV_NATIVE)
		hostname = "native";
#else
		hostname = uuid::read_flash_string(F("esp-"));
		String mac = WiFi.macAddress();
		mac.replace(F(":"), F(""));
		hostname.append(mac.c_str());
#endif
	}

	return hostname;
}

std::string AppShell::context_text() {
	auto shell_context = static_cast<ShellContext>(context());

	if (shell_context == ShellContext::MAIN) {
		return std::string{'/'};
	} else if (shell_context == ShellContext::FILESYSTEM) {
		return uuid::read_flash_string(F("/fs"));
	} else {
		return std::string{};
	}
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

void AppShell::generic_exit_context_function(Shell &shell, const std::vector<std::string> &arguments) {
	shell.exit_context();
};

void AppShell::main_help_function(Shell &shell, const std::vector<std::string> &arguments) {
	shell.print_all_available_commands();
}

void AppShell::main_exit_function(Shell &shell, const std::vector<std::string> &arguments) {
	if (shell.has_flags(CommandFlags::ADMIN)) {
		AppShell::main_exit_admin_function(shell, NO_ARGUMENTS);
	} else {
		AppShell::main_exit_user_function(shell, NO_ARGUMENTS);
	}
}

void AppShell::main_logout_function(Shell &shell, const std::vector<std::string> &arguments) {
	if (shell.has_flags(CommandFlags::ADMIN)) {
		AppShell::main_exit_admin_function(shell, NO_ARGUMENTS);
	}
	AppShell::main_exit_user_function(shell, NO_ARGUMENTS);
};

void AppShell::main_exit_user_function(Shell &shell, const std::vector<std::string> &arguments) {
	shell.stop();
};

void AppShell::main_exit_admin_function(Shell &shell, const std::vector<std::string> &arguments) {
	shell.logger().log(LogLevel::INFO, LogFacility::AUTH, "Admin session closed on console %s", to_shell(shell).console_name().c_str());
	shell.remove_flags(CommandFlags::ADMIN);
};

void AppShell::set_command(Shell &shell) {
	Config config;
	if (shell.has_flags(CommandFlags::ADMIN | CommandFlags::LOCAL)) {
		shell.printfln(F_(wifi_ssid_fmt), config.wifi_ssid().empty() ? uuid::read_flash_string(F_(unset)).c_str() : config.wifi_ssid().c_str());
		shell.printfln(F_(wifi_password_fmt), config.wifi_password().empty() ? F_(unset) : F_(asterisks));
	}
	if (shell.has_flags(CommandFlags::ADMIN)) {
		shell.printfln(F_(ddns_url_fmt), config.ddns_url().empty() ? uuid::read_flash_string(F_(unset)).c_str() : config.ddns_url().c_str());
		shell.printfln(F_(ddns_password_fmt), config.ddns_password().empty() ? F_(unset) : F_(asterisks));
	}
#if defined(ARDUINO_ARCH_ESP8266)
	if (shell.has_flags(CommandFlags::ADMIN)) {
		shell.printfln(F_(ota_enabled_fmt), config.ota_enabled() ? F_(enabled) : F_(disabled));
	}
	if (shell.has_flags(CommandFlags::ADMIN | CommandFlags::LOCAL)) {
		shell.printfln(F_(ota_password_fmt), config.ota_password().empty() ? F_(unset) : F_(asterisks));
	}
#endif
}

#ifndef ENV_NATIVE
std::vector<bool> AppConsole::ptys_;
#endif

AppConsole::AppConsole(App &app, Stream &stream, bool local)
		: APP_SHELL_TYPE(app, stream, ShellContext::MAIN,
			local ? (CommandFlags::USER | CommandFlags::LOCAL) : CommandFlags::USER),
		  name_(uuid::read_flash_string(F("ttyS0")))
#ifndef ENV_NATIVE
		  ,
		  pty_(std::numeric_limits<size_t>::max()),
		  addr_(),
		  port_(0)
#endif
		{

}

#ifndef ENV_NATIVE
AppConsole::AppConsole(App &app, Stream &stream, const IPAddress &addr, uint16_t port)
		: APP_SHELL_TYPE(app, stream, ShellContext::MAIN, CommandFlags::USER),
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
#endif

AppConsole::~AppConsole() {
#ifndef ENV_NATIVE
	if (pty_ != SIZE_MAX) {
		logger().info(F("Shutdown console %s for connection from [%s]:%u"),
			name_.c_str(), uuid::printable_to_string(addr_).c_str(), port_);

		ptys_[pty_] = false;
		ptys_.shrink_to_fit();
	}
#endif
}

std::string AppConsole::console_name() {
	return name_;
}

} // namespace app
