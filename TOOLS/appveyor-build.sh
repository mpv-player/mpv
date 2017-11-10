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
    --disable-egl-angle-lib \
    --enable-crossc \
    --enable-d3d-hwaccel \
    --enable-d3d11 \
    --enable-egl-angle \
    --enable-jpeg \
    --enable-lcms2 \
    --enable-libarchive \
    --enable-libass \
    --enable-lua \
    --enable-rubberband \
    --enable-shaderc \
    --enable-uchardet \
    --enable-vulkan
"$PYTHON" waf build
