#!/bin/bash -e
set -euo pipefail

#sys_root = '/tmp': workaround to remove host include search paths
mkdir -p /tmp/usr/include

subprojects="subprojects"
if [ ! -d "$subprojects" ]; then
    mkdir -p "$subprojects"
fi

cat > "meson_cross.txt" <<EOF
[binaries]
c = ['clang', '--target=$ARCH-pc-windows-msvc', '-Xmicrosoft-windows-sys-root', '$WINSDK' , '--rtlib=platform']
cpp = ['clang++', '--target=$ARCH-pc-windows-msvc', '-Xmicrosoft-windows-sys-root', '$WINSDK' , '--rtlib=platform']
as = ['clang', '--target=$ARCH-pc-windows-msvc', '-Xmicrosoft-windows-sys-root', '$WINSDK' , '--rtlib=platform']
c_ld = 'lld-link'
cpp_ld = 'lld-link'
windres = ['llvm-windres', '--target=$ARCH-pc-windows-msvc', '--preprocessor-arg=--target=$ARCH-pc-windows-msvc', '--preprocessor-arg=-Xmicrosoft-windows-sys-root', '--preprocessor-arg=$WINSDK']
dlltool = 'llvm-dlltool'
ar = 'llvm-ar'
lib = 'llvm-lib'
ranlib = 'llvm-ranlib'
objcopy = 'llvm-objcopy'
strip = 'llvm-strip'
nasm = 'nasm'
pkg-config = 'pkg-config'
cmake = 'cmake'

[built-in options]
c_args = '-w'
cpp_args = '-w'

[host_machine]
system = 'windows'
cpu_family = '$ARCH'
cpu = '$ARCH-pc-windows-msvc'
endian = 'little'

[properties]
sys_root = '/tmp'

[cmake]
CMAKE_TOOLCHAIN_FILE = '$PWD/toolchain.cmake'
EOF

#-lm: workaround to fix luajit hostvm linking
cat > "meson_native.txt" <<EOF
[binaries]
c = 'clang'
cpp = 'clang++'
[built-in options]
c_args = '-w'
c_link_args = '-lm'
cpp_args = '-w'
cpp_link_args = '-lm'
EOF

cat > "toolchain.cmake" <<EOF
SET(CMAKE_SYSTEM_NAME Windows)
SET(CMAKE_SYSTEM_PROCESSOR $ARCH)
SET(CMAKE_TOOLCHAIN_PREFIX $ARCH-pc-windows-msvc)
SET(CMAKE_C_COMPILER clang)
SET(CMAKE_CXX_COMPILER clang++)
SET(CMAKE_ASM_COMPILER clang)
SET(CMAKE_RC_COMPILER "llvm-windres --target=$ARCH-pc-windows-msvc --preprocessor-arg=--target=$ARCH-pc-windows-msvc --preprocessor-arg=-Xmicrosoft-windows-sys-root --preprocessor-arg=$WINSDK")
SET(CMAKE_RC_COMPILER_INIT llvm-windres)
SET(CMAKE_C_FLAGS "-fuse-ld=lld --target=$ARCH-pc-windows-msvc -Xmicrosoft-windows-sys-root $WINSDK --rtlib=platform")
SET(CMAKE_CXX_FLAGS "-fuse-ld=lld --target=$ARCH-pc-windows-msvc -Xmicrosoft-windows-sys-root $WINSDK --rtlib=platform")
SET(CMAKE_ASM_FLAGS "-fuse-ld=lld --target=$ARCH-pc-windows-msvc -Xmicrosoft-windows-sys-root $WINSDK --rtlib=platform")
EOF

if [ ! -d "$subprojects/shaderc_cmake" ]; then
    git clone https://github.com/google/shaderc --depth 1 "$subprojects/shaderc_cmake"
    cat > "$subprojects/shaderc_cmake/p.diff" <<'EOF'
diff --git a/third_party/CMakeLists.txt b/third_party/CMakeLists.txt
index d44f62a..54d4719 100644
--- a/third_party/CMakeLists.txt
+++ b/third_party/CMakeLists.txt
@@ -87,7 +87,11 @@ if (NOT TARGET glslang)
       # Glslang tests are off by default. Turn them on if testing Shaderc.
       set(GLSLANG_TESTS ON)
     endif()
