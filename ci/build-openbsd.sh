#!/bin/sh
set -e

# libplacebo on openBSD is too old; use a subproject
rm -rf subprojects
mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --depth 1 --recurse-submodules subprojects/libplacebo

meson setup build \
    -Dlibmpv=true \
    -Dlua=enabled \
    -Dopenal=enabled \
    -Dpulse=enabled \
    -Dtests=true \
    -Dvulkan=enabled \
    -Ddvdnav=enabled \
    -Dcdda=enabled

meson compile -C build
./build/mpv -v --no-config
