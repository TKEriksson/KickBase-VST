# PROGRESS-9a (polyfoni steg 1, branch content-poly1)

Status: kod + test klara, ej pushat/mergat.

- Pattern.h/.cpp: Note::extra[3], API stackSize/getStack/addExtra/setStack/removeStack, maxSimultaneous (räknar extras), sparfält `/e/pitch/len/vel/slide` efter ackordet (ackordet paddas till 3 fält). Trasig `/e` hoppas över.
- Uppspelning: InstrumentProcessor.cpp handleEvent (inte StepScheduler). Glide-extra återanvänder röst.
- Test: testPolyphonyStacks i Tests/EngineTests.cpp.
- Baslinje: rendersig/rendersigwide/rendersigtb303 identiska med content-fx1 (0 diff).
- Avvikelse: maxSimultaneous saknades i 14613e5 (a4 har egen oincheckad); min signatur är samma.
- Ingen UI/PianoRoll ändrad.
- Nästa: AN steg 2 (A1-A3 i MixAnalysis).

## LC-UI (branch content-lcui)

- LIVE CODE-fliken: Source/Ui/LiveCodePage.h/.cpp, registrerad i InstrumentEditor och FxEditor (efter AI CHAT). PLAY (debounce 400 ms, LiveCodeRunner), STOP (cancel), CREATE (live_code_create), felrad, AGPL-banner + strudel.cc. Anslutningspunkter: onPatternChanged, onError.
- ab70c7a: Copilot tog bort konfliktmarkörerna i ToolLayer.cpp och committade sidan (bygge + tester gröna enligt Copilot).
- Polyfoni i LC: LiveCode.cpp toPatternArgs staplar toner per tick (första = huvudton, nästa = "stack":true, pitch-dubbletter hoppas över, max Pattern::maxStack); ToolLayer.cpp set_pattern använder Pattern::addExtra för "stack":true. livetest har fall för c2,e2,g2 och kapning; livetest ligger i TemporaryUserFolders-listan (Tools/Tool.cpp).
- Ej provat: PLAY/STOP/CREATE med riktiga musklick (Tobias i Release nr 2). Livetestet klickar knapparna headless (triggerClick, asynkront: kräver meddelandeloop) och alla fall är gröna.
- 20 okt kväll: lcui (6d624c0) mergad i release-test av projektledaren (tillsammans med AN, poly1, poly2 20d9a84). Jag kör ett livetest som klickar PLAY/STOP/CREATE på LiveCodePage (ej committat än, väntar på bygget).

## 2026-10-21 – CI run 12: kbproj-bygget och antest (Arbetare 1)
- 4ea434a: ProjectFile tar `juce::AudioProcessor&` + rotnamn (`apvts.state.getType()`), ingen PluginBase.h (JucePlugin_Name finns bara i pluginmålen). PluginBase.h orörd. Anropen i Tools/StateTests.cpp uppdaterade.
- antest "kick+bas utan kollision: describePhase ger inga fynd" (d1db2ca): orsak (hypotes, ej körd) = fixturen har lika stor kick och bas (båda 0,5), och sedan cde9d61 är basen inom 3 dB av kicken ett riktigt fynd (kick_bass_lead). Regeln är rätt, testunderlaget var gammalt. Basen skalas 0,55 (ca -5 dB; mellan lead-gränsen 3 dB och svans-gränsen 6 dB). FLAGGAT: testunderlag ändrat, inga trösklar. Testen skriver nu ut vilket fynd som uppstår (OBS-rad) om det ändå felar.
- kbproj-UI: Tobias valde layout A (PROJECT-knapp med PopupMenu på AI-knappens plats); byggs efter att kärnan bygger.

## 2026-10-21 - kbproj-UI, layout A (Arbetare 1)
PROJECT-knapp i HeaderBar (Modules.cpp showProjectMenu/saveProject/openProject): Save/Open project, Export, AI server (kryssad rad), statuspunkt i paintOverChildren. KICKBASE_VERSION via CMake även för Modules.cpp. Ej byggd av mig.

## 2026-10-21 - F2 steg 3 (Arbetare 1, content-g)
- fix-slicer-test d254e2d (gren fix-slicer-test, worktree KickBase-VST-fs): persisttest 128-slice-fallet får en 0,8 s WAV. Inga 4a2-krav ändrade.
- sampleMode (BLEND/INDEPENDENT/OFF) sist i InstrumentField (Params.h/.cpp), SAMPLE-gruppen i MatrixPage.cpp:51.
- R9b steg 7, StepSequencer.h/.cpp: `displayOrder` (makeDisplayOrder: kick, varje instrument följt av sina underrader ur RowRegistry), `rowOf` = Pattern::lengthRowOf, rowRects har numRowSlots platser. Underrader ritas som dämpad remsa "INSTRUMENT SAMPLE/SLICER" utan rattar. Timern bygger om raderna när en underrad tillkommer/försvinner.
- AVVIKELSE: underrader kan inte väljas eller redigeras i v1 (targetAt ger Target::none), selectLayer oförändrad. Ej byggd av mig.
- Slicer-delen (SLICER-rad, MAP TO, sampler-flik, state v2) byggs senare från release-int på besked.
