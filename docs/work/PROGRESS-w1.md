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

## 27 okt kväll – UI: toppraden omgrupperad (Arbetare 1)
- `Source/Ui/Modules.cpp` `HeaderBar::resized`/`paint`, `Modules.h`: PREVIEWS (högtalare + notväljare) och LOOP (med från/till-takt) ligger nu i samma ruta som tempo/takt och JOG; LOOP överst i jog-delen. En gemensam ram (`transportGroup`) med avdelare mellan PREVIEWS och transporten.
- dB-mätaren ligger längst till höger, med en MASTER-knob (`masterKnob`) bredvid. Knoben är en `SliderAttachment` på `ParamID::gain` (samma parameter som OUTPUT-faderns i mixern, `MixerPage.cpp:184`). Ingen ny parameter, inga ändrade ID:n.
- Funktionellt oförändrat. Debug-bygget och KickbaseTests gröna. Ej visuellt kontrollerat i FL (Tobias ser layouten först i nästa release).
- Kvar från PL:s andra UI-lista: Add instrument-knapp, EDIT INSTRUMENT-knapp + högerklick, kickens SYNTH/SAMPLE på/av.

## ADD INSTRUMENT + EDIT INSTRUMENT (Arbetare 1, 27 okt)
- **ADD INSTRUMENT**: knappen "+ ADD INSTRUMENT" under sista raden i mönstrets instrumentlista, syns när det finns lediga platser (< 10) och alla rader är synliga. Höjden (24+3 px) dras av radernas utrymme (`StepSequencer.cpp` `resized`, `canAdd`). Kopplad till `InstrumentHost::addInstrument()` (nu publik) i `InstrumentEditor.cpp`.
- **EDIT**: knapp längst till höger i varje radkort (kicken + instrument) och "Edit instrument" överst i högerklicksmenyn (`InstrumentMenu::show`, nytt valfritt `onEdit`). Kicken -> flik KICK, instrument -> flik INSTRUMENT med just det instrumentet valt. Radkortet är 38 px bredare (`editColumn`), stegen blir lika mycket smalare.
- Påverkar det Tobias testar: mönstrets radkort (bredare, ny knapp), ny knapp under listan, ny menypunkt.
- Status: byggd i Debug, se nedan för testresultat. Ej pushad/mergad.

## Kö (från PL, 27 okt)
- Efter graf-pop-out: pianorullens glide. Plus-ikon vid glide-tonen som startar en glide-linje och snappar till målnoten; linjer från notens mitt; plus under tonen om musen är under, ovanför om över. Förklara först hur glide ritas idag (i rapporten).
- (PL, 27 okt) Pianorullen, före glide-plus: (1) FOLLOW stängs av automatiskt när man drar i sidoscrollbars (knappen visar det); (2) GHOST NOTES saknas i koden (bara klistra-in-"ghost" i PianoRoll.cpp:1583 finns): bygg multiselect-dropdown i verktygsraden, synlig även i POP OUT; instrumentets färg, halvgenomskinligt/konturer, endast visning (PROGRESS-copilot-2026-10-22.md punkt 7).
- (PL rättelse, 27 okt) Spöktoner finns redan (`PianoRoll.cpp:1476-1503`, "spökton", alltid på). Bygg BARA valet: GHOST NOTES-dropdown med multiselect (vilka instrument + av/på), standard alla på, sparas i projektet om enkelt, synlig i verktygsraden även i POP OUT. Ritningen ändras inte.
- (PL, 27 okt, efter glide-plus) SOUNDSTAGE: (1) multiselect-dropdown för vad som går genom soundstage (kick, varje instrument, FX/övrigt), standard = dagens beteende; (2) "above Hz"-val: undersök först hur MONO BELOW fungerar (görs redan på hela signalen?) och föreslå i rapporten WIDEN ABOVE-knob eller bara tydligare namn/tooltip. Nya parametrar sist, rendersig oförändrad, full testrutin Debug+Release. PL ska flyttas om Arbetare 2 äger soundstage-koden.
- (PL, 27 okt) Färg per instrument ihop med GHOST-dropdownen. OBS: finns delvis redan: `PluginBase::getLayerColour/setLayerColour`, `InstrumentMenu` undermeny "Color" (palett om 10), `Theme::layerColourOverrides`. Att göra: kolla att färgen används överallt (pianorullens toner/spöktoner, kanalrack, dropdownens färgprick) och lägg färgväljare i instrumentets inställningar.

## Vektorskop (StereoView, Arbetare 1, 27 okt) – ej byggt/testat än
`Scopes.cpp` StereoView: skala 0.5 + radiell begränsning vid 0.97 (kurvan stannar i cirkeln), efterglöd via 4 bildrutor (halverad alfa per ruta, varannan sampel), färg blå→röd efter nivå (`max(|L|,|R|)²`). REDUCE ANIMATIONS: bara senaste rutan ritas. Takten följer redan `VisualTimer` (soundstage). Ren visning, ingen parameter. Obs: Debug-bygget för Add/EDIT startades först från fel mapp och startades om; det kan ha plockat upp detta också.

Testresultat (27 okt): Debug `build.bat Debug all` byggde och "Alla tester gick igenom" (Add/EDIT instrument + vektorskop). Ej provat i FL; UI-punkter att prova: ADD INSTRUMENT-knappen, EDIT per rad, högerklick > Edit instrument, vektorskopets cirkel/efterglöd/färg.

## Kick SYNTH/SAMPLE på/av (Arbetare 1, 27 okt)
Nya bool-parametrar sist: `kickSynthOn`, `kickSampleOn` (Params.h/.cpp, standard på → gamla projekt låter likadant). Logiken samlad i `KickEngine::effectiveSampleMode` (KickEngine.h): synth av + sample på = som Replace; synth av + inget sample = tyst; sample av = som om inget sample vore laddat (synthens egen klick spelas); båda på = som förut. Ingen MIX-knapp: LEVEL (`kickSampleLevel`) är balansen. Click/Layer/Replace är ett läge, inte en strömbrytare; förut gick synthen inte att stänga av utan Replace och samplet inte utan att tas bort. UI: SYNTH- och SAMPLE-knappar + hjälptext längst ner i KICK-modulen (Modules.cpp, båda vyerna). Provlyssning av sample sätter sampleOn (InstrumentProcessor.cpp, FxProcessor.cpp:96). Hjälptexter i Help.cpp.
Test: Debug `build.bat Debug all` + "Alla tester gick igenom"; paramtest 0 fel; persisttest ok; rendersig oförändrad (Init 94f0a1b6bd8a3c7f, Empty 7d67e7bb66bb432b, instr 1 f7139084f716c127). Att prova i FL: SYNTH/SAMPLE-knapparna i KICK (båda vyerna), av/på med och utan laddat sample, spara/ladda projekt.

## NAVIGATION i tre rader (Arbetare 1, 27 okt)
**Vad:** Rad 1 = huvudmeny + brödsmula (KICKBASE > ANALYZER > SPECTRUM) i stället för 38 px-rubriken. Rad 2 = sidans kontroller (väljare, +/−, på/av, preset, groove-bar) i mörk remsa, visas bara på flikar som har sådana. Rad 3 = sidans egna flikar (ANALYZER i remsa-stil, instrumentets flikrad som förut). TOOLS-remsan har etikett TOOLS. Knappbredd = textbredd + 2×14 px (+ LED/ström), texten krymper aldrig, samma typstorlek (11).
**Hur:** `TabbedPanel.h` (`pillWidth`, `extrasRow`, `crumbArea`, `crumbSuffix`, `setStripLabel`), `KickbaseLookAndFeel.cpp:313-330` (egenskapen "noshrink"), `AnalyzerPage.h` (remsa + `currentViewName`), `InstrumentEditor.cpp` (synths-höjd 540→522 så sidorna får samma höjd som förut, TOOLS).
**Test:** Debug-bygge + alla tester gick igenom. Inga parametrar ändrade, rendersig oförändrad (bara UI).
**FL-punkter:** kolla att alla flikar ryms i bredd (inga avklippta texter), KICK/INSTRUMENT/PATTERN rad 2 ser rätt ut, brödsmulan ändras när ANALYZER-underfliken byts, ström-knappen på KICK-fliken, förhandsknappen längst till höger i rad 1.

