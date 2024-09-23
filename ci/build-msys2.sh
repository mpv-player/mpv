#!/bin/sh -e

args=(
  --werror
  -Dc_args='-Wno-error=deprecated -Wno-error=deprecated-declarations'
  -D{d3d-hwaccel,d3d11,dvdnav,jpeg,lcms2,libarchive}=enabled
  -D{libbluray,lua,shaderc,spirv-cross}=enabled
  -D{libmpv,tests}=true
)

[[ "$SYS" != "clang32" && "$SYS" != "mingw32" ]] && args+=(
  -D{cdda,egl-angle-lib,egl-angle-win32,pdf-build}=enabled
  -D{rubberband,uchardet,vapoursynth,win32-smtc}=enabled
)

meson setup build "${args[@]}"
meson compile -C build
./build/mpv.com -v --no-config
