#!/usr/bin/env python3
"""
Wrapper around docutils rst2x commands,
converting their dependency files to a format understood by meson/ninja.
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

import os
import subprocess
import sys


def convert_depfile(output, depfile):
    with open(depfile, "r") as f:
        deps = f.readlines()

    with open(depfile, "w") as f:
        f.write(os.path.abspath(output))
        f.write(": \\\n")
        for dep in deps:
            dep = dep[:-1]
            f.write("\t")
            f.write(os.path.abspath(dep))
            f.write(" \\\n")

def remove(path):
    try:
        os.remove(path)
    except FileNotFoundError:
        pass

argv = sys.argv[1:]

depfile = None
output = argv[-1]

for opt, optarg in zip(argv, argv[1:]):
    if opt == "--record-dependencies":
        depfile = optarg

try:
    proc = subprocess.run(argv, check=True)
    if depfile is not None:
        convert_depfile(output, depfile)
except Exception:
    remove(output)
    if depfile is not None:
        remove(depfile)
    sys.exit(1)

sys.exit(proc.returncode)
