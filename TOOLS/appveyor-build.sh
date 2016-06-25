#!/usr/bin/bash
set -e

export DEST_OS=win32
export CC=gcc
export PKG_CONFIG=/usr/bin/pkg-config
export PERL=/usr/bin/perl
export PYTHON=/usr/bin/python3

"$PYTHON" bootstrap.py
"$PYTHON" waf configure \
    --check-c-compiler=gcc \
    --disable-cdda \
    --enable-egl-angle \
    --enable-jpeg \
    --enable-lcms2 \
    --enable-libarchive \
    --enable-libass \
    --enable-lua \
    --enable-rubberband \
    --enable-uchardet
"$PYTHON" waf build
