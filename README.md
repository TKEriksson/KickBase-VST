# Kickbase

VST3-plugins för kick och rullande bas i psytrance, byggda med [JUCE](https://juce.com) 8.

- **Kickbase** (instrument): kick + tre baslager som spelar ett mönster så länge en ton hålls i
  DAW:en. Tonen bestämmer tonarten, DAW:ens taktstreck var stegen hamnar.
- **Kickbase FX** (effekt): läggs på mixerkanalen där en annan synth (t.ex. Serum) hamnar.
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

**Flikarna** under kick och bas:
- **PATTERN:** stegsekvenseraren, mönsterlängd, basens rutnät (1/4 till 1/32, trioler).
- **EFFECTS:** filter, distortion, EQ, chorus, phaser, echo (ping-pong) och reverb. Varje effekt
  ligger PRE (före gaten, klipps av den) eller POST (efter, svansar klingar ut); dra raderna för ordningen.
- **SIDECHAIN:** ducka basen efter kickens envelope eller en egen kurva (dra punkter, dubbelklicka
  för att lägga till, högerklicka för att ta bort, dra i romberna för att böja).
- **LFO:** två tempo-synkade LFO:er med två mål var.
- **MASTER:** drive, soft clip och limiter med tak, plus mätare.

**Presets:** varje del (hela pluginet, kick, bas, mönster, effektkedjan, varje effekt, sidechain,
LFO, master) har en egen presetmeny med fabrikspresets, egna presets, "Save preset..." och
"Open preset folder". Egna presets sparas som filer under `Dokument\Kickbase\Presets\`.
De fyra "(... style)"-presetsen är anpassade mot referenslåtar med `fitbeat` (se nedan); spela
dem i låtens tempo och med tonen som står i kommentaren i `Presets.cpp` (E4 eller G4).

**EXPORT** (instrumentet): exporterar kicken, basen, hela mixen eller alla tre som WAV-loopar på
4, 8 eller 16 takter, i DAW:ens senaste tempo och med den senast spelade tonen. Filerna föreslås
under `Dokument\Kickbase\Exports\`.

**Gränssnittet:**
- Dra i handtagen i kurvorna för att forma envelopes (shift = finjustering, dubbelklick = återställ).
- Klicka/dra i stegen för att slå på/av. Välj VEL/GATE/PITCH/SLIDE för basstegens lane:
  dra i staplarna, skrolla för tonhöjd, högerklicka för att återställa ett steg.
- Dra in en WAV på kickmodulen (eller LOAD) för att använda en sampling som klick.
- Instrumentet har separata kick- och basutgångar utöver huvudutgången (som alltid har hela mixen).

## Utvecklingsverktyg

`MyOwnFruitTool` (byggs tillsammans med pluginsen) testar utan DAW:

```
MyOwnFruitTool render      instrument|fx <ut.wav> [bpm] [takter] [ton] [preset, t.ex. 16+k3+b5]
MyOwnFruitTool export      <ut.wav> full|kick|bass <takter> [bpm] [ton] [preset]
MyOwnFruitTool snapshot    instrument|fx <ut.png> [flik]
MyOwnFruitTool analyze     <ljudfil> [start] [slut]
MyOwnFruitTool fitbeat     <ljudfil> <start> <slut> rolling|galloping|triplet|offbeat
MyOwnFruitTool comparebeat <ljudfil> <start> <slut> <mönster> <rendering.wav>
```

- `render` skriver en WAV och en PNG med vågformen och skriver ut toppvärde per sextondel.
- `export` gör samma sak som EXPORT-knappen.
- `snapshot` sparar en skärmbild av gränssnittet.
- `analyze` mäter en låt (eller en rendering): tempo, kickens svep, svans och envelope, basgången
  sextondel för sextondel och basens övertoner.
- `fitbeat` söker kick-, bas- och mixinställningar som låter som ett parti av en låt (analys
  genom syntes): ett medelslag per position i takten jämförs med vår kick + bas i tre band,
  kickens svep och basens övertoner. Skriver ut värdena i pluginets parametrar. Tar ~15 minuter.
- `comparebeat` mäter en rendering av ett preset på samma sätt och visar hur nära den kommer.

Referenslåtar läggs i `research/` (ignoreras av git och får inte checkas in).

## Struktur

```
Source/Core/        klocka, mönster, kick- och basmotorer, parametrar, presets (inget UI)
Source/Ui/          utseende, rattar, kurvredigerare, stegsekvenserare, moduler
Source/Instrument/  instrumentets processor och editor
Source/Fx/          FX-versionens processor och editor
Tests/              tester för kärnan, körs av build.bat
Tools/              MyOwnFruitTool
```