-    set(GLSLANG_ENABLE_INSTALL $<NOT:${SKIP_GLSLANG_INSTALL}>)
+    if (SKIP_GLSLANG_INSTALL)
+      set(GLSLANG_ENABLE_INSTALL OFF)
+    else()
+      set(GLSLANG_ENABLE_INSTALL ON)
+    endif()
     add_subdirectory(${SHADERC_GLSLANG_DIR} glslang)
   endif()
   if (NOT TARGET glslang)
EOF
    git -C "$subprojects/shaderc_cmake" apply --ignore-whitespace p.diff
fi

if [ ! -d "$subprojects/shaderc" ]; then
    mkdir -p "$subprojects/shaderc"
fi
cat > "$subprojects/shaderc/meson.build" <<'EOF'
project('shaderc', 'cpp', version: '2024.1')

python = find_program('python3')
run_command(python, '../shaderc_cmake/utils/git-sync-deps', check: true)

cmake = import('cmake')
opts = cmake.subproject_options()
opts.add_cmake_defines({
    'CMAKE_MSVC_RUNTIME_LIBRARY': 'MultiThreaded',
    'CMAKE_POLICY_DEFAULT_CMP0091': 'NEW',
    'SHADERC_SKIP_INSTALL': 'ON',
    'SHADERC_SKIP_TESTS': 'ON',
    'SHADERC_SKIP_EXAMPLES': 'ON',
    'SHADERC_SKIP_COPYRIGHT_CHECK': 'ON'
})
shaderc_proj = cmake.subproject('shaderc_cmake', options: opts)
shaderc_dep = declare_dependency(dependencies: [
    shaderc_proj.dependency('shaderc'),
    shaderc_proj.dependency('shaderc_util'),
    shaderc_proj.dependency('SPIRV-Tools-static'),
    shaderc_proj.dependency('SPIRV-Tools-opt'),
    shaderc_proj.dependency('glslang'),
])
meson.override_dependency('shaderc', shaderc_dep)
EOF

if [ ! -d "$subprojects/spirv-cross-c-shared" ]; then
    mkdir -p "$subprojects/spirv-cross-c-shared"
fi
cat > "$subprojects/spirv-cross-c-shared/meson.build" <<'EOF'
project('spirv-cross', 'cpp', version: '0.59.0')
cmake = import('cmake')
opts = cmake.subproject_options()
opts.add_cmake_defines({
    'CMAKE_MSVC_RUNTIME_LIBRARY': 'MultiThreaded',
    'CMAKE_POLICY_DEFAULT_CMP0091': 'NEW',
    'SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS': 'ON',
    'SPIRV_CROSS_CLI': 'OFF',
    'SPIRV_CROSS_ENABLE_TESTS': 'OFF',
    'SPIRV_CROSS_ENABLE_MSL': 'OFF',
    'SPIRV_CROSS_ENABLE_CPP': 'OFF',
    'SPIRV_CROSS_ENABLE_REFLECT': 'OFF',
    'SPIRV_CROSS_ENABLE_UTIL': 'OFF'
})
spirv_cross_proj = cmake.subproject('spirv-cross', options: opts)
spirv_cross_c_dep = declare_dependency(dependencies: [
    spirv_cross_proj.dependency('spirv-cross-c'),
    spirv_cross_proj.dependency('spirv-cross-core'),
    spirv_cross_proj.dependency('spirv-cross-glsl'),
    spirv_cross_proj.dependency('spirv-cross-hlsl'),
])
meson.override_dependency('spirv-cross-c-shared', spirv_cross_c_dep)
EOF

if [ ! -d "$subprojects/vulkan" ]; then
    mkdir -p "$subprojects/vulkan"
fi

#USE_GAS=OFF: workaround for 'gen_defines.asm': No such file or directory
cat > "$subprojects/vulkan/meson.build" <<'EOF'
project('vulkan', 'cpp', version: '1.3.285')
cmake = import('cmake')
opts = cmake.subproject_options()
opts.add_cmake_defines({
    'UPDATE_DEPS': 'ON',
    'USE_GAS': 'OFF'
})
opts.append_link_args(['-lcfgmgr32', '-Wl,/def:../subprojects/vulkan-loader/loader/vulkan-1.def'], target: 'vulkan')
vulkan_proj = cmake.subproject('vulkan-loader', options: opts)
vulkan_dep = vulkan_proj.dependency('vulkan')
meson.override_dependency('vulkan', vulkan_dep)
EOF

