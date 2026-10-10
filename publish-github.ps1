$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git is not installed. Install Git for Windows first.' }
if (-not (Test-Path '.git')) { git init }
git add -A
if ($LASTEXITCODE -ne 0) { throw 'git add failed.' }
$staged = git diff --cached --name-only
$bad = $staged | Where-Object { $_ -match '(^|/)(secrets\.h|secrets\.toml|\.env|private/|.*firebase-adminsdk.*\.json|.*service-account.*\.json)$' }
if ($bad) { git reset | Out-Null; throw "Refusing to commit secret file(s): $($bad -join ', '). Remove them from Git and check .gitignore." }
git status --short
$msg = Read-Host 'Commit message [Initial GeoAlert deployment]'
if ([string]::IsNullOrWhiteSpace($msg)) { $msg = 'Initial GeoAlert deployment' }
git commit -m $msg
if ($LASTEXITCODE -ne 0) { throw 'Commit failed. Configure git user.name and user.email, then retry.' }
$remote = git remote get-url origin 2>$null
if (-not $remote) {
    $remote = Read-Host 'Paste your NEW empty GitHub repository URL (HTTPS or SSH)'
    if ([string]::IsNullOrWhiteSpace($remote)) { Write-Host 'Local commit created; no remote was configured.'; exit 0 }
    git remote add origin $remote
}
git branch -M main
git push -u origin main
if ($LASTEXITCODE -ne 0) { throw 'Push failed. Authenticate with GitHub and verify the repository URL.' }
Write-Host 'Project pushed to GitHub. Verify repository contents and ensure no secrets appear.' -ForegroundColor Green