## PLAY-grupp (Arbetare 1, 27 okt)
Kompakt ruta: tempo/takt + PREVIEWS (`HeaderBar::resized`, Modules.cpp). Menyikonen "≡" (`playMenuButton`) döljer/visar LOOP och JOG; valet sparas i Settings `playExtras` (standard = visas). Förhandsknapparna ALL/ROLL är play/stopp-ikoner (`ListenButton::setPlayIcon`); högtalaren finns kvar där den betyder mute/lyssna.
Test: Debug `build.bat Debug all` + "Alla tester gick igenom". Ingen parameter, rendersig oförändrad.
FL-punkter: ikonerna på förhandsknapparna, PLAY-menyn döljer/visar LOOP+JOG, valet finns kvar efter omstart.

## Navigationsjustering (Arbetare 1, 27 okt)
En på/av per modul: kickens bara på KICK-fliken (`InstrumentEditor.cpp:21`, `embedInTab (false)`); instrumentets längst till vänster i rad 2 (`Modules.cpp` Header::resized, `powers[]`). Flikar med inbyggd på/av räknar bredden med ikonen + marginal (`TabbedPanel.h` pillWidth/powerSpace, LookAndFeel `powerWidth`). Rad 1/2 36 px (+6), mer luft (flikgap 8, padding 18), ANALYZER-remsan 36. `baseHeight` 1028, synths 534.
Test: Debug `build.bat Debug all` + "Alla tester gick igenom". UI-only, ingen parameter, rendersig oförändrad.
FL-punkter: bara en på/av per modul, instrumentets på/av längst till vänster, KICK-flikens text/ikon, högre knappar, inget klippt i rad 1-3.

## JOG: SCRUB / SPEED / JOG (Arbetare 1, 27 okt)
**Vad:** JogPanel har tre typer (`jogKind`): JOG (gamla STOP/KEEP GOING/REC), SCRUB (standard i nya projekt) och SPEED (hjulet styr `jogSpeed`, ingen återfjädring). Hjulet pausar bara vår egen pattern/ljud, DAW:en går vidare; vår PLAY återupptar. SCRUB framåt = jogManual-hastighet (0..4), bakåt = nudge (ett varv = 4 ppq).
**Hur:** `TransportClock` (`setScrub`, `nudge`, `scrubRequested/scrubbing/pendingNudgePpq`, beginBlock), `PluginBase.h` (`jogKind`, `scrubActive`, `scrubNudgePpq`; atomics), `PluginBase.cpp` (egenskap "jogKind"; saknas den = JOG, alltså gamla projekt beter sig som förut), `InstrumentProcessor.cpp` (~155, `exchange` på nudge), `JogPanel.h`.
**Begränsning:** global LOOP är hela takter (int `globalLoopStartBar/EndBar`); finare loop (~1/64T) stöds inte.
**Bugg hittad av audiotest (min egen, från Kick SYNTH/SAMPLE):** `kickSynthOn/kickSampleOn` lades bara i instrumentets layout. FX-pluginets `readKick` (Params.cpp:600-601) slog upp okänt ID → `jassertfalse` (allokerar i Debug) och i Release 0.0 = synth+sample av, alltså tyst kick i FX-pluginet. Fix: `Params::addKickToggleParams` (Params.cpp, Params.h) anropas från både `addJogParams` (samma ordning som förut) och FxProcessor.cpp createLayout. Lärdom i LESSONS.md.
**Test:** Debug `build.bat Debug all` + "Alla tester gick igenom"; audiotest "Ljudtrådstestet gick igenom" (0 allokeringar); rendersig oförändrad (Init 94f0a1b6bd8a3c7f, Empty 7d67e7bb66bb432b, instr 1 f7139084f716c127); persisttest ok. Release: `build.bat Release all` + "Alla tester gick igenom".
**DAW-punkter (Lemon):** SCRUB är standard i nytt projekt, gammalt projekt = JOG; hjulet pausar bara vår pattern medan DAW går; vår PLAY återupptar; SPEED-hjulet ändrar JOG SPEED-knappen; kind sparas i projektet; kickens SYNTH/SAMPLE-knappar fungerar i FX-pluginet.

## JOG RESET + lampfix för lyssna-knappar (Arbetare 1, 27 okt)
- JogPanel: "REMOVE AUTOMATION" ersatt av liten RESET bredvid hastighetsratten (JogPanel.h, `resetSpeed`): jogSpeed=1 i en gest, rör inga LFO-rutter. Släckt när hastigheten redan är 1. Statustexten "No LFO on JOG SPEED." kvar.
- Lampbugg (piano rollens PLAY låst, sedan mönstrets PLAY): motorn har redan EN källa (`previewingLayer`, InstrumentProcessor.cpp:342). Felet var ListenButtons lokala `latched`/`held` (ListenButton.h) som aldrig rensades när en annan källa tog över. Nu släpps de när motorn slutar rapportera knappen som aktiv (`setActive`).
- Kvar i single-playback-mode: flikbyte ska stoppa bara enskilda preview-knappar (inte PLAY/piano roll-PLAY).
- Build Debug + alla inbyggda tester gröna.
- DAW-test (Lemon): (1) dubbelklicka piano rollens PLAY (fastlåst), tryck mönstrets PLAY: piano rollens lampa släcks och mönstret spelar. (2) JOG-panelen: vrid SPEED, tryck RESET: tillbaka till 1x, ett Ctrl+Z ångrar.

## 3g/3g-2 Presetkategorier + piltangenter (Arbetare 1, 27 okt)
- Kategori = attribut `category` i .mofp (kick/bass), mapp fortfarande platt (avvikelse från "undermapp + attribut": `findUserPreset` slår upp platt). Fabrikskategorier via namnmap i `PresetCategories.cpp` (PresetData.inl orörd, index/hashar oförändrade). Standard: GAMLA DEFAULT.
- `PresetButton.cpp`: undermenyer per kategori, spara-dialog med Category-lista, `keyPressed` (vänster/upp = förra, höger/ned = nästa, samma ordning som menyn).
- Ny trailing-parameter `category` på save-funktionerna (PluginBase.h/.cpp).
- Test: Debug build + alla tester gröna; rendersig oförändrad (Init 94f0a1b6bd8a3c7f, Empty 7d67e7bb66bb432b, instr 1 f7139084f716c127).
- DAW-test (Lemon): (1) klicka preset-knappen på Kick/Bass: kategorier som undermenyer. (2) Spara preset: välj kategori, syns under rätt rubrik. (3) Klicka knappen, tryck piltangenter: bläddrar. Om Lemon tar tangenterna: notera.

