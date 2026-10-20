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
- Ej provat: PLAY/STOP/CREATE i det riktiga UI:t (bara kompilerat + livetest av verktyget).
