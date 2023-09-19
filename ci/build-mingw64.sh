#!/bin/bash -e

prefix_dir=$PWD/mingw_prefix
mkdir -p "$prefix_dir"
ln -snf . "$prefix_dir/usr"
ln -snf . "$prefix_dir/local"

wget="wget -nc --progress=bar:force"
gitclone="git clone --depth=1 --recursive"

# -posix is Ubuntu's variant with pthreads support
export CC=$TARGET-gcc-posix
export CXX=$TARGET-g++-posix
export AR=$TARGET-ar
export NM=$TARGET-nm
export RANLIB=$TARGET-ranlib

export CFLAGS="-O2 -pipe -Wall -D_FORTIFY_SOURCE=2"
export LDFLAGS="-fstack-protector-strong"

# anything that uses pkg-config
export PKG_CONFIG_SYSROOT_DIR="$prefix_dir"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/lib/pkgconfig"

# autotools(-like)
commonflags="--disable-static --enable-shared"

# meson
fam=x86_64
[[ "$TARGET" == "i686-"* ]] && fam=x86
cat >"$prefix_dir/crossfile" <<EOF
[built-in options]
buildtype = 'release'
wrap_mode = 'nodownload'
[binaries]
c = ['ccache', '${CC}']
cpp = ['ccache', '${CXX}']
ar = '${AR}'
strip = '${TARGET}-strip'
pkgconfig = 'pkg-config'
windres = '${TARGET}-windres'
dlltool = '${TARGET}-dlltool'
[host_machine]
system = 'windows'
cpu_family = '${fam}'
cpu = '${TARGET%%-*}'
endian = 'little'
EOF

# CMake
cmake_args=(
    -Wno-dev
    -DCMAKE_SYSTEM_NAME=Windows
    -DCMAKE_FIND_ROOT_PATH="$PKG_CONFIG_SYSROOT_DIR"
    -DCMAKE_RC_COMPILER="${TARGET}-windres"
    -DCMAKE_BUILD_TYPE=Release
)

export CC="ccache $CC"
export CXX="ccache $CXX"

function builddir {
    [ -d "$1/builddir" ] && rm -rf "$1/builddir"
    mkdir -p "$1/builddir"
    pushd "$1/builddir"
}

function makeplusinstall {
    if [ -f build.ninja ]; then
        ninja
        DESTDIR="$prefix_dir" ninja install
    else
        make -j$(nproc)
        make DESTDIR="$prefix_dir" install
    fi
}

function gettar {
    local name="${1##*/}"
    [ -d "${name%%.*}" ] && return 0
    $wget "$1"
    tar -xaf "$name"
}

