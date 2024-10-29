#!/bin/bash -e

. ./ci/build-common.sh

args=(
  -D{cdda,d3d-hwaccel,d3d11,dvdnav,jpeg,lcms2,libarchive}=enabled
  -D{libbluray,lua,shaderc,spirv-cross,uchardet,vapoursynth}=enabled
)

[[ "$SYS" != "mingw32" ]] && args+=(
  -D{egl-angle-lib,egl-angle-win32,pdf-build,rubberband,win32-smtc}=enabled
)

[[ "$SYS" == "clang64" ]] && args+=(
  -Db_sanitize=address,undefined
)

meson setup build $common_args "${args[@]}"
meson compile -C build
./build/mpv.com -v --no-config
