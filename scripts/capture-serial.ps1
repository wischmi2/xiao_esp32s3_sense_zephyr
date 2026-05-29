# Capture west espressif monitor output to logs/serial/
# Usage: .\scripts\capture-serial.ps1 [-Port COM16] [-OutFile name.txt] [-Seconds 45]

param(
    [string]$Port = "COM16",
    [string]$OutFile = "",
    [int]$Seconds = 45
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$LogDir = Join-Path $RepoRoot "logs\serial"
$West = "C:\ncs\toolchains\66cdf9b75e\opt\bin\Scripts\west.exe"

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

if (-not $OutFile) {
    $OutFile = "serial_{0:yyyyMMdd_HHmmss}.txt" -f (Get-Date)
}

$OutPath = Join-Path $LogDir $OutFile
$ErrPath = $OutPath -replace '\.txt$', '.err.txt'

& (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port

$env:PATHEXT = ".PY;" + $env:PATHEXT
Set-Location "C:\zephyrproject\zephyr"

$proc = Start-Process -FilePath $West -ArgumentList @("espressif", "monitor", "-p", $Port) `
    -PassThru -NoNewWindow -RedirectStandardOutput $OutPath -RedirectStandardError $ErrPath

Start-Sleep -Seconds $Seconds

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

Start-Sleep -Seconds 1
& (Join-Path $RepoRoot "scripts\kill-serial-monitor.ps1") -Port $Port

Write-Host "Saved: $OutPath"
if (Test-Path $ErrPath) {
    Write-Host "Stderr: $ErrPath"
}
