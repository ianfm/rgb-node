$ErrorActionPreference = "Stop"
$env:PYTHONIOENCODING = "utf-8"

Write-Host "[rgb-node] Building React Web UI..." -ForegroundColor Cyan
npm --prefix web run build

Write-Host "[rgb-node] Building C++ Firmware..." -ForegroundColor Cyan
pio run -e esp32c3-supermini -e esp32c3-supermini-ota

Write-Host "[rgb-node] Build completed successfully." -ForegroundColor Green
