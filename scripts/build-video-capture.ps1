# Build samples/drivers/video/capture for XIAO ESP32S3 Sense + OV3660 module
# Usage: .\scripts\build-video-capture.ps1 [-Flash] [-Port COM16] [-Pristine]

param(
    [switch]$Flash,
    [switch]$Pristine,
    [string]$Port = "COM16"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$ExtraConf = (Join-Path $RepoRoot "config\video-capture-sense.conf") -replace '\\', '/'
$Overlay = (Join-Path $RepoRoot "boards\xiao_esp32s3_sense_ov3660.overlay") -replace '\\', '/'
$Module = (Join-Path $RepoRoot "modules\ov3660") -replace '\\', '/'

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
$env:PATHEXT = ".PY;" + $env:PATHEXT

Set-Location $ZephyrDir

$buildArgs = @("build")
if ($Pristine) {
    $buildArgs += "-p", "always"
}
$buildArgs += @(
    "-b", $Board,
    "samples/drivers/video/capture",
    "--",
    "-DEXTRA_CONF_FILE=$ExtraConf",
    "-DZEPHYR_EXTRA_MODULES=$Module",
    "-DDTC_OVERLAY_FILE=$Overlay"
)

& $West @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build OK: $ZephyrDir\build\zephyr\zephyr.elf"

if ($Flash) {
    & (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port
    & $West flash -- --esp-device $Port
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Flashed. Monitor: west espressif monitor -p $Port"
    Write-Host "When done: .\scripts\kill-serial-monitor.ps1 -Port $Port"
}
