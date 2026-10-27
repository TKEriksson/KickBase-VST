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

## A4-resten (staplade toner under en glide-ton) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): `InstrumentProcessor.cpp:1044` – när grundtonen är en glide-ton som glider görs inte längre `return` direkt; staplade vanliga toner (`note.extra`, ej glide) slår ändå an (de tappades förut). Staplade glide-toner ger samma glide som grundtonen och hoppas över. Hela ackordet glider redan lika långt (`InstrumentVoices::glideTo`).
Prova i FL: glide-ton med en stapel på samma tick – stapeltonen ska höras.

## D1 (glidestreck) – Debug grönt, ej provad i FL
Gjort (Arbetare 1): högerdrag på tomt rutnät ritar ett streck (`PianoRoll.cpp` `mouseDown` ~1906, `addStroke`, `mouseUp` ~2259). Högerklick utan drag (<6 px) visar menyn som förut (`strokeMenuNow`). Alt är suddgummi och rörs inte.
Strecket blir en vanlig glide-ton (`Note.slide`) – eller en staplad glide-ton om en ton redan ligger på starttick – så sparning, uppspelning och MIDI-export återanvänds. Slutet fastnar på en ton som börjar där (glidemålet = tonens tonhöjd), annars tonhöjden vid slutpunkten. Glidar också före en ton.
Prova i FL: högerdrag uppåt/nedåt i luften; dra till en befintlig ton; högerklick utan drag ska ge menyn; spara/ladda; MIDI-export (approximeras, varning).

## D3 global LOOP – Debug grönt, ej provad i FL
Gjort (Arbetare 1): `loopButton` + start-/slutetiketter (1-räknade, båda inräknade) till vänster om tempot (`Modules.cpp` konstruktorn, `applyLoopBars`, `resized`). På: `PluginBase::globalLoop`, hoppar till starttakten (`seekBarRequest`). `TransportClock::setLoop` (`TransportClock.h`) tvingar egen klocka; `beginBlock` (`TransportClock.cpp:52-61`) hoppar tillbaka vid slutet och ökar `generation` så stegmarkörerna börjar om. `InstrumentProcessor.cpp` anropar `setLoop` före `beginBlock`.
Begränsningar: hoppet sker vid blockgräns (inte sampleexakt) – överskottet behålls, så längden stämmer men en not exakt vid loopgränsen kan ligga upp till ett block fel. En VST3 kan inte stoppa DAW:en (FL saknar stöd), så loopen ignorerar DAW:ens position och användaren stoppar DAW:en själv; tipset på knappen säger det. Bara instrumentet.
Prova i FL: LOOP på med 1–2, ändra till 2–3 under gång, stäng av, DAW som spelar samtidigt.

## Fas C (undo/historik) – KLAR (27 okt), Debug grönt, `undotest` grön (12 kontroller), ej provad i FL
Slutstatus: (a)–(d) gröna. `PluginBase::EditScope` är generisk och nästlad (`editDepth`/`outermost`, PluginBase.h): inre scope ingår i det yttres steg (pasteTrack = ett steg). Använd i mallmenyer, kick-FILL, gate-fill, längdmenyn, Live Code run, stegsekvenserarens drag (`dragUndo`), radera pianorulle. AI-chatten får ingen ångring i historiken (Tobias beslut 27 okt; chatten har egen undo). Kör testet: `KickbaseTool.exe undotest`.
FL-test: Ctrl+Z efter knobvridning, stegritning, mallval, delpreset, klistra in spår; Meny > History...; huvudpreset/projekt tömmer historiken. OBS: FL kan ta Ctrl+Z först om editorn saknar tangentbordsfokus (klicka i pluginfönstret).
Begränsning: värdautomation hamnar i historiken på samma sätt som knobar och går inte att skilja ut.
Biblioteksnotis till Tobias: `juce_UndoManager.h` (avsnitten `beginNewTransaction` och `UndoableAction`), ca 15 min.
Ej gjort: mätning av radsnapshotens storlek; ToolLayer-ställen (UI-åtgärder) ej genomgångna; PianoRoll-urvalsuppdatering finns bara i Ctrl+Z-grenen.
(Äldre anteckning:)
Gjort (Arbetare 1): en `juce::UndoManager` i PluginBase (30000 ms/200 steg), gest-namn med tidsstämpel, pianorulle (`recordRowEdit`), parametrar (`nameParameterGesture` efter `copyState()`), modrutter (egen snapshot), delpreset som ett steg (`PresetUndoScope`, `snapshotAux/applyAux`), huvudpreset och projektladdning tömmer historiken, `HistoryPanel.h` (Meny > History...), Ctrl+Z/Ctrl+Shift+Z/Ctrl+Y (`UndoKeys.h`). Test: `KickbaseTool.exe undotest` (`Tools/StateTests.cpp`).
Läge: b och c grönna. Röda: (a) "ångra ger startvärdet/gör om" – den testade parametern råkar vara `patternBars` (inte en ren APVTS-flush-parameter; välj en vanlig knob-parameter i testet eller undersök om patternBars är bunden på annat sätt) och (d) "ett steg" – listan visar ett steg men med tom andra post (`"Load preset: Full-On Pluck | "`), dvs. ett extra tomt steg från APVTS-flush runt TreeAction; undersök `PresetUndoScope`-destruktorn.
Kvar: pasteTrack/init-clearHistory, Live Code run, AI-chatt, applyTemplate/setKickSteps, stegsekvenserare och andra egna kontroller, mätning av radsnapshot, hela byggsetet + gamla tester, FL-lista (notera att FL kan ta Ctrl+Z först utan tangentbordsfokus). Felsökningsutskrifter (`printf` "steg/efter undo/d-steg") i undotest ska bort när (a)/(d) är lösta.
JUCE-fynd: `flushToTree` skriver bara vid `needsUpdate` (juce_AudioProcessorValueTreeState.cpp:120-141), körs i timer (≤500 ms sen) och i `copyState()` → knobändring kan hamna i nästa transaktion; därför `copyState()` vid gestens slut. Värdautomation går samma väg och kan inte skiljas ut (kan ej uppfyllas till 100 %). `replaceState` tömmer historiken. `ParameterAttachment::beginGesture` startar en onamnad transaktion → namn sätts vid gestens slut.
MIDI-export (D1): slide/glide-flaggan exporteras inte (`RollFiles.cpp:98-179`); strokes blir korta vanliga toner utan varning.
