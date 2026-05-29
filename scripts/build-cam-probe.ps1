# Build cam_i2c_probe — Phase 2 step 1 (confirm OV3660 on I2C)
# Usage: .\scripts\build-cam-probe.ps1 [-Flash] [-Port COM16]

param(
    [switch]$Flash,
    [string]$Port = "COM16"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$AppDir = Join-Path $RepoRoot "app\cam_i2c_probe"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$ExtraConf = (Join-Path $RepoRoot "config\picolibc-module.conf") -replace '\\', '/'

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
$env:PATHEXT = ".PY;" + $env:PATHEXT

Set-Location $ZephyrDir

$buildArgs = @(
    "build", "-p", "always",
    "-b", $Board,
    $AppDir,
    "--",
    "-DEXTRA_CONF_FILE=$ExtraConf"
)

& $West @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Build OK: $ZephyrDir\build\zephyr\zephyr.elf"

if ($Flash) {
    & (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port
    & $West flash -- --esp-device $Port
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Flashed. Run: west espressif monitor -p $Port"
    Write-Host "When done, run: .\scripts\kill-serial-monitor.ps1 -Port $Port"
}
