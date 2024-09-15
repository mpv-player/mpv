#!/usr/bin/env python3

# Modify X-KDE-Protocols in the mpv.desktop file based on output from
# mpv --list-protocols.

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
from subprocess import check_output

if __name__ == "__main__":
    with open(sys.argv[1], "r", encoding="UTF-8") as f:
        next(f)
        mpv_desktop = dict([line.split("=", 1) for line in f])

    if not mpv_desktop["X-KDE-Protocols"]:
        raise ValueError("Missing X-KDE-Protocols entry in mpv.desktop file")

    mpv_protocols = check_output(
        [sys.argv[2], "--no-config", "--list-protocols"],
        encoding="UTF-8",
    )
    mpv_protocols = {
        line.strip(" :/")
        for line in mpv_protocols.splitlines()
        if "://" in line
    }
    if len(mpv_protocols) == 0:
        raise ValueError("Unable to parse any protocols from mpv '--list-protocols'")

    protocol_list = set(mpv_desktop["X-KDE-Protocols"].strip().split(","))
    compatible_protocols = sorted(mpv_protocols & protocol_list)
    mpv_desktop["X-KDE-Protocols"] = ",".join(compatible_protocols) + "\n"

    with open(sys.argv[3], "w", encoding="UTF-8") as f:
        f.write("[Desktop Entry]" + "\n")
        for key, value in mpv_desktop.items():
            f.write(f"{key}={value}")
