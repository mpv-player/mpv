#!/bin/sh
set -e

meson setup build \
  --werror        \
  -Dc_args="-Wno-error=deprecated -Wno-error=deprecated-declarations" \
  -Db_sanitize=address,undefined \
  -Dcdda=enabled          \
  -Ddvbin=enabled         \
  -Ddvdnav=enabled        \
  -Dlibarchive=enabled    \
  -Dlibmpv=true           \
  -Dmanpage-build=enabled \
  -Dpipewire=enabled      \
  -Dtests=true            \
  -Dvulkan=enabled
meson compile -C build
./build/mpv -v --no-config
