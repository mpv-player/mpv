[CmdletBinding()]
param(
    [string]$MediaPath,
    [string]$MpvPath = (Join-Path $PSScriptRoot "mpv.com"),
    [string]$AudioDevice = "auto",
    [string]$AudioChannels = "fl-fr-fc-lfe-bl-br-sl-sr-tfl-tfr-tbl-tbr",
    [string]$LogPath = (Join-Path (Get-Location) "wasapi-spatial-test.log"),
    [switch]$ListDevices
)

$ErrorActionPreference = "Stop"

function Stop-Test([string]$Message) {
    Write-Host $Message -ForegroundColor Red
    exit 2
}

if (-not (Test-Path -LiteralPath $MpvPath -PathType Leaf)) {
    Stop-Test "mpv console executable not found: $MpvPath"
}

$MpvPath = (Resolve-Path -LiteralPath $MpvPath).Path

if ($ListDevices) {
    Write-Host "Enumerating endpoints through the wasapi-spatial driver..."
    & $MpvPath "--no-config" "--ao=wasapi-spatial" "--audio-device=help"
    exit $LASTEXITCODE
}

if ([string]::IsNullOrWhiteSpace($MediaPath)) {
    Stop-Test "Pass -MediaPath <file> or use -ListDevices."
}
if (-not (Test-Path -LiteralPath $MediaPath -PathType Leaf)) {
    Stop-Test "Media file not found: $MediaPath"
}
$MediaPath = (Resolve-Path -LiteralPath $MediaPath).Path

$logDirectory = Split-Path -Parent $LogPath
if ($logDirectory -and -not (Test-Path -LiteralPath $logDirectory)) {
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
}
# Never let a previous successful run satisfy this test.
if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
    Remove-Item -LiteralPath $LogPath -Force
}

Write-Host "Checking that this mpv build registers wasapi-spatial..."
$aoHelp = (& $MpvPath "--no-config" "--ao=help" 2>&1 | Out-String)
$aoExit = $LASTEXITCODE
if ($aoExit -ne 0) {
    Stop-Test "mpv --ao=help failed with exit code $aoExit. $aoHelp"
}
if ($aoHelp -notmatch "(?im)\bwasapi-spatial\b") {
    Stop-Test "This mpv binary does not list wasapi-spatial. Use the Windows SDK Spatial Audio build artifact."
}
Write-Host "Audio driver registration: OK" -ForegroundColor Green

Write-Host ""
Write-Host "Starting STRICT Spatial Audio playback (no WASAPI fallback)."
Write-Host "Device:       $AudioDevice"
Write-Host "Channel map:  $AudioChannels"
Write-Host "Media:        $MediaPath"
Write-Host "Log:          $LogPath"
Write-Host ""
Write-Host "The default test requires 12 static objects and a 12-channel 7.1.4 AO."
Write-Host "Playback may take the full duration of the media. Press Ctrl+C to stop."

$arguments = @(
    "--no-config",
    "--ao=wasapi-spatial",
    "--audio-device=$AudioDevice",
    "--audio-channels=$AudioChannels",
    "--ad-lavc-downmix=no",
    "--msg-level=all=debug",
    "--log-file=$LogPath"
)

& $MpvPath @arguments $MediaPath
$mpvExit = $LASTEXITCODE

$logText = ""
if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
    $logText = Get-Content -LiteralPath $LogPath -Raw
}

$escapedChannels = [regex]::Escape($AudioChannels)
$spatialMatch = [regex]::Match(
    $logText,
    "(?m)Using Windows Spatial Audio:.*$escapedChannels.*\(12 static objects\)\."
)
$aoMatch = [regex]::Match(
    $logText,
    "(?m)AO: \[wasapi-spatial\].*$escapedChannels\s+12ch"
)

if ($spatialMatch.Success -and $aoMatch.Success) {
    Write-Host ""
    Write-Host "STRICT SPATIAL AUDIO VALIDATION PASSED." -ForegroundColor Green
    Write-Host $spatialMatch.Value
    Write-Host $aoMatch.Value
    Write-Host "Listening quality and long-run stability still require hardware verification."
} else {
    Write-Warning "Strict Spatial Audio validation failed."
    if (-not $spatialMatch.Success) {
        Write-Warning "Expected the selected channel map and exactly 12 static objects."
    }
    if (-not $aoMatch.Success) {
        Write-Warning "Expected the wasapi-spatial AO to report the selected map and 12ch."
    }
    Write-Warning "Inspect the fresh log: $LogPath"
}

if ($mpvExit -ne 0) {
    Write-Warning "mpv exited with code $mpvExit."
    exit $mpvExit
}
if (-not ($spatialMatch.Success -and $aoMatch.Success)) {
    exit 1
}
exit 0
