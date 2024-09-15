#!/usr/bin/env python3

# Finds the macos swift library directory and prints the full path to stdout.
# First argument is the path to the swift executable.

import os
import sys
from shutil import which
from subprocess import check_output

def find_swift_lib():
    swift_lib_dir = os.environ.get("SWIFT_LIB_DYNAMIC", "")
    if swift_lib_dir:
        return swift_lib_dir

    # first check for lib dir relative to swift executable
    xcode_dir = os.path.dirname(os.path.dirname(sys.argv[1]))
    swift_lib_dir = os.path.join(xcode_dir, "lib", "swift", "macosx")

    if os.path.isdir(swift_lib_dir):
        return swift_lib_dir

    # fallback to xcode-select path
    xcode_select = which("xcode-select")
    if not xcode_select:
        sys.exit(1)

    xcode_path = check_output([xcode_select, "-p"], encoding="UTF-8")

    swift_lib_dir = os.path.join(
        xcode_path,
        "Toolchains/XcodeDefault.xctoolchain/usr/lib/swift/macosx"
    )
    if os.path.isdir(swift_lib_dir):
        return swift_lib_dir

    # last resort if we still haven't found a path
    swift_lib_dir = os.path.join(xcode_path, "usr/lib/swift/macosx")
    if not os.path.isdir(swift_lib_dir):
        sys.exit(1)
    return swift_lib_dir

if __name__ == "__main__":
    swift_lib_dir = find_swift_lib()
    sys.stdout.write(swift_lib_dir)
