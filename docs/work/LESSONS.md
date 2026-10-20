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
