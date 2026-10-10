$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
if (-not (Get-Command py -ErrorAction SilentlyContinue)) { throw 'Python launcher (py.exe) not found. Install Python 3.11 from python.org and select the Python launcher.' }
& py -3.11 --version | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Python 3.11 is required. Install Python 3.11, then rerun this script.' }
if (-not (Test-Path '.\.venv\Scripts\python.exe')) { & py -3.11 -m venv .venv }
& '.\.venv\Scripts\python.exe' -m pip install --upgrade pip
& '.\.venv\Scripts\python.exe' -m pip install -r '.\dashboard\requirements.txt' -r '.\backend\requirements.txt' platformio
Write-Host 'Setup complete. Next run scripts/configure.ps1, then scripts/build-firmware.ps1 and scripts/run-dashboard.ps1.' -ForegroundColor Green
