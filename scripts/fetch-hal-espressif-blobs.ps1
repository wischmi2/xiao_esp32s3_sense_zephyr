# Download hal_espressif binary blobs when west blobs fetch fails (SSL).
param([string]$ZephyrProject = "C:\zephyrproject")
$ErrorActionPreference = "Stop"
$ModuleYml = Join-Path $ZephyrProject "modules\hal\espressif\zephyr\module.yml"
$BlobRoot = Join-Path $ZephyrProject "modules\hal\espressif\zephyr\blobs"
$text = Get-Content $ModuleYml -Raw
$matches = [regex]::Matches($text, '(?m)^  - path: (.+)$[\s\S]*?^    url: (.+)$')
Write-Host "Fetching $($matches.Count) blobs..."
$i = 0
foreach ($m in $matches) {
    $i++
    $rel = $m.Groups[1].Value.Trim()
    $url = $m.Groups[2].Value.Trim()
    $dest = Join-Path $BlobRoot $rel
    $dir = Split-Path $dest -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    if ((Test-Path $dest) -and (Get-Item $dest).Length -gt 0) { continue }
    Write-Host "[$i/$($matches.Count)] $rel"
    curl.exe -L -k -s -S -o $dest $url
    if ($LASTEXITCODE -ne 0) { throw "curl failed: $url" }
}
Write-Host "Blob fetch complete."
