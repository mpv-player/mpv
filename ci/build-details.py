#!/usr/bin/env python3

# Prints what a build was made from as Markdown: every git checkout and
# downloaded archive in the source tree, the compilers and the build
# environment. Run it from the source tree with the meson build directory.

import hashlib
import http.client
import json
import os
import shutil
import socket
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path


def output(*cmd: str) -> str:
    return subprocess.run(cmd, check=True, stdout=subprocess.PIPE, text=True).stdout.strip()


def git(path: str, *args: str) -> str:
    # An explicit git dir also works in a tree that another user owns. git
    # resolves the gitfile of a submodule against the last slash in it.
    return output("git", "--git-dir", path + "/.git", *args)


def table(title: str, header: tuple[str, ...], rows: Sequence[tuple[str, ...]]) -> None:
    if not rows:
        return
    widths = [max(map(len, column)) for column in zip(header, *rows)]
    print(f"### {title}\n")
    for row in (header, tuple("-" * width for width in widths), *rows):
        print("| " + " | ".join(map(str.ljust, row, widths)) + " |")
    print()


def checkouts() -> list[tuple[str, str, str, str]]:
    rows = []
    for root, dirs, files in os.walk("."):
        if ".git" in dirs or ".git" in files:
            path = Path(root).as_posix()
            commit, date = git(path, "log", "-1", "--format=%H %cs").split()
            remote = git(path, "config", "--default", "", "--get", "remote.origin.url")
            rows.append((path, commit, date, remote))
        dirs.sort()
    return rows


def archives() -> list[tuple[str, str]]:
    files = [*Path().glob("*.tar.*"), *Path("subprojects/packagecache").glob("*")]
    return [(f.as_posix(), hashlib.sha256(f.read_bytes()).hexdigest()) for f in sorted(files)]


class DockerConnection(http.client.HTTPConnection):
    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_UNIX)
        self.sock.connect("/var/run/docker.sock")


def docker(path: str) -> dict:
    connection = DockerConnection("localhost")
    connection.request("GET", path)
    return json.load(connection.getresponse())


def environment(build: Path, rust: bool) -> list[tuple[str, str]]:
    info = json.loads((build / "meson-info/meson-info.json").read_text())
    compilers = json.loads((build / "meson-info/intro-compilers.json").read_text())
    tools = {lang: c["full_version"] or c["version"] for lang, c in compilers["host"].items()}
    tools["meson"] = info["meson_version"]["full"]
    for tool in ("rustc", "cargo", "cmake", "nasm") if rust else ("cmake", "nasm"):
        if tool not in tools and shutil.which(tool):
            tools[tool] = output(tool, "--version").splitlines()[0]
    if "ImageOS" in os.environ:
        tools["runner image"] = os.environ["ImageOS"] + " " + os.environ["ImageVersion"]
    # GitHub gives job containers the Docker socket.
    if "CONTAINER_ID" in os.environ:
        container = docker("/containers/" + os.environ["CONTAINER_ID"] + "/json")
        image = docker("/images/" + container["Image"] + "/json")
        tools["container image"] = image["RepoDigests"][0]
    return list(tools.items())


sources = checkouts()
# cargo leaves this file in a checkout it has built.
rust = any(Path(path, "target/.rustc_info.json").exists() for path, *_ in sources)
table("Checkouts", ("Path", "Commit", "Date", "Remote"), sources)
table("Archives", ("File", "SHA-256"), archives())
table("Environment", ("Name", "Version"), environment(Path(sys.argv[1]), rust))
