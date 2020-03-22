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
    --enable-spirv-cross \
    --enable-d3d-hwaccel \
    --enable-d3d11 \
    --enable-jpeg \
    --enable-lcms2 \
    --enable-libarchive \
    --enable-lua \
    --enable-rubberband \
    --enable-shaderc \
    --enable-uchardet \
    --enable-vulkan
"$PYTHON" waf build
