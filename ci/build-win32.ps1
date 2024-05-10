$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$subprojects = "subprojects"
if (-not (Test-Path $subprojects)) {
    New-Item -Path $subprojects -ItemType Directory | Out-Null
}

# {avcodec,tests}: rename the bundled Mesa AV1 vulkan video headers
if (-not (Test-Path "$subprojects/packagefiles/ffmpeg")) {
    New-Item -Path "$subprojects/packagefiles/ffmpeg" -ItemType Directory | Out-Null
}
Invoke-WebRequest -Uri "https://github.com/FFmpeg/FFmpeg/commit/e06ce6d2b45edac4a2df04f304e18d4727417d24.patch" `
                  -OutFile "$subprojects/packagefiles/ffmpeg/e06ce6d2b45edac4a2df04f304e18d4727417d24.patch"

# Download pre-built shaderc, it is quite big to build each time
# For download link see https://github.com/google/shaderc/blob/main/downloads.md
$url = "https://storage.googleapis.com/shaderc/badges/build_link_windows_vs2019_release.html"
$shaderc = "shaderc.zip"
$resp = Invoke-WebRequest -Uri $url
if ($resp.Content -match '<meta http-equiv="refresh" content="\d+; url=(?<url>[^"]+)"') {
    $url = $matches['url']
}
Invoke-WebRequest -Uri $url -OutFile $shaderc
if (Test-Path "$subprojects/shaderc") {
    Remove-Item -LiteralPath "$subprojects/shaderc" -Force -Recurse
}
Expand-Archive -Path $shaderc -DestinationPath "$subprojects/shaderc"
Move-Item -Path "$subprojects/shaderc/install/*" -Destination "$subprojects/shaderc"

Set-Content -Path "$subprojects/shaderc/meson.build" -Value @"
project('shaderc', 'c', version: '2024.1')
cc = meson.get_compiler('c')
shaderc_dep = declare_dependency(
  dependencies: cc.find_library('shaderc_combined', dirs: meson.current_source_dir() / 'lib'),
  include_directories: include_directories('include')
)
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

$projects = @(
    @{
        Path = "$subprojects/ffmpeg.wrap"
        URL = "https://gitlab.freedesktop.org/gstreamer/meson-ports/ffmpeg.git"
        Revision = "meson-6.1"
        Patch = "ffmpeg/e06ce6d2b45edac4a2df04f304e18d4727417d24.patch"
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
        Path = "$subprojects/spirv-cross.wrap"
        URL = "https://github.com/KhronosGroup/SPIRV-Cross"
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
    if ($project.ContainsKey('Patch')) {
        $content += "`ndiff_files = $($project.Patch)"
    }
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
    -Djavascript=enabled `
    -Dlua=luajit `
    -Ddrm=disabled `
    -Dlibarchive=disabled `
    -Drubberband=disabled `
    -Dwayland=disabled `
    -Dx11=disabled
ninja -C build mpv.exe mpv.com libmpv.a
./build/mpv.com -v --no-config