function build_if_missing {
    local name=${1//-/_}
    local mark_var=_${name}_mark
    local mark_file=$prefix_dir/${!mark_var}
    [ -e "$mark_file" ] && return 0
    echo "::group::Building $1"
    _$name
    echo "::endgroup::"
    if [ ! -e "$mark_file" ]; then
        echo "Error: Build of $1 completed but $mark_file was not created."
        return 2
    fi
}


## mpv's dependencies

_iconv () {
    local ver=1.17
    gettar "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ver}.tar.gz"
    builddir libiconv-${ver}
    ../configure --host=$TARGET $commonflags
    makeplusinstall
    popd
}
_iconv_mark=lib/libiconv.dll.a

_zlib () {
    local ver=1.3
    gettar "https://zlib.net/fossils/zlib-${ver}.tar.gz"
    pushd zlib-${ver}
    make -fwin32/Makefile.gcc clean
    make -fwin32/Makefile.gcc PREFIX=$TARGET- CC="$CC" SHARED_MODE=1 \
        DESTDIR="$prefix_dir" install \
        BINARY_PATH=/bin INCLUDE_PATH=/include LIBRARY_PATH=/lib
    popd
}
_zlib_mark=lib/libz.dll.a

_ffmpeg () {
    [ -d ffmpeg ] || $gitclone https://github.com/FFmpeg/FFmpeg.git ffmpeg
    builddir ffmpeg
    local args=(
        --pkg-config=pkg-config --target-os=mingw32
        --enable-cross-compile --cross-prefix=$TARGET- --arch=${TARGET%%-*}
        --cc="$CC" --cxx="$CXX" $commonflags
        --disable-{doc,programs,muxers,encoders}
        --enable-encoder=mjpeg,png
    )
    pkg-config vulkan && args+=(--enable-vulkan --enable-libshaderc)
    ../configure "${args[@]}"
    makeplusinstall
    popd
}
_ffmpeg_mark=lib/libavcodec.dll.a

_shaderc () {
    if [ ! -d shaderc ]; then
        $gitclone https://github.com/google/shaderc.git
        (cd shaderc && ./utils/git-sync-deps)
    fi
    builddir shaderc
    cmake .. "${cmake_args[@]}" \
        -DBUILD_SHARED_LIBS=OFF -DSHADERC_SKIP_TESTS=ON
    makeplusinstall
    popd
}
_shaderc_mark=lib/libshaderc_shared.dll.a

_spirv_cross () {
    [ -d SPIRV-Cross ] || $gitclone https://github.com/KhronosGroup/SPIRV-Cross
    builddir SPIRV-Cross
    cmake .. "${cmake_args[@]}" \
        -DSPIRV_CROSS_SHARED=ON -DSPIRV_CROSS_{CLI,STATIC}=OFF
    makeplusinstall
    popd
}
_spirv_cross_mark=lib/libspirv-cross-c-shared.dll.a

_vulkan_headers () {
    [ -d Vulkan-Headers ] || $gitclone https://github.com/KhronosGroup/Vulkan-Headers -b v1.3.266
    builddir Vulkan-Headers
    cmake .. "${cmake_args[@]}"
    makeplusinstall
    popd
}
_vulkan_headers_mark=include/vulkan/vulkan.h

_vulkan_loader () {
    [ -d Vulkan-Loader ] || $gitclone https://github.com/KhronosGroup/Vulkan-Loader -b v1.3.266
    builddir Vulkan-Loader
    cmake .. "${cmake_args[@]}" \
        -DENABLE_WERROR=OFF
    makeplusinstall
    popd
}
_vulkan_loader_mark=lib/libvulkan-1.dll.a

_libplacebo () {
    [ -d libplacebo ] || $gitclone https://code.videolan.org/videolan/libplacebo.git
    builddir libplacebo
    meson setup .. --cross-file "$prefix_dir/crossfile" \
        -Ddemos=false -D{opengl,d3d11}=enabled
    makeplusinstall
    popd
}
_libplacebo_mark=lib/libplacebo.dll.a

_freetype () {
    local ver=2.13.1
    gettar "https://mirror.netcologne.de/savannah/freetype/freetype-${ver}.tar.xz"
    builddir freetype-${ver}
    meson setup .. --cross-file "$prefix_dir/crossfile"
    makeplusinstall
    popd
}
_freetype_mark=lib/libfreetype.dll.a

_fribidi () {
    local ver=1.0.13
    gettar "https://github.com/fribidi/fribidi/releases/download/v${ver}/fribidi-${ver}.tar.xz"
    builddir fribidi-${ver}
    meson setup .. --cross-file "$prefix_dir/crossfile" \
        -D{tests,docs}=false
    makeplusinstall
    popd
}
_fribidi_mark=lib/libfribidi.dll.a

_harfbuzz () {
    local ver=8.1.1
    gettar "https://github.com/harfbuzz/harfbuzz/releases/download/${ver}/harfbuzz-${ver}.tar.xz"
    builddir harfbuzz-${ver}
    meson setup .. --cross-file "$prefix_dir/crossfile" \
        -Dtests=disabled
    makeplusinstall
    popd
}
_harfbuzz_mark=lib/libharfbuzz.dll.a

_libass () {
    [ -d libass ] || $gitclone https://github.com/libass/libass.git
    builddir libass
    [ -f ../configure ] || (cd .. && ./autogen.sh)
    ../configure --host=$TARGET $commonflags
    makeplusinstall
    popd
}
_libass_mark=lib/libass.dll.a

_luajit () {
    [ -d LuaJIT ] || $gitclone https://github.com/LuaJIT/LuaJIT.git
    pushd LuaJIT
    local hostcc="ccache cc"
    local flags=
    [[ "$TARGET" == "i686-"* ]] && { hostcc="$hostcc -m32"; flags=XCFLAGS=-DLUAJIT_NO_UNWIND; }
    make TARGET_SYS=Windows clean
    make TARGET_SYS=Windows HOST_CC="$hostcc" CROSS="ccache $TARGET-" \
        BUILDMODE=static $flags amalg
    make DESTDIR="$prefix_dir" INSTALL_DEP= FILE_T=luajit.exe install
    popd
}
_luajit_mark=lib/libluajit-5.1.a

for x in iconv zlib shaderc spirv-cross; do
    build_if_missing $x
done
if [[ "$TARGET" != "i686-"* ]]; then
    build_if_missing vulkan-headers
    build_if_missing vulkan-loader
fi
for x in ffmpeg libplacebo freetype fribidi harfbuzz libass luajit; do
    build_if_missing $x
done

## mpv

[ -z "$1" ] && exit 0

CFLAGS+=" -I'$prefix_dir/include'"
LDFLAGS+=" -L'$prefix_dir/lib'"
export CFLAGS LDFLAGS
build=mingw_build
rm -rf $build

meson setup $build --cross-file "$prefix_dir/crossfile" \
    --buildtype debugoptimized \
    -Dlibmpv=true -Dlua=luajit \
    -D{shaderc,spirv-cross,d3d11}=enabled

meson compile -C $build

if [ "$2" = pack ]; then
    mkdir -p artifact/tmp
    echo "Copying:"
    cp -pv $build/player/mpv.com $build/mpv.exe artifact/
    # copy everything we can get our hands on
    cp -p "$prefix_dir/bin/"*.dll artifact/tmp/
    shopt -s nullglob
    for file in /usr/lib/gcc/$TARGET/*-posix/*.dll /usr/$TARGET/lib/*.dll; do
        cp -p "$file" artifact/tmp/
    done
    # pick DLLs we need
    pushd artifact/tmp
    dlls=(
        libgcc_*.dll lib{ssp,stdc++,winpthread}-[0-9]*.dll # compiler runtime
        av*.dll sw*.dll lib{ass,freetype,fribidi,harfbuzz,iconv,placebo}-[0-9]*.dll
        lib{shaderc_shared,spirv-cross-c-shared}.dll zlib1.dll
        # note: vulkan-1.dll is not here since drivers provide it
    )
    mv -v "${dlls[@]}" ..
    popd

    echo "Archiving:"
    pushd artifact
    rm -rf tmp
    zip -9r "../mpv-git-$(date +%F)-$(git rev-parse --short HEAD)-${TARGET%%-*}.zip" -- *
    popd
fi
