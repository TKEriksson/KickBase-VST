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

## Matrix-problemet (branch fix-matrix, -mx, 21 okt)
- Lasning: Matrix (MatrixPage.cpp:337-359) och drag (Components.cpp:313-325) ger identiska ModRoutes::add med samma indexrum (getParameters-index = Modulator.offsets, Modulation.cpp:264/441-451). Ring: KnobComponent::rings (Components.cpp:127-143) generell for alla knobs. LFO1-3 aktiva som standard (Modulation.h:143).
- Rotorsak EJ funnen via lasning. Test skrivet: Tools/Tool.cpp matrixTest (kommando matrixtest): alla modulerbara mal, Matrix-vag vs drag-vag, find/ring/offset/apply + kallval. EJ byggt an (vantar pa PL bygg nu).
- Nasta kon (PL): F2/F3/F4 (branch content-f2 fran release-test efter Matrix-commit), sedan AUTO.

### Matrixtest körd (21 okt)
`KickbaseTool.exe matrixtest`: grönt för alla 1782 modulerbara mål (Matrix-väg = drag-väg: koppling, ring, förskjutning, påverkat värde; 0 avvikelser). Ingen kodbugg reproducerbar i datavägen. Kvarstår: Tobias exakta mål/LFO som "inte gick" (ej känt), UI-specifikt (5 Hz-timer, vilken LFO som valdes, mål utan synlig ratt). Testet behålls som vakt. Ingen fix gjord. Obs: `.set` i CREATE (ToolLayer.cpp ~:960-1000, test Tool.cpp:288 i -rel) ej verifierat av 9a.
Kodläsning UI (PL:s fråga): (1) Matrix-vägen (MatrixPage.cpp:337-359) anropar inte repaint, men behöver inte: varje KnobComponent har en VisualTimer (Components.cpp:83, 30 Hz) som ritar om när `modRoutes.version` ändras (Components.cpp:171-193), och `add` bumpar version. Ringen ritas alltså inom ~33 ms, lika för båda vägarna. (2) rings() (Components.cpp:127-143) hittar ratten via parameterIndex(); mål som finns i Matrix-menyn men saknar synlig KnobComponent (t.ex. dolda/slot-parametrar) får koppling men ingen ring. Inget tydligt UI-fel funnet: **behöver Tobias exakta fall** (vilket mål, vilken sida, vilken LFO).
