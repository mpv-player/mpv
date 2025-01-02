#!/usr/bin/env bash

set -ex

meson setup build
meson compile -C build

./build/mpv --version

./TOOLS/osxbundle.py --skip-deps build/mpv
if [[ $1 == "--static" ]]; then
    dylibbundler
        --bundle-deps
        --dest-dir build/mpv.app/Contents/MacOS/lib/
        --install-path @executable_path/lib/
        --fix-file build/mpv.app/Contents/MacOS/mpv
    ./build/mpv.app/Contents/MacOS/mpv --version
fi
