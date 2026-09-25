#!/usr/bin/env bash
#
# Static mpv and libmpv builds on top of the FFmpeg-Builds container images
# (ghcr.io/btbn/ffmpeg-builds/<target>-<variant>, see
# https://github.com/BtbN/FFmpeg-Builds). The image ships the cross toolchain,
# a matching /cross.meson and every FFmpeg dependency as static library in
# $FFBUILD_PREFIX, so only FFmpeg itself and the things FFmpeg does not need
# are built here. Run it from the mpv source tree inside the container:
#
#   docker run --rm -v "$PWD:/mpv" -w /mpv \
#       ghcr.io/btbn/ffmpeg-builds/win64-gpl:latest ./ci/build-mingw64-full.sh
#
# The mpv binaries end up in artifact/, the libmpv package in artifact-libmpv/.
set -euo pipefail

: "${FFBUILD_PREFIX:?not running in a FFmpeg-Builds image}"
: "${VARIANT:?not running in a FFmpeg-Builds image}"

. ./ci/build-common.sh

# The dependency headers are not ours to fix, keep --werror to mpv's own code.
CFLAGS="${CFLAGS/-I${FFBUILD_PREFIX}\/include/-isystem${FFBUILD_PREFIX}/include}"
CXXFLAGS="${CXXFLAGS/-I${FFBUILD_PREFIX}\/include/-isystem${FFBUILD_PREFIX}/include}"

gpl=false
[[ $VARIANT == gpl* ]] && gpl=true

gitclone="git clone --depth=1 --recursive --shallow-submodules"
group() { echo "::group::$1"; }
endgroup() { echo "::endgroup::"; }

group "Building FFmpeg"
$gitclone https://github.com/FFmpeg/FFmpeg.git ffmpeg
pushd ffmpeg
# rav1e and librsvg bring their own Rust runtime, which clashes with subrandr's.
./configure --prefix="$FFBUILD_PREFIX" --pkg-config-flags="--static" \
            $FFBUILD_TARGET_FLAGS $FF_CONFIGURE \
            --extra-cflags="$FF_CFLAGS" --extra-cxxflags="$FF_CXXFLAGS" \
            --extra-libs="$FF_LIBS" --extra-ldflags="$FF_LDFLAGS" \
            --extra-ldexeflags="$FF_LDEXEFLAGS" \
            --cc="$CC" --cxx="$CXX" --ar="$AR" --ranlib="$RANLIB" --nm="$NM" \
            --enable-static --disable-shared --disable-{doc,programs} \
            --disable-{librav1e,librsvg,openal} || { cat ffbuild/config.log; exit 1; }
make -j"$(nproc)"
make install
popd
endgroup

group "Building LuaJIT"
$gitclone -b v2.1 https://github.com/LuaJIT/LuaJIT.git
pushd LuaJIT
# Strip -ldl, not needed for Windows.
sed -i "/^Libs\.private/d" etc/luajit.pc
# FILE_T and INSTALL_DEP are only needed to glue install target.
make TARGET_SYS=Windows PREFIX="$FFBUILD_PREFIX" HOST_CC="$HOST_CC" \
     CFLAGS="$HOST_CFLAGS" CROSS="$FFBUILD_CROSS_PREFIX" TARGET_CFLAGS="$CFLAGS" \
     BUILDMODE=static XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT FILE_T=luajit.exe \
     INSTALL_DEP=src/luajit.exe amalg install
popd
endgroup

group "Building subrandr"
build_subrandr "$FFBUILD_PREFIX" --target "$FFBUILD_RUST_TARGET" \
               --static-library true --shared-library false
endgroup

group "Building cppwinrt"
# win32-smtc needs the C++/WinRT projection headers, generated from the
# Windows metadata that windows-rs carries.
cppwinrt_ver=2.0.250303.1
windows_rs_ver=73
wget -q "https://github.com/microsoft/cppwinrt/archive/${cppwinrt_ver}.tar.gz" -O - | tar -xz
git clone --depth=1 --filter=blob:none --sparse -b "$windows_rs_ver" \
    https://github.com/microsoft/windows-rs.git
git -C windows-rs sparse-checkout set crates/libs/bindgen/default
mkdir -p "cppwinrt-$cppwinrt_ver/build"
pushd "cppwinrt-$cppwinrt_ver/build"
CFLAGS="$HOST_CFLAGS" CXXFLAGS="$HOST_CXXFLAGS" LDFLAGS= \
    cmake -GNinja -DCMAKE_CXX_COMPILER="$HOST_CXX" -DCMAKE_BUILD_TYPE=Release \
          -DCPPWINRT_BUILD_VERSION="$cppwinrt_ver" ..
ninja cppwinrt
./cppwinrt -input ../../windows-rs/crates/libs/bindgen/default \
           -output "$FFBUILD_PREFIX/include"
popd
endgroup

group "Building mpv"
mkdir -p subprojects
meson wrap install mujs
meson subprojects download

mpv_args=(
    --cross-file /cross.meson $common_args
    --buildtype=release
    --prefer-static
    --default-library=shared
    -Dc_link_args="$FF_LIBS"
    -Dcpp_link_args="$FF_LIBS"
    -Dgpl=$gpl
    --force-fallback-for=mujs
    -Dmujs:werror=false
    -Dmujs:default_library=static
    -Dlua=luajit
    -D{amf,d3d11,javascript,lcms2,libbluray,libcurl,shaderc,spirv-cross}=enabled
    -D{subrandr,vulkan,win32-smtc,zimg}=enabled
)
if $gpl; then
    # Only the GPL image carries the dependencies of the GPL features.
    mpv_args+=(-D{dvda,dvdnav,rubberband}=enabled)
fi
meson setup build "${mpv_args[@]}"
meson compile -C build
endgroup

group "Packaging"
license=LICENSE.LGPL
if $gpl; then
    license=LICENSE.GPL
fi
mkdir -p artifact artifact-libmpv/include/mpv
cp -p build/mpv.{exe,com} etc/mpv-*.bat "$license" artifact/
cp -p build/libmpv*.dll build/libmpv*.dll.a "$license" artifact-libmpv/
cp -p include/mpv/*.h artifact-libmpv/include/mpv/
cp -p "$FFBUILD_PREFIX"/share/java/libbluray-*.jar artifact/
cp -p "$FFBUILD_PREFIX"/share/java/libbluray-*.jar artifact-libmpv/
"${FFBUILD_CROSS_PREFIX}strip" artifact/mpv.{exe,com} artifact-libmpv/libmpv*.dll
ls -l artifact artifact-libmpv
endgroup
