#!/bin/sh
set -e

. ./ci/build-common.sh

meson setup build $common_args \
 -Dlibplacebo:vulkan=disabled \
 -Dlua=enabled \
 -Dwayland=enabled
meson compile -C build
./build/mpv -v --no-config
