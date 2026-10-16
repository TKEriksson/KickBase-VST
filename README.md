# Kickbase

VST3-plugins för kick och rullande bas i psytrance, byggda med [JUCE](https://juce.com) 8.

- **Kickbase** (instrument): kick + tre baslager som spelar ett mönster så länge en ton hålls i
  DAW:en. Tonen bestämmer tonarten, DAW:ens taktstreck var stegen hamnar.
- **Kickbase FX** (effekt): läggs på mixerkanalen där en annan synth (t.ex. Serum) hamnar.
  Synthens ljud gate:as av mönstret och duckas under en egen kick.

Designen finns i [docs/DESIGN.md](docs/DESIGN.md), planerna för ljuddesign-skill och AI-styrning i
[docs/sound-design-roadmap.md](docs/sound-design-roadmap.md).

## Bygga

Kräver Visual Studio Build Tools med C++-verktygen (CMake och Ninja följer med).

```
build.bat                    # Release (med link-time optimization, långsam att länka)
build.bat RelWithDebInfo     # snabbare att bygga under utveckling
build.bat RelWithDebInfo all # bygger också utvecklingsverktygen (KickbaseMcp / MyOwnFruitTool) och KickbaseHostTest
```

JUCE laddas ner automatiskt första gången. `build.bat` kör testerna efter bygget. Utan `all` byggs bara det som behövs för att
prova: instrumentet, FX-pluginet och testerna.

**Snabbare byggen:** JUCE:s tunga headers är förkompilerade (`Source/PchPlugin.h`, `Source/PchCore.h`), felsökningsinformationen
ligger i objektfilerna (/Z7) så att kompileringarna inte väntar på en gemensam PDB, och verktyget och MCP-bryggan är ett och samma
program (`KickbaseMcp.exe`, som också kopieras till `MyOwnFruitTool.exe`) så att all kod bara kompileras en gång. Om `sccache` (eller
`ccache`) finns installerat används det automatiskt, vilket gör ren ombyggnad och byte av gren mycket snabbare
(`winget install Mozilla.sccache`). Det hjälper också att utesluta mappen `build` och repot ur Windows Defender.

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

**Flikarna:**
- **PATTERN, KICK, BASS 1-3** (överst): mönstret, kicken och de tre baslagren. Mönstret har två lägen,
  **PATTERN** (tonerna: stegen är en enkel pianorulle, **PIANO ROLL** den avancerade varianten, och ett lager som
  redigerats där visas som en miniatyr) och **GATE** (en egen av/på-rytm per lager med eget rutnät som hackar ljudet). Uppe i hörnet på varje flik väljer du effektkanal och mängd (FX).
- **EFFECTS** (nederst): kanalerna FX A-E, var och en med flikarna FX (fem platser, 13 effekttyper), EQ
  (dra noderna, live-spektrum) och COMP (kompressor med vågformsvy). Dra platserna för att byta ordning.
- **SIDECHAIN:** ducka basen efter kickens envelope eller en egen kurva; DUCK per lager, och vilka
  källor som påverkas.
- **LFO:** tre tempo-synkade LFO:er. Dra en LFO på en ratt för att modulera den.
- **MASTER:** **MIXER** (gain, pan, dB-mätare, SC och MAX per källa) och **MAXIMIZER** (drive, clip,
  limiter med tak).
- **SOUNDSTAGE:** spektrum, stereobild, bredd, mono under en frekvens och stereoförstärkare.

**Presets:** varje del (hela pluginet, kick, bas, mönster, effektkedjan, sidechain, LFO, master) har
en egen presetmeny med egna presets, "Save preset..." och "Open preset folder". Egna presets sparas
som filer under `Dokument\Kickbase\Presets\`. Fabrikspresets byggs om med `fitbeat` (se nedan) när
synten är klar; tills dess finns bara Init/Default.

**EXPORT** (instrumentet): exporterar kicken, basen, hela mixen eller alla tre som WAV-loopar på
4, 8 eller 16 takter, i DAW:ens senaste tempo och med den senast spelade tonen. Filerna föreslås
under `Dokument\Kickbase\Exports\`.

**Gränssnittet:**
- Dra i handtagen i kurvorna för att forma envelopes (shift = finjustering, dubbelklick = återställ).
- Klicka/dra i stegen för att slå på/av. Välj VEL/GATE/PITCH/SLIDE för tonernas lane:
  dra i staplarna, skrolla för tonhöjd, högerklicka för att återställa en ton. Rutnätet (GRID) väljer
  upplösning, 1/4 till 1/64 och trioler.
- **Pianorullen** (PIANO ROLL): klicka i tomt för en ny ton, dra för att flytta, dra kanten för längd,
  högerklicka för att ta bort, **GLIDE**-knappen ritar glide-toner (som slide-toner i FL: de glider tonen som klingar
  till sin tonhöjd under sin egen längd, och tonen blir kvar). Skroll = tonhöjd (±96 halvtoner från tonarten),
  Ctrl + skroll = zoom, Shift + skroll = tid. Remsan längst ner är velocity.
- **PRESET** i pianorullen sparar lagrets pianorulle som en fil (`Dokument\Kickbase\Presets\Piano Roll`) och laddar sparade i valfritt lager.
  **MIDI** exporterar pianorullen som en MIDI-fil eller importerar en MIDI-fil till lagret (glide-toner exporteras som vanliga toner).
- **Kicken är ett notlager** som baslagren: egen KICK-knapp bland lagren (rutnät, FILL-mallar, velocity, tonhöjd i lanet och pianorullen). En kicknots tonhöjd flyttar kickens svans, så man kan spela kick-melodier. Gamla projekt med kickens 16 steg öppnas som toner.
- **Maximizer som effekt**: i effektkanalerna finns nu en Maximizer (GAIN, CLIP, CEILING, RELEASE, MIX; ingen fördröjning). Skicka kicken till en kanal med en Maximizer (FX SEND 100 %) för att göra bara kicken högljudd och jämn, vid sidan av mastern.
- **Sampling på basen och kicken**: släpp en WAV på ett baslagers sida (eller LOAD) så blir den en tredje ljudkälla bredvid oscillatorerna (nivå, start, root, loop, KEY låter den följa noterna). På kicken kan en sampling vara klick, ett lager eller hela kicken (MODE: Click, Layer, Replace).
- **Fler kick-lager och former**: vågform på kroppen, sub, FM, brusig topp, fler drive-lägen (Fold), bitkrossning, lågt skär och velocity som styr pitch-svepet.
- **Längd per rad**: rutan bredvid radens namn (kick, bas 1-3) ger raden en egen längd (1-64 takter; Pattern = hela mönstret). En kortare rad loopar tills den längsta har spelat klart (visas som dämpade kopior), eller spelar en gång om **LOOP ROWS** är av. Mönstret kan vara upp till 64 takter; stegvyn visas fyra takter i taget (< >), och **FOLLOW** låter stegvyn och pianorullen bläddra med när låten spelas.
- Raderna i PATTERN har små rattar för volym och FX-send, som FL Studios Channel Rack.
- Dra in en WAV på kickmodulen (eller LOAD) för att använda en sampling som klick.
- Instrumentet har separata kick- och basutgångar utöver huvudutgången (som alltid har hela mixen).

## AI via MCP (Claude Desktop)

`KickbaseMcp.exe` är en MCP-server som låter en AI-app (t.ex. Claude Desktop) skapa ljud och presets i Kickbase:
läsa och sätta parametrar, effekter, kickens envelopes, mönster och sidechain, rendera och mäta (med spektrogram),
spara presets och ångra. Verktygen och guiden som AI:n läser finns i `Source/Core/ToolLayer.cpp` och `docs/AGENT_GUIDE.md`.

1. Bygg (`build.bat RelWithDebInfo`) och kör `powershell -ExecutionPolicy Bypass -File .\install-mcp.ps1`.
   Skriptet kopierar bryggan till `%LOCALAPPDATA%\Kickbase` och lägger in den i Claude Desktops inställningar (med backup).
2. Starta om Claude Desktop helt. Be Claude använda Kickbase-verktygen (den läser `get_guide` först).
3. Utan DAW jobbar AI:n mot en egen instans ("headless"). Vill du att den ska styra pluginet i FL: klicka på **AI** i
   pluginets topprad, och be AI:n köra `list_instances` och `use_instance`. Länken är lokal (named pipe) och av som standard.

Förhandslyssningar och spektrogram hamnar i `Dokument\Kickbase\Previews`, presets i `Dokument\Kickbase\Presets`.
`KickbaseMcp --selftest-link` testar länken, och `KickbaseHostTest <plugin.vst3>` laddar pluginet som en värd
(spelar, öppnar och stänger editorn, laddar ur) för att hitta krascher vid avstängning.

## Utvecklingsverktyg

`MyOwnFruitTool` (byggs med `build.bat RelWithDebInfo all`; samma program som `KickbaseMcp`, som också går att köra som `KickbaseMcp tool <kommando>`) testar utan DAW:

```
MyOwnFruitTool render      instrument|fx <ut.wav> [bpm] [takter] [ton] [preset, t.ex. 16+k3+b5]
MyOwnFruitTool export      <ut.wav> full|kick|bass <takter> [bpm] [ton] [preset]
MyOwnFruitTool snapshot    instrument|fx <ut.png> [flik]
MyOwnFruitTool analyze     <ljudfil> [start] [slut]
MyOwnFruitTool fitbeat     <ljudfil> <start> <slut> rolling|galloping|triplet|offbeat
MyOwnFruitTool comparebeat <ljudfil> <start> <slut> <mönster> <rendering.wav>
```

- `render` skriver en WAV och en PNG med vågformen och skriver ut toppvärde per sextondel. Slumpen
  är seedad: samma inställningar ger bit-identisk ljud. `p:id=värde` i presetfältet sätter parametrar.
- `export` gör samma sak som EXPORT-knappen.
- `snapshot` sparar en skärmbild av gränssnittet. Flik-numret är ensiffrigt för nedre panelen, tiotal
  för övre panelen, hundratal för EFFECTS-vyn (0 EQ, 1 COMP, 2 FX) och tusental 1 för pianorullen.
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
