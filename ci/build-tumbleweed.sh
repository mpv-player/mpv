#!/bin/sh
set -e

./bootstrap.py
./waf configure \
  --enable-cdda          \
  --enable-dvdread       \
  --enable-dvdnav        \
  --enable-libmpv-shared \
  --enable-zsh-comp      \
  --enable-manpage-build \
  --enable-libarchive    \
  --enable-dvbin         \
  --enable-vulkan        \
  --enable-shaderc
./waf build --verbose
