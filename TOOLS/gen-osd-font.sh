#!/bin/sh

# This script is expected to be called as TOOLS/gen-osd-font.sh (it will access
# TOOLS/mpv-osd-symbols.sfdir), and it will write sub/osd_font.otf.

# Needs fontforge with python scripting

fontforge -lang=py -c 'f=open(argv[1]); f.generate(argv[2])' \
    TOOLS/mpv-osd-symbols.sfdir sub/osd_font.otf
