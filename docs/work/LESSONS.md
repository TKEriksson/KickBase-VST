# Lärdomar och testregler (läs innan du bygger eller testar)

Lägg till en rad direkt när ett fel upptäcks (av dig, Opus eller Tobias): vad som gick fel, och regeln som hindrar det. Kort, en rad.

## Testregler (testa inte i onödan, varje körning kostar)
- Under arbetet: bygg bara det mål du behöver, kör bara det test som rör det du ändrade.
- Kör inte om ett test som redan gått grönt om ingen kod det täcker har ändrats sedan dess.
- Vid rött test: läs utdata, rätta, kör om bara det testet. Inte hela sviten.
- rendersig-trion + blocksizetest + audiotest: en gång per delsteg före commit, och bara om ljudtrådskod ändrats (Synth, Modulation, Pattern-läsning, Fx, processBlock).
- persisttest / KickbaseHostTest: bara om tillstånd/sparning ändrats.
- UI-ändring utan ljudkod: bygg + titta på en skärmbild av just den vyn. Inga ljudtester.
- Hela sviten, 42-bildssviten och pngdiff: bara vid fasens slut, en gång.
- Ingen Release-build om inte Opus ber om det.

## Kostnadsregler (20 okt: 68 % av förbrukningen skedde vid >150k kontext)
- Filtrera utdata från bygge och test: visa bara fel och PASS/FAIL-rader (`| tail -20` eller `| grep -E "error|FAIL|PASS"`), aldrig hela loggen.
- Läs filer i delar (offset/limit eller grep). Läs inte om en fil som redan finns i kontexten.
- Rapporter till Opus: korta, med hash, testresultat och de frågor som behöver svar. Inga diffar i meddelandet, Opus läser själv.
- En fas eller ett stort delsteg per chatt. Uppdatera PROGRESS.md och avsluta med "compact-redo" i stället för att fortsätta på lång kontext.
- En arbetare åt gången när förbrukningen är hög. Två bara när Opus säger det.
- Bilder: målet är minst tokens med säkrad kvalitet, inget bildförbud. Räcker en textkontroll (get_state, state-dump, grep, pngdiff-resultat) tas ingen bild; kan bara en bild visa att layouten stämmer tas en. Öppna aldrig en bild som pngdiff säger är oförändrad. Vid fasslut: 42-bildssviten + pngdiff, titta bara på bilder som skiljer, högst 3.

