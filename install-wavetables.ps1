# Hämtar Adventure Kid Waveforms (AKWF, över 4000 enkelcykel-vågformer, CC0 / public domain) till wavetable-biblioteket.
#
#   powershell -ExecutionPolicy Bypass -File .\install-wavetables.ps1
#
# Filerna hamnar i Dokument\Kickbase\Wavetables\AKWF (en mapp per kategori). I plugin:ets TABLE-meny dyker de upp under LIBRARY:
# välj en fil, eller "[ Whole folder as one wavetable ]" för att göra en hel kategori till en wavetable.
# Källa: https://github.com/schollz/AKWF-FREE (originalet: https://www.adventurekid.se/akrt/waveforms/adventure-kid-waveforms/)
$ErrorActionPreference = "Stop"

$library = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Kickbase\Wavetables"
$target = Join-Path $library "AKWF"
New-Item -ItemType Directory -Force $library | Out-Null

$work = Join-Path ([IO.Path]::GetTempPath()) ("akwf_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force $work | Out-Null
try {
    $zip = Join-Path $work "akwf.zip"
    Write-Host "Hämtar AKWF (cirka 20 MB) ..."
    Invoke-WebRequest -Uri "https://github.com/schollz/AKWF-FREE/archive/refs/heads/master.zip" -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $work -Force

    $source = Join-Path $work "AKWF-FREE-master\AKWF"
    if (-not (Test-Path $source)) { throw "Hittade inte AKWF-mappen i arkivet." }

    New-Item -ItemType Directory -Force $target | Out-Null
    Copy-Item (Join-Path $source "*") $target -Recurse -Force
    $license = Join-Path $work "AKWF-FREE-master\LICENSE"
    if (Test-Path $license) { Copy-Item $license (Join-Path $library "LICENSE-CC0.md") -Force }

    $count = (Get-ChildItem $target -Recurse -Filter *.wav).Count
    Write-Host "Klart: $count vågformer i $target"
}
finally {
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}
