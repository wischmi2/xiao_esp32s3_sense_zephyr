# Build app/wifi_connect — Phase 4 Wi-Fi connectivity
# Usage: .\scripts\build-wifi-connect.ps1 [-Flash] [-Port COM16] [-Pristine]
#
# Before first build, copy config/wifi-credentials.conf.example to
# config/wifi-credentials.conf and set your SSID/password.

param(
    [switch]$Flash,
    [switch]$Pristine,
    [string]$Port = "COM16"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$AppDir = Join-Path $RepoRoot "app\wifi_connect"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$WifiConf = (Join-Path $RepoRoot "config\wifi-sense.conf") -replace '\\', '/'
$CredConf = Join-Path $RepoRoot "config\wifi-credentials.conf"
$Overlay = (Join-Path $RepoRoot "boards\xiao_esp32s3_sense_wifi.overlay") -replace '\\', '/'

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
$env:PATHEXT = ".PY;" + $env:PATHEXT

if (-not (Test-Path $CredConf)) {
    Write-Host "WARNING: $CredConf not found."
    Write-Host "Copy config/wifi-credentials.conf.example and set SSID/password."
    Write-Host "You can still use the shell: wifi scan / wifi connect"
    $ExtraConf = $WifiConf
} else {
    $CredConfUnix = $CredConf -replace '\\', '/'
    $ExtraConf = "$WifiConf;$CredConfUnix"
}

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
Write-Host "Flash:   .\scripts\build-wifi-connect.ps1 -Flash -Port $Port"
Write-Host "Expect:  >>> Wi-Fi ready — ping <ip> <<< on serial"

if ($Flash) {
    & (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port
    & $West flash -- --esp-device $Port
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Flashed. Monitor: west espressif monitor -p $Port"
}
