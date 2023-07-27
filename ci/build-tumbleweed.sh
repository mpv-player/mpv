#!/bin/sh
set -e

meson setup build \
  -Db_sanitize=address,undefined \
  -Dcdda=enabled          \
  -Ddvbin=enabled         \
  -Ddvdnav=enabled        \
  -Dlibarchive=enabled    \
  -Dlibmpv=true           \
  -Dmanpage-build=enabled \
  -Dpipewire=enabled      \
  -Dshaderc=enabled       \
  -Dtests=true            \
  -Dvulkan=enabled
meson compile -C build
./build/mpv -v --no-config
