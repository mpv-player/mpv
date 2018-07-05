#!/bin/sh
set -e

./bootstrap.py
./waf configure \
  --enable-cdda          \
  --enable-dvbin         \
  --enable-dvdnav        \
  --enable-dvdread       \
  --enable-libarchive    \
  --enable-libmpv-shared \
  --enable-libsmbclient  \
  --enable-manpage-build \
  --enable-shaderc       \
  --enable-vulkan        \
  --enable-zsh-comp
./waf build --verbose
