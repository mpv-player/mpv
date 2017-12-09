#!/usr/bin/bash
set -e

# Write an empty fonts.conf to speed up fc-cache
export FONTCONFIG_FILE=/dummy-fonts.conf
cat >"$FONTCONFIG_FILE" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig></fontconfig>
EOF

# Install build dependencies for mpv
pacman -S --noconfirm --needed \
    $MINGW_PACKAGE_PREFIX-toolchain \
    $MINGW_PACKAGE_PREFIX-angleproject-git \
    $MINGW_PACKAGE_PREFIX-cmake \
    $MINGW_PACKAGE_PREFIX-lcms2 \
    $MINGW_PACKAGE_PREFIX-libarchive \
    $MINGW_PACKAGE_PREFIX-libass \
    $MINGW_PACKAGE_PREFIX-libjpeg-turbo \
    $MINGW_PACKAGE_PREFIX-lua51 \
    $MINGW_PACKAGE_PREFIX-ninja \
    $MINGW_PACKAGE_PREFIX-rubberband \
    $MINGW_PACKAGE_PREFIX-uchardet \
    $MINGW_PACKAGE_PREFIX-vulkan

# Delete unused packages to reduce space used in the Appveyor cache
pacman -Sc --noconfirm

# Compile ffmpeg
(
    git clone --depth=1 https://github.com/FFmpeg/ffmpeg.git && cd ffmpeg

    mkdir build && cd build
    ../configure \
        --prefix=$MINGW_PREFIX \
        --target-os=mingw32 \
        --arch=$MSYSTEM_CARCH \
        --disable-static \
        --disable-doc \
        --disable-asm \
        --enable-gpl \
        --enable-version3 \
        --enable-shared \
        --enable-pic \
        --enable-d3d11va \
        --enable-dxva2 \
        --enable-schannel
    make -j4 install
)

# Compile shaderc
(
    git clone --depth=1 https://github.com/google/shaderc && cd shaderc
    git clone --depth=1 https://github.com/google/glslang.git third_party/glslang
    git clone --depth=1 https://github.com/KhronosGroup/SPIRV-Tools.git third_party/spirv-tools
    git clone --depth=1 https://github.com/KhronosGroup/SPIRV-Headers.git third_party/spirv-headers

    mkdir build && cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DSHADERC_SKIP_TESTS=ON \
          -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX ..
    ninja install
    cp -f libshaderc/libshaderc_shared.dll $MINGW_PREFIX/bin/
)

# Compile crossc
(
    git clone --depth=1 https://github.com/rossy/crossc && cd crossc
    git submodule update --init

    make -j4 install prefix=$MINGW_PREFIX
)
