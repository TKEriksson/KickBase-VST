# HANDOFF: slutför planen till release-bygge (skriven av kickbase-vst-72, 20 okt natt)

## Aktuell status 2026-10-21

- Releasearbete sker lokalt i `KickBase-VST-rel`, branch `release-test`; inget har pushats.
- E (`4f40a85`) är byggt och testat. F1 och Gemini är integrerade. Mergecommits: `112757e` Gemini, `bf71f4d` LCUI/E, `7712a02` F1.
- F1-grenens build och tester passerade. E:s Release-build, tester, renderjämförelser, `blocksizetest` och `audiotest` passerade.
- Integrerad Release-build passerade med exit 0; instrument-/FX-VST3 och testverktyg länkades.
- Integrerade tester passerade: `KickbaseTests`; `livetest`, `persisttest`, `presettest`, `perftest`, `chattest`; `rendersig`, `rendersigreal`, `rendersigwide`, `rendersigtb303`; `blocksizetest`, `audiotest`; samt `KickbaseHostTest` (Init, uppspelning, editor öppna/stänga).
- F1:s parameterträd fick en JS-smoketest för aliases, `.get()`, `.set()`, reserverade namn och get-after-set-skydd. Ingen automatisk end-to-end-interaktion med `.set/.get` i Live Code-UI; Tobias bör prova detta i DAW före push.
- Releaseblockerare hittad och rättad: F1:s CREATE-callback skickar dialogens `ow`-val vidare till `live_code_create` (`Source/Ui/LiveCodePage.cpp:507-512`), så "Lock these" inte längre blir `overwrite=true`. Release-ombyggnad passerade (`EXIT:0`), C4100-varningen för `ow` är borta, Live Code-regressionstest och HostTest passerade. Dialogknapparna är inte klicktestade automatiskt; Tobias kan verifiera Overwrite/Lock these/Cancel i DAW. Upptäckt och testregel står i `docs/work/LESSONS.md`.
- Tobias handtestade CREATE i DAW med två `$:`-block: `note("c2 [e2 g2]*2").s("i1")` och `note("D2 [G3 b2]*4").s("i5")`. Bekräftelsen visade "Written: instrument 1, instrument 5". Empty hade bara ett instrument; Tobias skapade instrument 5 manuellt efteråt, och då syntes de redan genererade tonerna i pianorullen. CREATE skapar alltså inte instrumentplatser. LFO och sliders är ännu inte DAW-verifierade; `.set()` ska köras via LIVE, inte CREATE.
- Uppföljande DAW-test av tre CREATE-rader passerade: kick (`bd*4`), instrument 1 med `.lfo("bass1Cutoff", 0.25, 0.45, "sine")`, och instrument 5 med `slider()`-gain. CREATE bekräftade "Written: instrument 1, instrument 5, kick"; två inline-sliders syntes i editorn. Detta verifierar att kodraderna körs och att slider-UI:t visas, men inte att cutoff-LFO:n hörs/modulerar eller att sliders påverkar ljudnivån hörbart. Instrument 5 fanns redan vid detta test. `.set()` ska köras via LIVE, inte CREATE.
- Bygget gav en oanvänd-parameter-varning i `Source/Ui/LiveCodePage.cpp:507` och två lokala namnöverskuggningar i `Tools/Tool.cpp:1344`/`:1961`; inga fel.
- Tobias bad att dokumentationen inte längre ska ignoreras. `docs/` är återställd och spårbar; CMake hämtar de tre MCP-guiderna från `Resources/` för ren-klon-build.
- Release-ombyggnaden och riktade regressionstester efter CREATE-rättningen passerade. Den fulla tidigare testsviten passerade före rättningen; ändringen är isolerad till bekräftelsens overwrite-flagga. AUTO-steget är fortsatt utanför denna release. F2–F4 är också utanför scope.

Statusen här är aktuell och ersätter den äldre lägesbilden och merge-checklistan längre ned.

För Copilot eller annan AI som tar över. Läs först: `CLAUDE.md`, `docs/work/PROGRESS.md`, `docs/work/PROGRESS-lc.md`, `docs/work/LESSONS.md`, `docs/plan-v2.md`, `docs/ENGINEERING.md`. Svara på svenska, kort, med `fil:rad`. Ägaren heter Tobias; han vill vara insatt, så förklara orsaken till fel och peka ut de få viktiga raderna.

