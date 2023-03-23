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

#include <uuid/console.h>

#include "console.h"

#if __has_include("../../src/console_app_shell_type.h")
# include "../../src/console_app_shell_type.h"
#else
# define APP_SHELL_TYPE AppShell
#endif

namespace app {

class AppConsole: public APP_SHELL_TYPE {
public:
	AppConsole(App &app, Stream &stream, bool local);
#ifndef ENV_NATIVE
	AppConsole(App &app, Stream &stream, const IPAddress &addr, uint16_t port);
#endif
	~AppConsole() override;

	std::string console_name();

private:
#ifndef ENV_NATIVE
	static std::vector<bool> ptys_;
#endif

	std::string name_;
#ifndef ENV_NATIVE
	size_t pty_;
	IPAddress addr_;
	uint16_t port_;
#endif
};

} // namespace app
