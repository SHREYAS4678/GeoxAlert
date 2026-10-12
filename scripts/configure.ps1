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
    $Value = $Value.Replace('\', '\\').Replace('"', '\"')
    $Value = $Value.Replace([string][char]13, '\r').Replace([string][char]10, '\n')
    return '"' + $Value + '"'
}
function TomlQuote([string]$Value) {
    if ($null -eq $Value) { $Value = '' }
    $Value = $Value.Replace('\', '\\').Replace('"', '\"')
    $Value = $Value.Replace([string][char]13, '').Replace([string][char]10, '\n')
    return '"' + $Value + '"'
}
function Write-Utf8NoBom([string]$Path, [string[]]$Lines) {
    $fullPath = Join-Path $Root $Path
    $content = ($Lines -join [Environment]::NewLine) + [Environment]::NewLine
    $encoding = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($fullPath, $content, $encoding)
}

New-Item -ItemType Directory -Force -Path '.\firmware\include', '.\dashboard\.streamlit' | Out-Null
Write-Host 'GeoAlert local configuration' -ForegroundColor Cyan
Write-Host 'Use newly rotated credentials. Never commit generated files.' -ForegroundColor Yellow

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

Write-Host ''
Write-Host 'Local configuration files created:' -ForegroundColor Green
Write-Host '  firmware/include/secrets.h'
Write-Host '  dashboard/.streamlit/secrets.toml'
Write-Host 'These files are ignored by Git. Never upload or share them.' -ForegroundColor Yellow
