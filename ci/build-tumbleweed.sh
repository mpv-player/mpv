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
    meson compile -C build
    ./build/mpv -v --no-config
fi

if [ "$1" = "waf" ]; then
    python3 ./waf configure  \
      --out=build_waf        \
      --enable-cdda          \
      --enable-dvbin         \
      --enable-dvdnav        \
      --enable-libarchive    \
      --enable-libmpv-shared \
      --enable-manpage-build \
      --enable-pipewire      \
      --enable-shaderc       \
      --enable-vulkan
    python3 ./waf build
    ./build_waf/mpv -v --no-config
fi