## Kö-tillägg från projektledaren (27 okt)
- Sist i rel-0.3/0.4: (1) HarmonicEditor (Source/Ui/HarmonicEditor.cpp, öppnas från OscPanel.cpp:207): genomgång + förslag (hitta, nollställ, förhandslyssning vid drag, undo, >64 övertoner, presets sågtand/fyrkant/udda). Kort plan hit först, skickas till PL före bygge. (2) ANALYZER-fliken mot docs/plan-v2.md:258-271: lista finns/saknas (energitopp där kick och bas möts, kickens svans som fångar basen, fasvarning slumpfas bas, för mycket 200 Hz), bygg det som saknas.
- S3 Unison/Reese och multisampling flyttade till Arbetare 2.
- Tillägg övertonsredigerare (Tobias, Serum-inspirerad, vi kopierar inget): syfte = ta bort basens grundton med ETT klick. Knappar REMOVE FUNDAMENTAL, REMOVE EVEN, ODD ONLY, RESET. Större fast editor i stället för CallOutBox: våg (tidsdomän) + styrke- och fasstaplar live-länkade, zoom på övertonsaxeln 1x/2x/4x. Penslar vid drag över flera staplar: linje, ramp upp/ner, kurva, slump. Presets SAW, SQUARE, ODD, EVEN, bara grundton. Senare: formelfält. Etapper: 1 knappar + större fönster, 2 live-våg, 3 penslar/formel. Ingår i planen som skickas till PL före bygge.
- Tillägg AUTO FIX (Tobias, via PL) = etapp 4, EFTER ANALYZER-mätningarna och övertonsredigeraren. Välj ett eller flera instrument (multiselect) mot kicken, tryck AUTO FIX. Mätning deterministisk (ingen AI): tailHz mot basens grundton per not, nivåskillnad, fas/korrelation i krockögonblicket, överskott ~200 Hz. Åtgärder: ta bort grundton i wavetable (gäller ALLA toner i tabellen, passar när basen ligger på en ton eller under kickens Hz), sänka nivå/velocity på krockande toner, PHASE ROTATION, sidechain-djup. UI: före/efter, åtgärdslista med kryss, APPLY bara för valda, allt som EN undo-gest, ändra aldrig utan att visa. Återanvänd AI-verktygen analyse_wavetable/edit_wavetable_harmonics. Planen ska också säga vad som INTE går automatiskt (per-not-korrigering av grundton). Bygg inte förrän PL sagt ok.
- Förtydligande AUTO FIX (Tobias): grundtonen tas bara bort på toner som krockar med kicken. Planen ska utreda två vägar och rekommendera en: A) kopia av instrumentet utan grundton, krockande noter flyttas dit (frågor: pianorulle, ghost notes, mix/FX, sidechain, håller kopian presetparametrar vid senare ändringar; kostar ett instrumentlager, defaultInstrumentCount 3 Params.h:25; ändrar mönstret). B) per-not-regel i motorn: grundtonsdämpning per oscillator som slås på när grundtonen ligger inom ett fönster kring kickens tailHz (på/av, fönster Hz/cent, nivå); ändrar inte mönstret; nya parametrar sist, ID/hash oförändrade; audiotrådssäkert, kort crossfade vid tabellbyte. Redovisa för/nackdelar, minne, risk. PLs gissning: B. Min bedömning görs efter genomgång av koden. Bygg inte förrän PL sagt ok.

## PLAN: FUNDAMENTAL DUCK + AUTO FIX (väg B vald) (Arbetare 1, 27 okt) — ingen kod än
**Rekommendation: B, som effekt som följer kickens envelope.** A (kopia av instrument) rör användarens mönster, kostar ett lager av tre (Params.h:25) och glider isär från originalet vid senare presetändringar. B ändrar inget i mönstret och ligger på ett ställe.
**Var det sitter:** `InstrumentProcessor::finishBlock` (InstrumentProcessor.cpp ~860-900), per baslager, FÖRE ducker/effektkanal. `duckData` (kickens envelope, redan per sample, rad ~832) finns redan i blocket och driver redan ducking, så ingen ny signalväg behövs.
**Hur:** en smal notch/peak-biquad per lager (stereo, två tillstånd) vars frekvens = lagrets nuvarande notgrundton (`bass.next()` måste exponera aktuell Hz; en float per lager skriven i `renderSamples`) och vars djup = `depth * duckData[i]`. Alltså dämpas grundtonen bara medan kicken klingar, och bara på toner som faktiskt ligger nära kickens svans: skalas med en vikt som är 1 när |grundton − tailHz| < fönstret (cent, mjuk kant) och 0 utanför. Då blir det per-not utan mönsterändring. Alternativ filter runt tailHz (fast frekvens) tas bort: kicken glider och basen kan ligga ovanför, så notens grundton är rätt mål.
**Klick/crossfade:** djup och frekvens slätas med samma enpolsfilter som Ducker (Ducker.h:16); koefficienter räknas om per sample bara när frekvensen ändrats mer än ~1 cent. Inget tabellbyte, alltså ingen crossfade av tabellen behövs; det är gain-smoothing som tar bort klick.
**Nya parametrar (sist i Params, ID/preset-index/hash oförändrade):** `fundamentalDuckOn` (bool, standard av), `fundamentalDuckDepth` (0-24 dB, standard 12), `fundamentalDuckWindow` (cent, standard 200), per lager 1-3 (6+ parametrar, en uppsättning per lager). Standard av ⇒ rendersig oförändrad (Init 94f0a1b6bd8a3c7f, Empty 7d67e7bb66bb432b, instr 1 f7139084f716c127).
**Audiotråd:** inga allokeringar; fasta biquad-tillstånd i lagret, parametrar läses som atomics en gång per block. Samma lärdom som kickSynthOn: nya parametrar måste in i BÅDE instrument- och FX-layouten (annars jassertfalse i audiotest).
**Risker:** (1) basen har egen sub/lågpass; notch före effektkedjan kan påverka effekter som är fas-känsliga, kontrolleras med rendersig + lyssning. (2) Notch ändrar fas runt grundtonen: PHASE ROTATION kan behöva justeras. (3) Aktuell Hz vid portamento/glide: använd glidande värdet, inte notens.
**Minne:** noll utöver några floats per lager. A hade kostat en hel basmotor + wavetable per kopia.
**AUTO FIX (etapp 4) på B:** väljer instrument (multiselect) mot kicken. Mäter deterministiskt (från ANALYZER-etappen): tailHz mot varje instruments noter i mönstret (vilka grundtoner ligger inom fönstret), nivå och korrelation i krockögonblicket, överskott runt 200 Hz. Förslagslista med kryss och före/efter-siffror: (a) slå på fundamentalDuck för lagret med fönster som täcker krockande noter, (b) sänk velocity/nivå på krockande noter, (c) PHASE ROTATION, (d) sidechain-djup. APPLY tillämpar bara valda rader som EN undogest (UndoManager-transaktion kring alla parameter/notändringar). Ändrar aldrig utan att visa listan. Grundtonsborttagning i wavetable (övertonsredigeraren) erbjuds bara när ALLA instrumentets noter krockar, eftersom den gäller alla toner i tabellen.
**Går INTE automatiskt:** att avgöra "krockar hörbart" är en mätning, inte en dom; noter med glide, LFO-modulerad pitch eller flera samtidiga toner mäts bara på noternas nominella tonhöjd; grundton som ligger över kickens Hz men krockar via övertoner (2:a övertonen) hanteras inte i första versionen.
**Etapper:** 1 ANALYZER-mätningarna, 2 fundamentalDuck (motor + parametrar + test), 3 övertonsredigeraren, 4 AUTO FIX. Bygg inget förrän PL sagt ok.

