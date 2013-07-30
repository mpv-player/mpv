#!/bin/sh
set -e

if [ $# -ne 2 ]; then
    echo >&2 "Usage: $0 input.svg output.ico"
    exit 1
fi

# For smooth rendering on high-DPI displays, the standard three app icon sizes
# (16x16, 32x32 and 48x48) have to be scaled to the four Windows DPI settings
# as in the table below:

#                   Small  Regular    Tiles
#  96 DPI (100%)    16x16    32x32    48x48
# 120 DPI (125%)    20x20    40x40    60x60
# 144 DPI (150%)    24x24    48x48    72x72
# 196 DPI (200%)    32x32    64x64    96x96

# Also, there should also be an extra large 256x256 icon and some low colour
# 8-bit and 4-bit variants, which are still used in Remote Desktop.

# Note: Windows Vista has a bug in its icon scaling that makes no sense.
# Instead of following the DPI setting, small icons are always a bit larger
# than they should be at 22x22, 26x26 and 36x36 for 120, 144 and 196 DPI. This
# script doesn't generate icons with those sizes, since computers with Vista
# and a high-DPI display are probably fairly rare these days.

temppng=".$(basename "$1" .svg)-temp.png"

inkscape --without-gui --export-png="$temppng" --export-dpi=72 \
         --export-background-opacity=0 --export-width=512 --export-height=512 \
         "$1" >/dev/null 2>&1

# Old versions of ImageMagick (like the one in Cygwin) use the wrong gamma when
# exporting icon files. To fix, add -gamma 2.2 after the input file.

convert png:"$temppng" -filter lanczos2 \
        \( -clone 0 -resize  96x96  \) \
        \( -clone 0 -resize  72x72  \) \
        \( -clone 0 -resize  64x64  \) \
        \( -clone 0 -resize  60x60  \) \
        \( -clone 0 -resize  48x48  \) \
        \( -clone 0 -resize  40x40  \) \
        \( -clone 0 -resize  32x32  \) \
        \( -clone 0 -resize  24x24  \) \
        \( -clone 0 -resize  20x20  \) \
        \( -clone 0 -resize  16x16  \) \
        \( -clone 0 -resize 256x256 \) \
        \( -clone 0 -resize  32x32  \( -clone 0 -alpha opaque -colors 255 \) \
                                    \( -clone 0 -channel A -threshold 50% \) \
               -delete 0 -compose CopyOpacity -composite -colors 256 \) \
        \( -clone 0 -resize  16x16  \( -clone 0 -alpha opaque -colors 255 \) \
                                    \( -clone 0 -channel A -threshold 50% \) \
               -delete 0 -compose CopyOpacity -composite -colors 256 \) \
        \( -clone 0 -resize  32x32  \( -clone 0 -alpha opaque -colors  15 \) \
                                    \( -clone 0 -channel A -threshold 50% \) \
               -delete 0 -compose CopyOpacity -composite -colors  16 \) \
        \( -clone 0 -resize  16x16  \( -clone 0 -alpha opaque -colors  15 \) \
                                    \( -clone 0 -channel A -threshold 50% \) \
               -delete 0 -compose CopyOpacity -composite -colors  16 \) \
        -delete 0 \
        -define png:compression-level=9 -define png:include-chunk=none ico:"$2"

rm "$temppng"
