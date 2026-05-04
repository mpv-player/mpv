#!/bin/sh
set -e

. ./ci/build-common.sh

mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --recurse-submodules --shallow-submodules \
    --depth=1 --branch v7.360.1 subprojects/libplacebo

meson setup build $common_args \
 -Dlibplacebo:vulkan=disabled \
 -Dlua=enabled \
 -Dwayland=enabled
meson compile -C build
./build/mpv -v --no-config
