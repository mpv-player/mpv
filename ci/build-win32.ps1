$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$subprojects = "subprojects"
if (-not (Test-Path $subprojects)) {
    New-Item -Path $subprojects -ItemType Directory | Out-Null
}

# Wrap shaderc to run git-sync-deps and patch unsupported generator expression
if (-not (Test-Path "$subprojects/shaderc_cmake")) {
    git clone https://github.com/google/shaderc --depth 1 $subprojects/shaderc_cmake
    Set-Content -Path "$subprojects/shaderc_cmake/p.diff" -Value @'
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
'@
    git -C $subprojects/shaderc_cmake apply --ignore-whitespace p.diff
}
if (-not (Test-Path "$subprojects/shaderc")) {
    New-Item -Path "$subprojects/shaderc" -ItemType Directory | Out-Null
}
Set-Content -Path "$subprojects/shaderc/meson.build" -Value @"
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
    shaderc_proj.dependency('SPIRV'),
    shaderc_proj.dependency('SPIRV-Tools-static'),
    shaderc_proj.dependency('SPIRV-Tools-opt'),
    shaderc_proj.dependency('glslang'),
    shaderc_proj.dependency('GenericCodeGen'),
    shaderc_proj.dependency('MachineIndependent'),
])
meson.override_dependency('shaderc', shaderc_dep)
"@

# Manually wrap spirv-cross for CMAKE_MSVC_RUNTIME_LIBRARY option
# This also allows us to link statically
if (-not (Test-Path "$subprojects/spirv-cross-c-shared")) {
    New-Item -Path "$subprojects/spirv-cross-c-shared" -ItemType Directory | Out-Null
}
Set-Content -Path "$subprojects/spirv-cross-c-shared/meson.build" -Value @"
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
    'SPIRV_CROSS_ENABLE_UTIL': 'OFF',
})
spirv_cross_proj = cmake.subproject('spirv-cross', options: opts)
spirv_cross_c_dep = declare_dependency(dependencies: [
    spirv_cross_proj.dependency('spirv-cross-c'),
    spirv_cross_proj.dependency('spirv-cross-core'),
    spirv_cross_proj.dependency('spirv-cross-glsl'),
    spirv_cross_proj.dependency('spirv-cross-hlsl'),
])
meson.override_dependency('spirv-cross-c-shared', spirv_cross_c_dep)
"@

# Manually wrap Vulkan-Loader for UPDATE_DEPS option
if (-not (Test-Path "$subprojects/vulkan")) {
    New-Item -Path "$subprojects/vulkan" -ItemType Directory | Out-Null
}
Set-Content -Path "$subprojects/vulkan/meson.build" -Value @"
project('vulkan', 'cpp', version: '1.3.285')
cmake = import('cmake')
opts = cmake.subproject_options()
opts.add_cmake_defines({
    'UPDATE_DEPS': 'ON',
    'USE_GAS': 'ON',
})
opts.append_link_args(['-lcfgmgr32', '-Wl,/def:../subprojects/vulkan-loader/loader/vulkan-1.def'], target: 'vulkan')
vulkan_proj = cmake.subproject('vulkan-loader', options: opts)
vulkan_dep = vulkan_proj.dependency('vulkan')
meson.override_dependency('vulkan', vulkan_dep)
"@

$projects = @(
    @{
        Path = "$subprojects/ffmpeg.wrap"
        URL = "https://gitlab.freedesktop.org/gstreamer/meson-ports/ffmpeg.git"
        Revision = "meson-7.0"
        Provides = @(
            "libavcodec = libavcodec_dep",
            "libavdevice = libavdevice_dep",
            "libavfilter = libavfilter_dep",
            "libavformat = libavformat_dep",
            "libavutil = libavutil_dep",
            "libswresample = libswresample_dep",
            "libswscale = libswscale_dep"
        )
    },
    @{
        Path = "$subprojects/libass.wrap"
        URL = "https://github.com/libass/libass"
        Revision = "master"
    },
    @{
        Path = "$subprojects/libplacebo.wrap"
        URL = "https://code.videolan.org/videolan/libplacebo.git"
        Revision = "master"
    },
    @{
        Path = "$subprojects/dav1d.wrap"
        URL = "https://code.videolan.org/videolan/dav1d"
        Revision = "master"
        Provides = @(
            "dav1d = dav1d_dep"
        )
    },
    @{
        Path = "$subprojects/spirv-cross.wrap"
        URL = "https://github.com/KhronosGroup/SPIRV-Cross"
        Revision = "main"
        Method = "cmake"
    },
    @{
        Path = "$subprojects/vulkan-loader.wrap"
        URL = "https://github.com/KhronosGroup/Vulkan-Loader"
        Revision = "main"
        Method = "cmake"
    }
)

foreach ($project in $projects) {
    $content = @"
[wrap-git]
url = $($project.URL)
revision = $($project.Revision)
depth = 1
clone-recursive = true
"@
    if ($project.ContainsKey('Method')) {
        $content += "`nmethod = $($project.Method)"
    }
    if ($project.ContainsKey('Provides')) {
        $provide = "[provide]`n$($project.Provides -join "`n")"
        $content += "`n$provide"
    }
    Set-Content -Path $project.Path -Value $content
}

meson setup build `
    --wrap-mode=forcefallback `
    -Ddefault_library=static `
    -Dlibmpv=true `
    -Dtests=true `
    -Dgpl=true `
    -Dffmpeg:gpl=enabled `
    -Dffmpeg:tests=disabled `
    -Dffmpeg:programs=disabled `
    -Dffmpeg:sdl2=disabled `
    -Dffmpeg:vulkan=auto `
    -Dffmpeg:libdav1d=enabled `
    -Dlcms2:fastfloat=true `
    -Dlcms2:jpeg=disabled `
    -Dlcms2:tiff=disabled `
    -Dlibusb:tests=false `
    -Dlibusb:examples=false `
    -Dlibplacebo:demos=false `
    -Dlibplacebo:lcms=enabled `
    -Dlibplacebo:shaderc=enabled `
    -Dlibplacebo:vulkan=enabled `
    -Dlibplacebo:d3d11=enabled `
    -Dxxhash:inline-all=true `
    -Dxxhash:cli=false `
    -Dluajit:amalgam=true `
    -Dd3d11=enabled `
    -Dvulkan=enabled `
    -Djavascript=enabled `
    -Dwin32-smtc=enabled `
    -Dlua=luajit `
    -Ddrm=disabled `
    -Dlibarchive=disabled `
    -Drubberband=disabled `
    -Dwayland=disabled `
    -Dx11=disabled
ninja -C build mpv.exe mpv.com libmpv.a
cp ./build/subprojects/vulkan-loader/vulkan.dll ./build/vulkan-1.dll
./build/mpv.com -v --no-config
