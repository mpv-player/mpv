#!/bin/bash
# Builds a portable Linux mpv bundle (mpv + libmpv.so.2 carrying vf_animejanai,
# plus the shared libs it needs) on the Ubuntu 22.04 baseline image, so the
# result runs on any mainstream distro from ~2022 on. Consumed by the
# mpv-upscale-2x_animejanai assembler (InstallLinuxMpv).
#
# Run inside ghcr.io/the-database/animejanai-linux-build:ubuntu2204 with this
# repo checked out at /work/mpv (or pass MPV_SRC). Outputs the bundle dir + a
# tar.zst at $OUT.
#
# Usage: build-linux-portable.sh <version-tag> [out-dir]
set -euo pipefail

VER="${1:?usage: build-linux-portable.sh <version-tag> [out-dir]}"
OUT="${2:-$PWD}"
MPV_SRC="${MPV_SRC:-$(cd "$(dirname "$0")/.." && pwd)}"
PREFIX=${PREFIX:-/opt/animejanai}
DEPS=/work/deps
JOBS=$(nproc)
export PKG_CONFIG_PATH="$PREFIX/lib/x86_64-linux-gnu/pkgconfig:$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$PREFIX/lib/x86_64-linux-gnu:$PREFIX/lib:${LD_LIBRARY_PATH:-}"
export PATH="/usr/local/cuda-13.2/bin:$PATH"
mkdir -p "$DEPS" "$PREFIX" "$OUT"

WAYLAND_TAG=1.23.1; WAYLAND_PROTOCOLS_TAG=1.41
FFNVCODEC_TAG=n13.0.19.0; FFMPEG_TAG=n7.1; SHADERC_TAG=v2024.0
VULKAN_TAG=vulkan-sdk-1.3.296.0  # jammy ships 1.3.204; mpv needs vulkan >= 1.3.238
say(){ echo -e "\n========== $* =========="; }
clone(){ [ -d "$2" ] || git clone --depth 1 -b "$3" "$1" "$2"; }

say "wayland $WAYLAND_TAG"
clone https://gitlab.freedesktop.org/wayland/wayland.git "$DEPS/wayland" "$WAYLAND_TAG"
meson setup "$DEPS/wayland/b" "$DEPS/wayland" --prefix="$PREFIX" --buildtype=release \
  -Dtests=false -Ddocumentation=false -Ddtd_validation=false 2>/dev/null || true
ninja -C "$DEPS/wayland/b" install

say "wayland-protocols $WAYLAND_PROTOCOLS_TAG"
clone https://gitlab.freedesktop.org/wayland/wayland-protocols.git "$DEPS/wayland-protocols" "$WAYLAND_PROTOCOLS_TAG"
meson setup "$DEPS/wayland-protocols/b" "$DEPS/wayland-protocols" --prefix="$PREFIX" -Dtests=false 2>/dev/null || true
ninja -C "$DEPS/wayland-protocols/b" install

say "nv-codec-headers $FFNVCODEC_TAG"
clone https://github.com/FFmpeg/nv-codec-headers.git "$DEPS/nv-codec-headers" "$FFNVCODEC_TAG"
make -C "$DEPS/nv-codec-headers" PREFIX="$PREFIX" install

say "FFmpeg $FFMPEG_TAG"
clone https://github.com/FFmpeg/FFmpeg.git "$DEPS/ffmpeg" "$FFMPEG_TAG"
( cd "$DEPS/ffmpeg" && ./configure --prefix="$PREFIX" --enable-gpl --enable-version3 \
    --enable-shared --disable-static --enable-ffnvcodec --enable-nvdec --enable-cuvid \
    --enable-nvenc --disable-programs --disable-doc --disable-debug \
    --extra-cflags="-I$PREFIX/include" --extra-ldflags="-L$PREFIX/lib" \
  && make -j"$JOBS" && make install )

say "Vulkan-Headers + Loader $VULKAN_TAG"
clone https://github.com/KhronosGroup/Vulkan-Headers.git "$DEPS/vulkan-headers" "$VULKAN_TAG"
cmake -B "$DEPS/vulkan-headers/b" -S "$DEPS/vulkan-headers" -DCMAKE_INSTALL_PREFIX="$PREFIX" >/dev/null
cmake --install "$DEPS/vulkan-headers/b"
clone https://github.com/KhronosGroup/Vulkan-Loader.git "$DEPS/vulkan-loader" "$VULKAN_TAG"
cmake -B "$DEPS/vulkan-loader/b" -S "$DEPS/vulkan-loader" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DVULKAN_HEADERS_INSTALL_DIR="$PREFIX" -DBUILD_TESTS=OFF
cmake --build "$DEPS/vulkan-loader/b" -j"$JOBS"; cmake --install "$DEPS/vulkan-loader/b"

