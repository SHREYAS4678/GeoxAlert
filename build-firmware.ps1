$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Pio = Join-Path $Root '.venv\Scripts\pio.exe'
if (-not (Test-Path $Pio)) { throw 'PlatformIO not installed. Run .\scripts\setup.ps1 first.' }
if (-not (Test-Path (Join-Path $Root 'firmware\include\secrets.h'))) { throw 'Missing firmware/include/secrets.h. Run .\scripts\configure.ps1 first.' }
& $Pio run -d (Join-Path $Root 'firmware')
if ($LASTEXITCODE -ne 0) { throw 'PlatformIO build failed; read the compiler error above.' }
