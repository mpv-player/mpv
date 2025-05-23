#!/bin/sh
set -e

. ./ci/build-common.sh

# clone exactly the oldest libplacebo we want to support
rm -rf subprojects
mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --recurse-submodules --shallow-submodules \
    --depth=1 --branch v6.338 subprojects/libplacebo \

meson setup build $common_args \
 -Dlibplacebo:vulkan=disabled \
 -Dlua=enabled
meson compile -C build
./build/mpv -v --no-config
