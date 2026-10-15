# Installerar Kickbase och Kickbase FX i C:\Program Files\Common Files\VST3.
# Kör i en PowerShell som administratör:  .\install.ps1  (eller .\install.ps1 RelWithDebInfo)

param([string]$Config = "Release")

$ErrorActionPreference = "Stop"
$vst3 = "C:\Program Files\Common Files\VST3"
$build = Join-Path $PSScriptRoot "build\$Config"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { throw "Kör skriptet i en PowerShell som administratör." }

if (Get-Process FL64, FL -ErrorAction SilentlyContinue) {
    throw "FL Studio är igång. Stäng det först (pluginfilerna är låsta medan de används)."
}

# Mappen måste finnas innan kopieringen. Annars döper Copy-Item om plugin-mappen till "VST3"
# och lägger dess innehåll direkt i VST3\Contents.
New-Item -ItemType Directory -Force $vst3 | Out-Null

# Rester från en sådan felkopiering: en Contents-mapp direkt i VST3 med vår gamla fil.
$stray = Join-Path $vst3 "Contents\x86_64-win\MyOwnFruit.vst3"
if (Test-Path $stray) {
    Write-Host "Tar bort felplacerad gammal kopia: $vst3\Contents"
    Remove-Item -Recurse -Force (Join-Path $vst3 "Contents")
}

# Pluginet hette tidigare MyOwnFruit: ta bort de gamla så att FL inte visar båda.
foreach ($old in @("MyOwnFruit.vst3", "MyOwnFruit FX.vst3")) {
    $oldPath = Join-Path $vst3 $old
    if (Test-Path $oldPath) {
        Write-Host "Tar bort gamla $oldPath"
        Remove-Item -Recurse -Force $oldPath
    }
}

$plugins = @(
    @{ Name = "Kickbase.vst3";    Source = "$build\MyOwnFruit_artefacts\$Config\VST3\Kickbase.vst3" },
    @{ Name = "Kickbase FX.vst3"; Source = "$build\MyOwnFruitFX_artefacts\$Config\VST3\Kickbase FX.vst3" }
)

foreach ($plugin in $plugins) {
    if (-not (Test-Path $plugin.Source)) { throw "Hittar inte $($plugin.Source). Kör build.bat $Config först." }

    # Ta bort den gamla först, så att Copy-Item inte lägger den nya inuti den gamla.
    $target = Join-Path $vst3 $plugin.Name
    if (Test-Path $target) { Remove-Item -Recurse -Force $target }

    Copy-Item -Recurse $plugin.Source $vst3
    Write-Host "Installerade $target"
}

Write-Host "`nKlart! Starta FL Studio och scanna om plugins om de inte syns."
