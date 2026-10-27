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
Efterarbete (27 okt): (1) Mätning (undotest): tät pianorulle, 64 takter/1024 toner → snapshot 20 877 byte, ca 44 ms i Debug; 200 steg × 2 ≈ 8 MB värsta fall, ingen delta behövs. (2) ToolLayer är AI/MCP-vägar (ingen historik, se beslutet); UI-vägarna "Load piano roll" och "Import MIDI" i PianoRoll.cpp har fått EditScope. (3) `PianoRoll::timerCallback` rensar urvalet från försvunna toner (`pruneSelection`), så ångra via History/meny uppdaterar urvalet. (4) MIDI-export: `RollFiles::countSlideNotes` + info-ruta "N glide note(s) were written as plain notes" efter lyckad export (PianoRoll.cpp och InstrumentMenu.h).
(Äldre anteckning:)
Gjort (Arbetare 1): en `juce::UndoManager` i PluginBase (30000 ms/200 steg), gest-namn med tidsstämpel, pianorulle (`recordRowEdit`), parametrar (`nameParameterGesture` efter `copyState()`), modrutter (egen snapshot), delpreset som ett steg (`PresetUndoScope`, `snapshotAux/applyAux`), huvudpreset och projektladdning tömmer historiken, `HistoryPanel.h` (Meny > History...), Ctrl+Z/Ctrl+Shift+Z/Ctrl+Y (`UndoKeys.h`). Test: `KickbaseTool.exe undotest` (`Tools/StateTests.cpp`).
Läge: b och c grönna. Röda: (a) "ångra ger startvärdet/gör om" – den testade parametern råkar vara `patternBars` (inte en ren APVTS-flush-parameter; välj en vanlig knob-parameter i testet eller undersök om patternBars är bunden på annat sätt) och (d) "ett steg" – listan visar ett steg men med tom andra post (`"Load preset: Full-On Pluck | "`), dvs. ett extra tomt steg från APVTS-flush runt TreeAction; undersök `PresetUndoScope`-destruktorn.
Kvar: pasteTrack/init-clearHistory, Live Code run, AI-chatt, applyTemplate/setKickSteps, stegsekvenserare och andra egna kontroller, mätning av radsnapshot, hela byggsetet + gamla tester, FL-lista (notera att FL kan ta Ctrl+Z först utan tangentbordsfokus). Felsökningsutskrifter (`printf` "steg/efter undo/d-steg") i undotest ska bort när (a)/(d) är lösta.
JUCE-fynd: `flushToTree` skriver bara vid `needsUpdate` (juce_AudioProcessorValueTreeState.cpp:120-141), körs i timer (≤500 ms sen) och i `copyState()` → knobändring kan hamna i nästa transaktion; därför `copyState()` vid gestens slut. Värdautomation går samma väg och kan inte skiljas ut (kan ej uppfyllas till 100 %). `replaceState` tömmer historiken. `ParameterAttachment::beginGesture` startar en onamnad transaktion → namn sätts vid gestens slut.
MIDI-export (D1): slide/glide-flaggan exporteras inte (`RollFiles.cpp:98-179`); strokes blir korta vanliga toner utan varning.

## Fas I 1) RING (B) – KLAR (27 okt), Debug grönt, `ringtest`/`paramtest`/`helpcheck`/`undotest` gröna, ej provad i FL
OSC 1 multipliceras med OSC 2, OSC 3 eller båda (ny parameter `ringSource`, sist i `InstrumentField`; WARP-valet `RING (B)` sist i listan, inga ID:n ändrade). Syntes: `InstrumentSynth.cpp` (`Oscillator::next`, ring-grenen efter AM; renderdelen räknar `modInput` från B/C:s råa våg). WARP AMOUNT blandar torrt (0) till ren ring (1). Källan måste vara PÅ (av = faktor 0 = tystnad vid full ring); dess LEVEL spelar ingen roll.
UI (Tobias tillägg): när warp = RING (B) ersätter `ringBox` formknapparna i OSC 1 med "RING: OSC 1 x OSC 2" / "x OSC 3" / "x OSC 2 x OSC 3" (`OscPanel.cpp`), med tooltip. Hjälp: `Help.cpp` (bassAWarp, `bassRingSource`), `docs/AGENT_GUIDE.md:61`.
Test: `KickbaseTool.exe ringtest` (`Tools/AudioTests.cpp`): AMOUNT 0 = oförändrat, full ring ändrar ljudet för alla tre källor, avstängd källa = tystnad.