## Hårda regler
- Pusha eller mergea ALDRIG till main (eller remote) utan Tobias. Allt sker lokalt.
- Inga nycklar/hemligheter i repot. Dokumentationen under `docs/` är spårad i releasegrenen enligt Tobias beslut.
- Init-preset får aldrig krascha. Baslinjer (rendersig/rendersigreal) ska ge 0 diff.
- Nya parametrar och modulationskällor läggs SIST i listorna (`docs/ENGINEERING.md`).
- En build i taget (`tasklist | findstr /i "cl.exe link.exe ninja"` ska vara tomt innan du bygger). Bygg bara när det behövs.
- Lägg inte till `installRel.bat` / `installRel.ps1` i git (otrackade med flit).
- Commit-meddelanden slutar med rad: `Co-Authored-By: <din modell>`.

## Worktrees (alla under C:\Users\hunga\Documents\GitHub\)
| Mapp | Branch | Innehåll |
|---|---|---|
| KickBase-VST | main | huvudrepo |
| KickBase-VST-lcui | content-lcui | Live Code A–E + sliders |
| KickBase-VST-lc | content-gemini | Gemini-chatt: modeller, kostnadsläge, sessioner (b2ad056). Klar enligt Arbetare 1 |
| KickBase-VST-f | content-f | F1 `.set/.get` på parametrar, commit d58a0a0 (från eb67ddd) |
| KickBase-VST-rel | release-test | sammanslagen release, bygger Release-VST3 |

## Läge före överlämningen (historiskt; se aktuell status högst upp)
- content-lcui eb67ddd: byggd + testad (livetest rc=0 inkl. 4 slider-fall, persisttest ok, Init-check ok).
- E (`.lfo`/`.env` från kod, nya modulationskällor "CODE 1–8" SIST, 74→82, routes ≥74 sparas aldrig) kodas av Arbetare 2 i -lcui. Kolla `git -C KickBase-VST-lcui log` om den committats. Granska innan merge.
- F1 (d58a0a0 i -f): motor klar (`Source/Core/LiveCode.cpp` HostObject, `Resources/strudel/glue.js` + samma svans i `strudel.all.js`: `buildParams`), sida klar (`Source/Ui/LiveCodePage.cpp` `applyParamWrites`). OBYGGD/OTESTAD vid skrivande; bygge i `KickBase-VST-f\build\Release` startades via `GitHub\f-build.bat`, logg `GitHub\f-build.log` (slutar med `EXIT <kod>`). Saknas i F1: tak ~30 skrivningar/s, livetest-fall (set, get, get-efter-set = felrad, okänt namn, låst instrument hoppas över, unika/giltiga namn över alla `list_parameters`-id, träd-roten får inte skugga Strudel-ord), persisttest (inget kodägt sparas), AI/MCP-verktygsbeskrivning + `docs/AGENT_GUIDE.md`-exempel.
- F2–F4 (lås, effekter, mätare) är designade i `KickBase-VST-lcui/docs/work/lc-f-design.md` men EJ kodade. Hoppa över dem i denna release; F1+E räcker.

## Historisk releaseplan (mergerna är genomförda; se aktuell status högst upp)
1. Verifiera F1-bygget: `tail GitHub\f-build.log`. Åtgärda ev. kompileringsfel (mest troligt: `LiveCodePage.cpp`, `ToolLayer.cpp`, `LiveCode.cpp`). Kör KickbaseTests, livetest, persisttest, Init-check (HostTest på Kickbase.vst3; exakta kommandon: se `docs/work/LESSONS.md`, `build.bat`, PROGRESS-filerna). Commit i -f.
2. Granska och verifiera E (när committad i -lcui). Granska även 6b:s Gemini-commit: `git -C KickBase-VST-lc show b2ad056`, särskilt `redact()` (nycklar/`key=`/headers/Bearer får aldrig hamna i export/logg) och `loadSession`.
3. Slå ihop i KickBase-VST-rel (branch release-test, lokalt): `git merge content-gemini`, `git merge content-lcui`, sist `git merge content-f`. Förväntade konflikter F↔E i `Resources/strudel/glue.js` (+ `strudel.all.js` ska spegla glue.js: bygg om/lägg till svansen), `Source/Core/LiveCodeRunner.h` (`Done`-signaturen: list, error, writes; E lägger eget) och `Source/Ui/LiveCodePage.cpp` (`runNow`). Lös så att båda funktionerna finns kvar.
4. Bygg Release i KickBase-VST-rel (vcvars64: `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat`, sedan `cmake --build build\Release`). Kör KickbaseTests, livetest, chattest, rendersig/rendersigreal (0 diff), Init-check.
5. Be Tobias köra `installRel.bat` (kräver admin, stäng FL Studio först; kopierar till `C:\Program Files\Common Files\VST3`) och scanna om plugins i FL Studio.
6. Pusha/mergea till main först när Tobias testat Live Code och Gemini med riktig nyckel och sagt ja.

