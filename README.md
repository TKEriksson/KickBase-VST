# MyOwnFruit

VST3-plugins för kick och rullande bas i psytrance, byggda med [JUCE](https://juce.com) 8.

- **MyOwnFruit** (instrument): kick + bassynt som spelar ett mönster så länge en ton hålls i
  DAW:en. Tonen bestämmer tonarten, DAW:ens taktstreck var stegen hamnar.
- **MyOwnFruit FX** (effekt): läggs på mixerkanalen där en annan synth (t.ex. Serum) hamnar.
  Synthens ljud gate:as av mönstret och duckas under en egen kick.

Designen finns i [docs/design.md](docs/design.md).

## Bygga

Kräver Visual Studio Build Tools med C++-verktygen (CMake och Ninja följer med).

```
build.bat                    # Release (med link-time optimization, långsam att länka)
build.bat RelWithDebInfo     # snabbare att bygga under utveckling
```

JUCE laddas ner automatiskt första gången. `build.bat` kör testerna efter bygget.

## Installera

Stäng FL Studio och kör i en PowerShell som administratör, i projektmappen:

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

Skriptet kopierar båda pluginsen till `C:\Program Files\Common Files\VST3\`, tar bort gamla
kopior och städar bort en felplacerad `VST3\Contents`-mapp om en tidigare kopiering hamnat fel.
Scanna sedan om plugins i DAW:en. Instrumentet finns också som fristående app:
`build\Release\MyOwnFruit_artefacts\Release\Standalone\MyOwnFruit.exe`.

## Använda

**Instrumentet:** lägg en lång ton i pianorullen (t.ex. F# över 8 takter) och tryck play.
Kick och bas spelar mönstret i PATTERN-modulen så länge tonen hålls. NOTE END i MIX
bestämmer vad som händer när tonen tar slut mitt i ett mönster.

**FX-versionen:** lägg den i en effektplats på synthens mixerkanal och välj tonart (KEY).
När DAW:en står still släpps ljudet igenom orört och ingen kick spelas.

**Gränssnittet:**
- Dra i handtagen i kurvorna för att forma envelopes (shift = finjustering, dubbelklick = återställ).
- Klicka/dra i stegen för att slå på/av. Välj VEL/GATE/PITCH/SLIDE för basstegens lane:
  dra i staplarna, skrolla för tonhöjd, högerklicka för att återställa ett steg.
- Dra in en WAV på kickmodulen (eller LOAD) för att använda en sampling som klick.
- Presets (ljud + mönster) finns i menyn bredvid logotypen, mönstermallar under TEMPLATES.
- Instrumentet har separata kick- och basutgångar utöver huvudutgången (som alltid har hela mixen).

## Utvecklingsverktyg

`MyOwnFruitTool` (byggs tillsammans med pluginsen) testar utan DAW:

```
MyOwnFruitTool render   instrument|fx <ut.wav> [bpm] [takter] [ton] [preset]
MyOwnFruitTool snapshot instrument|fx <ut.png>
```

`render` skriver en WAV och en PNG med vågformen och skriver ut toppvärde per sextondel.
`snapshot` sparar en skärmbild av gränssnittet.

## Struktur

```
Source/Core/        klocka, mönster, kick- och basmotorer, parametrar, presets (inget UI)
Source/Ui/          utseende, rattar, kurvredigerare, stegsekvenserare, moduler
Source/Instrument/  instrumentets processor och editor
Source/Fx/          FX-versionens processor och editor
Tests/              tester för kärnan, körs av build.bat
Tools/              MyOwnFruitTool
```
