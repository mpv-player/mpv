#!/bin/sh
set -e

. ./ci/build-common.sh

meson setup build $common_args \
  -Db_sanitize=address,undefined \
  -Dcdda=enabled \
  -Ddvbin=enabled \
  -Ddvdnav=enabled \
  -Dlibarchive=enabled \
  -Dmanpage-build=enabled \
  -Dpipewire=enabled \
  -Dvulkan=enabled
meson compile -C build
./build/mpv -v --no-config
