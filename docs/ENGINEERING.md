# Så bygger och rättar man i Kickbase

För den som implementerar nästa sak (en agent eller Tobias). Samlar det granskningen (oktober 2026, `docs/audit/`) kom fram till:
reglerna, verktygen och fällorna. Läs avsnitt 1–3 innan du ändrar något; resten när du rör den delen. Håll filen aktuell: hittar du
en ny fälla, skriv in den här.

## 1. Grundregler

1. **Ljudet får aldrig ändras av misstag.** En ändring som inte ska låta annorlunda ska ge **bit för bit** samma ljud: bevisa det
   (avsnitt 2). Ska den låta annorlunda: mät hur mycket, skriv det, och fråga Tobias först om det hörs.
2. **Inget otestat.** Varje rättning eller funktion har ett test i sviten, eller en mätning som står i commit-meddelandet.
3. **Parameter-ID:n, presetmappar och plugin-identiteten byts aldrig** (`bass1Cutoff`, `mixKickGain`, `fxA…`, mappen `Bass`,
   `PLUGIN_CODE`, `PLUGIN_MANUFACTURER_CODE`, `BUNDLE_ID`). DAW-projekt och presets hittar parametrarna via ID:t. Visningsnamn får ändras.
4. **Nya parametrar har standardvärden som ger dagens ljud**, så att gamla projekt och presets låter som förut.
5. **Små commits**, en sak per commit, med vad som ändrats, varför och hur det verifierats.
6. **Rör inte Tobias presets, fabrikspresets eller `Resources/InitPreset.mofp`.** Ljuddesign görs som kopior
   (`<original> (edited by …)`) bland användarpresetsen.
7. **Fel som rör JUCE: läs JUCE-källan för den installerade versionen** (`build/<cfg>/_deps/juce-src/modules`, JUCE 8.0.15) innan du
   bygger något runt felet. Flera fel under granskningen var JUCE-beteenden (avsnitt 9).

## 2. Bygga, testa, verifiera

**Bygga:** `build.bat Release` eller `build.bat RelWithDebInfo all` (`all` tar med verktygen). Riktat: vcvars64, sedan
`cmake --build build\RelWithDebInfo --target <mål>`. Mål: `Kickbase_VST3`, `KickbaseFX_VST3`, `Kickbase_Standalone`, `KickbaseTests`,
`KickbaseMcp` (även verktyget; kopieras till `KickbaseTool.exe`), `KickbaseHostTest`. (Målet `Kickbase` utan `_VST3` är bara det delade
biblioteket: det bygger ingen VST3.) Installera: `createVst.bat` som administratör, FL Studio stängt.

**Hela sviten** (alla ska gå igenom före varje commit som rör `Source/`):
- `KickbaseTests.exe`;
- `KickbaseMcp tool` + `eqtest rolltest lengthtest instrumenttest presettest auditiontest settingstest chattest listentest perftest persisttest audiotest blocksizetest`;
- `KickbaseMcp --selftest-link`;
- `KickbaseHostTest <Kickbase.vst3>` och `KickbaseHostTest <Kickbase FX.vst3>`;
- `tool rendersig`, `tool rendersigreal`, `tool rendersigwide`.

**Bit-identiskt ljud bevisas med fingeravtrycken:**
- `rendersig` = Init från koden + fabriksljuden;
- `rendersigreal` = det riktiga, inbakade Init;
- `rendersigwide` = ett tungt läge i båda pluginsen: 10 instrument, alla effekttyper, modulering, udda blockstorlekar och 44,1–96 kHz.

Jämför före/efter med `diff` (utan tidsrader). Spara utdata *före* ändringen. Gällande baslinjer (efter fråga 1, kontrolltakten):
`docs/audit/baseline-kod-kontrolltakt.txt`, `baseline-init-kontrolltakt.txt`, `baseline-wide-kontrolltakt.txt`. (`baseline-kod.txt` och
`baseline-init.txt` är från före fråga 1 och gäller inte längre.) Ändras ljudet med avsikt: skriv nya baslinjer i samma commit och säg hur
mycket det ändrades (nolltest). `tool repairpresets` (utan `apply`) mäter alla användarpresets.