say "shaderc $SHADERC_TAG"
if [ ! -d "$DEPS/shaderc" ]; then
  git clone --depth 1 -b "$SHADERC_TAG" https://github.com/google/shaderc.git "$DEPS/shaderc"
  ( cd "$DEPS/shaderc" && ./utils/git-sync-deps )
fi
cmake -B "$DEPS/shaderc/b" -S "$DEPS/shaderc" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON
cmake --build "$DEPS/shaderc/b" -j"$JOBS"; cmake --install "$DEPS/shaderc/b"

say "libplacebo"
clone https://code.videolan.org/videolan/libplacebo.git "$DEPS/libplacebo" v7.360.1
git -C "$DEPS/libplacebo" submodule update --init --recursive
meson setup "$DEPS/libplacebo/b" "$DEPS/libplacebo" --prefix="$PREFIX" --buildtype=release \
  -Dlibdovi=disabled -Ddemos=false -Dtests=false 2>/dev/null || true
ninja -C "$DEPS/libplacebo/b" install

say "mpv (this fork)"
meson setup "$MPV_SRC/b" "$MPV_SRC" --prefix="$PREFIX" --buildtype=release \
  -Dlibmpv=true -Dcuda-hwaccel=enabled -Dvulkan=enabled -Dlua=enabled 2>/dev/null || true
ninja -C "$MPV_SRC/b" install

# ---- collect a self-contained bundle -------------------------------------
# mpv + libmpv + every NEEDED .so, minus the host-provided set (glibc/libstdc++,
# the GL/Vulkan/DRM driver loaders, and the display libs that must match the host
# compositor/driver). libwayland-client speaks the stable wire protocol, so it is
# safe to bundle.
say "bundling"
BUNDLE="$OUT/mpv-linux-x64-$VER"
rm -rf "$BUNDLE"; mkdir -p "$BUNDLE"
cp -a "$PREFIX/bin/mpv" "$BUNDLE/"
for so in "$PREFIX"/lib/x86_64-linux-gnu/libmpv.so* "$PREFIX"/lib/libmpv.so*; do
  [ -e "$so" ] && cp -aL "$so" "$BUNDLE/" 2>/dev/null || true
done
# host-provided, never bundled (driver/display-coupled + the C/C++ runtime)
# libvulkan is bundled (our newer loader; it still finds the host's NVIDIA ICD at
# runtime). libGL/EGL/drm/gbm/cuda/nvidia stay host-provided (driver-coupled).
EXCLUDE='ld-linux|libc\.so|libm\.so|libdl\.so|libpthread|librt\.so|libresolv|libstdc\+\+|libgcc_s|libGL|libEGL|libGLX|libGLdispatch|libOpenGL|libdrm|libgbm|libva\.|libva-|libcuda\.so|libnvidia'
collect(){ # recursively copy a binary's non-excluded NEEDED libs
  ldd "$1" 2>/dev/null | awk '/=>/{print $3}' | grep -E '^/' | while read -r lib; do
    base=$(basename "$lib")
    echo "$base" | grep -Eq "$EXCLUDE" && continue
    [ -e "$BUNDLE/$base" ] && continue
    cp -aL "$lib" "$BUNDLE/$base" && collect "$lib"
  done
}
export -f collect; export BUNDLE EXCLUDE
collect "$BUNDLE/mpv"
for so in "$BUNDLE"/libmpv.so*; do collect "$so"; done

say "bundle glibc/GLIBCXX floor"
for f in "$BUNDLE/mpv" "$BUNDLE"/libmpv.so.2 "$BUNDLE"/libplacebo.so* "$BUNDLE"/libavcodec.so*; do
  [ -e "$f" ] || continue
  g=$(objdump -T "$f" 2>/dev/null | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sort -V | tail -1)
  printf "  %-28s GLIBC<=%s\n" "$(basename "$f")" "${g:-none}"
done

# flat archive (mpv + .so at top level) so the assembler extracts straight into mpv/
tar --zstd -C "$BUNDLE" -cf "$OUT/mpv-linux-x64-$VER.tar.zst" .
echo "$OUT/mpv-linux-x64-$VER.tar.zst"