cat > "$subprojects/ffmpeg.wrap" <<'EOF'
[wrap-git]
url = https://gitlab.freedesktop.org/gstreamer/meson-ports/ffmpeg.git
revision = meson-7.1
depth = 1
clone-recursive = true
[provide]
dependency_names = libavcodec, libavdevice, libavfilter, libavformat, libavutil, libswresample, libswscale
program_names = ffmpeg
EOF

cat > "$subprojects/libass.wrap" <<'EOF'
[wrap-git]
url = https://github.com/libass/libass
revision = master
depth = 1
clone-recursive = true
EOF

cat > "$subprojects/libplacebo.wrap" <<'EOF'
[wrap-git]
url = https://code.videolan.org/videolan/libplacebo.git
revision = master
depth = 1
clone-recursive = true
EOF

cat > "$subprojects/dav1d.wrap" <<'EOF'
[wrap-git]
url = https://code.videolan.org/videolan/dav1d
revision = master
depth = 1
clone-recursive = true
[provide]
dav1d = dav1d_dep
EOF

cat > "$subprojects/spirv-cross.wrap" <<'EOF'
[wrap-git]
url = https://github.com/KhronosGroup/SPIRV-Cross
revision = main
depth = 1
clone-recursive = true
method = cmake
EOF

cat > "$subprojects/vulkan-loader.wrap" <<'EOF'
[wrap-git]
url = https://github.com/KhronosGroup/Vulkan-Loader
revision = main
depth = 1
clone-recursive = true
method = cmake
EOF

#nasm.wrap: workaround to fix --wrap-mode=forcefallback that causes meson to use wrapdb's nasm.exe which unrunnable on linux
cat > "$subprojects/nasm.wrap" <<'EOF'
[wrap-git]
url = https://aomedia.googlesource.com/aom
revision = main
depth = 1
clone-recursive = true
method = cmake
[provide]
aom = aom_dep
EOF

#win32-smtc=disabled: workaround for case sensitivity issues with winrt headers
meson setup build \
    --wrap-mode=forcefallback \
    -Ddefault_library=static \
    -Dlibmpv=true \
    -Dtests=false \
    -Dgpl=true \
    -Dffmpeg:gpl=enabled \
    -Dffmpeg:tests=enabled \
    -Dffmpeg:programs=enabled \
    -Dffmpeg:sdl2=disabled \
    -Dffmpeg:vulkan=auto \
    -Dffmpeg:libdav1d=enabled \
    -Dffmpeg:libaom=enabled \
    -Dlcms2:fastfloat=true \
    -Dlcms2:jpeg=disabled \
    -Dlcms2:tiff=disabled \
    -Dlibass:test=enabled \
    -Dlibjpeg-turbo:tests=disabled \
    -Dlibusb:tests=false \
    -Dlibusb:examples=false \
    -Dlibplacebo:demos=false \
    -Dlibplacebo:lcms=enabled \
    -Dlibplacebo:shaderc=enabled \
    -Dlibplacebo:tests=false \
    -Dlibplacebo:vulkan=enabled \
    -Dlibplacebo:d3d11=enabled \
    -Dxxhash:inline-all=true \
    -Dxxhash:cli=false \
    -Dluajit:amalgam=true \
    -Dluajit:lua52compat=true \
    -Dluajit:sysmalloc=true \
    -Dd3d11=enabled \
    -Dvulkan=enabled \
    -Djavascript=enabled \
    -Dwin32-smtc=disabled \
    -Dlua=luajit \
    -Ddrm=disabled \
    -Dlibarchive=disabled \
    -Drubberband=disabled \
    -Dwayland=disabled \
    -Dx11=disabled \
    --cross-file meson_cross.txt \
    --native-file meson_native.txt

ninja -C build mpv.exe mpv.com libmpv.a
cp ./build/subprojects/vulkan-loader/vulkan.dll ./build/vulkan-1.dll