## Fas I 2) LC-rester – redan klara (27 okt, Arbetare 1)
Allt fanns redan: `.lfo/.env` glue.js:93-114, MCP `live_code_run`/`stop` ToolLayer.cpp:302-304/571-572, AGPL-banner LiveCodePage.cpp:96/152, `THIRD-PARTY.md`. `livetest` och `helpcheck` gröna. Ingen kod ändrad.
"RING på OSC 2/3" återkallades av Tobias; inget gjort.

## Fas I 3) IO – autospara (27 okt, Arbetare 1)
`.kbproj` spara/öppna fanns redan (ProjectFile, Modules.cpp:212-236). Nytt: `ProjectFile::AutoSaver` (ProjectFile.h/.cpp), ägd av PluginBase (timer 3 min, meddelandetråden). Skriver bara när tillståndet ändrats (bytejämförelse), 3 roterande filer per instans i `Dokument\Kickbase\Autosave`, max 12 filer i mappen. PROJECT-menyn: "Autosave every 3 min" (av/på, inställning `autosave`) och "Open autosave..." (läggs sist efter History, Arbetare 2:s rader orörda). Test: persisttest (6 nya checks) grön.
Kvar i IO: ZIP/import-export-center, arrangemang (ingen spec finns; kbproj-spec säger "inte i v1").

## Fas I 4) S och S3 – kontroll mot koden (Arbetare 1, 27 okt)
Inget saknas, inget byggt. Fynd:
- FILTER av/på hela instrumentet + per källa (OSC 1–3, SAMPLE, NOISE): `Params.cpp:392`, `InstrumentSynth.cpp:423`, `FilterPanel.cpp:242,254`, hjälp `Help.cpp:107-110`.
- Brustyper White/Pink/Brown/Vinyl: `Params.cpp:396`, `InstrumentSynth.cpp:467`, `Modules.cpp:1000`.
- WARP ENV-källa (Filter Env/Direct/ENV 2/ENV 3) + Direct-decay + varning "needs a WARP mode": `InstrumentSynth.cpp:396-405`, `Params.cpp:397-398`, `Modules.cpp:1005,1030`.
- OSC-display WAVE/SCOPE/SPECTRUM med klick-för-lyssning: `OscPanel.cpp:55-64,195-212`.
- OSC-presets (.oscpreset, meny, Save, RESET): `OscPresets.cpp`, `OscPanel.cpp:238,496-573`. Inga färdiga .oscpreset-filer ligger i repot (mappen skapas vid första Save).
- S3 unison/Reese per OSC: `aUnison/aDetune` m.fl. (`Params.h:86-88`), `InstrumentSynth.cpp:94`; noterat som klart redan i PROGRESS-lc.md:73.

## Fas I 5) FX: riktig sidechain i slots (Arbetare 1, 27 okt)
Echo, Flanger, multiband (BAND DUCKER) och FX2-typerna fanns redan. Nytt, sist i `EffectType` (ID:n oförändrade): SIDECHAIN CURVE och SIDECHAIN 2-BAND (`Effects.h` enum, `Effects.cpp` knapptabeller, `SidechainCurveFx`/`SidechainBandsFx`, `EffectChain::setDuck`, anrop i `EffectRack::process`). De får samma duckningssignal som SIDECHAIN-fliken (kickens envelope eller kurvan). Test: `Tools/Tool.cpp` (instrumenttest, "FX2: SIDECHAIN ..."), 8/8 ok. blocksizetest ok, rendersig tb303 instr 10 oförändrad (`03fbeb761547e195`).
Antaganden: (1) 2-BAND använder den gemensamma globala kurvan, med eget djup och egen release per band (LOW, MID-HI); en egen kurva per band kräver ny kurvredigerare. (2) Slot-Maximizern (`MaximizerFx`) har egen algoritm; att den är identisk med master-limitern är inte verifierat. (3) Hjälptexter i Help.cpp för de nya rattarna är inte skrivna.
Rättat: `shortName`-listan hade för få poster (shimmer/lowMultiplier utanför arrayen); nu fyra poster till.
Gammalt fel (inte nytt): audiotest ger FEL med 4312 allokeringar/frigöranden på ljudtråden i tunga lägen (fx tungt: 112896), identiskt på ren HEAD utan mina ändringar. Mätpunkten är `Tools/AudioTests.cpp:94-99`; vilken allokering det är har inte spårats. Behöver egen utredning.
Kvar i FX (väntar, inte första releasen): MASTER-flik med 5 slots och effektpresets per slot (plan-v2.md:299-302). Läggs sist i FX-listan.

