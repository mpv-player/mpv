#!/bin/sh
set -e

. ./ci/build-common.sh

# FFmpeg on openBSD (4.4.4) is too old; use a subproject
rm -rf subprojects
mkdir -p subprojects
cat <<EOF > subprojects/ffmpeg.wrap
[wrap-git]
url = https://gitlab.freedesktop.org/gstreamer/meson-ports/ffmpeg.git
revision = meson-7.1
depth = 1
[provide]
libavcodec = libavcodec_dep
libavdevice = libavdevice_dep
libavfilter = libavfilter_dep
libavformat = libavformat_dep
libavutil = libavutil_dep
libswresample = libswresample_dep
libswscale = libswscale_dep
EOF

meson setup build $common_args \
  -Dffmpeg:vulkan=auto \
  -Dffmpeg:werror=false \
  -Dlua=enabled \
  -Dopenal=enabled \
  -Dpulse=enabled \
  -Dvulkan=enabled \
  -Ddvdnav=enabled \
  -Dcdda=disabled
meson compile -C build
./build/mpv -v --no-config
