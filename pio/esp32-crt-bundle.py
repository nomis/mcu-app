#!/usr/bin/env python3
# mcu-app - Microcontroller application framework
# Copyright 2022  Simon Arlott

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# PlatformIO usage:
#
# [env:...]
# extra_scripts = pre:app/pio/esp32-crt-bundle.py

import os
import subprocess

run = False

def genhdr_before_build(env, node):
	global run

	if not run:
		# PlatformIO sets PYTHONPATH which may break virtualenv
		subprocess.run(
			["env", "-u" "PYTHONPATH", "make", "-C", "app/pio/certs", "-L"],
			check=True, universal_newlines=True)
		run = True

	return node

if __name__ == "SCons.Script":
	Import("env")

	env.AddBuildMiddleware(genhdr_before_build, env["PROJECT_SRC_DIR"] + "/*")
