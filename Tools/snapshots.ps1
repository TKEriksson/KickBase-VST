# Tar skärmbilder av alla flikar (42 st): instrumentet med 1, 3 och 10 instrument, övre flikarna PATTERN/KICK/AI och Kickbase FX.
#   powershell -File Tools\snapshots.ps1 -Exe <KickbaseMcp.exe> -Out <mapp>
# Jämför två mappar (före/efter) pixel för pixel med:  python Tools\pngdiff.py <före> <efter>
param([string]$Exe, [string]$Out)
New-Item -ItemType Directory -Force $Out | Out-Null
$jobs = @()
# instrumentet: övre flik 2 (INSTRUMENT), nedre flik 0-8, med 1, 3 och 10 instrument
foreach ($count in 1, 3, 10) {
    $env:KB_INSTRUMENTS = "$count"
    foreach ($lower in 0..8) {
        $tab = 20 + $lower
        & $Exe tool snapshot instrument "$Out\inst_n${count}_tab$tab.png" $tab | Out-Null
    }
}
Remove-Item Env:KB_INSTRUMENTS -ErrorAction SilentlyContinue
# övriga övre flikar (0 PATTERN, 1 KICK, 3 AI) med nedre flik 0 och 8
foreach ($upper in 0, 1, 3) {
    foreach ($lower in 0, 8) {
        $tab = $upper * 10 + $lower
        & $Exe tool snapshot instrument "$Out\inst_upper${upper}_tab$tab.png" $tab | Out-Null
    }
}
# Kickbase FX: nedre flikar 0-8
foreach ($lower in 0..8) {
    & $Exe tool snapshot fx "$Out\fx_tab$lower.png" $lower | Out-Null
}
(Get-ChildItem $Out -Filter *.png).Count
