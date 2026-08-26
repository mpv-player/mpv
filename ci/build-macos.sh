#!/usr/bin/env bash

set -e

. ./ci/build-common.sh

objc_args="-Wno-error=deprecated -Wno-error=deprecated-declarations"
c_args=""
if [[ "${MACOS_ARCH}" == "test" ]]; then
    # remove -werror + tests and keep libmpv for test builds only
    common_args="-Dlibmpv=true"
    # add back -werror for compiling only, not for linking
    objc_args="-Werror ${objc_args}"
    c_args="-Werror"
fi

PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig/" CC="${CC}" CXX="${CXX}" \
meson setup build $common_args \
  -Dobjc_args="${objc_args}" \
  -Dc_args="${c_args}" \
  -D{caca,cdda,dvda,dvdnav,gl,iconv,lcms2,libarchive,libbluray,lua,jpeg}=enabled \
  -D{plain-gl,rubberband,zimg,zlib}=enabled \
  -D{cocoa,coreaudio,gl-cocoa,videotoolbox-gl,videotoolbox-pl}=enabled \
  -D{swift-build,macos-cocoa-cb,macos-media-player,macos-touchbar,vulkan}=enabled \
  -Dswift-flags="${SWIFT_FLAGS}"

meson compile -C build -j4
meson install -C build
./build/mpv -v --no-config