## Tillägg till FUNDAMENTAL DUCK (från projektledaren, Tobias önskemål)
1. **UI-sektion per baslager** (inte gömd): på/av, djup, fönster och en lampa som visar att den lyssnar på kicken.
2. **Källväljare** (kick / instrument 1-3 / valfri): samma envelope-väg. `duckData` blir en envelope per källa (kick = `kick.getEnvelope()`, instrument = lagrets `amplitudeBuffer`-envelope, utjämnad som Ducker). Ny parameter sist per lager (`fundamentalDuckSource`, standard 0 = kick) så rendersig är oförändrad. Byggs först efter projektledarens ok på UI-sektionen.
3. **Synlig verkan**: liten live-display i lagrets sektion (vågform/spektrum före/efter notchen + meter för aktuellt dämpningsdjup). Audiotråden skriver bara atomics/FIFO (djup, notch-Hz), inga allokeringar; UI läser i timer.
4. **Riktiga diagrammet**: dämpningen ritas i ANALYZER-spektrumet/CollisionsView som en markör för notchens Hz och djup, så man ser grundtonen sjunka vid varje kick. Vilken vy som passar föreslås när motorn står (spektrumvyn är första val: notch-kurvan ovanpå spektrumet).
5. Enkelspelarläge: flikbyte stoppar flikens egen LISTEN (TabbedPanel.h setCurrentTab, ListenButton::isHeldOrLatched).
6. **Förtydligat av Tobias**: "det riktiga diagrammet" = motorns/oscillatorpanelens vågform/spektrum per motor (OscPanel), inte ANALYZER. Dämpningen ritas där: före/efter notchen live (vågen/spektrumet ändras vid varje kick och ser olika ut mellan kickarna). Audiotråden skriver ett litet block före/efter (FIFO/atomics, ingen allokering), OscPanel läser i timer. Punkt 4 ovan ersätts av detta; ANALYZER-markör bara som tillägg senare.
7. **Tillägg av Tobias: även i ANALYZER** (gäller i tillägg till punkt 6). Samma atomics/FIFO-väg (notch-Hz, djup, aktiv-flagga per lager). Passar i `CollisionsView` (Source/Ui/CollisionsView.h:11): där kick och bas möts ritas en markör för notchens Hz som sjunker ned vid varje kick, plus spektrumets före/efter i `SpectrumView` (Scopes.h:57) som en andra kurva. Byggs efter motorn och OscPanel-visningen; rapporteras till projektledaren innan.

## Enkelspelarläge (flikbyte stoppar flikens egen LISTEN)
- Source/Ui/TabbedPanel.h `setCurrentTab`: byter man flik stoppas den gamla flikens förhandsspelning, men bara om flikens LISTEN-knapp själv startade den (`ListenButton::isHeldOrLatched`, ListenButton.h). Mönstrets PLAY och pianorullens PLAY rörs inte. Gäller alla TabbedPanel (huvudflikar och underflikar).
- Debug-bygge + alla tester OK (rendersig oförändrad).
- **Testrad DAW (Lemon):** fastlås LISTEN (dubbelklick) på INSTRUMENT, byt till KICK: ljudet tystnar. Starta mönstrets PLAY och byt flik: PLAY fortsätter.

## Tillägg: FUNDAMENTAL DUCK för hela instrumentet (INSTR-nivå) — plan, ej byggd (Arbetare 1, 27 okt)
Namn i UI: FUNDAMENTAL DUCK (kort FUND DUCK). Kod: `fundamentalDuck*`.
**Utvärdering:** dämpningen sker redan per lager på notens egen grundton (`getFundamentalHz`, röst för röst), inte på summan, så instrumentnivå är en fråga om parametrar och arv, inte ny signalväg. Polyfoni: med VOICES > 1 följer jag i första versionen den senast spelade rösten (som glide); full per-röst-notch (ett biquad per röst, 8 per lager) är möjlig men kostar 24 filter och kräver att rösternas Hz exponeras. Rekommendation: per-lager nu, per-röst som etapp 2b om Tobias spelar ackord i basen.
**Parametrar (sist, standard av, rendersig oförändrad):**
- Instrument (ett set): `fundamentalDuckOn`, `Depth`, `Window`, `Source`.
- Per lager 1-3: `fundamentalDuckMode` (Choice: ärv instrumentet / av / egen) i stället för bara på/av, plus egna `Depth`/`Window`/`Source` som gäller i läget "egen". Standard = ärv, och instrumentets On är av, alltså av överallt.
- Samplespelaren: tonen är känd per not (samma `cachedFrequency`-väg), så den kan ärva instrumentets inställning med en egen av/på. Kräver att sampelrösten exponerar sin uppspelnings-Hz (rotton + pitch); tas i etapp 2c efter att jag läst sampelmotorn. Ej lovad förrän läst.
**UI:** en INSTR-ruta (på/av, djup, fönster, källa) och per lager en liten väljare ÄRV / AV / EGEN som visar de egna rattarna först vid EGEN.
**Fråga till projektledaren:** ok på tre-lägesväljaren (ärv/av/egen) i stället för bara bool per lager? Det ändrar parameteruppsättningen jag var på väg att bygga (`fundamentalDuckOn` per lager blir `Mode`). Per-lager-motorn är densamma i båda fallen.
## FUNDAMENTAL DUCK – motor byggd, UI-skiss (Arbetare 1, 27 okt)
**Motor (klar, Debug + alla tester OK, rendersig oförändrad: Init 94f0a1b6bd8a3c7f, Empty 7d67e7bb66bb432b):**
- `Source/Core/FundamentalDuck.h`: RBJ peaking-biquad (Q 1,4, negativ förstärkning), stereo, per motor. Av = full förbigång + nollat tillstånd.
- `InstrumentProcessor.cpp`: `readLayerFields` (config ÄRV/AV/EGEN), `fundamentalDuckWeight` (källans envelope × fönster), `finishBlock` (filter efter gaten, före ducker; "före"-tappen orörd), `fdHz` = motorns grundton (`getFundamentalHz`, senast spelade rösten) per block, koefficienter var 8:e sampel. `fundamentalDuckDb[layer]` (atomic) för lampa/mätare.
- Parametrar sist i Params: globala `fundamentalDuckOn/Depth/Window/Source`, per motor `bassN FundamentalDuckMode/Depth/Window/Source`. Källa: Kick (standard), Instr 1-10, Any.
- Hjälptexter skrivna (inkl. gränsen "följer senast spelade rösten") + de saknade: jogSpeed, SIDECHAIN 2-BAND (SPLIT, LOW/HIGH DEPTH, LOW/HIGH REL), PHASE ROTATION: ROTATION.
- Ej gjort: 2b (per röst), 2c (samplespelaren) – bara om behov visar sig.

**UI-skiss (byggs härnäst):**
1. Instrumentruta märkt "FUNDAMENTAL DUCK – ALLA MOTORER": på/av, DEPTH, WINDOW, SOURCE. Tooltip: "Dämpar grundtonen i varje motor medan kicken klingar".
2. Per motor en liten väljare ÄRV / AV / EGEN. ÄRV: instrumentets värden visas nedtonade med texten "från instrument". EGEN: egna rattar (depth/window/source) aktiva.
3. Lampa per motor (läser `fundamentalDuckDb[layer]` i timer, tänd när > 0 dB) + dB-siffra.
4. OscPanel: vågform/spektrum före/efter notchen. ANALYZER (CollisionsView notch-Hz-markör, SpectrumView andra kurva) som tillägg efteråt.

**Testrad DAW (Lemon):** slå på FUNDAMENTAL DUCK på instrumentet (DEPTH 12 dB, källa Kick), spela ett mönster med bas på kickens ton: basens grundton ska sjunka vid varje kick och komma tillbaka; sätt en motor på AV: den motorn är oförändrad; sätt EGEN med DEPTH 0: ingen dämpning. (UI-delen testas efter att den byggts.)

