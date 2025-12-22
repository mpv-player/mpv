#!/bin/sh
set -e

. ./ci/build-common.sh
export CFLAGS="$CFLAGS -Wno-unused-function"

# clone exactly the oldest libplacebo we want to support
mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --recurse-submodules --shallow-submodules \
    --depth=1 --branch v6.338 subprojects/libplacebo
git clone https://gitlab.freedesktop.org/wayland/wayland-protocols \
    --depth=1 --branch 1.38 subprojects/wayland-protocols

meson setup build $common_args \
 -Dlibplacebo:vulkan=disabled \
 -Dlua=enabled \
 -Dwayland=enabled
meson compile -C build
./build/mpv -v --no-config
