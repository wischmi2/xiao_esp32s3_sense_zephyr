# Build samples/hello_world for XIAO ESP32S3 Sense (Phase 0 smoke test)
# Usage: .\scripts\build-hello.ps1 [-Pristine]

param(
    [switch]$Pristine
)

$ErrorActionPreference = "Stop"

$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"
$ZephyrDir = "C:\zephyrproject\zephyr"
$Board = "xiao_esp32s3/esp32s3/procpu/sense"
$PicolibcConf = "C:/zephyrproject/picolibc-module.conf"

$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"

if (-not (Test-Path $PicolibcConf)) {
    @"
CONFIG_PICOLIBC_USE_MODULE=y
"@ | Set-Content -Path "C:\zephyrproject\picolibc-module.conf" -Encoding ascii
    Write-Host "Created $PicolibcConf"
}

Set-Location $ZephyrDir

$buildArgs = @(
    "build",
    "-b", $Board,
    "samples/hello_world",
    "--",
    "-DEXTRA_CONF_FILE=$PicolibcConf"
)

if ($Pristine) {
    $buildArgs = @("build", "-p", "always") + $buildArgs[1..($buildArgs.Length - 1)]
}

& $West @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "Build OK: $ZephyrDir\build\zephyr\zephyr.elf"
Write-Host "Flash:   west flash"
Write-Host "Monitor: west espressif monitor"
