#!/bin/sh
set -e

export CFLAGS="$CFLAGS -isystem/usr/local/include"
export CXXFLAGS="$CXXFLAGS -isystem/usr/local/include"
export LDFLAGS="$LDFLAGS -L/usr/local/lib"

if [ ! -e "./waf" ] ; then
    python3 ./bootstrap.py
fi

python3 ./waf configure \
    --enable-libmpv-shared \
    --enable-lua \
    --enable-egl-drm \
    --enable-openal \
    --enable-sdl2 \
    --enable-vaapi-wayland \
    --enable-vdpau \
    --enable-vulkan \
    $(pkg info -q v4l_compat && echo --enable-dvbin) \
    $(pkg info -q libdvdnav && echo --enable-dvdnav) \
    $(pkg info -q libcdio-paranoia && echo --enable-cdda) \
    $NULL

python3 ./waf build
