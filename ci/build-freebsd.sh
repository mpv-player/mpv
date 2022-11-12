#!/bin/sh
set -e

export CFLAGS="$CFLAGS -isystem/usr/local/include"
export CXXFLAGS="$CXXFLAGS -isystem/usr/local/include"
export LDFLAGS="$LDFLAGS -L/usr/local/lib"

meson setup build \
    -Dlibmpv=true \
    -Dlua=enabled \
    -Degl-drm=enabled \
    -Dopenal=enabled \
    -Dsdl2=enabled \
    -Dsndio=enabled \
    -Dtests=true \
    -Dvdpau=enabled \
    -Dvulkan=enabled \
    -Doss-audio=enabled \
    $(pkg info -q v4l_compat && echo -Ddvbin=enabled) \
    $(pkg info -q libdvdnav && echo -Ddvdnav=enabled) \
    $(pkg info -q libcdio-paranoia && echo -Dcdda=enabled) \
    $(pkg info -q pipewire && echo -Dpipewire=enabled) \
    $NULL

meson compile -C build
./build/mpv -v --no-config
