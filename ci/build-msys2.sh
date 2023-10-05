#!/bin/sh -e

mkdir subprojects
cat > subprojects/libplacebo.wrap <<EOF
[wrap-git]
url = https://code.videolan.org/videolan/libplacebo.git
revision = v6.338.1
depth = 1
clone-recursive = true
EOF

meson setup build            \
  --werror                   \
  -Dlibplacebo:werror=false  \
  -Dlibplacebo:demos=false   \
  -Dlibplacebo:default_library=static \
  -Dc_args="-Wno-error=deprecated -Wno-error=deprecated-declarations" \
  -D cdda=enabled            \
  -D d3d-hwaccel=enabled     \
  -D d3d11=enabled           \
  -D dvdnav=enabled          \
  -D egl-angle-lib=enabled   \
  -D egl-angle-win32=enabled \
  -D jpeg=enabled            \
  -D lcms2=enabled           \
  -D libarchive=enabled      \
  -D libbluray=enabled       \
  -D libmpv=true             \
  -D lua=enabled             \
  -D pdf-build=enabled       \
  -D rubberband=enabled      \
  -D shaderc=enabled         \
  -D spirv-cross=enabled     \
  -D tests=true              \
  -D uchardet=enabled        \
  -D vapoursynth=enabled
meson compile -C build
./build/mpv.com -v --no-config
