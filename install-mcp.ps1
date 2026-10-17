# Installerar MCP-bryggan (KickbaseMcp.exe) för Claude Desktop.
#
#   powershell -ExecutionPolicy Bypass -File .\install-mcp.ps1 [RelWithDebInfo|Release] [-Key <åtkomstnyckel>]
#
# Kopierar programmet till %LOCALAPPDATA%\Kickbase och lägger in "kickbase" bland MCP-servrarna i Claude Desktops
# konfiguration. Resten av konfigurationsfilen rörs inte (en backup sparas som .bak). Starta om Claude Desktop efteråt
# (Avsluta helt i aktivitetsfältet, inte bara stäng fönstret).
param([string]$Config = "RelWithDebInfo", [string]$Key = "")

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

# JSON kräver dubbla backslash i sökvägen.
$exeJson = $dest -replace '\\', '\\'
$envText = ""
if ($Key -ne "") { $envText = ', "env": { "KICKBASE_KEY": "' + $Key + '" }' }  # åtkomstnyckeln från SETTINGS > AI LINK i Kickbase
$serverText = '"kickbase": { "command": "' + $exeJson + '", "args": []' + $envText + ' }'
$utf8 = New-Object System.Text.UTF8Encoding($false)

if (Test-Path $target) {
    Copy-Item $target "$target.bak" -Force
    $text = [System.IO.File]::ReadAllText($target)
    if ($text -match '"kickbase"\s*:') {
        Write-Host "Kickbase finns redan i $target (inget ändrat)."
        if ($Key -ne "") { Write-Host "Lägg till nyckeln för hand i 'kickbase': , `"env`": { `"KICKBASE_KEY`": `"$Key`" }" }
    }
    elseif ($text -match '"mcpServers"\s*:\s*\{') {
        # Det finns redan andra servrar: lägg vår först i listan.
        $text = [regex]::Replace($text, '"mcpServers"\s*:\s*\{', { param($m) $m.Value + ' ' + $serverText + ',' }, 1)
        [System.IO.File]::WriteAllText($target, $text, $utf8)
        Write-Host "Lade in 'kickbase' bland servrarna i $target"
    }
    elseif ($text.Trim().Length -gt 0) {
        # Ingen mcpServers än: lägg in raden direkt efter den första {, och rör inget annat.
        $index = $text.IndexOf('{')
        $text = $text.Substring(0, $index + 1) + "`n  `"mcpServers`": { " + $serverText + " }," + $text.Substring($index + 1)
        [System.IO.File]::WriteAllText($target, $text, $utf8)
        Write-Host "Lade in 'kickbase' i $target (backup: $target.bak)"
    }
    else {
        [System.IO.File]::WriteAllText($target, '{ "mcpServers": { ' + $serverText + ' } }', $utf8)
        Write-Host "Skapade $target"
    }
}
else {
    [System.IO.File]::WriteAllText($target, '{ "mcpServers": { ' + $serverText + ' } }', $utf8)
    Write-Host "Skapade $target"
}

# Kontrollera att filen fortfarande är giltig JSON; annars återställs backupen.
try {
    Get-Content $target -Raw | ConvertFrom-Json | Out-Null
    Write-Host "Inställningsfilen är giltig JSON."
}
catch {
    Write-Host "VARNING: filen blev inte giltig JSON. Återställer från backup."
    if (Test-Path "$target.bak") { Copy-Item "$target.bak" $target -Force }
    throw
}

Write-Host ""
Write-Host "Starta om Claude Desktop helt (Avsluta i aktivitetsfältet)."
Write-Host "Skriv sedan till Claude: 'Använd Kickbase-verktygen: läs get_guide och skapa en kick.'"