## FUNDAMENTAL DUCK – UI byggt (Arbetare 1, 27 okt)
SIDECHAIN-fliken (bara instrumentet): ruta längst ned, `Source/Ui/SidechainPage.cpp` (FundamentalDuckPanel) + `.h:70`. Överst ON/DEPTH/WINDOW/SOURCE för instrumentet; en rad per aktiv motor med ÄRV/AV/EGEN, lampa + dB-siffra (`fundamentalDuckDb` i PluginBase.h), ärvda värden i grått ("from instrument"), egna reglage i EGEN. Tooltip innehåller "dämpar grundtonen i varje motor medan kicken klingar". UI-texter är engelska (som övriga fliken).
Inte gjort: förhandsvisning i OscPanel och ANALYZER-markör (kräver ljudtråd→UI-väg), tas senare vid behov.
Bygge Debug all: grönt, "Alla tester gick igenom" (rendersig oförändrad).
**Testrad DAW (Lemon):** öppna SIDECHAIN, slå på FUNDAMENTAL DUCK (DEPTH 12 dB, källa Kick), spela bas på kickens ton: lampan på motorraden tänds vid varje kick och dB-siffran rör sig; ÄRV visar grå värden; AV släcker lampan; EGEN med DEPTH 0 ger ingen dämpning.

## Multisampling – steg 3 persistens (Arbetare 1, 28 okt)
- Testet "tangent 76 väljer höga zonen" var ett testfel, inte motorfel: motorns mjuka begränsare komprimerade zonen med gain 0,8. Nivåerna sänkta till 0,05/0,4 (AudioTests.cpp, spelartestet). Zonvalet var rätt.
- Zonerna sparas i eget barn "BassMultiSamples" (PluginBase.cpp, instrumentMultiSamplesToValueTree/FromValueTree). Skrivs bara när zoner finns, så gamla projekt är byte för byte oförändrade. Rundturstest i StateTests.cpp ("Multi: …").
- Kvar: TYPE-väljaren (sampleSource Single/Multi) i UI, testrad, SFZ-import.

## Multisampling – TYPE-väljare i UI (Arbetare 1, 28 okt)
- SAMPLE-fliken (`Source/Ui/Modules.cpp` SamplePage): ny ruta Single/Multi (`sourceBox`, kopplad till `sampleSource`) i översta raden. I Multi tar LOAD flera filer på en gång (zoner), rottonen läses ur filnamnet (Piano_C3, bass_048; annars C4), tangentområdena sprids automatiskt halvvägs mellan rottonerna. X rensar alla zoner, etiketten visar antal zoner. HEAR är av i Multi.
- PluginBase: `addInstrumentMultiSamples` / `clearInstrumentMultiSamples` (PluginBase.cpp, före `syncSliceRows`).
- Inte gjort: zonredigering (tangentområde/anslag per zon i UI), SFZ-import (steg 4), zoner i preset-export.
**Testrad DAW (Lemon):** SAMPLE-fliken på ett baslager, välj Multi, LOAD och markera 3 filer namngivna t.ex. Bass_C2, Bass_C3, Bass_C4: etiketten visar "3 zones"; spela C1, C3, C5 från pianorullen/MIDI: varje tangent låter från närmaste zon och tonhöjden följer tangenten; X tömmer; byt till Single: den vanliga samplingen låter som förut; spara och öppna projektet: zonerna finns kvar.

## Multisampling – SFZ-kärnan (Arbetare 1, 28 okt)
- `MultiSampling::parseSfz`, `MultiSampleSet::addSfz`, `addFile(file, explicitMapping)` i `Source/Core/MultiSample.cpp`. Stöd: `<control> default_path`, `<global>/<master>/<group>/<region>` med arv, opcodes sample/key/lokey/hikey/pitch_keycenter/lovel/hivel/tune/transpose, tonnamn (c4 = 60).
- Ingen UI-koppling och inget KickbaseTool-kommando: Arbetare 2 tar dem plus testerna (projektledarens beslut).
- Bygget grönt förutom ett tidstest (wavetable: 120 tabeller på 30,7 s i Debug, tröskeln gick över medan flera byggen kördes); multisampletest och helpcheck gröna. Ej verifierat mot en riktig .sfz-fil än.
- DAW-test (Lemon): inget nytt att testa än, funktionen syns först när UI/verktyg kopplats.

## ANALYZER-mätningar + FUNDAMENTAL DUCK-visning (Arbetare 1, 28 okt)
- ANALYZER-mätningarna (kick vinner, svansen tar basen, RAND på bas, 200 Hz-lera) fanns redan: `MixAnalysis.cpp:782,795,856-899,1072` (cde9d61, e44fad8) med tester. Noten i PROGRESS-lc.md:115 är föråldrad. Inget att bygga.
- FUNDAMENTAL DUCK i motorns vågvisning: markör "DUCK -x.x dB" nere till vänster i `WavetableView` (`OscPanel.cpp` paintDuck, timerCallback läser `fundamentalDuckDb`). Ingen ny parameter, ingen ny ljudtrådskod.
- DAW-test (Lemon): slå på FUNDAMENTAL DUCK med en bas och en kick; spela, markören ska tändas i basens oscillatorbild medan kicken klingar och släckas efteråt.

## Broms B: BRAKE-knapp i JOG (Arbetare 1, 28 okt)
- Broms A (kurvpresets TAPE STOP/SPIN UP/SLOWDOWN) fanns inte i koden; byggde bara B enligt planen.
- BRAKE i JogPanel (Source/Ui/JogPanel.h, startBrake/stepBrake): engångsbroms, stannar på 1,2 s, vilar 0,3 s, snurrar upp 0,9 s till JOG SPEED. Skriver bara jogManual (ingen ny parameter, inget sparas). Hand på hjulet avbryter.
- Build grön utom den lastkänsliga wavetable-tiden (41 s), orelaterad.
- DAW-test: spela mönstret i DAW, öppna JOG, tryck BRAKE: tempot ska sakta in till stopp, vila, snurra upp igen; ta tag i hjulet under bromsen: bromsen avbryts.
- Kvar i JOG-resten: LOOP in i JOG-panelen (ej gjort).

## GHOST NOTES-rullgardin + FOLLOW av vid scroll (Arbetare 1, 28 okt)
- GHOST-knapp i pianorullens knapprad (`PianoRoll.cpp` showGhostMenu): flerval över KICK och aktiva instrument (färgprick, ALL ON/ALL OFF), menyn står kvar öppen. Maskan `ghostMask` i `PluginBase.h` delas med POP OUT, sparas inte i projektet, standard alla på.
- FOLLOW slås av när man drar den horisontella rullningslisten (`scrollBarMoved`).
- Ingen ny parameter. Build Debug all grön ("Alla tester gick igenom").
- DAW-test (Lemon): öppna pianorullen, GHOST, bocka av en instrument: dess spöktoner försvinner; menyn stannar öppen. Spela med FOLLOW på och dra rullningslisten: FOLLOW släcks.

## Glide-plus i pianorullen (Arbetare 1, 28 okt)
- Så ritas glide idag: en glide-ton (`note.slide`, rosa) har en ramp från tonhöjden som klingar (`pitchBefore`) till notens mitt vid dess slut (`PianoRoll.cpp` paint, "Rampen"); högerdrag på tomt rutnät ritar ett glidestreck (`addStroke`) som blir en glide-ton och fastnar på en ton som börjar vid strecket slut.
- Nytt: plus-ikon vid glide-tonens slut (`plusRect`/`updatePlus`), under tonen om musen är under notens mitt, annars ovanför. Drag från plusset startar ett glidestreck från notens mitt/slut och snappar till tonen under pekaren (mouseDrag). Klick utan drag gör inget. Ingen ny parameter, ingen ljudtrådskod.
- Build Debug all grön.
- DAW-test (Lemon): skapa en glide-ton (GLIDE på, rita), för musen över den: plus syns ovanför/under efter musens sida. Dra från plusset till en annan ton: strecket snappar till tonens början och en ny glide-ton skapas.

