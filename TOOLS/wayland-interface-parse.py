#!/usr/bin/env python3
"""
Simple script to parse out interface names and version in wayland protocols
for meson.
"""

#
# This file is part of mpv.
#
# mpv is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# mpv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
#

import re
import sys

def parse_interface(line):
    line = re.sub('[<">\n]', '', line.strip())
    line = line.replace("interface name", "interface_name")
    split = line.split()
    interface = ""
    for entry in split:
        key, value = entry.split("=")
        if key == "interface_name":
            interface += value.upper() + "_VERSION "
        if key == "version":
            interface += value
    return interface

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file>")
        sys.exit(1)

    interfaces = []
    with open(sys.argv[1], 'r') as f:
        for line in f:
            if "interface name=" in line:
                interfaces.append(parse_interface(line))
    sys.stdout.write(','.join(interfaces))
