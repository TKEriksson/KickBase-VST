# Installerar MCP-bryggan (KickbaseMcp.exe) för Claude Desktop.
#
#   powershell -ExecutionPolicy Bypass -File .\install-mcp.ps1 [RelWithDebInfo|Release]
#
# Kopierar programmet till %LOCALAPPDATA%\Kickbase och lägger in "kickbase" bland MCP-servrarna i Claude Desktops
# konfiguration (med backup). Starta om Claude Desktop efteråt (Avsluta helt i aktivitetsfältet, inte bara stäng fönstret).
param([string]$Config = "RelWithDebInfo")

$ErrorActionPreference = "Stop"
$source = Join-Path $PSScriptRoot "build\$Config\KickbaseMcp_artefacts\$Config\KickbaseMcp.exe"
if (-not (Test-Path $source)) { throw "Hittar inte $source. Bygg först: .\build.bat $Config" }

$destFolder = Join-Path $env:LOCALAPPDATA "Kickbase"
New-Item -ItemType Directory -Force $destFolder | Out-Null
$dest = Join-Path $destFolder "KickbaseMcp.exe"
Copy-Item $source $dest -Force
Write-Host "Kopierade bryggan till $dest"

# Claude Desktop från Microsoft Store ligger i ett paket med egen konfigurationsmapp; annars %APPDATA%\Claude.
$candidates = @()
foreach ($package in Get-ChildItem (Join-Path $env:LOCALAPPDATA "Packages") -Filter "Claude_*" -Directory -ErrorAction SilentlyContinue) {
    $candidates += Join-Path $package.FullName "LocalCache\Roaming\Claude\claude_desktop_config.json"
}
$candidates += Join-Path $env:APPDATA "Claude\claude_desktop_config.json"

$target = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $target) { $target = $candidates | Where-Object { Test-Path (Split-Path $_) } | Select-Object -First 1 }
if (-not $target) { $target = $candidates[0] }
New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null

$json = [pscustomobject]@{}
if (Test-Path $target) {
    Copy-Item $target "$target.bak" -Force
    $text = Get-Content $target -Raw
    if ($text.Trim().Length -gt 0) { $json = $text | ConvertFrom-Json }
}
if (-not $json.PSObject.Properties["mcpServers"]) { $json | Add-Member -NotePropertyName mcpServers -NotePropertyValue ([pscustomobject]@{}) }
$entry = [pscustomobject]@{ command = $dest; args = @() }
$json.mcpServers | Add-Member -NotePropertyName kickbase -NotePropertyValue $entry -Force

[System.IO.File]::WriteAllText($target, ($json | ConvertTo-Json -Depth 20), (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Lade in 'kickbase' i $target"
Write-Host ""
Write-Host "Starta om Claude Desktop helt. Skriv sedan till Claude: 'Använd Kickbase-verktygen: läs get_guide och skapa en kick.'"
