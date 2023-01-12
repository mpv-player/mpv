#!/usr/bin/env python3
"""
Helper for getting a list of supported protocols in the compiled
mpv binary. Outputs a comma separated list to stdout.
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

import sys
from shutil import which
from subprocess import check_output

# Get the list of protocols in the binary and make it a comma separated string.
protocol_list = check_output([sys.argv[1], "--list-protocols"], encoding="UTF-8").splitlines()
protocol_str = ""
for protocol in protocol_list:
    if not "://" in protocol:
        continue
    protocol_str += protocol.strip().replace("://", "") + ","
# remove the trailing comma
protocol_str = protocol_str[:-1]

# Read the input and create a .desktop file with the listed protocols.
outfile = open(sys.argv[3], "w", encoding="UTF-8")
with open(sys.argv[2], 'r', encoding="UTF-8") as infile:
    for line in infile:
        if "X-KDE-Protocols=" in line:
            line = "X-KDE-Protocols=" + protocol_str + "\n"
        outfile.write(line)
