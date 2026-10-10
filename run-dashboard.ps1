$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Python = Join-Path $Root '.venv\Scripts\python.exe'
if (-not (Test-Path $Python)) { throw 'Run .\scripts\setup.ps1 first.' }
if (-not (Test-Path (Join-Path $Root 'dashboard\.streamlit\secrets.toml'))) { Write-Warning 'Dashboard secrets are missing. Run .\scripts\configure.ps1 first; without credentials the dashboard uses demo/offline data.' }
Push-Location (Join-Path $Root 'dashboard')
try { & $Python -m streamlit run app.py --server.port 8501 }
finally { Pop-Location }
