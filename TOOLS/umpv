#!/usr/bin/env python3

"""
This script emulates "unique application" functionality on Linux. When starting
playback with this script, it will try to reuse an already running instance of
mpv (but only if that was started with umpv). Other mpv instances (not started
by umpv) are ignored, and the script doesn't know about them.

This only takes filenames as arguments. Custom options can't be used; the script
interprets them as filenames. If mpv is already running, the files passed to
umpv are appended to mpv's internal playlist. If a file does not exist or is
otherwise not playable, mpv will skip the playlist entry when attempting to
play it (from the GUI perspective, it's silently ignored).

If mpv isn't running yet, this script will start mpv and let it control the
current terminal. It will not write output to stdout/stderr, because this
will typically just fill ~/.xsession-errors with garbage.

mpv will terminate if there are no more files to play, and running the umpv
script after that will start a new mpv instance.

Note: you can supply custom mpv path and options with the MPV environment
      variable. The environment variable will be split on whitespace, and the
      first item is used as path to mpv binary and the rest is passed as options
      _if_ the script starts mpv. If mpv is not started by the script (i.e. mpv
      is already running), this will be ignored.
"""

import sys
import os
import socket
import errno
import subprocess
import string

files = sys.argv[1:]

# this is the same method mpv uses to decide this
def is_url(filename):
    parts = filename.split("://", 1)
    if len(parts) < 2:
        return False
    # protocol prefix has no special characters => it's an URL
    allowed_symbols = string.ascii_letters + string.digits + '_'
    prefix = parts[0]
    return all(map(lambda c: c in allowed_symbols, prefix))

# make them absolute; also makes them safe against interpretation as options
def make_abs(filename):
    if not is_url(filename):
        return os.path.abspath(filename)
    return filename
files = (make_abs(f) for f in files)

SOCK = os.path.join(os.getenv("HOME"), ".umpv_socket")

sock = None
try:
    sock = socket.socket(socket.AF_UNIX)
    sock.connect(SOCK)
except socket.error as e:
    if e.errno == errno.ECONNREFUSED:
        sock = None
        pass  # abandoned socket
    elif e.errno == errno.ENOENT:
        sock = None
        pass # doesn't exist
    else:
        raise e

if sock:
    # Unhandled race condition: what if mpv is terminating right now?
    for f in files:
        # escape: \ \n "
        f = f.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n")
        f = "\"" + f + "\""
        sock.send(("raw loadfile " + f + " append\n").encode("utf-8"))
else:
    # Let mpv recreate socket if it doesn't already exist.

    opts = (os.getenv("MPV") or "mpv").split()
    opts.extend(["--no-terminal", "--force-window", "--input-ipc-server=" + SOCK,
                 "--"])
    opts.extend(files)

    subprocess.check_call(opts)
