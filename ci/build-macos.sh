#!/usr/bin/env bash

set -e

FFMPEG_SYSROOT="${HOME}/deps/sysroot"
MPV_INSTALL_PREFIX="${HOME}/out/mpv"
MPV_VARIANT="${TRAVIS_OS_NAME}"

if [[ -d "./build/${MPV_VARIANT}" ]] ; then
    rm -rf "./build/${MPV_VARIANT}"
fi

if [[ ! -e "./waf" ]] ; then
    python3 ./bootstrap.py
fi

PKG_CONFIG_PATH="${FFMPEG_SYSROOT}/lib/pkgconfig/" CC="${CC}" CXX="${CXX}" python3 \
  ./waf configure \
    --variant="${MPV_VARIANT}" \
    --prefix="${MPV_INSTALL_PREFIX}" \
    --enable-{gl,iconv,lcms2,libmpv-shared,lua,jpeg,plain-gl,zlib} \
    --enable-{cocoa,coreaudio,gl-cocoa,macos-cocoa-cb,macos-touchbar,videotoolbox-gl} \
    --swift-flags="${CI_SWIFT_FLAGS}"

python3 ./waf build --variant="${MPV_VARIANT}" -j4

python3 ./waf install --variant="${MPV_VARIANT}"
