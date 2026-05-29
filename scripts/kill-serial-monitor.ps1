# Kill leftover west espressif monitor / idf_monitor processes that lock COM ports.
# Run this BEFORE west flash or west espressif monitor if you see "Access is denied".
#
# Usage:
#   .\scripts\kill-serial-monitor.ps1
#   .\scripts\kill-serial-monitor.ps1 -Port COM16

param(
    [string]$Port = "COM16"
)

$ErrorActionPreference = "SilentlyContinue"

Write-Host "Looking for processes holding serial port $Port ..." -ForegroundColor Cyan

$killed = 0

Get-CimInstance Win32_Process |
  Where-Object {
    $_.Name -match '^(python|pythonw|west)\.exe$' -and
    $_.CommandLine -match 'idf_monitor|espressif monitor|esptool'
  } |
  ForEach-Object {
    Write-Host "Killing PID $($_.ProcessId): $($_.Name)" -ForegroundColor Yellow
    Write-Host "  $($_.CommandLine)"
    Stop-Process -Id $_.ProcessId -Force
    $killed++
  }

# Also match command lines that reference this COM port explicitly
Get-CimInstance Win32_Process |
  Where-Object {
    $_.CommandLine -and $_.CommandLine -match [regex]::Escape($Port)
  } |
  ForEach-Object {
    if ($_.Name -match 'python|idf|monitor|west|esptool') {
      Write-Host "Killing PID $($_.ProcessId) (port in cmdline): $($_.Name)" -ForegroundColor Yellow
      Write-Host "  $($_.CommandLine)"
      Stop-Process -Id $_.ProcessId -Force
      $killed++
    }
  }

if ($killed -eq 0) {
  Write-Host "No obvious idf_monitor/esptool processes found." -ForegroundColor Green
  Write-Host "If port is still locked, use Resource Monitor (resmon.exe) -> CPU -> Handles -> search $Port"
} else {
  Write-Host "Stopped $killed process(es). Wait 2 seconds, then retry west flash." -ForegroundColor Green
  Start-Sleep -Seconds 2
}

Write-Host ""
Write-Host "Available COM ports:"
[System.IO.Ports.SerialPort]::getportnames() | ForEach-Object { Write-Host "  $_" }
