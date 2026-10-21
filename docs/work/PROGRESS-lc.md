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

Arbetare 1, 21 okt: FX2 del 1 byggd och testad (KickbaseTests, instrumenttest, livetest, helpcheck, persisttest, hosttest gröna). rendersig-hashar efter FX2 (ingen tidigare baslinje sparad; ändringen är additiv, nya typer sist): instr1 f7139084f716c127, instr2 1d01add63f4afc92, instr3 4f53eb8dad4f7db5. Kvar: dynamic EQ, transient oktav ner/upp, ducking per band.

rendersig-baslinje 21 okt (efter FX2 del 1, commit 93f0005). Jämför framtida körningar mot dessa:
[rendersig]
full 0 Init                        94f0a1b6bd8a3c7f peak 0.9661 rms 0.3273
full 1 Basic Morph                 f7d9d390f481771d peak 0.6526 rms 0.2540
full 2 Empty                       7d67e7bb66bb432b peak 0.0000 rms 0.0000
kick 0 Full-On Punch               7886462dba2f318d peak 0.9661 rms 0.3841
kick 1 Dark Forest Boom            c048b8d40cdd519c peak 0.9661 rms 0.4917
kick 2 Progressive Round           a24d2319b7127d4a peak 0.9661 rms 0.3870
kick 3 Hi-Tech Click               6f34a4ee92738e1a peak 0.9661 rms 0.3678
kick 4 Zenonesque Punch            557ccdee0b8bc5a8 peak 0.9661 rms 0.4061
kick 5 Hard Psy                    cb379f897e702437 peak 0.9661 rms 0.4027
instr 0 Rolling Classic            838a280fa442b7f2 peak 0.9661 rms 0.3148
instr 1 Full-On Pluck              f7139084f716c127 peak 0.9661 rms 0.3161
instr 2 Dark Forest                1d01add63f4afc92 peak 0.9661 rms 0.3051
instr 3 Hi-Tech Zap                4f53eb8dad4f7db5 peak 0.9661 rms 0.2884
instr 4 Acid Squelch               e31d7e0f60e0ef33 peak 0.9661 rms 0.3209
instr 5 Wide Reese                 2ae1036747b6becb peak 0.9661 rms 0.3151
instr 6 Sub Sine                   03228c9b40d3441b peak 0.9661 rms 0.3090
[rendersigwide]
heavy instr 10 @48k/512      6a3e65e696d14d10 peak 0.9661 rms 0.2694  (ny 21 okt: Scenarios.h:160 fördelar effekttyper med % (numEffectTypes - 2); FX2 la till 3 typer så kanal 6-11 fick andra effekter. Ingen ljudregression)
heavy instr 3 @44.1k/128     2816863f43a25e6e peak 0.9661 rms 0.2438
heavy instr 1 @96k/1024      f7fd75b8484528d0 peak 0.9661 rms 0.2158
heavy instr 4 @48k/37        2ec6b99ec3839ac7 peak 0.9661 rms 0.2416
heavy fx @48k/512            32f867e32e4553e8 peak 0.9661 rms 0.2114
heavy fx @44.1k/64           8351baeb46687930 peak 0.9661 rms 0.2111
[rendersigtb303]
tb303 instr 10 @48k/512      03fbeb761547e195 peak 0.9661 rms 0.2578  (ny 21 okt, samma orsak som heavy instr 10)
tb303 instr 3 @44.1k/128     71e075e5a23b3ea9 peak 0.9661 rms 0.2145
tb303 instr 1 @96k/1024      fe53e05eee3a4108 peak 0.9661 rms 0.2080
tb303 instr 4 @48k/37        4fa127f582bc0dc8 peak 0.9661 rms 0.2146

Arbetare 1, 21 okt (senare): FX2 komplett i kod, ej byggt (PL bygger i ci). content-g: aaf6e9e Dynamic EQ, 1d7e0ae Octaver (oktav ner/upp, TRANSIENT-gate), därefter Band Ducker (LR4-split, LOW/HIGH DUCK). Alla nya typer sist i listan. Nya instrumenttest-checkar för var och en. rendersig förväntas 0 diff mot baslinjen ovan.

Arbetare 1, 21 okt: AN lektionsvarningar klara (e44fad8, RAND-fas på bas + 200 Hz). INTE gjort: "kick vinner där kick+bas möts" och "svans tar bas" - Capture-datan räcker inte; tas upp när den gör det. Nästa: S3 UNISON/Reese per OSC.

- 21 okt: kbproj-spec.md skriven (docs/ ignorerad, ligger lokalt, ej committad). Matrix-bugg: CODE-källor saknas i MatrixPage sourceMenu (16-42); instr.1 POSITION-orsak ej funnen statiskt, kräver riktig UI-test (Arbetare 2:s filer).