## Egen färgväljare för instrument (Arbetare 1, 2026-10-28)
- Instrumentmenyn > Color > "Custom colour...": fri färgväljare i en CallOutBox (`InstrumentMenu.h`, klassen `ColourPicker`). Varje ändring sparas direkt i projektet som palettvalen (`setLayerColour`), inga nya parametrar.
- Bygge `Debug all` grönt, alla tester gick igenom.
- DAW-test (Lemon): högerklicka ett instrument > Color > Custom colour..., dra i färgen. Kolla att pianorullens noter och spöknoter, radkortet och instrumentväljaren byter färg medan du drar, och att färgen finns kvar efter att projektet sparats och öppnats.

## SOUNDSTAGE: bussuppdelning + flerval av källor (Arbetare 1, 2026-10-28) – PLAN
- Idag: WIDTH / MONO BELOW / ENHANCE gäller hela summan (`Master.cpp:116-155`, efter master-EQ). Alla källor (kick, instrument 1-10, FX A-E = 16, `Mixer.h:15`) summeras i `maxBus` av `Mixer::mix` (`Mixer.cpp:108`).
- Uppdelning: 16 bool-parametrar `mix<Källa>Stage` (en per källa, SIST i layouten via `Mixer::addStageParameters`, standard = på = dagens ljud; ändrat från masken). Källor med bit av går i en ny `stageSkipBus` i stället för `maxBus` (samma gain/pan/send, bara annan mål-buss). Mastern kör EQ på båda (en andra `SourceEq` med samma parametrar, linjär så EQ(a)+EQ(b)=EQ(a+b)), kör stereoblocket bara på summan av de valda, lägger sedan tillbaka skip-bussen före DC/drive/limiter.
- Ljudtråden: ingen allokering (bussen förallokeras som `maxBus`). När alla bitar är på (standard) körs ingen skip-väg alls: samma kod och samma ljud som idag, rendersig oförändrad. Merkostnad annars: en EQ + en buss-addition per block.
- Inga ID:n, preset-index eller hash ändras. Bypass-bussen (förbi maximizern) berörs inte.
- UI: SOUNDSTAGE-sidan får en rad källknappar (flerval, färgprickar som GHOST) och tydligare namn/tips: MONO BELOW -> "BASS MONO", WIDTH -> "WIDTH (sides)". Hjälptexter i Help.cpp.

## LIVE CODE autocomplete (Arbetare 1, 2026-10-28)
- `LiveCodeComplete.h`: fler metoder/globaler (ply, iter, scale, arp, mask, range m.fl.), `sine.`/`saw.` m.fl. ger metodlistan. `LiveCodePage.cpp` codeDocumentTextInserted: öppnas automatiskt vid '.', '"' och efter 2 bokstäver; stängs vid andra tecken så Enter inte sväljs efter ')' eller mellanslag. Ctrl+Space som förut. Inga parametrar.
- Build Debug all grön, alla tester gick igenom.
- DAW-test (Lemon): LIVE CODE: skriv `no` -> lista öppnas; `s("bd").` -> metoder; Enter efter `)` ger ny rad; `sine.` ger metoder; Ctrl+Space öppnar manuellt.

## SOUNDSTAGE: genomfört (Arbetare 1, 2026-10-28)
- Kod: `Mixer::addStageParameters`/`inStage`, `mix(..., skipBus)` (Mixer.cpp), `MasterSection::process(..., skip)` med egen `eqSkip` (Master.cpp), `stageSkipBus` i InstrumentProcessor (summas efter stereoblocket, före DC/drive/limiter; masterfadern gäller även den). UI: 16 knappar "AFFECTS" i `SoundstagePage.cpp`, WIDTH/BASS MONO med tydligare tips, Help.cpp-text för *Stage och MAX.
- Build Debug all grön, alla tester igenom (LNK1163 på skadade .obj: raderade Modules/StepSequencer .obj och byggde om). paramtest 3048/961 ok.
- DAW-test (Lemon): SOUNDSTAGE: alla knappar på = låter som förut. Stäng av KICK, höj WIDTH till 200 % och slå på BASS MONO: kicken ligger orörd, övriga breddas. Stäng av en instrumentknapp: den hoppar över bredd men får master-EQ/limiter. Spara/ladda projekt behåller valet.

## Gemensam hjälpdialog (Arbetare 1, 2026-10-28)
- `Source/Ui/HelpDialog.h/.cpp`: markdown-lik vy (rubriker, punkter, **fet**, `kod`) i Viewport med rullningslist, icke-modal DialogWindow med en flik per ämne. Nytt ämne = en rad i `topics[]` + en text. `HelpDialog::setupButton` ger HELP-knapp med tooltip. Fliken SOUNDSTAGE har Tobias text ordagrant. Inga parametrar.
- HELP-knappar: pianorullen (nedre raden, längst till höger; den långa notis-tooltipen kortad), LIVE CODE (efter FORMAT), SOUNDSTAGE (vid AFFECTS). Motorerna och övriga flikar kommer en i taget.
- Build Debug all grön, helpcheck 0 utan förklaring.
- DAW-test (Lemon): klicka HELP i pianorullen: dialogen öppnas på PIANO ROLL, rullningslisten fungerar, flikarna LIVE CODE/SOUNDSTAGE byter text. HELP på LIVE CODE och SOUNDSTAGE öppnar rätt flik; är dialogen redan öppen byter den flik. Hovra HELP: tooltip. Esc stänger.

## HELP för FILTER + engelska UI-strängar (Arbetare 1, 2026-10-28)
- FILTER-fliken har HELP-knapp (FilterPanel.cpp, switchRow) och ämnet FILTER i HelpDialog.cpp.
- Engelska: SOUNDSTAGE-hjälpen, LiveCodePage flikar CODE/CONSOLE och tom-konsol-texten, Help.cpp 'from instrument'. Inga tester eller sparade layouter använde de gamla namnen.
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna FILTER-fliken, klicka HELP, kontrollera att FILTER-fliken visas; LIVE CODE ska visa CODE/CONSOLE.

## HELP för FM och MIX (Arbetare 1, 2026-10-28)
- FM-fliken: HELP-knapp uppe till höger (FmPanel.cpp, bredvid FILTER), ämnet FM. MIX-fliken: HELP-knapp bredvid WIDE (InstrMixPanel.cpp), ämnet MIX; rubriken kortad till "COMBINED WAVEFORM (110 Hz)" så knapparna får plats.
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna FM- och MIX-flikerna, klicka HELP, kontrollera rätt flik och att knapparna inte täcker rubrik eller knappar.

## HELP för SAMPLE (Arbetare 1, 2026-10-28)
- SAMPLE-fliken: HELP-knapp uppe till höger (Modules.cpp, SamplePage::resized, vänster om FILTER-rutan), ämnet SAMPLE (HelpDialog.cpp sampleText). Projektledaren godkände ändringen i Modules.h/.cpp (bara knapp och ämne).
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna SAMPLE-fliken, klicka HELP, kontrollera att SAMPLE-fliken visas och att knappen inte täcker FILTER-rutan eller etiketten.

## HELP för ENV (Arbetare 1, 2026-10-28)
- ENV-sidan: HELP-knapp bredvid instrumentväljaren (EnvPage.cpp, resized), ämnet ENV (HelpDialog.cpp envText).
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna ENV, klicka HELP, kontrollera att ENV-fliken visas och att knappen inte täcker väljaren.

## HELP för LFO (Arbetare 1, 2026-10-28)
- LFO-sidan: HELP-knapp bredvid SHOW ONLY SELECTED (LfoPage.cpp, resized), ämnet LFO (HelpDialog.cpp lfoText).
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna LFO, klicka HELP, kontrollera att LFO-fliken visas och att knappen inte täcker SHOW ONLY SELECTED.

