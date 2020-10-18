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
  --enable-vulkan        \
  --enable-tests

python3 ./waf build --verbose

zypper install gdb

gdb -ex=r --args ./build/mpv --unittest=all-simple