- 21 okt: AN: AGENT_GUIDE (c77b865) och helptexter (70478fc) klara. RAND-fyndet behåller kategori "phase" (orsaken är fasen); ingen ändring. Kvar: Init-check, ev. suggested_fixes i REPORT toText.

- 21 okt: AN Init-check: ny `inittest` (Tool.cpp, commit 4553263): fasmått på tom insignal, mixanalys + toText + JSON på kod-Init och inbakat Init, ändliga fynd. Ej byggd av mig. suggested_fixes i REPORT toText hoppas över (fyndens detail-text har redan samma råd). Rendersig 0 diff: inga ljudändringar (bara test, text, hjälp).
- Notis: plan-v2.md:337 säger att CMake läser docs/AGENT_GUIDE.md; CMakeLists.txt:141 läser Resources/. docs/AGENT_GUIDE.md är spårad men avviker (föråldrad kopia).
- 21 okt: E0 klar (read-only): docs/work/E0-report.md. plan-v2.md:337 rättad. Viktigast: Resources/AGENT_GUIDE.md saknar Live Code .set-avsnittet (finns bara i docs/AGENT_GUIDE.md:27-35); namn/färg/previewNote följer inte Full-preset; ingen allt-på-round-trip.
- 21 okt: Guiderna ihopslagna (d4de6db, Live Code .set/.get i Resources/AGENT_GUIDE.md). E0 punkt 5 (MCP/skill/toText) i E0-report.md; skillen behöver peka på get_guide (PL beslutar).
- 21 okt: `fulltest` (11c0248, content-g): allt-på-round-trip projekt + Full-preset i Tools/StateTests.cpp (fullStateTest). Ej byggd/körd av mig. Namn/färg/previewNote i Full-preset är avstängda bakom `strictKnownPresetGaps=false` (rapporteras som LUCKA). Init-element och version-höjning ej testade/ändrade: Tobias beslut.

- 21 okt: skillen pekar på get_guide (9923f25). E0-beslut.md skriven (öppna beslut för Tobias). F2-granskning klar: docs/work/f2-review-A1.md (rek: R9b dynamiska rader före F2). Väntar på CI-resultat från PL.

## 21 okt, A1: R9b-spec + antest
- docs/work/r9b-spec.md skriven (3 frågor till Tobias: 64 rader/nyckel i3.sample, MIDI-export av underrader, version 2).
- antest: Tools/AnTests.cpp, commit fbc86b5 på content-g. Ej byggd/körd av A1; PL kör `tool antest`. OBS-rader skrivs för tveksamma regler (lera saknar absolut golv).

- fas23-gap.md skriven (A1, 21 okt): skill tunn, compare/sweep/score saknas, brygga (fas 4) finns redan.

## Arbetare 1, content-g: fas 2–3 (21 okt)
- f8036c4: skillen med loop-recept och stoppvillkor (ingen C++).
- 1a23df9: `rendertooltest` (StateTests.cpp renderToolTest, Tool.cpp dispatch): render_preview deterministisk via verktygslagret. Ej körd av mig.
- e11d8e4: `compare_renders` (ToolLayer.cpp compareRenders, minns render_preview per `name`), tre kontroller i rendertooltest. Ej byggd.
- Kvar i gap-listan: sweep_parameter, score_vs_reference, effect-fält i list_parameters.

- 1e8946a sweep_parameter (+test i rendertooltest), 89a5be8 SKILL-rad, c52bdf6 score_vs_reference (+test, AGENT_GUIDE), f5d2504 SKILL-rad. Ej byggt/kört av A1.

- 555c245: guide/skill rättade (kind=kick vid score_vs_reference, namn+exempel). docs/work/f2-sampler-spec.md skriven (v1, frågor längst ned).
- 21 okt: f23a1ae antest (tyst makeCapture gav 'Nothing plays' i evaluate, gmax < -75; stereo/limiter/lera fick spelande källa). 6ac4563 rendertooltest: score-testet mäter hela filen (seconds 60) och kräver lika många termer. Väntar på PL:s bygge (antest, rendertooltest).
- 21 okt, A2-granskning verktygslagret (ingen kodändring): (1) bass1Cutoff:250 är rätt, set_parameters tar egna enheter (Hz), ToolLayer.cpp:201. (2) sweep-namnschemat sweep_<param>_<n> är dokumenterat i verktygsbeskrivningen (:237) och sätts i :1807; testet följer det, ingen ändring. (3) Tools/ParamSweepTests.cpp finns inte på content-g, bara på content-s2; punkten måste göras där (kandidatlista: första som skiljer sig, annars '1 - standard', annars steg+1).

