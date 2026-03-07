#!/bin/sh
set -e

. ./ci/build-common.sh

mkdir -p subprojects
git clone https://code.videolan.org/videolan/libplacebo.git \
    --recurse-submodules --shallow-submodules \
    --depth=1 --branch v7.360.1 subprojects/libplacebo

meson setup build $common_args \
  -Dffmpeg:vulkan=auto \
  -Dffmpeg:werror=false \
  -Dlua=enabled \
  -Dopenal=enabled \
  -Dpulse=enabled \
  -Dvulkan=enabled \
  -Ddvdnav=enabled \
  -Dcdda=disabled
meson compile -C build
./build/mpv -v --no-config
