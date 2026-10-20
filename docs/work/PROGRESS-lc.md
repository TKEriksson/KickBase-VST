# PROGRESS-lc (projektledare kickbase-vst-72, tidigare 49)

## Releaseöverlämning 2026-10-21
- E-commit `4f40a85` är integrerad i `release-test` via LCUI-merge `bf71f4d`; Gemini (`112757e`) och F1 (`7712a02`) är också mergade lokalt.
- E:s Release-build, enhetstester, livetest, persisttest, HostTest, renderjämförelser, `blocksizetest` och `audiotest` passerade i LCUI-worktree.
- Den integrerade releasebyggningen i `KickBase-VST-rel` passerade med exit 0 och producerade instrument-/FX-VST3 + testverktyg.
- Releasegrenens `KickbaseTests`, Live Code, persistens, preset, prestanda, chatt, fyra renderkontroller, blockstorlek, audiotest och HostTest (Init/editor) passerade. F1-parameterträdet klarade JS-smoketest; interaktivt `.set/.get` i DAW återstår för Tobias.
- Releaseblockerare: CREATE-bekräftelsen ignorerar `ow` och skickar alltid `overwrite=true` (`LiveCodePage.cpp:507-512`), trots att "Lock these" skickar `false` (`:486`). Nuvarande låsfilter i `ToolLayer.cpp:1073` skyddar låsta rader normalt, men lås-/notändring medan dialogen är öppen kan kringgå konfliktkontrollen. Byggvarningen C4100 pekade ut den oanvända parametern. Se `docs/work/LESSONS.md`; behöver rättas och byggas om före DAW-installation.
- `docs/` återställdes och gitignoren togs bort enligt Tobias. CMake/MCP-guidekällor ligger i `Resources/`.
- Inget pushat. VST-installationstest pausas tills blockeraren är fixad; AUTO och F2–F4 är utanför release.

## Sessioner (namn ändras vid omstart, adressera via ListAgents)
- 72 = projektledare. 40 = "arbetare 2" (Live Code, worktree KickBase-VST-lcui, branch content-lcui). 6b = "arbetare 1" (Gemini, worktree KickBase-VST-lc, branch content-gemini från main 64928e6).

## Läge 20 okt natt
- LC-motorn (Strudel 1.2.6 i QuickJS) klar. LIVE CODE-flik i content-lcui: A 59f5202 (`$:`/const-block, FEL-flik, LIVE av/på), B 43a7a40, C f1da542, D d13c209 (LC-kod + locks sparas i Full-presets/state, `<LiveCode>` sist i state; LIVE/CODE ONLY/owned sparas aldrig). Granskade och godkända.
- Empty-preset: ddf0d8e (release-test).
- Poly1 a1dce99, poly2 20d9a84. release-test = AN + poly1 + lcui + poly2. Inget pushat/mergat till main; L/S/K och content-grenar väntar på Tobias test.
- Release-VST3 byggd i KickBase-VST-rel (har Empty, inte B–D). Tobias installerar med `installRel.bat` (+ `.ps1`, otrackade i main, lägg inte till utan Tobias). FL Studio läser `C:\Program Files\Common Files\VST3`; FL stängd, admin.
- Stack (flera rader) fungerar enligt Tobias.

## Pågår
- 40: load-while-LIVE-fix (ej byggd: `liveCodeLoads` atomic, `checkLoaded()` stoppar LIVE vid presetladdning) → väntar "bygg nu". Sedan inline-sliders (`host.slider`, egen commit, kodar utan bygge).
- 40: E-design (`.LFO()`/`.ENV()` från kod; modulationskällor sist enligt docs/ENGINEERING.md, inga parametrar mitt i listan, baslinjer 0 diff; ~15 rader: filer, risker, tester). Finns INTE än. Jag granskar före E-kod. Uppdatera även docs/work/lc-f-design.md.
- 6b: Gemini del 1 (dynamisk models.list, probe, redigerbar modell, default gemini-3.6-flash, felmeddelanden, ✓/✗) byggs i -lc. Därefter egna commits: kostnadsläge (Sparsam/Normal/Rik + varning före render_preview/analyze_audio), sessioner (spara chatt, auto-trim, export före trim, nyckel aldrig i export/logg). Använd `?key=` konsekvent.
- Bygg: en i taget, bara på mitt "bygg nu" (`Get-Process cl,link,ninja`).

## Nästa release (Tobias beslut 20 okt natt)
- Bygg ny release när BÅDA arbetarna är klara: 40 (load-while-LIVE-fix, sliders) och 6b (del 1, kostnadsläge, sessioner/spara chatt). Vänta på alla tre Gemini-stegen.
- Plan: granska sista commits → merga content-gemini + content-lcui i release-test (KickBase-VST-rel, lokalt, ingen push) → bygg Release → KickbaseTests, livetest, chattest, init-check → Tobias kör installRel.bat.

## Att fråga Tobias
- Flytta docs/AGENT_GUIDE.md + psytrance-kick-bass-reference.md till Resources/? (CMakeLists.txt:138 läser dem, docs/ är gitignorerat → ren klon kan inte bygga).
- Ny releasebygge med B/C/D (+ sliders)?
- Merge först efter att han testat L/S och Gemini med riktig nyckel (KEY → GEMINI → GET A FREE KEY).

## Kända kantfall
- SOLO på redan ägt instrument skiljs inte från CODE ONLY (LiveCodePage.cpp:211).

## Idéer (Tobias)
- AI-chatt för nya användare: knapp "Skaffa gratis med guide" → leverantörens nyckelsida, tre steg, inklistringsruta, testa nyckeln direkt. Ingen delad nyckel i repot/binären. Proxy med gränser senare. Kolla aktuella gratisvillkor.

## Kvar
- F-design KLAR (72, 20 okt natt): `KickBase-VST-lcui/docs/work/lc-f-design.md`, tillägg längst ned. Kodas efter E, F1→F2→F3→F4, en commit per steg.
- F: mätarlista, `instr1.lock()`, `.set/.get`, `fx.slotN.setEffect`. G: flytande Live Code-fönster, KONSOL/flikar/notmarkering.
- LC senare: MCP live_code_run/stop, AGPL-banderoll + THIRD-PARTY.md, helpcheck 21 poster (LFO 9/10), AN visuell koll KICK+BASS/COLLISIONS.
- Init-check + rendersig mot a1dce99-referens. Tobias handtestar B/C/D, Empty i nya builds, Gemini.
- Städa worktrees -test, -rel, -refbb när de inte behövs. Docs-historik-omskrivning: ej beslutat.

## Nästa steg efter E och F1: AUTO i stället för LIVE (Tobias idé, 20 okt natt)
- Koden körs bara när texten ändrats OCH är giltig; då skapas/uppdateras raderna direkt ("CREATED"). Ogiltig kod = bara felrad, förra giltiga resultatet ligger kvar.
- Tas en kodrad som koden själv äger (`liveCodeOwned`) bort ur koden töms raden. Rör aldrig handgjorda rader. Verifiera först hur owned-rader beter sig idag vid borttagning/stopp (`LiveCodePage.cpp`).
- Handritade toner skyddas fortfarande av `warnOverwrite`. Inget får köras av sig självt vid laddning av projekt/preset (jfr f1e5167).
- LIVE-knappen blir en AUTO-bryterare, på som standard. Klockstyrda saker (`slider`, F1 `.set`) skriver redan bara vid ändring.
- Rör `LiveCodePage` och owned-reglerna som E också rör: gör efter E-merge. Tester: ogiltig kod ändrar inget, borttagen rad töms, handrad kvar, laddning kör inget, Init kraschar inte.
