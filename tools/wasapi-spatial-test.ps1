[CmdletBinding()]
param(
    [string]$MediaPath,
    [string]$MpvPath = (Join-Path $PSScriptRoot "mpv.com"),
    [string]$AudioDevice = "auto",
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
Write-Host "Device: $AudioDevice"
Write-Host "Media:  $MediaPath"
Write-Host "Log:    $LogPath"
Write-Host ""
Write-Host "Playback may take the full duration of the media. Press Ctrl+C to stop."

$arguments = @(
    "--no-config",
    "--ao=wasapi-spatial",
    "--audio-device=$AudioDevice",
    "--msg-level=ao=debug",
    "--log-file=$LogPath"
)

& $MpvPath @arguments $MediaPath
$mpvExit = $LASTEXITCODE

$logText = ""
if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
    $logText = Get-Content -LiteralPath $LogPath -Raw
}

if ($logText -match "Using Windows Spatial Audio:") {
    Write-Host ""
    Write-Host "SPATIAL AUDIO STREAM INITIALIZED." -ForegroundColor Green
    $match = [regex]::Match($logText, "Using Windows Spatial Audio:.*")
    if ($match.Success) {
        Write-Host $match.Value
    }
    Write-Host "Check the listening result and the full log before marking the hardware test complete."
} else {
    Write-Warning "The log did not contain the Spatial Audio initialization message."
    Write-Warning "Read the log for the activation HRESULT and endpoint details: $LogPath"
}

if ($mpvExit -ne 0) {
    Write-Warning "mpv exited with code $mpvExit."
    exit $mpvExit
}

if ($logText -notmatch "Using Windows Spatial Audio:") {
    exit 1
}

exit 0
