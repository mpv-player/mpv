#!/bin/sh
set -e

python3 ./waf configure \
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
python3 ./waf build --verbose
