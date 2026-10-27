# PROGRESS Arbetare 1 (worktree KickBase-VST-w1, gren play-pack)

Uppdrag från projektledaren: A1 + A2 + A3, sedan B1, A4, B3, B5. Aldrig push/merge.

## A1 (T10, C5 från FL startar inte mönstret) – fixad, ej provad i FL
Orsak (godkänd av specialisten):
- Pianorullen sätter `midiInLayer` när den är öppen (default INSTRUMENT): `Source/Ui/PianoRoll.cpp:224`.
- `handleMidi` anropar `handleLiveMidi` först och returnerar om den tog noten, så `startPattern` nås aldrig.
- `handleEvent` filtrerar dessutom bort steg från andra rader via `midiInLayer`.
- Ladda om preset stänger rullen (`midiInLayer = -1`, `PianoRoll.cpp:245`), därför "fixade" det buggen.
- Beslut (Tobias): spelar värden startar värdens noter alltid mönstret. MIDI IN gäller bara när värden står still.

Gjort (Arbetare 1):
- `TransportClock::isHostPlaying()` = värdens råa status (`hostPlaying`, satt i `beginBlock`), skild från `followingHost`.
- `handleLiveMidi`: hoppar bara över NOTE-ON när värden spelar (och ingen `rowOverride`); note-off/all-notes-off passerar.
- `handleEvent`: släpper `midiInLayer`-villkoret när värden spelar; `previewLayer`-filtret orört.

## A2 (T13, längd / Loop) – fixad i schedulern, test grönt
Rättelse: "rad 1 takt + Loop 1-2 loopar raden och är rätt". Felet visar sig när loopintervallet inte är en multipel av radens längd,
när `loopStart >= rowTicks`, när `from` ignorerades i loopgrenen, och när loopen klipptes mot raden i stället för mönstret.

Gjort (Arbetare 1):
- `StepScheduler::windowTick(rowTicks, totalTicks, position)` (publik): loopen går i mönstertid, `loopEnd` klipps mot `total`,
  start mitt i loopen via `(from - loopStart) + elapsed` mod längd, radens tick = `loopTick % rowTicks`.
- Test i `Tests/EngineTests.cpp` (testRowLengths): Loop 1-2 med rader på 1 och 2 takter, Loop 1-3 med 2-takters rad, Loop 2-3 med 1-takters rad,
  start mitt i loopen, loopEnd klippt mot mönstret. Bygge Debug + hela testsviten: grönt (en körning).

Ej klart: "takt 3 får bara 2 kickslag" i ett 3-takters mönster. Misstänkt `total` ≠ `rows` (`contentBars` i UI vs `lengthBars` i schedulern,
`refreshLengths` `Pattern.cpp:98-133`, `readPatternLengthsWith` `Params.h:170-199`). Ska först bekräftas med ett test, sedan ev. fix.

T13-uppföljning (Arbetare 1): test med kick på takt 0, 1, 2 ger `contentBars == 3 && lengthBars == 3` och `barInRow(0,2)==2`,
`barInRow(0,3)==0` – kärnans längdberäkning är konsekvent. Kvarvarande misstanke: UI-visning som inte uppdateras, eller en
redigeringsväg som inte kör `patternChanged()`/`refreshLengths()`, eller att `windowTick`-fixen redan löste det. Behöver FL-test.

## A3 (ett-takts-loop i FL) – diffen applicerad för hand, bygge Debug grönt, ej provad i FL
Gjort (Arbetare 1): `TransportClock::setForceFreeRun` + `forceFreeRun` (`TransportClock.h`, `beginBlock` i `.cpp`);
`clock.setForceFreeRun (previewLayer >= 0)` före `beginBlock` i `processChunk`; `handlePreviewRequest` tvingar egen klocka och
kör `restartFreeRunAt (0)` (förut hoppades omstarten över när värden spelade).
Samspel med A1: `hostPlaying` är rå och påverkas inte av `forceFreeRun`, så MIDI IN-logiken är oförändrad under förhandsspelning.
`FxProcessor` har egen klocka (aldrig tvingad) – orörd. Specialisten ska granska före merge. FL-test (a-f) återstår.

## PREVIEW har företräde (Tobias beslut via projektledaren) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): `handleMidi` (`InstrumentProcessor.cpp:~612`) returnerar tidigt medan `previewLayer >= 0`: värdens ton-på avbryter inte
förhandsspelningen och startar inget mönster; ton-av stoppar den inte. Tidigare `previewLayer = -1` vid ton-på är borttaget.
`handlePreviewRequest` hoppar inte längre över när en värdton hålls. Inget enhetstest (processorn saknar testsele) – FL-test nedan.

## A4 (T9, glide i samma takt som tonen ska flytta alla klingande toner) – fixad, test grönt
Orsak: `InstrumentVoices::glideTo` (`InstrumentVoices.cpp:177`) gled bara `lastVoice`, så resten av ett ackord stod kvar.
Gjort (Arbetare 1): `InstrumentSynth::isSounding()/getTargetSemitones()` (+ `targetSemitones` satt i `noteOn`/`glideTo`).
`glideTo` räknar `shift = mål − lastVoice-målet` och glider varje klingande, icke utfadande röst till sitt eget mål + shift.
En röst ger samma beteende som förut. Test i `testInstrumentGlide`-blocket i `EngineTests.cpp` (ackord 0/4/7 + glide till 12 → 5/9/12).
Hela sviten: exit 0. Kvar att undersöka: stackade extra-glidetoner behandlas före grundtonen och kan glida den förra tonen, inte den nya.

## FL-testlista (samlas av projektledaren)
- A1: C5 från FL startar mönstret med pianorullen öppen; MIDI IN gäller bara när värden står still.
- A2/T13: rad 1 takt + Loop 1-2; 3-takters mönster med kick i varje takt (takt 3 ska ha alla slag); "AUTO 2 takter"-misstanken (UI-visning/redigeringsväg utan `refreshLengths`).
- A3: ett-takts-loop i FL ska inte styra förhandsspelningen (a-f i masterplanen).
- PREVIEW-företräde: starta PREVIEW medan FL spelar noter; den ska löpa i Kickbases egen loop; stäng PREVIEW → värden tar över; första noten får inte dubbleras eller tappas.
- A4: ackord + glide-not i samma takt – alla toner ska glida lika långt.

## Lärdomar
- `build.bat` i worktreen: kör via PowerShell `& cmd.exe /c "<full sökväg>\build.bat" Debug` (cmd /c från bash gav en tom prompt). Varningen om `vswhere.exe` är ofarlig.
- Ny testkod som använder `StepScheduler` kräver `#include` i `EngineTests.cpp` (missades första bygget).
