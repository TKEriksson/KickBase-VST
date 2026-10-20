# PROGRESS-9a (polyfoni steg 1, branch content-poly1)

Status: kod + test klara, ej pushat/mergat.

- Pattern.h/.cpp: Note::extra[3], API stackSize/getStack/addExtra/setStack/removeStack, maxSimultaneous (räknar extras), sparfält `/e/pitch/len/vel/slide` efter ackordet (ackordet paddas till 3 fält). Trasig `/e` hoppas över.
- Uppspelning: InstrumentProcessor.cpp handleEvent (inte StepScheduler). Glide-extra återanvänder röst.
- Test: testPolyphonyStacks i Tests/EngineTests.cpp.
- Baslinje: rendersig/rendersigwide/rendersigtb303 identiska med content-fx1 (0 diff).
- Avvikelse: maxSimultaneous saknades i 14613e5 (a4 har egen oincheckad); min signatur är samma.
- Ingen UI/PianoRoll ändrad.
- Nästa: AN steg 2 (A1-A3 i MixAnalysis).
