#!/usr/bin/env python

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

Note that you can control the mpv instance by writing to the command fifo:

    echo "cycle fullscreen" > ~/.umpv_fifo

Note: you can supply custom mpv path and options with the MPV environment
      variable. The environment variable will be split on whitespace, and the
      first item is used as path to mpv binary and the rest is passed as options
      _if_ the script starts mpv. If mpv is not started by the script (i.e. mpv
      is already running), this will be ignored.
"""

import sys
import os
import errno
import subprocess
import fcntl
import stat
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
files = [make_abs(f) for f in files]

FIFO = os.path.join(os.getenv("HOME"), ".umpv_fifo")

fifo_fd = -1
try:
    fifo_fd = os.open(FIFO, os.O_NONBLOCK | os.O_WRONLY)
except OSError as e:
    if e.errno == errno.ENXIO:
        pass  # pipe has no writer
    elif e.errno == errno.ENOENT:
        pass # doesn't exist
    else:
        raise e

if fifo_fd >= 0:
    # Unhandled race condition: what if mpv is terminating right now?
    fcntl.fcntl(fifo_fd, fcntl.F_SETFL, 0) # set blocking mode
    fifo = os.fdopen(fifo_fd, "w")
    for f in files:
        # escape: \ \n "
        f = f.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n")
        f = "\"" + f + "\""
        fifo.write("raw loadfile " + f + " append\n")
else:
    # Recreate pipe if it doesn't already exist.
    # Also makes sure it's safe, and no other user can create a bogus pipe
    # that breaks security.
    try:
        os.unlink(FIFO)
    except OSError as e:
        pass
    os.mkfifo(FIFO, 0o600)

    opts = (os.getenv("MPV") or "mpv").split()
    opts.extend(["--no-terminal", "--force-window", "--input-file=" + FIFO,
                 "--"])
    opts.extend(files)

    subprocess.check_call(opts)
