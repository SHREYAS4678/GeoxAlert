$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Python = Join-Path $Root '.venv\Scripts\python.exe'
if (-not (Test-Path $Python)) { throw 'Run .\scripts\setup.ps1 first.' }
$Dataset = Join-Path $Root 'data\geoalert_training_dataset.csv'
if (-not (Test-Path $Dataset)) { throw "Dataset not found at $Dataset. Copy geoalert_training_dataset.csv from the deployment bundle into .\data\." }
& $Python (Join-Path $Root 'train_models.py')
if ($LASTEXITCODE -ne 0) { throw 'Model training failed. Read the Python error above.' }
Write-Host 'Training complete. Review .\reports\training_report.txt and .\models\.' -ForegroundColor Green