**Gränssnittet bevisas med skärmbilder:**
- Kommandot är `tool snapshot instrument|fx <ut.png> <flik>`. Flikcifferet: lägre siffra 0 EFFECTS … 8 SETTINGS, tiotal = övre flik, 20 = INSTRUMENT.
- Miljövariabler: `KB_INSTRUMENTS=10`, `KB_SPECTRUM=1`, `KB_OPTIMIZATIONS=1` m.fl.
- Jämför pixel för pixel mot bilder från före ändringen (granskningen använde ett litet Python-skript utan PIL som avkodar PNG).
- Titta också på bilderna: 1, 3 och 10 instrument, och Kickbase FX (lägre fönster).

**Verktyg för att hitta fel och kostnader:**
- `audiotest`: räknar allokeringar på ljudtråden. Med `KB_ALLOC_TRACE=1` visas varifrån de kommer.
- `blocksizetest`: ljudet ska vara exakt detsamma vid alla blockstorlekar.
- `persisttest`: sparande, laddning och presets.
- `profile <init|heavy|fx> [instrument] [s]`: ljudtråden, per funktion och källrad.
- `uiprofile <flik> <init|heavy> [instrument] [s]`: meddelandetråden med editorn öppen.
- `helpcheck`: parametrar utan förklaring.
- `params instrument|fx`: hela registret.

**Mät rättvist:**
- Datorn kan ha bakgrundslast (VS Code:s indexering efter stora ändringar tog ~25 % och gjorde allt ~1,7 gånger långsammare).
- Jämför gammal och ny binär omväxlande under samma minut, aldrig mot en gammal siffra.

## 3. Ljudtråden

- **Ingen allokering, inga lås, ingen fil- eller nätverksåtkomst i `processBlock`.** `audiotest` fångar allokeringar, också när
  rattar ändras mellan blocken.
  - Bygg buffertar i `prepareToPlay` (`ensureScratchSize`).
  - Läs parametrar via cachade pekare (`Effects::EqRefs`, `ParamRefs`), aldrig genom att bygga ID-strängar.
  - Skapa inga JUCE-koefficientobjekt per block (`Coefficients::make*` allokerar): skriv värdena på plats, se `Biquad.h` och `eqArrayCoefficients`.
- **Det ljudtråden läser och meddelandetråden byter ut** (samplingar, wavetables, kurvor) frigörs via `DeferredRelease`
  ([DeferredRelease.h](../Source/Core/DeferredRelease.h)), aldrig direkt: två byten inom ett block frigjorde minne som ljudtråden läste.
  Fält som ljudtråden läser är atomiska.
- **NaN/inf:** en enda icke-ändlig sample fastnar för alltid i filter och återkopplingar. FX-insignalen tvättas
  (`replaceNonFinite`), och båda pluginsen nollställer DSP-tillståndet om ett block ändå blir icke-ändligt (`allFinite`, `resetDspState`).
  Ett nytt DSP-block med tillstånd ska ha en `reset` som `resetDspState` anropar.
