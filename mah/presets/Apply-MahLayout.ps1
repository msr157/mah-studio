#Requires -Version 5.1
# Apply mah/presets/layout-loved.json into %AppData%\obs-studio\user.ini (OBS must be quit)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$presetPath = Join-Path $Root "presets\layout-loved.json"
$userIni = Join-Path $env:APPDATA "obs-studio\user.ini"
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw "Quit OBS first" }
if (-not (Test-Path $userIni)) { throw "Open OBS once first: $userIni missing" }
$preset = Get-Content $presetPath -Raw | ConvertFrom-Json
$bak = "$userIni.bak-mah-layout-$(Get-Date -Format yyyyMMddHHmmss)"
Copy-Item $userIni $bak -Force
$ini = Get-Content $userIni -Raw
function Set-IniKey([string]$text, [string]$key, [string]$value) {
  if ($text -match "(?m)^$key=") { return [regex]::Replace($text, "(?m)^$key=.*$", "$key=$value") }
  if ($text -match "(?m)^\[BasicWindow\]") { return [regex]::Replace($text, "(?m)^(\[BasicWindow\])", "`$1`r`n$key=$value") }
  return $text + "`r`n$key=$value`r`n"
}
$ini = Set-IniKey $ini "ExtraBrowserDocks" $preset.ExtraBrowserDocks
$ini = Set-IniKey $ini "SideDocks" "true"
$ini = Set-IniKey $ini "DocksLocked" "true"
if ($preset.DockState) { $ini = Set-IniKey $ini "DockState" $preset.DockState }
if ($preset.geometry) { $ini = Set-IniKey $ini "geometry" $preset.geometry }
[IO.File]::WriteAllText($userIni, $ini)
Write-Host "Applied loved layout. Backup: $bak"
