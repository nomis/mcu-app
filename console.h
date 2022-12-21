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

#include <memory>
#include <string>
#include <vector>

#include "app.h"

#ifdef LOCAL
# undef LOCAL
#endif

namespace app {

enum CommandFlags : unsigned int {
	USER = 0,
	ADMIN = (1 << 0),
	LOCAL = (1 << 1),
};

enum ShellContext : unsigned int {
	MAIN = 0,
	FILESYSTEM,
#if __has_include("../console_shellcontext_enum.h")
# include "../console_shellcontext_enum.h"
#endif
	END
};

class App;

class AppShell: public uuid::console::Shell {
public:
	~AppShell() override = default;

	virtual std::string console_name() = 0;

	static void generic_exit_context_function(Shell &shell, const std::vector<std::string> &arguments);
	static void main_help_function(Shell &shell, const std::vector<std::string> &arguments);
	static void main_exit_function(Shell &shell, const std::vector<std::string> &arguments);
	static void main_logout_function(Shell &shell, const std::vector<std::string> &arguments);

	App &app_;

protected:
	AppShell(App &app, Stream &stream, unsigned int context, unsigned int flags);

	static std::shared_ptr<uuid::console::Commands> commands_;

	void started() override;
	void display_banner() override;
	std::string hostname_text() override;
	std::string context_text() override;
	std::string prompt_suffix() override;
	void end_of_transmission() override;
	void stopped() override;

private:
	static void main_exit_user_function(Shell &shell, const std::vector<std::string> &arguments);
	static void main_exit_admin_function(Shell &shell, const std::vector<std::string> &arguments);
};

} // namespace app
