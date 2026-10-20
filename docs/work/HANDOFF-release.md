# HANDOFF: slutför planen till release-bygge (skriven av kickbase-vst-72, 20 okt natt)

## Aktuell status 2026-10-21

- Releasearbete sker lokalt i `KickBase-VST-rel`, branch `release-test`; inget har pushats.
- E (`4f40a85`) är byggt och testat. F1 och Gemini är integrerade. Mergecommits: `112757e` Gemini, `bf71f4d` LCUI/E, `7712a02` F1.
- F1-grenens build och tester passerade. E:s Release-build, tester, renderjämförelser, `blocksizetest` och `audiotest` passerade.
- Integrerad Release-build passerade med exit 0; instrument-/FX-VST3 och testverktyg länkades.
- Integrerade tester passerade: `KickbaseTests`; `livetest`, `persisttest`, `presettest`, `perftest`, `chattest`; `rendersig`, `rendersigreal`, `rendersigwide`, `rendersigtb303`; `blocksizetest`, `audiotest`; samt `KickbaseHostTest` (Init, uppspelning, editor öppna/stänga).
- F1:s parameterträd fick en JS-smoketest för aliases, `.get()`, `.set()`, reserverade namn och get-after-set-skydd. Ingen automatisk end-to-end-interaktion med `.set/.get` i Live Code-UI; Tobias bör prova detta i DAW före push.
- Releaseblockerare hittad: F1:s CREATE-bekräftelse ignorerar `ow` och skickar alltid `overwrite=true` (`Source/Ui/LiveCodePage.cpp:507-512`), fast "Lock these" anropar retry med `false` (`:486`). `ToolLayer.cpp:1073` hoppar just nu över låsta rader före konfliktkontrollen, så vanligt klickflöde skyddar dem; om lås/noter ändras medan bekräftelsen är öppen kan konfliktkontrollen kringgås. Byggvarningen C4100 (`ow` oanvänd) pekade ut felet. Se även `docs/work/LESSONS.md`; fixa och bygg om före Tobias DAW-test.
- Bygget gav en oanvänd-parameter-varning i `Source/Ui/LiveCodePage.cpp:507` och två lokala namnöverskuggningar i `Tools/Tool.cpp:1344`/`:1961`; inga fel.
- Tobias bad att dokumentationen inte längre ska ignoreras. `docs/` är återställd och spårbar; CMake hämtar de tre MCP-guiderna från `Resources/` för ren-klon-build.
- Releasebuild och automatiska tester passerar, men installera inte förrän CREATE-bekräftelsefelet är fixat och ombyggt. AUTO-steget är fortsatt utanför denna release. F2–F4 är också utanför scope.

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

## Nästa steg efter E och F1: AUTO i stället för LIVE (Tobias idé, 20 okt natt)
- Koden körs bara när texten ändrats OCH är giltig; då skapas/uppdateras raderna direkt ("CREATED"). Ogiltig kod = bara felrad, förra giltiga resultatet ligger kvar.
- Tas en kodrad som koden själv äger (`liveCodeOwned`) bort ur koden töms raden. Rör aldrig handgjorda rader. Verifiera först hur owned-rader beter sig idag vid borttagning/stopp (`LiveCodePage.cpp`).
- Handritade toner skyddas fortfarande av `warnOverwrite`. Inget får köras av sig självt vid laddning av projekt/preset (jfr f1e5167).
- LIVE-knappen blir en AUTO-bryterare, på som standard. Klockstyrda saker (`slider`, F1 `.set`) skriver redan bara vid ändring.
- Rör `LiveCodePage` och owned-reglerna som E också rör: gör efter E-merge. Tester: ogiltig kod ändrar inget, borttagen rad töms, handrad kvar, laddning kör inget, Init kraschar inte.