## D3b (JOG / LP, scratch av egna klockan) – Debug grönt, tester se nedan, ej provad i FL
Nytt: parametern `jogSpeed` (0..4, standard 1 = dagens ljud, sist i layouten, `Params.cpp` addJogParams; automatiserbar och LFO-modulerbar via `Modulation.cpp` isModulatable). `TransportClock::setJogRate` skalar `ppqPerSample` bara när klockan är egen (aldrig mot värden, `TransportClock.cpp:36-39`); nära 0 står klockan still. `PluginBase.h`: `jogManual` (hjulet, <0 = ingen touch, annars ersätter parametern) och `jogMode`. UI: liten LP-knapp bredvid restart (`Modules.cpp` jogButton, ikonen lyser när hastigheten inte är 1x eller en LFO styr den) öppnar `Source/Ui/JogPanel.h` (hjul, två lägen, ratt för JOG SPEED, statusrad, REMOVE AUTOMATION som tar bort LFO-rutter till jogSpeed och sätter 1x; DAW-automation tas bort i DAW:en).
Antaganden: (1) "två lägen" = STOP (hjulet håller still musiken, framåtvridning spelar i vridhastighet, släpp = normalt) och KEEP GOING (hastighet = 1 + vridhastighet). (2) Ett varv på 1,33 s = 1x. (3) Ingen baklängesscratch (klockan går bara framåt). (4) Läget sparas inte i projektet. (5) Värdens uppspelning påverkas aldrig; när värden spelar och klockan följer den gör hjulet inget (panelen säger det).
FL-testlista: 1) Tryck LP bredvid restart: panelen öppnas. 2) Starta LISTEN, håll hjulet i STOP: musiken stannar; vrid framåt: den spelar snabbare/långsammare efter hastigheten; släpp: normalt tempo. 3) KEEP GOING: musiken fortsätter; vrid framåt = snabbare, bakåt = långsammare (aldrig baklänges). 4) Dra JOG SPEED till 2: dubbelt tempo, LP-ikonen lyser. 5) Dra en LFO till JOG SPEED: statusraden säger vilken; REMOVE AUTOMATION tar bort den och sätter 1x. 6) Högerklick på ratten: DAW-automation (skriv ett kurvstycke i FL): hastigheten följer. 7) Starta FL:s uppspelning med DAW SYNC: hjulet ändrar inte FL:s tempo/position. 8) Stäng panelen medan hjulet hålls: tempot återgår. 9) Öppna ett gammalt projekt: jogSpeed = 1, ljudet oförändrat.

## D3b-tillägg: REC-läge, LP-utseende, audiotest-allokeringar (27 okt, Arbetare 1) – Debug grönt, ej provad i FL
REC (`JogPanel.h`, tredje läget bredvid STOP/KEEP GOING): hjulet skriver `jogSpeed` som parametergest (`beginChangeGesture` vid mouseDown, `setValueNotifyingHost`, vid släpp tillbaka till 1x och `endChangeGesture`), så att DAW:en kan spela in scratchen. `jogManual` är -1 i REC, parametern bär hastigheten. STOP och KEEP GOING oförändrade. Antagande: läget (`jogMode`) sparas fortfarande inte i projektet (antagande 4 ovan gäller; billigt att lägga till senare). Panelen blev 380 px bred så att tre knappar ryms.
FL-testlista REC: 1) Välj REC, sätt JOG SPEED-automationen i Write/Touch/Latch, starta inspelning, scratcha på hjulet: kurvan för JOG SPEED ritas i FL. 2) Spela upp: musiken följer scratchen (egen klocka, DAW SYNC av). 3) Släpp hjulet: kurvan går tillbaka till 1x och blir kvar där. 4) Byt till STOP och scratcha: ingen kurva skrivs.
LP-utseende: hjulet ritas som vinylskiva (metallplatta, mörk ring, spår, två fasta reflexer, svart etikett med roterande streck, silverarm med nålhuvud, hastighetstext). Bara ritkod och Theme-färger. Bild: `docs/work/jog-lp.png`, tas med `KB_JOG=1` + `KickbaseTool snapshot instrument <png>` (`Tools/Tool.cpp`, nytt, panelen ligger annars i en CallOutBox).
Allokeringar på ljudtråden (audiotest, bara Debug): orsak 1) `ParamID::rowLoop/rowBars` byggde `std::string`-nycklar till en `std::map`; MSVC:s debug-iteratorer allokerar en `_Container_proxy` per sträng (`Params.cpp` ~114-140). Fix: statiska tabeller. Orsak 2) FX-pluginet anropade `params.bass(0, fxSend)` som saknas där, och `jassert` i Debug allokerar strängar (`FxProcessor.cpp:149`). Fix: `Params::hasBass` (`Params.h`). Ingen effekt i Release.

