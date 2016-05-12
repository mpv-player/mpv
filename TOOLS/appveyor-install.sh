#!/usr/bin/bash
set -e

case $MSYSTEM in
MINGW32)
    export MINGW_PACKAGE_PREFIX=mingw-w64-i686
    ;;
MINGW64)
    export MINGW_PACKAGE_PREFIX=mingw-w64-x86_64
    ;;
esac

# Write an empty fonts.conf to speed up fc-cache
export FONTCONFIG_FILE=/dummy-fonts.conf
cat >"$FONTCONFIG_FILE" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig></fontconfig>
EOF

# Install build dependencies for mpv
pacman -S --noconfirm --needed \
    $MINGW_PACKAGE_PREFIX-gcc \
    $MINGW_PACKAGE_PREFIX-angleproject-git \
    $MINGW_PACKAGE_PREFIX-ffmpeg \
    $MINGW_PACKAGE_PREFIX-lcms2 \
    $MINGW_PACKAGE_PREFIX-libarchive \
    $MINGW_PACKAGE_PREFIX-libass \
    $MINGW_PACKAGE_PREFIX-libjpeg-turbo \
    $MINGW_PACKAGE_PREFIX-lua51 \
    $MINGW_PACKAGE_PREFIX-rubberband \
    $MINGW_PACKAGE_PREFIX-uchardet-git

# Delete unused packages to reduce space used in the Appveyor cache
pacman -Sc --noconfirm
