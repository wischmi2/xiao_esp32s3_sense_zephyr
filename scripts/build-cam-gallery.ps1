# Build app/cam_gallery — Phase 2+5 combined capture + Wi-Fi gallery
# Usage: .\scripts\build-cam-gallery.ps1 [-Flash] [-Port COM13] [-Pristine]
#
# Requires config/wifi-credentials.conf and microSD inserted.

param(
    [switch]$Flash,
    [switch]$Pristine,
    [string]$Port = "COM13"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$West = "C:\ncs\toolchains\0b393f9e1b\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$AppDir = Join-Path $RepoRoot "app\cam_gallery"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$GalleryConf = (Join-Path $RepoRoot "config\cam-gallery-sense.conf") -replace '\\', '/'
$CredConf = Join-Path $RepoRoot "config\wifi-credentials.conf"
$OvOverlay = (Join-Path $RepoRoot "boards\xiao_esp32s3_sense_ov3660.overlay") -replace '\\', '/'
$WifiOverlay = (Join-Path $RepoRoot "boards\xiao_esp32s3_sense_wifi.overlay") -replace '\\', '/'
$DeferOverlay = (Join-Path $RepoRoot "boards\xiao_esp32s3_sense_cam_gallery.overlay") -replace '\\', '/'
$Module = (Join-Path $RepoRoot "modules\ov3660") -replace '\\', '/'

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.4"
$env:PATHEXT = ".PY;" + $env:PATHEXT

if (-not (Test-Path $CredConf)) {
    Write-Error "Missing $CredConf - copy config/wifi-credentials.conf.example and set SSID/password."
}

$CredConfUnix = $CredConf -replace '\\', '/'
$ExtraConf = "$GalleryConf;$CredConfUnix"
$Overlay = "$OvOverlay;$WifiOverlay;$DeferOverlay"

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
    "-DZEPHYR_EXTRA_MODULES=$Module",
    "-DDTC_OVERLAY_FILE=$Overlay"
)

& $West @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build OK: $ZephyrDir\build\zephyr\zephyr.elf"
Write-Host "Flash:   .\scripts\build-cam-gallery.ps1 -Flash -Port $Port"
Write-Host 'Expect:  >>> Gallery: http://<ip>/  |  BOOT = capture <<<'

if ($Flash) {
    & (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port
    & $West flash -- --esp-device $Port
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Flashed. Monitor: west espressif monitor -p $Port"
}
