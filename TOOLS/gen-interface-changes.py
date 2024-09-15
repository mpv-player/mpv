#!/usr/bin/env python3

# Generate a new interface-changes.rst based on the entries in
# the interface-changes directory.

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

import pathlib
import sys
import textwrap
from shutil import which
from subprocess import check_output


def add_new_entries(docs_dir, out, git):
    changes_dir = pathlib.Path(docs_dir) / "interface-changes"
    files = []
    for f in pathlib.Path(changes_dir).glob("*.txt"):
        if f.is_file() and f.name != "example.txt":
            timestamp = check_output([git, "log", "--format=%ct", "-n", "1", "--",
                                      f], encoding="UTF-8")
            if timestamp:
                content = f.read_text()
                files.append(content)
            else:
                print(f"Skipping file not tracked by git: {f.name}")

    # Sort the changes by "severity", which roughly corresponds to
    # alphabetical order by accident (e.g. remove > deprecate > change > add)
    for file in reversed(sorted(files)):
        for line in file.splitlines():
            line = textwrap.fill(line.rstrip(), width=80,
                                  initial_indent="    - ",
                                  subsequent_indent="      ")
            out.write(line + "\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <version>")
        sys.exit(1)

    git = which("git")
    if not git:
        print("Unable to find git binary")
        sys.exit(1)

    # Accept passing only the major version number and the full 0 version.
    major_version = -1
    if sys.argv[1].isdigit():
        major_version = sys.argv[1]
    else:
        ver_split = sys.argv[1].split(".")
        if len(ver_split) == 3 and ver_split[1].isdigit():
            major_version = ver_split[1]

    if major_version == -1:
        print(f"Invalid version number: {sys.argv[1]}")
        sys.exit(1)

    docs_dir = pathlib.Path(sys.argv[0]).resolve().parents[1] / "DOCS"
    interface_changes = docs_dir / "interface-changes.rst"
    with open(interface_changes) as f:
        lines = [line.rstrip() for line in f]

    ver_line = " --- mpv 0." + major_version + ".0 ---"
    next_ver_line = " --- mpv 0." + str(int(major_version) + 1) + ".0 ---"
    found = False
    with open(interface_changes, "w", newline="\n") as f:
        for line in lines:
            if line == ver_line:
                f.write(next_ver_line + "\n")
            f.write(line + "\n")
            if line == ver_line:
                add_new_entries(docs_dir, f, git)
                found = True
    if not found:
        print(f"Nothing changed! The following line was not found:\n{ver_line}")