## Arbetare 1, sammanfattning sedan 20 okt (skriven 21 okt på projektledarens begäran)
Alla commits på content-g, byggda och körda av projektledaren (CI), aldrig av mig. Status per commit nedan = vad jag vet; körning 5 på 6ac4563 pågår.
- fbc86b5 antest (syntetiska AN-regeltester). Körning 3: 6 FEL, orsak i testet, rättat i f23a1ae.
- 9923f25, f8036c4, 89a5be8, f5d2504: skillen (get_guide, loop-recept, sweep, score). Ingen C++.
- 1a23df9 rendertooltest (determinism), e11d8e4 compare_renders, 1e8946a sweep_parameter, c52bdf6 score_vs_reference, alla med test i rendertooltest. Körning 3: gröna.
- 555c245 guide/skill: kind=kick, namngivna renders. Bara docs.
- f23a1ae antest: tyst makeCapture gav "Nothing plays" i evaluate (gmax < -75); stereo-, limiter- och lerafallen fick en spelande källa. TESTLOGIK ÄNDRAD, flaggad till projektledaren för Tobias. Produktkoden orörd.
- 6ac4563 rendertooltest score-test: seconds 60 (hela filen), kräver lika många termer. TESTLOGIK ÄNDRAD, flaggad. Följer arbetare 2:s granskning punkt 1–2.
- A2-granskning, övriga punkter: ingen kodändring (bass1Cutoff-enheten och sweep-namnschemat var rätt); ParamSweepTests.cpp ligger på content-s2, tas av arbetare 2.
- Dokument: fas23-gap.md, f2-sampler-spec.md (v1, filreferenser rättade).
- Väntar på: körning 5 på 6ac4563 (antest, rendertooltest, inittest, fulltest, rendersig, helpcheck) och Tobias svar på de 7 frågorna i f2-sampler-spec.md.

## Arbetare 1: AN-trösklar efter A2:s granskning (21 okt)
Tillämpat (commit se nedan, ej byggt, PL kör antest):
- Lera (MixAnalysis.cpp, evaluate): golv `low > 1e-4 * frames` (medeleffekt -40 dB i 40-150 Hz i mastern) före kvoten 0,6. Beteendeändring bara för nästan tyst master.
- Gemensamma konstanter (anonym namnrymd överst): phaseMinActive 4, phaseCorrWarn -0,3, phaseCorrProblem -0,6, phaseSumWarnDb -3, phaseSumProblemDb -6, lowStereoCorrMin 0,3. Används i fynden och i verktygsvägen (suggested_fixes); samma värden som förut, ingen beteendeändring.
- TESTLOGIK ÄNDRAD (flagga till Tobias): AnTests.cpp lera-faint-fallet var en `note()` ("regeln har inget absolut golv"), är nu `check (! ...)` med en spelande kick-källa. Övriga fall orörda (positiva lera-fall ligger på -10 dB, långt över golvet).
Förslag, ej gjorda (produktbeslut för Tobias):
- Limiter (:587): percentil (t.ex. 95:e över kickträffarna) eller andel av tiden över 3 dB i stället för enstaka maxblock; behåll max som siffra i detaljen. Trösklarna 5/9 dB är gissningar.
- Fasregel: `activeCount >= max(4, 0,15 * platser)` i stället för absolut 4; energiviktat medelvärde; "aktiv" över -40 dB i stället för -60 dB (kicksvans räknas idag som möte); kontrollera hur sumVsMaxDb räknas (-3/-6 dB).
- Stereo: bara band 0 (0-120 Hz, inkl. DC); tyst band räknas som 1,0 först under 1e-9; en enda takt räcker; förslag golv ~1e-5, minst 2 takter eller 25 %, nivåer info <0,8 / varning <0,5 / problem <0.
- Lera: mät på kick+bas-spåren i stället för mastern (pad/lead räknas idag); bandbredd 110 vs 90 Hz (effekt, inte täthet); 0,6 är en gissning och saturerad bas har övertoner i 160-250 Hz.

Commit: ddaadc2 (content-g, ovanpå 6ac4563). Kör: antest, inittest, rendersig.

## Arbetare 1: de fyra AN-förslagen genomförda (21 okt, Tobias: "kör på det"; ej byggt, PL kör CI)
Ett commit per steg på content-g ovanpå ddaadc2:
1. 4639a40 Limiter: percentil över kickträffar i stället för enstaka maxblock.
2. 09cb3d4 Fas: relativ aktiv-gräns (max(4, 15 % av platserna)) och energiviktat medelvärde (PhaseMetrics.lowWeight sist). TESTVILLKOR ÄNDRAT: "fyra hörbara av 32" ger inget fynd (golv 5 av 32).
3. 447c58a Stereo: golv 1e-5, band från 20 Hz, minst 2 takter eller 25 %, nivåer info <0,8 / varning <0,5 / problem <0. TESTVILLKOR ÄNDRAT: gamla tröskeln 0,3 ersatt av nivåerna.
4. 2984fcf Lera: mäts på kick + instrument med minst hälften av energin under 150 Hz, inte mastern. TESTVILLKOR ÄNDRAT: AnTests lera-fall och Tool.cpp setMasterMud (nu på kicken). Nya fall: bas med, pad ej, master ej.
Kör: antest, inittest, rendersig, tooltest. Rendersig ska vara oförändrad (bara analys).

