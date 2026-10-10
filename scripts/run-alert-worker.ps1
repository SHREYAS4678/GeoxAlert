$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Python = Join-Path $Root '.venv\Scripts\python.exe'
if (-not (Test-Path $Python)) { throw 'Run .\scripts\setup.ps1 first.' }
if (-not (Test-Path (Join-Path $Root '.env'))) { throw 'Run .\scripts\configure.ps1 first.' }
& $Python (Join-Path $Root 'backend\twilio_worker.py')
