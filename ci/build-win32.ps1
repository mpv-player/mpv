$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$subprojects = "subprojects"
if (-not (Test-Path $subprojects)) {
    New-Item -Path $subprojects -ItemType Directory | Out-Null
}

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
    -Dlibplacebo:vulkan=enabled `
    -Djavascript=enabled
ninja -C build mpv.exe mpv.com libmpv.a
./build/mpv.com -v --no-config