## Arbetare 1: R9b steg 1 (RowRegistry)
- Vad: `Source/Core/RowRegistry.h` (fast pool om 64 atomer, id <-> nyckel `i3.sample`/`i3.slicer`/`kick`, pool full ⇒ -1, instrument raderas ⇒ underrader följer) + `rowkeytest` (`Tools/RowTests.cpp`, Tool.cpp, CMakeLists). Rent tillägg, ingen ändring i ljud/state.
- Vem: Arbetare 1, content-g. Hash: se `git log` (R9b steg 1). Kör: `rowkeytest`, plus bygget.
- Nästa: steg 2 Pattern-arrayer till 64 rader (rendersig 0 diff).
- Från PL (21 okt): kbproj D1 = XML, D3 = UI vid AI-server-knappen (föreslå Project/Verktyg-meny vs knappar i kbproj-spec.md, bygg inte UI), D2 embed, D4 nej. AN: "kick vinner där kick+bas möts" och "svans tar bas" ska med som varnings-/tipsfynd; utred vad Capture saknar (nivå per takt/band separat för kick-svans och bas), utöka additivt, tester. Efter R9b-steget.

## Arbetare 1: R9b steg 2 (Pattern till 64 radplatser)
- Vad: `Pattern::numRowSlots = 64` (= RowRegistry::maxRows) för noteBlocks, lengthOf/contentOf/manualOf/extraOf och custom; `clampedLayer`, `isCustom/setCustom`, `clearNotes`, konstruktorn och `refreshLengths` går över alla platser; `Pattern::rows` (RowRegistry) som medlem; static_assert att index stämmer. `numNoteLayers` (11) är kvar som "grundrader" så alla gamla loopar, sparning och UI beter sig som förut. Underrader har ingen gate (gateBlocks är kvar på 10 rader, redan klampad) – beslut att ta i F2.
- Minnet: noteBlocks växer från 88 KB till ca 512 KB i Pattern (en per plugininstans, heap).
- Förväntat: rendersig oförändrad (inga underrader finns), alla gamla tester. Ej byggt av mig.
- Nästa: steg 3 (Params/radparametrar), 4 (spara/ladda `row`-attribut), 5 (ToolLayer/MCP/Live Code 64 bit), 6 (MIDI-export), 7 (UI).

## kbproj-spec uppdaterad med Tobias beslut (Arbetare 1, 21 okt)
- Vad: `docs/work/kbproj-spec.md`: D1 XML, D2 bädda in, D4 inte v1, D3 UI vid AI-knappen; layoutförslag A (meny "PROJECT" med Save/Open/AI-läge, statusprick) / B (två knappar) / C (menyrad), rek. A. Bara docs, inget UI byggt.
- Av: Arbetare 1. Resultat: se hash i rapport till projektledaren.

## AN: "kick vinner" och "svans tar bas" (Arbetare 1)
- Vad: PhaseMetrics fick lowKickDb/lowBassDb/lowKickHit (per 16-del, 30-150 Hz); summariseKickBass i MixAnalysis.cpp; findings i evaluate (kategori kickbass: varning "kicken vinner inte", tips "svans tar bas") och i describePhase (kick_bass_lead, kick_tail_takes_bass). Trösklar: 3 dB på slag, 6 dB mellan slag, minst 2 16-delar och 25 %, aktiv över -50 dB.
- Tolkning: "svans tar bas" = kicksvansen dominerar basen mellan slagen (tips, bekräftat av projektledaren).
- Tester: nytt block i Tools/AnTests.cpp. Ej byggt av mig; projektledaren kör antest och rendersig (ska vara oförändrad, mätningen läser bara).

## Arbetare 1: R9b steg 3 (radlängder/loop för underrader)
- Vad: `PatternLengths` rymmer 64 platser (`numLengthRows`); plats 0-10 oförändrad (0 = kick, 1-10 = instrument, parametrarna rowLoop/rowBars), underrader använder sitt radid (11-63) via `Pattern::lengthRowOf`. `Pattern::loopsRow/setLoopsRow` (default true) bär loop för underrader, eftersom inga nya host-parametrar skapas (ID:n och ordning oförändrade, så presets och rendersig påverkas inte). `readPatternLengthsWith` tar med aktiva underrader (tom pool = ingen skillnad). Sparas i steg 4.
- Av: Arbetare 1, content-g. Ej byggt av mig. Kör: bygget, `rowkeytest`, `rendersig` (oförändrad), `persisttest`/`fulltest`.