## HELP för MATRIX (Arbetare 1, 2026-10-28)
- MATRIX-sidan: HELP-knapp efter CLEAR ALL (MatrixPage.cpp, resized), ämnet MATRIX (HelpDialog.cpp matrixText).
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna MATRIX, klicka HELP, kontrollera att MATRIX-fliken visas och att knappen inte täcker CLEAR ALL eller räknaren.

## HELP för EFFECTS (Arbetare 1, 2026-10-28)
- EFFECTS-sidan: HELP-knapp till vänster om FX Preset (EffectsPage.cpp, resized), ämnet EFFECTS (HelpDialog.cpp effectsText).
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna EFFECTS, klicka HELP, kontrollera att EFFECTS-fliken visas och att platsraden fortfarande får plats.

## HELP för MASTER (Arbetare 1, 2026-10-28)
- MasterPage.cpp (resized): HELP-knapp till vänster om Master Preset i headern; ämnet MASTER (HelpDialog.cpp masterText) täcker MIXER och MAXIMIZER.
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna MASTER i både MIXER- och MAXIMIZER-vyn, klicka HELP, kontrollera att knappen syns och inte täcker flikarna.

## HELP för SIDECHAIN (Arbetare 1, 2026-10-28)
- SidechainPage.cpp (resized): HELP-knapp till höger om Preset i vänsterkolumnen (preset blir 82 px bred); ämnet SIDECHAIN (HelpDialog.cpp sidechainText) täcker MODE, LENGTH, kurva, HALFTIME, DUCK/DEPTH, scope, FUNDAMENTAL DUCK och SIDECHAIN AFFECTS.
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna SIDECHAIN, klicka HELP, kontrollera att knappen syns och att preset-namnet fortfarande går att läsa.

## HELP för SETTINGS (Arbetare 1, 2026-10-28)
- SettingsPage.cpp (resized): HELP-knapp (44 px) längst till höger i raden med OPTIMIZATIONS/ABOUT; ämnet SETTINGS (HelpDialog.cpp settingsText) täcker WINDOW SIZE, OPTIMIZATIONS, ABOUT, AI LINK, FOLDERS, WAVETABLE PACK och FX-versionen.
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna SETTINGS (instrument och FX), klicka HELP, kontrollera att knappen syns och att OPTIMIZATIONS-knappen fortfarande går att läsa.

## HELP för KICK (Arbetare 1, 2026-10-28)
- Modules.cpp (KickModule, resized): HELP-knapp (44 px) till höger i källraden (SYNTH/SAMPLE-reglagen), syns i båda vyerna. Bara knapp + include i Modules.h/.cpp; projektledaren informerad. Ämnet KICK (HelpDialog.cpp kickText) täcker SYNTH-vyn, SAMPLE-vyn (bibliotek, LOAD, MODE, OFFSET) och källorna.
- Bygge grönt, alla tester gick igenom.
- DAW-test (Lemon): öppna KICK i SYNTH- och SAMPLE-vyn, klicka HELP, kontrollera att knappen syns och att balans-texten fortfarande får plats.

## TOP DECAY (kickNoiseDecay 5–2000 ms)
- `Params.cpp:196`: intervall 5–400 → 5–2000 ms, mittpunkt 60 → 100 ms, default oförändrad (60 ms). Inget nytt parameter-ID. Värdet lagras som riktig ms, så gamla presets/projekt låter lika.
- Rösten slutar vid kickens LENGTH (`KickEngine.cpp:210`, `lengthSeconds`), så en lång topp-svans kapas av LENGTH. Texten i `Help.cpp:166` säger det.
- Test: build Debug all grön, presettest, paramtest, helpcheck OK; rendersig körd (inga lagrade referenshashar i repot att jämföra mot; ändringen rör bara reglagets mappning, inte lagrade ms-värden).
- DAW (Lemon): vrid TOP DECAY, lågt läge ska vara lika lätt att ställa som förr; höga värden ger lång luft om LENGTH är lång.

## Broms A: BRAKE-kurvan sparas med projektet (Arbetare 1, 28 okt)
- Broms A (cherry-pick av 1193cc9 från pl-presets, konflikten med LOOP-koden löst genom att bara ta rullistan): VINYL/TAPE/HARD/SLOW.
- `PluginBase.cpp:613` sparar `brakeCurve` som state-property (som `jogKind`, inget host-parameter-ID, så ID/preset-index/hash rörs inte); `PluginBase.cpp:727` läser med standard 0 (VINYL = dagens beteende, gamla projekt låter lika).
- Test: `Tools/StateTests.cpp:952,970` (spara -> ladda ger samma kurva). Debug all grön utom wavetable-tidtestet (lastkänsligt, orelaterat); persisttest OK vid ensam körning.
- DAW (Lemon): välj TAPE, spara projektet, öppna det igen: kurvan ska vara TAPE.

## Merge w2 + FX-UI: Echo TIME MS, Flanger SHAPE/SPREAD (Arbetare 1, 28 okt)
- Mergade w2 (a601d2e, Echo-kärnan; 69f1ab0, Flanger) in i play-pack (dc4e972).
- Raderna i FX-slotten byggs av `Effects::knobs` (EffectsPage.cpp:221, stoppar på knobsPerSlot = 11), så elfte ratten TIME MS och Flanger SHAPE/SPREAD dyker upp utan layoutändring; bredden delas av antalet rattar (EffectsPage.cpp ~262).
- Hjälptexter: `Help.cpp:364-365` (Flanger SHAPE, SPREAD), `Help.cpp:457` (TIME MS).
- Debug all grön, alla tester gick igenom.
- DAW (Lemon): Echo i en FX-plats, TIME MS 0 låter som förut, 500 ger fast 500 ms; Flanger SHAPE Triangle och SPREAD 0/25/100 %; kolla att elva rattar ryms i FX-raden.

## HELP för INSTR, ANALYZER och AI CHAT (Arbetare 1, 28 okt)
- Nya ämnen `instr`, `analyzer`, `aiChat` i HelpDialog.h/.cpp. HELP-knappar: INSTR vid LISTEN i grafens hörn (Modules.cpp `layoutInstrPage`), ANALYZER längst till höger i underflikraden (AnalyzerPage.h), AI CHAT i huvudraden efter MEM (AiChatPage.cpp).
- Texterna bygger på sidornas egna tooltips.
- Kvar utan HELP: GATE (FxEditor), KickLibraryPanel, HistoryPanel, AboutPanel, OptimizationsPanel (små/ytliga, tas om Tobias vill).
- DAW (Lemon) att kolla: att INSTR-knappen inte täcker grafens handtag, och att ANALYZER/AI CHAT-knapparna får plats vid smal bredd.

