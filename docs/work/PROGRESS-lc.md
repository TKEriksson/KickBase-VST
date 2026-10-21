# PROGRESS-lc (projektledare kickbase-vst-72, tidigare 49)

## Releaseöverlämning 2026-10-21
- E-commit `4f40a85` är integrerad i `release-test` via LCUI-merge `bf71f4d`; Gemini (`112757e`) och F1 (`7712a02`) är också mergade lokalt.
- E:s Release-build, enhetstester, livetest, persisttest, HostTest, renderjämförelser, `blocksizetest` och `audiotest` passerade i LCUI-worktree.
- Den integrerade releasebyggningen i `KickBase-VST-rel` passerade med exit 0 och producerade instrument-/FX-VST3 + testverktyg.
- Releasegrenens `KickbaseTests`, Live Code, persistens, preset, prestanda, chatt, fyra renderkontroller, blockstorlek, audiotest och HostTest (Init/editor) passerade. F1-parameterträdet klarade JS-smoketest; interaktivt `.set/.get` i DAW återstår för Tobias.
- Releaseblockerare: CREATE-callbacken skickar nu `ow` vidare i stället för alltid `overwrite=true` (`LiveCodePage.cpp:507-512`). Release-ombyggnad (`EXIT:0`), Live Code-test och HostTest passerade; C4100-varningen för `ow` är borta. Dialogens tre val bör Tobias handtesta i DAW. Bakgrund i `docs/work/LESSONS.md`.
- Tobias handtestade CREATE i DAW med två `$:`-block (`note("c2 [e2 g2]*2").s("i1")` och `note("D2 [G3 b2]*4").s("i5")`); UI:t bekräftade "Written: instrument 1, instrument 5". Empty hade bara ett instrument, så Tobias skapade instrument 5 manuellt efteråt; då visades dess redan genererade toner i pianorullen. CREATE skapar inte instrumentplatser. LFO och sliders är ännu inte DAW-verifierade; `.set()` hör till LIVE, inte CREATE.
- Uppföljande CREATE-test körde kick, instrument 1 med cutoff-LFO och instrument 5 med `slider()`-gain. Bekräftelsen listade instrument 1, instrument 5 och kick; två sliders syntes. Kod/slider-UI är verifierat, men inte hörbart LFO-/nivåbeteende. `.set()` hör till LIVE, inte CREATE.
- `docs/` återställdes och gitignoren togs bort enligt Tobias. CMake/MCP-guidekällor ligger i `Resources/`.
- Inget pushat. Tobias kan prova VST3 lokalt och handtesta dialogens tre val; AUTO och F2–F4 är utanför release.

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

- 21 okt: F1 `.set/.get` i CREATE + LIVE klart i release-test (tak 30 skriv/s, lås hoppas över, "Did you mean"-fel, blankrads-regel i guide/verktygsbeskrivning). Release byggd; KickbaseTests, livetest, chattest, persisttest och Init-check (HostTest) gröna. Rendersig ej körd (ingen ljudtrådskod).
- Gemini ogiltig-JSON: tillåten igen (misstanke buildRequest AiProvider.cpp:298). Ollama hoppas över tills vidare (Tobias). Kö: AGPL-banner + THIRD-PARTY.md + helpcheck, MCP live_code_run/stop, Gemini-fix.

- [content-docs, 21 okt] MCP live_code_run/stop (b751a7e), glue .set (9d4d4d4), AGPL/THIRD-PARTY (ce77236) byggda och gröna. BLOCKERAT: `git merge content-f2` nekades av auto-läget (behörighet) – behöver Tobias/PL att köra mergen eller ge tillåtelse. Gemini: enstaka live-test gav 503 (hög belastning), orsak ej bekräftad live.
- Gemini "invalid JSON" (content-docs 50f81b3, ej byggd): orsak = hypotes. Gemini 3 skickar functionCall.id som kastades och inte returnerades i functionResponse (AiProvider.cpp). Ett live-test (3.5-flash-lite) gav 200 på grundformatet; 3.6-flash gav 503. Chattest för id-rundresan tillagd. Väntar på bygge (Arbetare 2 bygger).
- BLOCKERAD, behöver Tobias: `git merge content-f2` nekades av behörighetsläget. Hoppad tills han svarar; release-test fast-forward väntar på grönt bygge.
- BLOCKERAD, behöver Tobias: fast-forward av release-test till content-docs (50f81b3, grönt bygge) nekades av behörighetsläget (git branch -f). Kommando: git branch -f release-test content-docs (om release-test inte är utcheckad).
- Ny kö (PL 21 okt): (1) G2 KONSOL på gren content-g från content-docs (console.log-kö i glue.js/strudel.all.js, liten konsolyta i LiveCodePage, små commits; Arbetare 2 rör samma fil för F4/AUTO); test i livetest. (2) G3 notmarkering om hap.context.locations nås. (3) G1 flytande fönster sist. (4) Om blockerad/klar: S3 UNISON/Reese per OSC (plan-v2, nya parametrar sist, rendersig 0 diff, Init ej krasch). Bygg bara på "bygg nu". Väntar på Tobias: release-test fast-forward + content-f2-merge.

## Arbetare 1, 21 okt: G2 + G3 klara (lokala commits, ej mergade)
- G2 KONSOL: livetest grön (bibliotekets startloggar rensas efter bundle-laddning, LiveCode.cpp load()).
- G3 notmarkering: JS 8075eb3 + C++/UI (LiveCode::Mark, LiveCodeRunner::getMarks, LiveCodePage MarkOverlay). Alla tester gröna: Tests, livetest, chattest, persisttest, helpcheck, HostTest. Ej visuellt granskat i editorn (Tobias sover).
- Kvar: G1 flytande fönster, sedan S3 UNISON/Reese.
- G1 flytande fönster: BLOCKERAD (öppna beslut i lc-g-design.md: överlägg eller DocumentWindow, `document` ut ur LiveCodePage som Arbetare 2 äger). Hoppad.
- S3 UNISON/Reese finns redan i koden (Params/InstrumentSynth/OscPanel), AN och FX1 finns. Nästa: FX2 (B8 widener, B9 dynamic EQ + flanger, B10 transient shaper), nya effekttyper sist i typlistan.

## Arbetare 1, 21 okt: FX2 del 1 (kod klar, EJ byggd)
- Widener, Flanger, Transient tillagda sist i EffectType (Effects.h:19, Effects.cpp knob-tabell + WidenerFx/FlangerFx/TransientFx, Slot), Help.cpp, test sist i instrumenttestet (Tool.cpp). Commit 6350f47.
- Väntar på PL "bygg nu": b-gem, KickbaseTests, KickbaseTool livetest/helpcheck/persisttest (typindex!), HostTest.
- Kvar i FX2: dynamic EQ, transient-oktav ner/upp, per-band ducking.
