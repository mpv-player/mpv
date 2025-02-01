#!/usr/bin/env python3

"""
This script emulates "unique application" functionality. When starting
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

import os
import shlex
import socket
import string
import subprocess
import sys
from collections.abc import Iterable
from typing import BinaryIO


def is_url(filename: str) -> bool:
    parts = filename.split("://", 1)
    if len(parts) < 2:
        return False
    # protocol prefix has no special characters => it's an URL
    allowed_symbols = string.ascii_letters + string.digits + "_"
    prefix = parts[0]
    return all(c in allowed_symbols for c in prefix)

def get_socket_path() -> str:
    if os.name == "nt":
        return r"\\.\pipe\umpv"

    base_dir = (
        os.getenv("UMPV_SOCKET_DIR") or
        os.getenv("XDG_RUNTIME_DIR") or
        os.getenv("HOME") or
        os.getenv("TMPDIR")
    )

    if not base_dir:
        raise Exception("Could not determine a base directory for the socket. "
                        "Ensure that one of the following environment variables is set: "
                        "UMPV_SOCKET_DIR, XDG_RUNTIME_DIR, HOME or TMPDIR.")

    return os.path.join(base_dir, ".umpv")

def send_files_to_mpv(conn: socket.socket | BinaryIO, files: Iterable[str]) -> None:
    try:
        send = conn.send if isinstance(conn, socket.socket) else conn.write
        for f in files:
            f = f.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
            send(f'raw loadfile "{f}" append-play\n'.encode())
    except Exception:
        print("mpv is terminating or the connection was lost.", file=sys.stderr)
        sys.exit(1)

def start_mpv(files: Iterable[str], socket_path: str) -> None:
    mpv = "mpv" if os.name != "nt" else "mpv.exe"
    mpv_command = shlex.split(os.getenv("MPV", mpv))
    mpv_command.extend([
        "--profile=builtin-pseudo-gui",
        f"--input-ipc-server={socket_path}",
        "--",
    ])
    mpv_command.extend(files)
    subprocess.Popen(mpv_command, start_new_session=True)

def main() -> None:
    files = (os.path.abspath(f) if not is_url(f) else f for f in sys.argv[1:])
    socket_path = get_socket_path()

    try:
        if os.name == "nt":
            with open(socket_path, "r+b", buffering=0) as pipe:
                send_files_to_mpv(pipe, files)
        else:
            with socket.socket(socket.AF_UNIX) as sock:
                sock.connect(socket_path)
                send_files_to_mpv(sock, files)
    except (FileNotFoundError, ConnectionRefusedError):
        start_mpv(files, socket_path)

if __name__ == "__main__":
    main()
