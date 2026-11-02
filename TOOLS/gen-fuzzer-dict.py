#!/usr/bin/env python3
"""
Generate dictionaries for the fuzzers from the names libmpv lists.
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

import argparse
import os
import subprocess

import matroska

# Names libmpv doesn't list
IPC = (
    "client_name", "get_time_us", "get_version", "get_property",
    "get_property_string", "set_property", "set_property_string",
    "observe_property", "observe_property_string", "unobserve_property",
    "request_log_messages", "enable_event", "disable_event", "command",
    "request_id", "async",
)
COMMAND_PREFIXES = (
    "no-osd", "osd-bar", "osd-msg", "osd-msg-bar", "osd-auto",
    "expand-properties", "raw", "repeatable", "nonrepeatable", "nonscalable",
    "async", "sync",
)
KEY_MODIFIERS = ("Shift", "Ctrl", "Alt", "Meta")
SUBTITLES = (
    # ASS
    "[Script Info]", "[V4+ Styles]", "[V4 Styles]", "[Events]", "[Fonts]",
    "[Graphics]", "ScriptType: v4.00+", "PlayResX:", "PlayResY:",
    "LayoutResX:", "LayoutResY:", "WrapStyle:", "ScaledBorderAndShadow:",
    "YCbCr Matrix:", "Kerning:", "Timer:", "Format:", "Style:", "Dialogue:",
    "Comment:", "fontname:", "filename:", "Layer", "Start", "End", "Name",
    "MarginL", "MarginR", "MarginV", "Effect", "Text", "Fontname", "Fontsize",
    "PrimaryColour", "SecondaryColour", "OutlineColour", "BackColour", "Bold",
    "Italic", "Underline", "StrikeOut", "ScaleX", "ScaleY", "Spacing", "Angle",
    "BorderStyle", "Outline", "Shadow", "Alignment", "Encoding", "Banner;",
    "Scroll up;", "Scroll down;", "{\\", "\\N", "\\n", "\\h", "\\pos(",
    "\\move(", "\\org(", "\\clip(", "\\iclip(", "\\fad(", "\\fade(", "\\t(",
    "\\an", "\\a", "\\k", "\\kf", "\\ko", "\\K", "\\q", "\\r", "\\b", "\\i",
    "\\u", "\\s", "\\bord", "\\xbord", "\\ybord", "\\shad", "\\xshad",
    "\\yshad", "\\be", "\\blur", "\\fn", "\\fs", "\\fscx", "\\fscy", "\\fsp",
    "\\frx", "\\fry", "\\frz", "\\fr", "\\fax", "\\fay", "\\fe", "\\c&H",
    "\\1c&H", "\\2c&H", "\\3c&H", "\\4c&H", "\\alpha&H", "\\1a&H", "\\2a&H",
    "\\3a&H", "\\4a&H", "\\p1", "\\pbo", "m 0 0 l ", " b ", " s ", " p ",
    " c",
    # SubRip, WebVTT
    "-->", "00:00:00,000", "00:00.000", "<i>", "</i>", "<b>", "</b>", "<u>",
    "</u>", "<s>", "</s>", "<font color=", "<font face=", "<font size=",
    "</font>", "WEBVTT", "NOTE", "STYLE", "REGION", "::cue", "align:",
    "line:", "position:", "size:", "vertical:", "<c.", "</c>", "<v ", "</v>",
    "<ruby>", "<rt>", "</rt>", "</ruby>", "<lang ", "</lang>", "&amp;",
    "&lt;", "&gt;", "&nbsp;", "&lrm;", "&rlm;",
    # MicroDVD, MPL2, SAMI, SubViewer, LRC
    "{0}{25}", "{y:i}", "{c:$", "{f:", "{s:", "{P:", "[0][10]", "<SAMI>",
    "<SYNC Start=", "<P Class=", "[INFORMATION]", "[SUBTITLE]", "[00:00.00]",
    "[ar:", "[ti:", "[offset:",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--names", required=True,
                        help="program that prints libmpv's names, see fuzzers/dict_names.c")
    parser.add_argument("--outdir", required=True)
    parser.add_argument("--dict", action="append", default=[], metavar="NAME=KIND,...")
    args = parser.parse_args()

    names: dict[str, set[str]] = {}
    out = subprocess.run([args.names], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                         check=True, text=True).stdout
    for line in out.splitlines():
        kind, _, name = line.partition(" ")
        names.setdefault(kind, set()).add(name)

    kinds: dict[str, set[str] | set[bytes]] = {
        "commands": names["command"] | set(COMMAND_PREFIXES),
        "ipc": set(IPC),
        "keys": names["key"] | set(KEY_MODIFIERS),
        "options": names["option"],
        "cli-options": {"--" + name for name in names["option"]},
        "properties": names["property"],
        "protocols": {name + "://" for name in names["protocol"]},
        "ebml": {bytes.fromhex(el.elid) for el in matroska.elementlist} | {b"matroska", b"webm"},
        "subtitles": set(SUBTITLES),
    }

    for spec in args.dict:
        name, _, wanted = spec.partition("=")
        tokens = set()
        for kind in wanted.split(","):
            tokens |= {t if isinstance(t, bytes) else t.encode() for t in kinds[kind]}
        with open(os.path.join(args.outdir, name + ".dict"), "w") as f:
            for token in sorted(tokens):
                f.write('"' + "".join(chr(b) if 0x20 <= b < 0x7f and b not in b'"\\'
                                      else f"\\x{b:02x}" for b in token) + '"\n')


if __name__ == "__main__":
    main()