## 5b POS DELAY / LENGTH / CURVE per motor (Arbetare 1, 28 okt)
- Plan: `docs/work/plan-start-per-motor.md`. 28 nya parametrar sist i InstrumentField (`Params.h`, `aDelayMs` … `fmCurve`; `ParamID::timingField (engine, which)`), registrerade i `Params::addEngineTimingParams`.
- DSP: `InstrumentSynth::engineGate` (InstrumentSynth.cpp) multiplicerar nivån för OSC 1–3, NOISE, SUB, FM. Exakt 1.0 när POS DELAY och LENGTH är 0, så rendersig är oförändrad. POS DELAY (ms eller SYNC från bpm) låses vid noteOn; oscillatorfasen nollas när fördröjningen passerat.
- UI: TIMING-vy i MIX (`InstrMixPanel`): POS DELAY, SYNC, LENGTH, CURVE per motor; gul varning "POS DELAY is longer than the note". Hjälptexter i `Help.cpp` och MIX-hjälpen.
- SAMPLE-faktorn (motor 3) klar efter merge av fb7dcbe: sample-grenen i InstrumentSynth.cpp hoppas över till noteTime >= POS DELAY (samplePosition och sampleAge står still), och engineGate (3) multiplicerar editorGain. Ingen egen test (kräver laddat sample); DAW (Lemon): ladda ett sample, POS DELAY 200 ms på SAMPLE, samplet ska börja från sin början efter 200 ms.
- Nytt test `tool delaytest` (`Tools/AudioTests.cpp`): POS DELAY 100 ms och SYNC 1/16 vid 120 BPM tystar bara OSC 1 i notens första 100 ms, LENGTH avtar, CURVE faller snabbare.
- DAW (Lemon) att kolla: TIMING-knappen i MIX, att alla standardvärden låter som förut, POS DELAY 100 ms på OSC 2 ensam, SYNC 1/4, LENGTH + CURVE.
- Fix: SAMPLE-motorns LENGTH/CURVE multiplicerar nu editorGain (InstrumentSynth.cpp:601) i stället för att skrivas över när AMP är på.
- HELP-flikar: nya ämnen GATE, HISTORY, ABOUT, OPTIMIZATIONS (`HelpDialog.h`, texter i `HelpDialog.cpp`), HELP-knapp i GateModule (Modules.cpp), HistoryPanel.h, AboutPanel.cpp, OptimizationsPanel.cpp. KickLibraryPanel har ingen egen knapp: den ligger i kickens SAMPLE-vy som redan har kick-HELP. build Debug all grön, helpcheck 0, delaytest ok.
- DAW (Lemon) att kolla: HELP-knappen syns och öppnar rätt flik på GATE (Kickbase FX), MENY > History, SETTINGS > ABOUT och OPTIMIZATIONS, även vid smalt fönster.

## snap-ticks (2026-10-28, Arbetare 1)
- Rattar vars värdetext slutar på " st" (KnobComponent::semitones) fastnar vid ±12/±24 (±0,4 st, Shift = fri) via `ModSlider::snapValue`, och får små tickmärken utanför ringen (`paintOverChildren`). Ingen parameter ändrad. Bygge + tester gröna; ej provat för hand i Lemon.
## kick-ui (2026-10-28, Arbetare 1)
- MIX-knopp (`kickMix`, sist i layouten, 0.5 = som förut) ersätter SYNTH/SAMPLE-reglagen; synthOn/sampleOn finns kvar som parametrar. Motorn: `KickEngine.cpp` renderVoice.
- Tre flikar SYNTH | ENVELOPE | SAMPLE (kickTab 2 = ENVELOPE, plotten har egen flik), smalare presetväljare (170 px), luft runt flikarna.
- Hjälptext + ToolLayer uppdaterade. Bygge + alla tester + paramtest + helpcheck gröna.
- Öppet: gamla presets med synthOn=false saknar reglage för att slå på det igen (MIX till höger ger nästan samma).
- synthOn=false: mappas INTE till MIX, eftersom ljudet inte blir identiskt (synthOn=false tvingar Replace-läge, MIX=1 behåller valt sampleMode). Därför finns små SYNTH/SAMPLE-knappar kvar bredvid MIX (Modules.cpp, sourceRow). Inga ID/index ändrade.

## glass-breadcrumb (Arbetare 1, 2026-10-28)
- Gren `glass-breadcrumb` = snap-ticks + kick-ui + detta (kedjad). Brödsmulan ritas som glasdisplay med glödtext: `Theme::paintGlassDisplay` / `Theme::drawGlowText` (Theme.h), anropas i `TabbedPanel.h` paint.
- Dubbletten (flikens namn som etikett i underflikarnas remsa) borttagen; krymper inte layouten (labelArea oanvänd).
- Build + alla tester gick igenom. Skärmdump: scratchpad `crumb-analyzer.png` (KickbaseTool snapshot).

## timing-levels (Arbetare 1, 2026-10-28)
- Gren `timing-levels` = glass-breadcrumb + TIMING/LEVELS (kedjad). Egen flik TIMING bredvid MIX (`Source/Ui/InstrTimingPanel.*`), all timing-UI borttagen ur MIX.
- 28 nya parametrar sist i layouten (`addEngineTimingExtraParams`, Params.cpp): MODE (GATE/SHIFT), LENGTH SYNC, DELAY x, LENGTH x per motor. Standard = som förut. SYNC-tid = antal x notvärde x takt, låst vid note-on. Hjälptexter + helpcheck + paramtest gröna.
- SHIFT: motorn startar om efter POS DELAY; FM-envelopen och OSC-warp-envelopen startar också om. Pitch-envelopen är gemensam och startas inte om.
- Diagram per motor (`gateAt`) med spelhuvud och släpplinje (atomer `notePlayTime`/`noteOffTime`, skrivs av senast spelade rösten).
- LENGTH SYNC "TO BEAT" hoppad över: synthen har ingen taktposition, bara `bpm`.
- blocksizetest: "tungt läge med 2/4/8 röster (stealing)" FALLER, men också på baslinjen 57ddb1a (byggd i separat worktree) med samma symptom. Alltså äldre fel, inte från TIMING. Behöver utredas separat (röststöld beror på blockstorlek, första skillnaden vid sample ~71-360).
- Skärmdump: `%TEMP%\timing2.png` (snapshot instrument 20, KB_SOURCETAB=8).

## collisions + detune (Arbetare 1, 2026-10-28)
- Gren `collisions` = timing-levels + detta (kedjad). COLLISIONS i `SpectrumPage.cpp`: styrka efter gap (starkaste minus näst starkaste): 0 dB = full, 18 dB eller mer = maskerad, ingen markering. Tröskel -50 dBFS (förut -66). Blå (svag) till röd (besvärlig), max alpha 0.5, fyra band som tonar ut nedåt, ingen allokering i paint. Konstanterna överst i filen. Engelsk text och tooltip uppdaterade. Nytt `KB_COLLISIONS=1` för skärmdump (flik 30).
- OSC 1-3 DETUNE standard 12 ct -> 0 ct (Params.cpp:275). Bara standardvärdet; index och ID oförändrade.
- blocksizetest (voice stealing) felar redan på baslinjen 57ddb1a och alla grenar ovanpå (timing-levels, collisions). Läggs på Arbetare 2 av projektledaren (InstrumentVoices).

## JOG förenklad (Arbetare 1, 2026-10-28)
- Gren `jog-simple` = collisions + detta (kedjad, innehåller allt). Ett hjul utan lägen: JOG SPEED, RESET, BRAKE och TAPE/VINYL borta (`jogSpeed` finns kvar som oanvänd parameter så ordningen är oförändrad; `Modulation.cpp:69`). Inget sparas, ingen migrering.
- LOOP FROM/TO i ticks (1 tick = 1/64 takt, visas bar.beat.sixteenth.tick): `TickField` i `Source/Ui/JogPanel.h` (klick/Enter, skriv, Enter/Esc, Upp/Ned = STEP). STEP 1/4-1/64 (standard 1/16), TEST BPM + fält 60-220 (standard 140). Konstanter överst (`JogConfig`). `TransportClock::ticksToPpq` (TransportClock.cpp:71).
- Test: tick-omvandling + visa/tolka-rundtur i `Tools/StateTests.cpp` (persisttest). Debug all grön, alla tester gick igenom. blocksizetest: se rapport (stealing-felet är äldre, på baslinjen).
- DAW (Lemon): öppna JOG: ett hjul, LOOP på, FROM 1.1.1.0 / TO 5.1.1.0; skriv en ny tid + Enter, Esc avbryter; Upp/Ned flyttar ett STEP; TEST BPM 140 spelar utan att DAW:en rullar.
