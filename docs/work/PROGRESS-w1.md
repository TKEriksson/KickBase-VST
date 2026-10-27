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

## B1 (förhandsknapp per flik) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): `TabbedPanel::setPreview(tab, onStart, onStop, tooltip)` (`TabbedPanel.h:125-142`): en delad `ListenButton` längst till höger i
flikraden (30 px reserverad i `resized`), synlig bara för flikar som har anrop. Koppling:
- Synth-panelen (`InstrumentEditor.cpp`): PATTERN/ANALYZER/AI CHAT/LIVE CODE = `previewRequest(previewAllRows)`, KICK = `previewRequest(kickLayer)`,
  INSTRUMENT = `HoldAudition` med `heldLayerAudition(instruments.getCurrent())`. Stopp = `previewStop` resp. `hold.stop()`.
- Lagrets källflikar (`Modules.cpp`, efter MIX): OSC 1-3 och SAMPLE = `heldSoloAudition(layer, källa)`, FILTER/FM/MIX = `heldLayerAudition(layer)`.
  INSTR har redan egen knapp i sidan.
- `InstrumentHost::getCurrent()` (`Modules.h`) nytt.
"MIDI-tangentbordet triggar detta": besvarat av Tobias – det är vilken ton man än spelar; inget extra att bygga.

## B3 (spelande toner lyser, tonnamn vid pitch-noder) – Debug grönt, ej provad i FL
- Pianorullen (`PianoRoll.cpp`, paint): tonen lyser upp när uppspelningsmarkören (`playTickNow()`) ligger inom den; repaint var 6:e tick (`timerCallback`).
- Tonnamn vid pitch-env-noderna fanns redan i UI:t (`KickDisplay.cpp:541-548`, `describe`). Nytt: `get_state` och kick-envelope-svaret ger
  `kick_pitch_nodes` = [{ ms, semitones_above_tail, hz, note }] (`ToolLayer.cpp`, `pitchNodeNotes`), samma uträkning som grafen (C4 = 60).

## B5 (LISTEN PIANO ROLL-knappen kommer sent) – Debug grönt, ej provad i FL
Orsak: `rollShown` kollades bara i `HeaderBar::timerCallback`, som går i TOP BAR-takten (`startVisualTimer`, `Modules.cpp:92`); OPTIMIZATIONS/REDUCE ANIMATIONS kan sänka den kraftigt.
Gjort (Arbetare 1): `HeaderBar::syncRollButton()` + egen `RollWatch`-timer på 10 Hz (`Modules.h`, `Modules.cpp`), bara i instrumentet (där `getKeyboardState()` finns).

## H2 (tappat ton-av i noteAuditionFifo) – Debug grönt, hela sviten exit 0, ej provad i FL
Orsak: `postNoteAudition` (`PluginBase.h:323`) släppte tyst ett ton-av när kön (32) var full, så tonen hängde kvar.
Gjort (Arbetare 1): `postAuditionAllOff(row)` (atomisk radmask, går utanför kön) anropas när ett ton-av inte får plats och när pianorullen
stängs/förstörs (`PianoRoll.cpp` `visibilityChanged`, destruktorn). Ljudtråden skickar all-notes-off per rad (`InstrumentProcessor.cpp:~145`).
Audition med `rowOverride >= 0` blockeras fortfarande inte när värden spelar (A1 orörd).

## FL-testlista (samlas av projektledaren)
- A1: C5 från FL startar mönstret med pianorullen öppen; MIDI IN gäller bara när värden står still.
- A2/T13: rad 1 takt + Loop 1-2; 3-takters mönster med kick i varje takt (takt 3 ska ha alla slag); "AUTO 2 takter"-misstanken (UI-visning/redigeringsväg utan `refreshLengths`).
- A3: ett-takts-loop i FL ska inte styra förhandsspelningen (a-f i masterplanen).
- PREVIEW-företräde: starta PREVIEW medan FL spelar noter; den ska löpa i Kickbases egen loop; stäng PREVIEW → värden tar över; första noten får inte dubbleras eller tappas.
- A4: ackord + glide-not i samma takt – alla toner ska glida lika långt.

## Lärdomar
- `build.bat` i worktreen: kör via PowerShell `& cmd.exe /c "<full sökväg>\build.bat" Debug` (cmd /c från bash gav en tom prompt). Varningen om `vswhere.exe` är ofarlig.
- Ny testkod som använder `StepScheduler` kräver `#include` i `EngineTests.cpp` (missades första bygget).

## D6 (spöktoner i pianorullen) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): `PianoRoll::paint` (`PianoRoll.cpp`, före "Tonerna") ritar övriga raders toner (`numNoteLayers`, inkl. staplade via `getStack`) svagt i radens färg,
ej klickbara. Höjd via `rowRootNote(annan) - rootNote()` så MIDI-tonen är gemensam axel. Ingen av/på-knapp än (föreslås om det stör).

## D5 (POP OUT, pianorullen i eget fönster) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): knappen POP OUT/DOCK i pianorullens nedre rad (`PianoRoll.cpp`, `popOutButton`, `onPopOut`). `PatternModule::togglePopOut` (`Modules.cpp`)
lägger rullen i ett `PopWindow` (`DocumentWindow`, inbyggd titelrad, storleksändring, `setContentNonOwned`). Stäng-knappen och DOCK för tillbaka rullen via `callAsync`.
Stegen syns kvar i huvudfönstret medan rullen är utbruten; `showPianoRoll(false)` docka tillbaka först. `popWindow` deklareras efter `pianoRoll` så att fönstret förstörs först.

## D3a (BPM/takt kan skrivas in, takt-scrub) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): `HeaderBar` har nu `bpmLabel`/`barLabel` (`Modules.cpp`, konstruktorn + `updateTransportLabels`, `mouseWheelMove`). Dubbelklick skriver in tempo (20–300, tomt = som förut) eller starttakt.
Mushjul över takten scrubbar: `transportHold` fryser klockan (`TransportClock::setHold`) och hoppar (`seekFreeRunToBar`) när hjulet vilat 450 ms (ett hjul har ingen släpp-händelse).
Gäller bara egen klocka (PREVIEW/ingen värd som spelar). När DAW:en styr är etiketterna låsta med tipset "styrs av DAW" – en VST3 kan inte ändra värdens tempo eller transport.
Inte byggt: global LOOP-knapp (bara loop per rad och loopområde finns; en global knapp är nytt arbete) – projektledaren avgör om den behövs.
Prova i FL: scrubba takten under PREVIEW; skriv 150 BPM i PREVIEW; kolla att etiketterna är låsta när FL spelar.
