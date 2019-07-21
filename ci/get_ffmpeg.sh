#!/usr/bin/env bash

set -e

FFMPEG_SRC_DIR="${HOME}/deps/src/ffmpeg"
FFMPEG_BUILD_DIR="${FFMPEG_SRC_DIR}/${TRAVIS_OS_NAME}"
FFMPEG_SYSROOT="${HOME}/deps/sysroot"
FFMPEG_HASH="18928e2bb4568cbe5e9061c3e6b63559392af3d2"

# Get the sauce if not around
if [[ ! -d "${FFMPEG_SRC_DIR}" ]] ; then
    git clone "https://git.videolan.org/git/ffmpeg.git" "${FFMPEG_SRC_DIR}"
fi

# pop into FFmpeg's source dir and clean up & check out our wanted revision
pushd "${FFMPEG_SRC_DIR}"
git reset --hard HEAD && git clean -dfx
git checkout "${FFMPEG_HASH}"
popd

# If a build dir of the same type is around, clean it up
if [[ -d "${FFMPEG_BUILD_DIR}" ]] ; then
    rm -rf "${FFMPEG_BUILD_DIR}"
fi

# Create and move into the build dir, configure and build!
mkdir -p "${FFMPEG_BUILD_DIR}" && pushd "${FFMPEG_BUILD_DIR}"

PKG_CONFIG_PATH="${FFMPEG_SYSROOT}/lib/pkgconfig/" ../configure \
  --disable-{autodetect,stripping} \
  --cc="${CC}" \
  --cxx="${CXX}" \
  --prefix="${FFMPEG_SYSROOT}" \
  --enable-{zlib,securetransport,videotoolbox}

make -j4 && make install && popd

exit 0
