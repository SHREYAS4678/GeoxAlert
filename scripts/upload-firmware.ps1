$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Pio = Join-Path $Root '.venv\Scripts\pio.exe'
if (-not (Test-Path $Pio)) { throw 'PlatformIO not installed. Run .\scripts\setup.ps1 first.' }
if (-not (Test-Path (Join-Path $Root 'firmware\include\secrets.h'))) { throw 'Missing firmware/include/secrets.h. Run .\scripts\configure.ps1 first.' }
$port = Read-Host 'ESP32 COM port (e.g. COM3; blank lets PlatformIO auto-detect)'
if ([string]::IsNullOrWhiteSpace($port)) { & $Pio run -d (Join-Path $Root 'firmware') -t upload }
else { & $Pio run -d (Join-Path $Root 'firmware') --upload-port $port -t upload }
if ($LASTEXITCODE -ne 0) { throw 'Upload failed. Check the COM port, USB data cable and ESP32 boot mode.' }
