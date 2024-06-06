#!/bin/sh -e

meson setup build            \
  --werror                   \
  -Dc_args="-Wno-error=deprecated -Wno-error=deprecated-declarations" \
  -D cdda=enabled            \
  -D d3d-hwaccel=enabled     \
  -D d3d11=enabled           \
  -D dvdnav=enabled          \
  -D jpeg=enabled            \
  -D lcms2=enabled           \
  -D libarchive=enabled      \
  -D libbluray=enabled       \
  -D libmpv=true             \
  -D lua=enabled             \
  -D shaderc=enabled         \
  -D spirv-cross=enabled     \
  -D tests=true              \
  -D uchardet=enabled        \
  -D vapoursynth=enabled

if [[ "$SYS" != "clang32" && "$SYS" != "mingw32" ]]; then
   meson configure build -D{egl-angle-lib,egl-angle-win32,pdf-build,rubberband}=enabled
fi

meson compile -C build
./build/mpv.com -v --no-config
