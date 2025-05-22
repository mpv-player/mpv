#!/bin/sh
set -e

. ./ci/build-common.sh

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