- **Kontrolltakten (fråga 1):** `PluginBase::processInControlChunks`
  ([PluginBase.h](../Source/Core/PluginBase.h)) delar varje DAW-block i bitar som börjar på en multipel av 32 samples (räknat från
  `prepareToPlay`). Hela blockbehandlingen (modulering, parametrar, effekter, master) körs per bit, så ljudet är **identiskt vid alla
  blockstorlekar** (`blocksizetest` kräver exakt noll skillnad).
  - Ny kod i `processChunk` ska bara bero på bitens innehåll och tillstånd, aldrig på DAW-blockets storlek.
  - En bit kan börja mitt i ett 32-fönster (när DAW-blocket slutade där); `controlPhase` säger var. Allt som uppdateras "per fönster"
    görs **bara när `controlPhase == 0`** och återanvänds i resten av fönstret: moduleringen, parameterläsningen, auto-filtrets svep och
    filtereffektens utjämnade cutoff.
  - En utjämning får aldrig stegas med bitens längd: 5 + 27 samples ger andra tal än 32 (det var felet i FilterFx som udda blockstorlekar
    i `blocksizetest` hittade).
  - Händelser på samma sample ordnas efter när de schemalades (`StepEvent::sequence`), inte efter vilken bit de hamnade i.
  - `getHostPosition()` ger positionen vid bitens början.
  - Räknare som "samples kvar" räknas ner per bit.
  - Felsökning: `KB_BS_RESET=ord1,ord2 tool blocksizetest` återställer de parametrar vars ID innehåller orden (skiftlägeskänsligt), så att man
    kan ringa in vilken del som beror på blockstorleken.
- **Parametrar läses bara på 32-rutnätet, och bara när de kan ändras.**
  - Ett nytt DAW-block ger en hel läsning vid nästa rutnätspunkt (`blockParamsPending`); automation tas alltså in på rutnätet.
  - Mönsterlängder, swing och humanize kan inte moduleras och läses bara i den hela läsningen.
  - Mellan två hela läsningar läser `readModulatedParams` bara om de instrument och den kick som en aktiv modulering går till.
    Ägarkartan heter `parameterOwner` och finns i [InstrumentProcessor.cpp](../Source/Instrument/InstrumentProcessor.cpp).
  - **Ny modulerbar parametergrupp** som inte börjar med `bassN` eller `kick`: läs den varje bit i sin egen modul (som `EffectRack`
    gör med `fx…`), eller lägg till den i ägarkartan. Annars rör sig moduleringen bara en gång per DAW-block.
  - Det som räknas om ur parametrar (koefficienter, `tanh`, `pow`) räknas bara när värdet ändras. Mönstret finns i Mixer, EffectChain,
    InstrumentSynth (`DriveShape`, SVF-setters) och KickEngine, och det ger samma tal.
