#!/bin/sh
set -e

# clone exactly the oldest libplacebo we want to support
rm -rf subprojects
mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --recurse-submodules --shallow-submodules \
    --depth=1 --branch v7.349 subprojects/libplacebo \

meson setup build \
    --werror      \
    -Dlibplacebo:vulkan=disabled \
    -Dlibmpv=true \
    -Dlua=enabled \
    -Dtests=true

meson compile -C build
./build/mpv -v --no-config