## Lärdomar
- R7: atomic lästes två gånger i samma uppdatering, vilket gav fashopp. Läs varje atomic en gång till en lokal variabel.
- R1–R3 (HoldAudition): `callAfterDelay` höll en referens till processorn, som kunde ha hunnit försvinna. Använd en Timer-medlem eller SafePointer. Gäller alla asynkrona callbacks (callAfterDelay, showAsync, showMenuAsync, enterModalState): fånga en SafePointer till en komponent och avbryt om den är null, fånga aldrig bara `&processor`. (Upprepades i R9 LengthMenu.)
- R1: längden byggdes som 1, 2, 4 … 64 fast spec sa fri längd utan tak. Läs Tobias spec-filer och planens avsnitt helt innan du bygger, och fråga Opus om de säger emot varandra.
- Shell-patchar: `\n` i heredoc eller `python -c` blir riktiga radbrytningar. Använd Edit/Write.
- Hemligheter: `.env-local` och API-nycklar får aldrig checkas in (repot är publikt). Kör en hemlighetskoll före push.
- R9: när maxBars höjdes till 1024 fyllde loopar `bar < Pattern::maxBars` (Presets, kick-FILL, gate via MCP) 1024 takter och AUTO-längden blev 1024. Sök alla `maxBars`/`maxTicks`-loopar när en gräns ändras; fyll bara de takter innehållet har.
- Bygge: cmake finns inte i bash. Använd build-mcp.bat via PowerShell (`cmd /c`), filtrera på `error|TARGET-BUILD-DONE`.
- Bildjämförelse: fönsterstorleken ändras mellan sessioner. Bygg tippen (basgrenen) i samma miljö och ta nya bilder som "före"; jämför aldrig mot gamla bildmappar.
- rendersigwide/tb303 skiljer sig efter R9 för att makeHeavy-mönstret ändrats (innehållet loopar nu per takt), inte för att ljudmotorn ändrats (bevisat: kvarstår med d2-Modulation). Skilj mönsterändring från motorändring innan baslinjen görs om.
- docs/audit/params-*.txt är föråldrade (från före fas R). Jämför parameterlistan mot senaste bygget (t.ex. basgrenens), inte mot de filerna.
- MCP-verktygets tester: lägg aldrig en InstrumentProcessor på stacken (stackoverflow, exit 0xC00000FD). Använd std::make_unique som de andra testerna.
- Vid byte från tick-ID till `NoteKey(tick, index)` i UI: migrera alla hover/drag-, rit-, urvals- och inputflöden innan bygge; kvarvarande tick-anrop skapar många följdfel. Börja felsökningen vid de första kompilatorfelen.
- PianoRoll NoteKey: en felriktad patchhunk lämnade `continue` utanför en loop (C2044). Efter partiella patchar, granska den ändrade funktionen innan nästa bygge; första riktade kompileringen fångade felet innan testerna kördes.
- F1 Live Code CREATE: `warnOverwrite` skickar `false` för "Lock these" (`LiveCodePage.cpp:486`), men callbacken ignorerade värdet och skickade alltid `overwrite=true` (`LiveCodePage.cpp:507-512`; C4100-varningen pekade ut oanvänd `ow`). Rättat genom att skicka `ow` till `live_code_create`. `applyLiveCode` hoppar över låsta rader (`ToolLayer.cpp:1073`); vid ändrat lås-/notläge medan dialogen är öppen ska bekräftelsens konfliktkontroll ändå gälla. Bygg om och testa Overwrite, Lock these och Cancel separat.
- AI-chatt CONTINUE-test (22 okt): mockservern måste hållas igång genom både max_tokens-svaret och fortsättningsanropet; att stoppa den efter första frågan gav falska CONTINUE-fel och lämnade chatten i retry. Kontrollera test-fixture-livscykeln innan du drar slutsatser av asynkrona tester.
- Arbetare 1 (27 okt): ny testkod som använder `StepScheduler` missade `#include` i `EngineTests.cpp` (C3878/C2059 vid första bygget). Lägg include först. `build.bat` körs via PowerShell: `& cmd.exe /c "<sökväg>\build.bat" Debug`.
- Projektledaren (2026-10-27): klippte masterplan.md med `head -n 53` och förlorade fas F/G, städning, arbetsfördelning och frågor. Klipp aldrig en fil med head/redirect; gör riktade Edit-ändringar och läs filen efteråt. Fas G fick rekonstrueras ur minnet, kontrollera den mot HANDOFF-release.md.
- Arbetare 1 (27 okt): `\n` i C++-strängar patchade via python/shell-heredoc blev riktiga radbrytningar i filen (två gånger, undotest). Använd Edit-verktyget för text med backslash. Orsak: skalet/pythons strängtolkning gör om `\n` till radbrytning innan filen skrivs, så C++-strängen delas över två rader (C2146). Undvik: skriv aldrig C++ med backslash via heredoc/`python -c`; Read filen och använd Edit. Kontrollera efteråt med Grep efter strängar som slutar utan citationstecken.
- Arbetare 1 (27 okt): Edit utan föregående Read i samma kontext avvisas (hände StepSequencer.h, PluginBase.h två gånger, undo-spec.md). Read filen först, även vid en liten ändring.
- Arbetare 1 (27 okt): JUCE APVTS skriver parametervärdet till trädet först vid flush (timer ≤500 ms eller `copyState()`), och en saknad "value"-egenskap skrivs UTAN undo. Följd: första gesten ångrades inte och `clearHistory` gav ett tomt extrasteg. Åtgärd: förifyll "value" i konstruktorn, kör `apvts.copyState()` före varje transaktionsgräns och i `clearHistory`. Läs `juce_AudioProcessorValueTreeState.cpp` (`flushToTree`) innan man bygger undo runt APVTS.
- Arbetare 1 (27 okt): heltalsparametern `patternBars` valdes av misstag som testparameter i undotest och gav falskt rött; den styrs av processorn. Välj en vanlig float-knob i parametertester.
- Arbetare 1 (27 okt): `shortName` i Effects.cpp saknade poster för shimmer/lowMultiplier (indexering utanför arrayen). Lägg alltid till namn, kortnamn och knapptabell tillsammans när en EffectType läggs till. Audiotest-felet (4312 allokeringar, `AudioTests.cpp:94`) är äldre än FX-stegen; jämför mot ren HEAD (git stash + bygg) innan du skyller på sin egen ändring.
- W1 27 okt (slarv): använde skyddade `params` från UI-kod (använd `getRawParameterValue`), tog const-pekare till `indexOf`, skrev fel filväg från minnet (`Tools/AudioTests.cpp`, inte `Source/Tests`), körde spårningen före bygget var klart, Edit utan föregående Read, startade bakgrundsbygge medan jag fortfarande redigerade. Regel: läs filen först, vänta på bygget, sök upp sökvägen.
- Debug-allokeringar i audiotest kan komma från MSVC:s debug-iteratorer (`std::string` till `std::map`-nyckel) eller från `jassert` som loggar. Det är inte Release-fel; fixa med statiska tabeller eller guards.
- AudioBuffer::clear (a, b) är (startSample, numSamples), inte (kanal, antal): `clear (1, numSamples)` började på sample 1 och gav jassert + allokeringar i Debug vid block på 1 sample (`InstrumentProcessor.cpp` finishBlock). Läs signaturen i JUCE innan ett anrop skrivs; spåra audiotest med `KB_ALLOC_TRACE=1` (Arbetare 1, 27 okt).