- **Modulerbart** = flyttalsparameter vars ID börjar med `kick`, `bass`, `fx` eller `gate` (`Modulation::isModulatable`).
  - Modulationskällornas nummer sparas i projekten (0–2 LFO, 3–6 makro, sedan fyra per instrument).
  - Nya källor läggs **sist**. `static_assert` i [PluginBase.cpp:9-14](../Source/Core/PluginBase.cpp#L9-L14) låser det som inte får flyttas.

## 4. Parametrar

- Läggs till i `Params::add…Params` (instrument), `Effects::addParameters` osv. Ordningen i listan är DAW:ens; lägg nya **sist i sin grupp**.
- **Standardvärdet ska ge dagens ljud** (rendersig* identiska efter tillägget, med standardvärdet).
- **Visningsnamn unika i DAW:en** (förut fanns två "Kick Length"), och tydliga: "FX1 Slot 1 Type", inte "FX1 1 Type".
- **Varje parameter och effektratt har en förklaring** i [Help.cpp](../Source/Ui/Help.cpp) (`tool helpcheck` ska ge 0). AI:n får samma
  text i `list_parameters`/`list_effects`.
- **Oanvända parametrar** tas inte bort (projekten har dem). De märks "(unused)" och tas ur MATRIX-målen (se `bassNGlide`).
- Uppdatera [AGENT_GUIDE.md](AGENT_GUIDE.md) och [AnalysisAbout.h](../Source/Core/AnalysisAbout.h) när en parameter eller ett beteende ändras.

## 5. Sparande och presets

- **En preset- eller projektladdning sätter varje parameter en gång, med sitt slutvärde.**
  - Ljudtråden spelar under tiden. "Först standardvärden, sedan filens värden" gav ett block med standardljud.
  - Sätt **alla** sektionens parametrar, också de som redan har rätt värde. JUCE:s råa värde uppdateras inte vid `approximatelyEqual`,
    och startvärdet är inte avrundat, så att hoppa över "lika" värden ändrade ljudet med några ULP (avsnitt 9).
  - Se `PluginBase::loadPresetXml` och `Presets::applyFactory`.
- **Data som inte är parametrar** (egna wavetables, samplingar, kurvor) ska följa med i projektet *och* i Full/instrument-presets, och
  rensas när ett tillstånd utan dem laddas. Rundtur och rensning testas i `persisttest`.
- **Sektioner:** `Presets::Section` och `belongsTo` avgör vad en preset rör. Routes (`mix*Max`, `mix*Duck`) finns bara i Full.
  Kick- och instrument-presets rör inte radens på/av (`kickOn`, `bassNOn`).
- **Tester som sparar filer** körs i `Scenarios::TemporaryUserFolders`, aldrig i `Dokument\Kickbase` (förut sparade `presettest`
  testfiler bland Tobias presets).

## 6. Gränssnittet

- **Alla vyer som ritas om hela tiden ärver `VisualTimer`** ([VisualTimer.h](../Source/Ui/VisualTimer.h)): `startVisualTimer (grupp, takt)`
  i konstruktorn, `followVisualRate()` först i `timerCallback`, `visualFilled()` för fyllda ytor. Då följer de SETTINGS > OPTIMIZATIONS
  och REDUCE ANIMATIONS direkt. En ny sorts vy får en egen grupp i `Settings::Visual` och en rad i `OptimizationsPanel`.
- **Läs aldrig inställningsfilen i `paint` eller en timer.** `Settings::get` tolkar `settings.xml` varje gång. Värden som läses ofta
  cachas som atomiska tal i Settings.cpp (`reduceAnimations`, `visualFpsStep` …).
- **En dold flik gör ingenting.** Timers kollar `isShowing()`. SETTINGS räknade 4000 filer tio gånger i sekunden fast den var dold:
  en halv processorkärna.
- **Rita bara om det som ändrats** (se `StepSequencer::repaintMarker`): hela rutnätet ritades om för varje markörsteg.
- **Etiketter ska få plats** med 1, 3 och 10 instrument och i Kickbase FX:s lägre fönster. Radbryt eller korta hellre än att klippa.
  Visa inte en halv rad hjälptext.
- **Vyer som hämtar data i timern** visar fel första bilden om konstruktorn inte anropar `timerCallback()` en gång.

## 7. Prestanda

- **Profilera först** (`profile`, `uiprofile`) och rätta det dyraste. Granskningen gjorde ljudtråden ~⅓ snabbare utan att ändra en bit:
  - egen `Biquad` med JUCE:s exakta räkneordning;
  - setters bara vid ändring;
  - cachad `tanh`;
  - monoton kö för limiterns fönsterminimum.
- **Kända kostnader:**
  - oscillatorernas wavetable-läsning (~17 %), reverb och delay;
  - live-spektrumets ritning (därför OPTIMIZATIONS);
  - kontrolltaktens arbete per bit (~14 % i Init med 3 instrument, efter besparingarna).
- **Mät med flera instrument och tunga lägen** (`Scenarios::makeHeavy`), inte bara Init.

## 8. Namn

- **I C++ heter det `instrument`**, inte `bass` (`InstrumentSynth`, `InstrumentField`, `InstrumentGrid`, `numInstrumentLayers` …).
  - "bass" finns kvar i ID:n (`bassN…`), XML-taggar (`BassSample`), mappen `Bass` och `Section::bass`: rör dem inte.
  - "bass" finns också kvar där det betyder basljud (`bassAcid`, `fitbass`).
- **Projektet heter Kickbase:** mål `Kickbase_*`, `KickbaseFX_*`, `KickbaseLookAndFeel`. Repomappen heter än så länge MyOwnFruit
  (fråga 8 i `docs/audit/questions-for-tobias.md`).
- **Gränssnittets texter är på engelska, kommentarer och docs på svenska.**

## 9. Fällor vi gått i (läs innan du skriver kod)

- **JUCE APVTS:** det råa värdet (det ljudtråden läser) uppdateras inte om det nya är `approximatelyEqual` med det gamla, och startvärdet
  är inte avrundat till parameterns steg (`juce_AudioProcessorValueTreeState.cpp:148-156`).
- **JUCE `IIR::Filter::processSample`** anropar `check()` och `getFilterOrder()` för varje sample.
  `IIR::Coefficients::make*` allokerar.
- **JUCE `ComboBox::getSelectedId`** jämför texten: efter `changeItemText` ger den 0. Sätt valet igen från din egen källa.
- **JUCE trådprioritet** på Windows: `low` = `THREAD_PRIORITY_LOWEST`, `background` = `THREAD_PRIORITY_IDLE`.
- **VST3-id:t** räknas ur `PLUGIN_MANUFACTURER_CODE` + `PLUGIN_CODE` (`juce_VST3ModuleInfo.h`, `JUCE_VST3_CAN_REPLACE_VST2=0`),
  inte ur CMake-målets namn. Bevis efter ett namnbyte: `moduleinfo.json` identisk, och `KickbaseHostTest <vst3> state save` med det
  gamla bygget följt av `state load` med det nya.
- **`std::max`/`std::min` i filer som inkluderar `windows.h`** (Settings.cpp): makrona krockar. Använd `juce::jmax`.
- **Medlemsnamn i en basklass** (`VisualTimer`) skuggar lokala variabler i underklasserna (C4458). Ge dem unika namn.
- **Bash heredoc med backslash** förstör C++ och Python (`\n` blir en riktig radbrytning). Skriv skript med Write-verktyget och ändra
  filer med ett Python-skript som bevarar radsluten (CRLF/LF), eller med Edit.
- **Radslut:** git har `autocrlf`; arbetskopian är oftast LF men inte alltid. `sed` på en CRLF-fil lämnar `\r`, och en infogad rad kan få
  fel radslut. Kontrollera med `file`.
- **Filer är låsta medan ninja kompilerar dem** (`sed: cannot rename … busy`): vänta tills bygget är klart.
- **Konsolverktyg måste skapa processorer med `make_unique`**: SignalTaps på flera MB spräcker stacken.
- **Python saknar PIL** här. PNG kan avkodas med `zlib` (filtertyperna 0–4).
- **Mät aldrig under bakgrundslast** och jämför aldrig med gamla siffror (avsnitt 2).

## 10. Arbetsgång för en ny funktion

1. **Läs** README, `docs/DESIGN.md`, `docs/AGENT_GUIDE.md`, rapporten (`docs/audit/audit-report.md`) och den här filen.
2. **Ny gren från `dev`.** Spara baslinjer: `rendersig*`, skärmbilder och `perftest`.
3. **Parametrar först**, med standardvärden som ger dagens ljud. Visa sedan med rendersig* att inget ändrats.
4. **Motorn:** ingen allokering, samma resultat vid alla blockstorlekar (`blocksizetest`), `reset` för NaN-skyddet.
5. **Sparande:** rundtur i `persisttest`, och att gamla projekt och presets laddar oförändrade.
6. **Gränssnittet:** `VisualTimer` för animerade vyer, hjälptexter (`helpcheck` 0), skärmbilder med 1/3/10 instrument och FX.
7. **Dokumentation:** AGENT_GUIDE, AnalysisAbout.h, README och Help.cpp.
8. **Hela sviten, Release-bygge och en kort rapport till Tobias** med vad som ändrats och hur det verifierats. Frågor som rör ljudet går
   till honom innan de görs.
