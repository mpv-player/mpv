#!/usr/bin/env bash

set -e

. ./ci/build-common.sh

MPV_INSTALL_PREFIX="${HOME}/out/mpv"
MPV_VARIANT="${TRAVIS_OS_NAME}"

if [[ -d "./build/${MPV_VARIANT}" ]] ; then
    rm -rf "./build/${MPV_VARIANT}"
fi

PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig/" CC="${CC}" CXX="${CXX}" \
meson setup build $common_args \
  -Dprefix="${MPV_INSTALL_PREFIX}" \
  -Dobjc_args="-Wno-error=deprecated -Wno-error=deprecated-declarations" \
  -D{caca,cdda,dvdnav,gl,iconv,lcms2,libarchive,libbluray,lua,jpeg}=enabled \
  -D{plain-gl,rubberband,zimg,zlib}=enabled \
  -D{cocoa,coreaudio,gl-cocoa,videotoolbox-gl,videotoolbox-pl}=enabled \
  -D{swift-build,macos-cocoa-cb,macos-media-player,macos-touchbar,vulkan}=enabled \
  -Dswift-flags="${SWIFT_FLAGS}"

meson compile -C build -j4
meson install -C build
./build/mpv -v --no-config
