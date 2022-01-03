#!/bin/sh
set -e

if [ "$1" = "meson" ]; then
    meson build \
      -Dlibmpv=true     \
      -Dlua=luajit      \
      -D{shaderc,spirv-cross,d3d11}=enabled
    meson compile -C build --verbose
    ./build/mpv
fi

if [ "$1" = "waf" ]; then
    if [[ -z "${CHECK_C_COMPILER}" ]]; then
      WAF_CC_ARG=""
    else
      WAF_CC_ARG="--check-c-compiler=${CHECK_C_COMPILER}"
    fi
    python3 ./waf configure \
        ${WAF_CC_ARG}            \
        --enable-libmpv-shared   \
        --lua=luajit             \
        --enable-{shaderc,spirv-cross,d3d11}
    python3 ./waf build --verbose
    ./build/mpv
fi
