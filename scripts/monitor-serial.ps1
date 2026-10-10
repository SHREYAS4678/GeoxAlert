$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Pio = Join-Path $Root '.venv\Scripts\pio.exe'
if (-not (Test-Path $Pio)) { throw 'PlatformIO not installed. Run .\scripts\setup.ps1 first.' }
$port = Read-Host 'ESP32 COM port (e.g. COM3)'
if ([string]::IsNullOrWhiteSpace($port)) { throw 'COM port is required for serial monitor.' }
& $Pio device monitor -d (Join-Path $Root 'firmware') --port $port --baud 115200
