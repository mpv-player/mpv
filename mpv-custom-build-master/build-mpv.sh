#!/bin/bash

# install homebrew
# ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

# install packages
# brew install pkg-config ffmpeg lua@5.1 mujs

# customize before compiling...

# compile mpv

./bootstrap.py

./waf configure --disable-build-date --disable-debug-build --disable-manpage-build --disable-libbluray --disable-zlib --disable-libarchive --disable-libavdevice --disable-jpeg  --disable-drm --disable-drmprime --disable-apple-remote --disable-macos-touchbar

./waf build -j4

./TOOLS/osxbundle.py build/mpv
find build/mpv.app -name '.gitkeep' -delete

# cleanup
# ./waf uninstall

# ./waf distclean
