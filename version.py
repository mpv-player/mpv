#!/usr/bin/env python3

import os
import sys
import time

from datetime import datetime,timezone
from shutil import which
from subprocess import check_output

srcdir = os.path.dirname(os.path.abspath(sys.argv[0]))
git_dir = os.path.join(srcdir, ".git")
git = which('git')

if git and os.path.exists(git_dir):
    version = check_output([git, "-C", srcdir, "describe", "--always", "--tags",
                            "--dirty"], encoding="UTF-8")
    semantic_version = check_output([git, "-C", srcdir, "describe", "--always",
                                     "--tags", "--abbrev=0"], encoding="UTF-8")
    version = version[1:].strip()
    semantic_version = semantic_version[1:].strip()

    if version != semantic_version:
        # arbitrary git hash; change the point release to 99
        semantic_version = semantic_version.rsplit(".", 1)[0] + ".99"
else:
    version_path = os.path.join(srcdir, "VERSION")
    with open(version_path, "r") as f:
        version = f.readline().strip()
    semantic_version = version

if len(sys.argv) < 2:
    print(version)
    sys.exit()

date = datetime.utcfromtimestamp(int(os.environ.get('SOURCE_DATE_EPOCH', time.time())))
if date == "":
    date = datetime.now(timezone.utc).astimezone()
date_str = date.strftime("%a %b %d %I:%M:%S %Y")

NEW_REVISION = "#define VERSION \"" + version + "\"\n"
OLD_REVISION = ""
SEMANTIC_VER = "#define SEMANTIC_VERSION \"" + semantic_version + "\"\n"
BUILDDATE = "#define BUILDDATE \"" + date_str + "\"\n"
MPVCOPYRIGHT = "#define MPVCOPYRIGHT \"Copyright \u00A9 2000-2022 mpv/MPlayer/mplayer2 projects\"" + "\n"

if os.path.isfile(sys.argv[1]):
    with open(sys.argv[1], "r") as f:
        OLD_REVISION = f.readline()

if NEW_REVISION != OLD_REVISION:
    with open(sys.argv[1], "w", encoding="utf-8") as f:
        f.writelines([NEW_REVISION, SEMANTIC_VER, BUILDDATE, MPVCOPYRIGHT])

