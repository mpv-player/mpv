#!/usr/bin/env bash
set -euo pipefail

. ./ci/build-common.sh

CFLAGS="${CFLAGS/-I${FFBUILD_PREFIX}\/include/-isystem${FFBUILD_PREFIX}/include}"
CXXFLAGS="${CXXFLAGS/-I${FFBUILD_PREFIX}\/include/-isystem${FFBUILD_PREFIX}/include}"

gitclone="git clone --depth=1 --recursive --shallow-submodules"
group_start() { echo "::group::$1"; }
group_end() { echo "::endgroup::"; }


group_start "Building FFmpeg"
$gitclone https://github.com/FFmpeg/FFmpeg.git ffmpeg
pushd ffmpeg
./configure --prefix="$FFBUILD_PREFIX" --pkg-config-flags="--static" \
            $FFBUILD_TARGET_FLAGS $FF_CONFIGURE \
            --extra-cflags="$FF_CFLAGS" \
            --extra-cxxflags="$FF_CXXFLAGS" \
            --extra-libs="$FF_LIBS" \
            --extra-ldflags="$FF_LDFLAGS" \
            --extra-ldexeflags="$FF_LDEXEFLAGS" \
            --cc="$CC" --cxx="$CXX" \
            --ar="$AR" --ranlib="$RANLIB" --nm="$NM" \
            --disable-librav1e --disable-openal \
            --enable-static --disable-shared \
            --disable-debug --disable-doc --disable-programs
make -j`nproc` && make install
popd
group_end

group_start "Building LuaJIT"
$gitclone -b v2.1 https://github.com/LuaJIT/LuaJIT.git
pushd LuaJIT
# LuaJIT's install target doesn't really support cross-compilation, apply minor patches.
sed -i "s|^prefix=/usr/local|prefix=${FFBUILD_PREFIX}|" etc/luajit.pc
# Strip -ldl, not needed for Windows.
sed -i "/^Libs\.private/d" etc/luajit.pc
# FILE_T and INSTALL_DEP are only needed to glue install target.
make TARGET_SYS=Windows PREFIX="$FFBUILD_PREFIX" HOST_CC="$HOST_CC" \
     CFLAGS="$HOST_CFLAGS" \CROSS="${FFBUILD_TOOLCHAIN}-" TARGET_CFLAGS="$CFLAGS" \
     BUILDMODE=static XCFLAGS=-DLUAJIT_ENABLE_LUA52COMPAT FILE_T=luajit.exe \
     INSTALL_DEP=src/luajit.exe amalg install
popd
group_end

group_start "Building subrandr"
build_subrandr "$FFBUILD_PREFIX" --target "$FFBUILD_RUST_TARGET" \
               --static-library true --shared-library false
group_end

group_start "Building cppwinrt"
cppwinrt_ver=2.0.250303.1
windows_rs_ver=73
wget -q "https://github.com/microsoft/cppwinrt/archive/${cppwinrt_ver}.tar.gz" -O cppwinrt.tar.gz
wget -q "https://github.com/microsoft/windows-rs/archive/${windows_rs_ver}.tar.gz" -O windows-rs.tar.gz
tar -xzf cppwinrt.tar.gz
tar -xzf windows-rs.tar.gz
mkdir -p "cppwinrt-$cppwinrt_ver/build"
pushd "cppwinrt-$cppwinrt_ver/build"
CFLAGS="$HOST_CFLAGS" CXXFLAGS="$HOST_CXXFLAGS" \
    cmake -GNinja -DCMAKE_CXX_COMPILER="$HOST_CXX" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCPPWINRT_BUILD_VERSION="$cppwinrt_ver" ..
ninja cppwinrt
./cppwinrt -input "../../windows-rs-$windows_rs_ver/crates/libs/bindgen/default" \
           -output "$FFBUILD_PREFIX/include"
popd
group_end

group_start "Setting up Meson wraps"
mkdir -p subprojects
meson wrap install mujs
meson subprojects download
group_end

group_start "Building mpv"
meson setup build --cross-file /cross.meson \
    $common_args \
    --buildtype=release \
    --prefer-static \
    --default-library=shared \
    --prefix="$FFBUILD_PREFIX" \
    -Dc_link_args="$FF_LIBS" \
    -Dcpp_link_args="$FF_LIBS" \
    -Dgpl=${GPL:-false} \
    --force-fallback-for=mujs \
    -Dmujs:werror=false \
    -Dmujs:default_library=static \
    -Dlua=luajit \
    -D{amf,d3d11,javascript,lua,shaderc,spirv-cross,subrandr,vulkan,win32-smtc}=enabled
meson compile -C build
group_end

group_start "Packaging artifacts"
mkdir -p artifact artifact-dev
# mpv
cp -pv etc/mpv-*.bat build/mpv.{exe,com} artifact/

# libmpv
cp -pv build/libmpv*.dll{,.a} artifact-dev/
mkdir -p artifact-dev/include/mpv
cp -pv include/mpv/*.h artifact-dev/include/mpv/
group_end
