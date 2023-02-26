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
    -Dvdpau=enabled \
    -Dvulkan=enabled \
    -Doss-audio=enabled \
    $(pkg info -q v4l_compat && echo -Ddvbin=enabled) \
    $(pkg info -q libdvdnav && echo -Ddvdnav=enabled) \
    $(pkg info -q libcdio-paranoia && echo -Dcdda=enabled) \
    $(pkg info -q pipewire && echo -Dpipewire=enabled) \
    $NULL

meson compile -C build
meson test -C build
./build/mpv -v --no-config

if [ ! -e "./waf" ] ; then
    python3 ./bootstrap.py
fi

python3 ./waf configure \
    --out=build_waf \
    --enable-libmpv-shared \
    --enable-lua \
    --enable-egl-drm \
    --enable-openal \
    --enable-sdl2 \
    --enable-sndio \
    --enable-vdpau \
    --enable-vulkan \
    --enable-oss-audio \
    $(pkg info -q v4l_compat && echo --enable-dvbin) \
    $(pkg info -q libdvdnav && echo --enable-dvdnav) \
    $(pkg info -q libcdio-paranoia && echo --enable-cdda) \
    $(pkg info -q pipewire && echo --enable-pipewire) \
    $NULL

python3 ./waf build
./build_waf/mpv -v --no-config