## D3b-tillägg 2: handikon och tonarm som följer mönstret (27 okt, Arbetare 1)
Handikon: `JogPanel.h` mouseMove/mouseExit/mouseDown/mouseUp sätter `DraggingHandCursor` över skivan (JUCE har ingen separat sluten hand). Tonarm: ny atomic `TransportStatus::patternProgress` (0..1 genom hela mönstret), skrivs sist i `processChunk` i `InstrumentProcessor.cpp` och `FxProcessor.cpp` (~212); `JogPanel` mjukar den (`shownProgress`, hoppar tillbaka vid loopen) och flyttar nålen från skivans kant (95 % av radien) in mot etiketten (35 %).
FL-testpunkt: nålen åker inåt medan mönstret spelar och hoppar ut vid loopen; handen visas över skivan.

Skiss (ej byggd, beslut hos projektledaren/Tobias): välja vad som joggas + flera JOGS.
- Idag: en gemensam `jogRate` i `TransportClock` (`setJogRate`, bara egen klocka). Per-lager-scratch kräver en klocka eller fasförskjutning per lager: `StepScheduler::collect` får en egen ppq-position per lager (kick/bas går på huvudklockan, valda instrument på jog-klockan). Det är den stora delen: ~1-2 dagars arbete, rör `TransportClock`, `StepScheduler`, båda processorerna, och måste hålla rendersig-signaturerna oförändrade när inget lager är valt.
- Mål-väljare: multiselect (bitmask-parameter eller lista av lager) i `JogPanel`, liten UI-del.
- Flera JOGS: en JOG = {mål-mask, jogSpeed-kurva/LFO-rutt, läge}. Kräver flera `jogSpeed`-parametrar (t.ex. 4 st, sist i layouten) och persistens i projektet; automationen blir en parameter per JOG. Mest arbete ligger i Modulation (rutter per JOG) och i att spara/ladda.
- Risk: krockar med Arbetare 2:s filer (schemaläggaren, lagerlistan), så det bör tas efter releasen.
Audiotest grönt: orsaken till 62832 allokeringar var inte REC/progress utan ett gammalt fel, `clear (1, numSamples)` på maxBus/bypassBus/tapBuffer (`InstrumentProcessor.cpp` finishBlock ~862): första argumentet är startSample, så anropet började på sample 1 och gav jassert (Debug) vid block på 1 sample. Fix: ett anrop per buffert (clear (0, n) rensar alla kanaler). Ljudet oförändrat (Init 94f0a1b6bd8a3c7f, Empty 7d67e7bb66bb432b, instr 1 f7139084f716c127). Ny bild: `docs/work/jog-lp.png` (nålen i startläget vid kanten).

## Fas I 6) OSC-startpresets (27 okt, Arbetare 1) – Debug grönt, ej provad i FL
`OscPresets::ensureFactory` (`OscPresets.cpp:49`) skriver 14 `.oscpreset` (Sub 2, Bass 4, Reese 2, Acid 2, Growl 2, Layer 2) första gången, bara om mappen Oscillators saknas; skriver aldrig över och återskapar aldrig raderade. Anropas först i `OscPanel::stepPreset` och `showPresetMenu`. Warp-index 0-4 bara (Off, Sync, Bend, PWM, Mirror), eftersom listan efter det skiljer sig mellan OSC 1 och 2/3. Filer utan `Level`/`Phase` osv. lämnar fälten orörda, men våra har alla fält. Test i `presettest` (`Tools/Tool.cpp` ~3517): alla filer tolkas, har tabell som finns, laddas i OSC 3, raderad fil skrivs inte tillbaka. Ljudet oförändrat (inga nya parametrar).
FL-test: radera mappen Dokument\Kickbase\Presets\Oscillators, öppna OSC-preset-menyn: 14 presets i 6 kategorier; ladda en i varje OSC.

