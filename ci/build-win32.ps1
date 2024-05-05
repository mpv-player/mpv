$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$subprojects = "subprojects"
if (-not (Test-Path $subprojects)) {
    New-Item -Path $subprojects -ItemType Directory | Out-Null
}

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

$projects = @(
    @{
        Path = "$subprojects/ffmpeg.wrap"
        URL = "https://gitlab.freedesktop.org/gstreamer/meson-ports/ffmpeg.git"
        Revision = "meson-6.1"
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
    # Remove harfbuzz wrap once the new version with build fixes is released.
    @{
        Path = "$subprojects/harfbuzz.wrap"
        URL = "https://github.com/harfbuzz/harfbuzz"
        Revision = "main"
        Provides = @(
            "harfbuzz = libharfbuzz_dep"
        )
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
    if ($project.ContainsKey('Provides')) {
        $provide = "[provide]`n$($project.Provides -join "`n")"
        $content += "`n$provide"
    }
    Set-Content -Path $project.Path -Value $content
}

meson setup build `
    -Ddefault_library=static `
    -Dlibmpv=true `
    -Dtests=true `
    -Dgpl=true `
    -Dffmpeg:gpl=enabled `
    -Dffmpeg:tests=disabled `
    -Dffmpeg:programs=disabled `
    -Dlcms2:fastfloat=true `
    -Dlibplacebo:demos=false `
    -Dlibplacebo:lcms=enabled `
    -Dlibplacebo:shaderc=enabled `
    -Dlibplacebo:vulkan=enabled `
    -Djavascript=enabled
ninja -C build mpv.exe mpv.com libmpv.a
./build/mpv.com -v --no-config
