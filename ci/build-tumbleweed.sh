#!/bin/sh
set -e

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
