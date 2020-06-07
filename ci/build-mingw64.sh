#!/bin/bash -e

prefix_dir=$PWD/mingw_prefix
mkdir -p "$prefix_dir"
ln -snf . "$prefix_dir/usr"
ln -snf . "$prefix_dir/local"

wget="wget -nc --progress=bar:force"
gitclone="git clone --depth=10"
commonflags="--disable-static --enable-shared"

export PKG_CONFIG_SYSROOT_DIR="$prefix_dir"
export PKG_CONFIG_LIBDIR="$PKG_CONFIG_SYSROOT_DIR/lib/pkgconfig"

export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export NM=$TARGET-nm
export RANLIB=$TARGET-ranlib

export CFLAGS="-O2 -pipe -Wall -D_FORTIFY_SOURCE=2"
export LDFLAGS="-fstack-protector-strong"

function builddir () {
    [ -d "$1/builddir" ] && rm -rf "$1/builddir"
    mkdir -p "$1/builddir"
    pushd "$1/builddir"
}

function makeplusinstall () {
    make -j$(nproc)
    make DESTDIR="$prefix_dir" install
}

function gettar () {
    name="${1##*/}"
    [ -d "${name%%.*}" ] && return 0
    $wget "$1"
    tar -xaf "$name"
}

## iconv
if [ ! -e "$prefix_dir/lib/libiconv.dll.a" ]; then
    ver=1.16
    gettar "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ver}.tar.gz"
    builddir libiconv-${ver}
    ../configure --host=$TARGET $commonflags
    makeplusinstall
    popd
fi

## zlib
if [ ! -e "$prefix_dir/lib/libz.dll.a" ]; then
    ver=1.2.11
    gettar "https://zlib.net/zlib-${ver}.tar.gz"
    pushd zlib-${ver}
    make -fwin32/Makefile.gcc PREFIX=$TARGET- SHARED_MODE=1 \
        DESTDIR="$prefix_dir" install \
        BINARY_PATH=/bin INCLUDE_PATH=/include LIBRARY_PATH=/lib
    popd
fi

## ffmpeg
if [ ! -e "$prefix_dir/lib/libavcodec.dll.a" ]; then
    [ -d ffmpeg ] || $gitclone https://github.com/FFmpeg/FFmpeg.git ffmpeg
    builddir ffmpeg
    ../configure --pkg-config=pkg-config --target-os=mingw32 \
        --enable-cross-compile --cross-prefix=$TARGET- --arch=${TARGET%%-*} \
        $commonflags \
        --disable-{stripping,doc,programs,muxers,encoders,devices}
    makeplusinstall
    popd
fi

## freetype2
if [ ! -e "$prefix_dir/lib/libfreetype.dll.a" ]; then
    ver=2.10.2
    gettar "https://download.savannah.gnu.org/releases/freetype/freetype-${ver}.tar.gz"
    builddir freetype-${ver}
    ZLIB_LIBS="-L'$prefix_dir/lib' -lz" \
    ../configure --host=$TARGET $commonflags --with-png=no
    makeplusinstall
    popd
fi
[ -f "$prefix_dir/lib/libfreetype.dll.a" ] || { echo "libtool fuckup"; exit 1; }

## fribidi
if [ ! -e "$prefix_dir/lib/libfribidi.dll.a" ]; then
    ver=1.0.9
    gettar "https://github.com/fribidi/fribidi/releases/download/v${ver}/fribidi-${ver}.tar.xz"
    builddir fribidi-${ver}
    ../configure --host=$TARGET $commonflags
    makeplusinstall
    popd
fi

## libass
if [ ! -e "$prefix_dir/lib/libass.dll.a" ]; then
    [ -d libass ] || $gitclone https://github.com/libass/libass.git
    builddir libass
    [ -f ../configure ] || (cd .. && ./autogen.sh)
    ../configure --host=$TARGET $commonflags
    makeplusinstall
    popd
fi

## lua
if [ ! -e "$prefix_dir/lib/liblua.a" ]; then
    ver=5.2.4
    gettar "https://www.lua.org/ftp/lua-${ver}.tar.gz"
    pushd lua-${ver}
    make PLAT=mingw INSTALL_TOP="$prefix_dir" TO_BIN=/dev/null \
        CC="$CC" AR="$AR r" all install
    make INSTALL_TOP="$prefix_dir" pc >"$prefix_dir/lib/pkgconfig/lua.pc"
    printf 'Name: Lua\nDescription:\nVersion: ${version}\nLibs: -L${libdir} -llua\nCflags: -I${includedir}\n' \
        >>"$prefix_dir/lib/pkgconfig/lua.pc"
    popd
fi

## mpv
PKG_CONFIG=pkg-config CFLAGS="-I'$prefix_dir/include'" LDFLAGS="-L'$prefix_dir/lib'" \
python3 ./waf configure \
    --enable-libmpv-shared --lua=52

python3 ./waf build --verbose