## AI-leverantörer: OpenAI och Ollama (27 okt, Arbetare 1) – Debug grönt, ej provad mot riktig nyckel/server
Chatten håller samtalet i Anthropics format; `OpenAiApi` (`AiProvider.h:52`, `AiProvider.cpp:~559`) översätter till Chat Completions och tillbaka, så `AiChat::run()` är gemensam. En adapter för båda: `ollama=true` ger `max_tokens`, ingen nyckel och inga bilder (`/v1/chat/completions`, standard `http://localhost:11434`). OpenAI: `max_completion_tokens`, `Authorization: Bearer`, bilder som `image_url`.
Nyckel krypteras som Gemini (`Settings::protect`), `AiChat::redact` rensar den ur loggar. Ollama har platshållaren "local" (`AiChat::activeKey`) som aldrig skickas; i UI räknas Ollama som ansluten när en modell är vald. Inget nät på ljudtråden (allt i `AiChat`-tråden/bakgrundstrådar). Inställningar sist: `openaiApiKey/Model` (standard gpt-5-mini), `ollamaBaseUrl/Model`.
UI: `AiChatPage.cpp` har fyra leverantörsknappar (ANTHROPIC, GEMINI, OPENAI, OLLAMA); för Ollama är nyckelfältet adressfält och SAVE hämtar modelllistan.
Test utan nyckel: `chattest` (`Tools/Tool.cpp` ~3246) kör sparade svar för modelllistor, text/tool_calls (sträng och objekt), vägran/length, request-bygget (tool → role "tool", ingen bild för Ollama) och felmeddelanden; testslutpunkter via `Settings::get` (`openaiEndpoint`, `openaiBase`). Ljudet oförändrat (inga ljudparametrar).
FL/handtest: klistra in en OpenAI-nyckel (eller starta Ollama med en verktygsmodell, t.ex. qwen3) och be om "gör kicken kortare".

## 27 okt kväll – samlad gren inför första releasen (Arbetare 1)
- w2 (9dfbf92, sedan toppen 6952d4e) mergad lokalt i play-pack: `5cb810b` + `e939b1f`. Inget pushat, inget till main.
- Konflikter: Params.h/.cpp (ringSource behölls), FxProcessor, InstrumentProcessor, Modules.h/.cpp (w2:s projektmeny + mina autosave-rader 8/9, `openProject (startFolder)`), LESSONS.md. Slarv loggat i LESSONS.md.
- Debug: KickbaseTests, rendersig/wide/tb303, blocksizetest (tar >10 min), audiotest (0 allokeringar), persisttest, undotest, chattest: alla gröna.
- Release byggd, KickbaseTests gröna.

## 27 okt kväll – PH: PHASE ROTATION (Arbetare 1, efter Release 03f1dc1)
- Ny effekttyp sist i `EffectType` (`Effects.h:21`, ID:n oförändrade): PHASE ROTATION (FREQ 20–300 Hz, ROTATION 0–360 deg, MIX). Två första ordningens allpass i serie, samma på L och R; fc = FREQ / tan(ROTATION/4), nivån oförändrad (`PhaseRotationFx` i `Effects.cpp`). Ny enhet `Unit::degrees`.
- Det Tobias testar i FL (release 03f1dc1) är orört: inga befintliga effekter eller parametrar ändrade, bara en ny typ sist i listan.
- Test: `Tools/Tool.cpp` (instrumenttest "FX2: PHASE ROTATION ..."): 60 Hz, 180 deg ger korrelation -1, 90 deg kvadratur, nivån oförändrad.
- Kvar i PH: AI-verktyg för att läsa/sätta grundtonens fas mot kicken (finns delvis: analyse_wavetable/edit_wavetable_harmonics), SHAPER-kontroll (spec till Opus/PL).
