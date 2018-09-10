#!/bin/sh
set -e

_mingw_sysroot=/usr/$TARGET/sysroot
_mingw_prefix=$_mingw_sysroot/mingw
_mingw_exec_prefix=$_mingw_prefix
_mingw_libdir=$_mingw_exec_prefix/lib
_mingw_datadir=$_mingw_prefix/share

export PKG_CONFIG_PATH="$_mingw_libdir/pkgconfig:$_mingw_datadir/pkgconfig";
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export NM=$TARGET-nm
export RANLIB=$TARGET-ranlib
export CFLAGS="-O2 -mtune=intel -g -ggdb -pipe -Wall --param=ssp-buffer-size=4 -mms-bitfields -fmessage-length=0 -D_FORTIFY_SOURCE=2 -fexceptions -fasynchronous-unwind-tables -fstack-protector-strong -fno-ident"
export LDFLAGS="-Wl,--no-keep-memory -fstack-protector-strong"

python3 ./waf configure \
    --enable-static-build \
    --enable-libmpv-shared \
    --enable-lua \
    --enable-javascript \
    --enable-libarchive \
    --enable-libass \
    --enable-libbluray \
    --enable-dvdread \
    --enable-dvdnav \
    --enable-uchardet \
    --enable-vulkan \
    --enable-shaderc \
    --enable-rubberband \
    --enable-lcms2
python3 ./waf build --verbose