## Kända öppna frågor till Tobias
- Städa gamla worktrees: -test, -refbb, -live, -poly1.

## Uppdatera efter jobbet
`docs/work/PROGRESS-lc.md` och `docs/plan-v2.md` med: content-f/F1, b2ad056, E-commit, release-bygget.

## ~~Nästa steg efter E och F1: AUTO i stället för LIVE~~ (STRUKET 22 okt: Tobias föredrar namnet LIVE; knappen behålls som den är)
- Koden körs bara när texten ändrats OCH är giltig; då skapas/uppdateras raderna direkt ("CREATED"). Ogiltig kod = bara felrad, förra giltiga resultatet ligger kvar.
- Tas en kodrad som koden själv äger (`liveCodeOwned`) bort ur koden töms raden. Rör aldrig handgjorda rader. Verifiera först hur owned-rader beter sig idag vid borttagning/stopp (`LiveCodePage.cpp`).
- Handritade toner skyddas fortfarande av `warnOverwrite`. Inget får köras av sig självt vid laddning av projekt/preset (jfr f1e5167).
- LIVE-knappen blir en AUTO-bryterare, på som standard. Klockstyrda saker (`slider`, F1 `.set`) skriver redan bara vid ändring.
- Rör `LiveCodePage` och owned-reglerna som E också rör: gör efter E-merge. Tester: ogiltig kod ändrar inget, borttagen rad töms, handrad kvar, laddning kör inget, Init kraschar inte.


## Release 22 okt kväll (Copilot)
- CI av 96e066c grön (bygge Release exit 0, alla tester exit 0, HostTest ok, baselines oförändrade: Init 94f0a1b6bd8a3c7f, instr1 f7139084f716c127, heavy 6a3e65e696d14d10, tb303 03fbeb761547e195). Logg: GitHub/ci-log-96e066c.txt.
- an-master mergad till main (--no-ff, a109d31) och pushad. Koden är identisk med den testade.
- Release-artefakter: KickBase-VST-ci\build\Release (Kickbase.vst3, Kickbase FX.vst3, KickbaseMcp). installRel.ps1 pekar nu på -ci (otrackad, committa inte).
- Kvar: Tobias kör installRel.bat och testar i FL Studio (MIX-fliken, AP/START, HALFTIME, KEEP/COMPARE, shimmer, Maximizer MODE, Low Multiplier, autocomplete Ctrl+Space och efter '.'). Ej hörtestat/sett än.
- Valfritt senare: ta bort /STACK:8388608 (CMakeLists.txt:243), städa -f2/-lc/-lce/-lcui/-mx/-rel, AGENT_GUIDE ('15 types').


## NY STATUS 2026-10-22 kväll (Copilot, ersätter äldre läge ovan)

- Arbetssätt: kod på `an-master` i `KickBase-VST-ai`; CI och releasebygge i `KickBase-VST-ci` (`ci-run.ps1`, `ci-build.bat`); docs i main. Merge till main med `--no-ff` efter grön CI.
- Gjort efter Tobias FL-test: pianorulle (loop, sökmarkör, audition, ram, egen knapp i PREVIEWS, Space/Shift+Space, mittenklick-panorering, zoom via rullningslisternas ändar), MIX (WIDTH-rattar, dB-mätare, vågformszoom), LOCK-rad och FLOAT (alla knappar, resize), SHOW IN SPECTRUM, SAMPLE-etiketter. Detaljer: `PROGRESS-copilot-2026-10-22.md` punkt 1-20.
- Inget av detta är provat i UI av mig; Tobias testar efter `installRel.bat`.
- PLAN / kvar att göra: (1) pianoroll i eget fönster (POP OUT), (2) GHOST NOTES, (3) sample-slicern enligt `slicer-ui-spec.md`, (4) ta bort `/STACK:8388608` (`CMakeLists.txt:243`), (5) städa gamla worktrees (`-f2`, `-lc`, `-lce`, `-lcui`, `-mx`, `-rel`) först när Tobias bekräftat, (6) AGENT_GUIDE-texten ("15 types").
- Regler gäller som förut: nya parametrar sist, helpcheck kräver hjälptext, inga nycklar, ingen `installRel.*` i git.

- **TILL SPECIALISTEN:** oförklarad uppspelningsbugg (loop på första takten, ej stoppbar, rullknapp som dök upp sent; löst av omstart av VST). Se PROGRESS-copilot-2026-10-22.md punkt 24-25.
