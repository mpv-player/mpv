#!/bin/sh
set -e

if [ "$1" = "meson" ]; then
    meson build \
      -Dcdda=enabled          \
      -Ddvbin=enabled         \
      -Ddvdnav=enabled        \
      -Dlibarchive=enabled    \
      -Dlibmpv=true           \
      -Dmanpage-build=enabled \
      -Dshaderc=enabled       \
      -Dvulkan=enabled
    meson compile -C build --verbose
    ./build/mpv
fi

if [ "$1" = "waf" ]; then
    python3 ./waf configure \
      --enable-cdda          \
      --enable-dvbin         \
      --enable-dvdnav        \
      --enable-libarchive    \
      --enable-libmpv-shared \
      --enable-manpage-build \
      --enable-shaderc       \
      --enable-vulkan
    python3 ./waf build --verbose
    ./build/mpv
fi
