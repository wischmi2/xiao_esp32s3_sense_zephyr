# Build samples/subsys/fs/fs_sample for XIAO ESP32S3 Sense (Phase 1)
# Usage: .\scripts\build-fs-sample.ps1 [-Pristine]

param(
    [switch]$Pristine
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$ExtraConf = ($RepoRoot -replace '\\', '/') + "/config/fs_sample-sense.conf"

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"

Set-Location $ZephyrDir

$buildArgs = @(
    "build",
    "-b", $Board,
    "samples/subsys/fs/fs_sample",
    "--",
    "-DEXTRA_CONF_FILE=$ExtraConf"
)

if ($Pristine) {
    $buildArgs = @("build", "-p", "always") + $buildArgs[1..($buildArgs.Length - 1)]
}

& $West @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build OK: $ZephyrDir\build\zephyr\zephyr.elf"
Write-Host "Flash:   `$env:PATHEXT = '.PY;' + `$env:PATHEXT; west flash"
Write-Host "Monitor: west espressif monitor -p COM16"
