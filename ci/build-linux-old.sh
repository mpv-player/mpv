#!/bin/sh
set -e

# clone exactly the oldest libplacebo we want to support
rm -rf subprojects
mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --depth 1 --branch v6.338 --recurse-submodules subprojects/libplacebo

meson setup build \
    -Dlibplacebo:vulkan=disabled \
    -Dlibmpv=true \
    -Dlua=enabled \
    -Dtests=true

meson compile -C build
./build/mpv -v --no-config
