#!/usr/bin/env python3

import os
import sys

from datetime import datetime,timezone
from subprocess import CalledProcessError, check_output, DEVNULL

try:
    version = check_output(["git", "describe", "--always", "--tags", "--dirty"],
                           encoding="UTF-8", stderr=DEVNULL)
    version = version[1:].strip()
except:
    with open("../VERSION", "r") as f:
        version = f.readline().strip()

if len(sys.argv) < 2:
    print(version)
    sys.exit()

date = datetime.now(timezone.utc).astimezone()
date_str = date.strftime("%a %b %d %I:%M:%S %p %Z %Y")

NEW_REVISION = "#define VERSION \"" + version + "\"\n"
OLD_REVISION = ""
BUILDDATE = "#define BUILDDATE \"" + date_str + "\"\n"
MPVCOPYRIGHT = "#define MPVCOPYRIGHT \"Copyright 息 2000-2021 mpv/MPlayer/mplayer2 projects\"" + "\n"

if os.path.isfile(sys.argv[1]):
    with open(sys.argv[1], "r") as f:
        OLD_REVISION = f.readline()

if NEW_REVISION != OLD_REVISION:
    with open(sys.argv[1], "w") as f:
        f.writelines([NEW_REVISION, BUILDDATE, MPVCOPYRIGHT])

