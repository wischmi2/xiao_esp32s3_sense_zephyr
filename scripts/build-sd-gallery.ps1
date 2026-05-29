# Build app/sd_gallery — Phase 5 wireless SD picture browser
# Usage: .\scripts\build-sd-gallery.ps1 [-Flash] [-Port COM16] [-Pristine]
#
# Requires config/wifi-credentials.conf with your AP SSID/password.
# SD card must contain JPEG files (e.g. ZEPHR000.JPG from cam_capture_sd).

param(
    [switch]$Flash,
    [switch]$Pristine,
    [string]$Port = "COM16"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$AppDir = Join-Path $RepoRoot "app\sd_gallery"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$WifiConf = (Join-Path $RepoRoot "config\wifi-sense.conf") -replace '\\', '/'
$GalleryConf = (Join-Path $RepoRoot "config\sd-gallery-sense.conf") -replace '\\', '/'
$CredConf = Join-Path $RepoRoot "config\wifi-credentials.conf"
$Overlay = (Join-Path $RepoRoot "boards\xiao_esp32s3_sense_wifi.overlay") -replace '\\', '/'

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
$env:PATHEXT = ".PY;" + $env:PATHEXT

if (-not (Test-Path $CredConf)) {
    Write-Error "Missing $CredConf — copy config/wifi-credentials.conf.example and set SSID/password."
}

$CredConfUnix = $CredConf -replace '\\', '/'
$ExtraConf = "$WifiConf;$GalleryConf;$CredConfUnix"

Set-Location $ZephyrDir

$buildArgs = @("build")
if ($Pristine) {
    $buildArgs += "-p", "always"
}
$buildArgs += @(
    "-b", $Board,
    $AppDir,
    "--",
    "-DEXTRA_CONF_FILE=$ExtraConf",
    "-DDTC_OVERLAY_FILE=$Overlay"
)

& $West @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build OK: $ZephyrDir\build\zephyr\zephyr.elf"
Write-Host "Flash:   .\scripts\build-sd-gallery.ps1 -Flash -Port $Port"
Write-Host "Expect:  >>> SD gallery at http://<ip>/ <<< on serial"

if ($Flash) {
    & (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port
    & $West flash -- --esp-device $Port
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Flashed. Open the printed URL in a browser on the same LAN."
}
