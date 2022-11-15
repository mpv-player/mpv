#!/bin/sh
set -e

if [ "$1" = "meson" ]; then
    meson setup build \
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
    meson compile -C build --verbose
    ./build/mpv --no-config -v --unittest=all-simple
fi

if [ "$1" = "waf" ]; then
    python3 ./waf configure \
      --enable-cdda          \
      --enable-dvbin         \
      --enable-dvdnav        \
      --enable-libarchive    \
      --enable-libmpv-shared \
      --enable-manpage-build \
      --enable-pipewire      \
      --enable-shaderc       \
      --enable-tests         \
      --enable-vulkan
    python3 ./waf build --verbose
    ./build/mpv -v --no-config -v --unittest=all-simple
fi
