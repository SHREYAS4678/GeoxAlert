$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Read-SecretText([string]$Prompt) {
    $sec = Read-Host $Prompt -AsSecureString
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec)
    try { return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
}
function CQuote([string]$Value) {
    if ($null -eq $Value) { $Value = '' }
    $Value = $Value.Replace('\', '\\').Replace('"', '\"').Replace("`r", '\r').Replace("`n", '\n')
    return '"' + $Value + '"'
}
function TomlQuote([string]$Value) {
    if ($null -eq $Value) { $Value = '' }
    $Value = $Value.Replace('\', '\\').Replace('"', '\"').Replace("`r", '').Replace("`n", '\n')
    return '"' + $Value + '"'
}
function EnvSafe([string]$Value) {
    if ($null -eq $Value) { return '' }
    return $Value.Replace("`r", '').Replace("`n", '')
}
function Write-Utf8NoBom([string]$Path, [string[]]$Lines) {
    $fullPath = Join-Path $Root $Path
    $content = ($Lines -join "`r`n") + "`r`n"
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($fullPath, $content, $encoding)
}

New-Item -ItemType Directory -Force -Path '.\firmware\include', '.\dashboard\.streamlit', '.\private' | Out-Null
Write-Host "GeoAlert local configuration" -ForegroundColor Cyan
Write-Host "Use NEW/rotated credentials. Never commit the generated files." -ForegroundColor Yellow

$wifi = Read-Host 'Wi-Fi SSID'
$wifiPass = Read-SecretText 'Wi-Fi password'
$api = Read-Host 'Firebase Web API key'
$email = Read-Host 'Firebase Authentication email'
$fbPass = Read-SecretText 'Firebase Authentication password'
$dbDefault = 'https://geoalert-ai-ee1fd-default-rtdb.asia-southeast1.firebasedatabase.app'
$db = Read-Host "Firebase Realtime Database URL [$dbDefault]"
if ([string]::IsNullOrWhiteSpace($db)) { $db = $dbDefault }
$node = Read-Host 'Node ID [geoalert-node-01]'
if ([string]::IsNullOrWhiteSpace($node)) { $node = 'geoalert-node-01' }
$apn = Read-Host 'SIM800L GPRS APN [www]'
if ([string]::IsNullOrWhiteSpace($apn)) { $apn = 'www' }
$phone = Read-Host 'Optional direct-SMS destination in +countrycode format (blank disables direct SMS)'
$direct = Read-Host 'Enable prototype direct SIM800L SMS? Type YES only after testing modem/network [no]'
$directEnabled = ($direct -ceq 'YES' -and -not [string]::IsNullOrWhiteSpace($phone))

$header = @(
    '#pragma once',
    '// Generated locally by scripts/configure.ps1. Do not commit this file.',
    "#define GEOALERT_WIFI_SSID $(CQuote $wifi)",
    "#define GEOALERT_WIFI_PASSWORD $(CQuote $wifiPass)",
    "#define GEOALERT_FIREBASE_API_KEY $(CQuote $api)",
    "#define GEOALERT_FIREBASE_DB_URL $(CQuote $db)",
    "#define GEOALERT_FIREBASE_EMAIL $(CQuote $email)",
    "#define GEOALERT_FIREBASE_PASSWORD $(CQuote $fbPass)",
    "#define GEOALERT_DEVICE_ID $(CQuote $node)",
    "#define GEOALERT_GPRS_APN $(CQuote $apn)",
    '#define GEOALERT_GPRS_USER ""',
    '#define GEOALERT_GPRS_PASS ""',
    "#define GEOALERT_ALERT_PHONE $(CQuote $phone)",
    "#define GEOALERT_DIRECT_SMS_ENABLED $($directEnabled.ToString().ToLowerInvariant())"
)
Write-Utf8NoBom '.\firmware\include\secrets.h' $header

$toml = @(
    '# Generated locally by scripts/configure.ps1. Do not commit this file.',
    "FIREBASE_API_KEY = $(TomlQuote $api)",
    "FIREBASE_EMAIL = $(TomlQuote $email)",
    "FIREBASE_PASSWORD = $(TomlQuote $fbPass)",
    'ALERT_WEBHOOK_URL = ""'
)
Write-Utf8NoBom '.\dashboard\.streamlit\secrets.toml' $toml

$service = Read-Host 'Path to a NEW Firebase Admin SDK service-account JSON for Twilio worker (blank to skip cloud worker)'
$serviceRel = 'private/firebase-service-account.json'
if (-not [string]::IsNullOrWhiteSpace($service)) {
    $service = $service.Trim().Trim('"')
    if (-not (Test-Path -LiteralPath $service -PathType Leaf)) { throw "File not found: $service" }
    try { $obj = Get-Content -LiteralPath $service -Raw | ConvertFrom-Json } catch { throw 'The service-account file is not valid JSON. Generate a fresh file from Firebase Console.' }
    if ($obj.type -ne 'service_account' -or -not $obj.private_key -or -not $obj.client_email) { throw 'The JSON is not a complete Firebase service-account key. Do not concatenate firmware or other text to it.' }
    Copy-Item -LiteralPath $service -Destination $serviceRel -Force
}

$configureTwilio = Read-Host 'Configure Twilio cloud SMS worker now? Type YES to configure, otherwise skip [no]'
$sid = ''; $twToken = ''; $from = ''
if ($configureTwilio -ceq 'YES') {
    $sid = Read-Host 'Twilio Account SID'
    $twToken = Read-SecretText 'Twilio Auth Token'
    $from = Read-Host 'Twilio sender phone number in +countrycode format'
}
$envLines = @(
    '# Generated locally. Do not commit this file.',
    "FIREBASE_DATABASE_URL=$(EnvSafe $db)",
    "FIREBASE_SERVICE_ACCOUNT_PATH=$(EnvSafe $serviceRel)",
    "TWILIO_ACCOUNT_SID=$(EnvSafe $sid)",
    "TWILIO_AUTH_TOKEN=$(EnvSafe $twToken)",
    "TWILIO_FROM_NUMBER=$(EnvSafe $from)",
    'POLL_INTERVAL_SECONDS=5'
)
Write-Utf8NoBom '.\.env' $envLines

Write-Host ''
Write-Host 'Local configuration files created:' -ForegroundColor Green
Write-Host '  firmware/include/secrets.h'
Write-Host '  dashboard/.streamlit/secrets.toml'
Write-Host '  .env'
if (Test-Path -LiteralPath $serviceRel) { Write-Host '  private/firebase-service-account.json' }
Write-Host 'These are ignored by Git. Never upload or share them.' -ForegroundColor Yellow
