# Masterplan (v1, projektledare: kickbase-vst-56, 2026-10-27)

STATUS: GODKÄND av specialisten (kickbase-vst-b5) med ändringarna nedan inarbetade. Arbete kan delas ut.

Bygger på: plan-v2.md (faser), HANDOFF-release.md, PROGRESS-copilot-2026-10-22.md, slicer-ui-spec.md, LESSONS.md, ENGINEERING.md och Tobias testlista `research/TEST2026-10-23/Testar appen 2027-10-23.txt` (T1-T15 nedan).
Regler (oförändrade): Claude skriver all C++; varje agent egen worktree + egen `PROGRESS-*.md`; ingen push utan Tobias; bara projektledaren mergar till main, när allt är testat; Opus (specialisten) bara när Sonnet inte räcker; nya parametrar och modulationskällor sist; testregler enligt LESSONS.md (rätt delmängd, ingen onödig svit); slarvfel loggas i LESSONS.md.

## Arbetssätt (Tobias 27 okt, gäller tills han ändrar det)
- Projektledaren delar ut arbete, arbetarna jobbar i egen worktree och för egen `PROGRESS-*.md` löpande. Specialisten (Opus) används sparsamt, bara efter projektledarens bedömning.
- Arbetarna bygger Debug och kör minimala, relevanta tester. Inga onödiga tester eller skärmbilder.
- Tobias gör alla FL-tester först när releasen är byggd, inte löpande under fas A. Arbetarna fortsätter därför utan att vänta på FL-test; obekräftade orsaker (t.ex. T13) samlas till release-testet.
- Releaser görs bara när en stor sektion är klar. Första: hela testlistan T1-T15 (fas A-E, H).
- Release: Release-bygge, Tobias testar i FL (kör `installRel.bat`), fel rättas, sedan påminner projektledaren Tobias om att merga till main som release och frågar innan. Först efter hans ja mergar projektledaren. Aldrig push utan Tobias.
- Projektledaren committar docs och masterplan när något ändrats. Copilot pushar/mergar bara om Tobias uttryckligen säger det.
- Alla läser `docs/work/LESSONS.md` innan de kodar, bygger och testar.
- Slarvfel loggas i `docs/work/LESSONS.md`. Uppdatera progressfilen före sessionens slut.
- Svenska, korta svar, rollnamn (Arbetare 1/2, specialisten, projektledaren).

## Tobias testlista (T-nummer)
| T | Ämne | Typ |
|---|------|-----|
| T1 | Dropdowns: fokus + piltangenter, Enter i multiselect | UX |
| T2 | Preview per flik (motor / instrument / pattern / kick), previewknapp längst till höger | Funktion |
| T3 | Meny: New Project, Ctrl+S, projektnamn i rubrik, senaste 3, döp om PROJECT, EXPORT in i menyn | UX |
| T4 | BPM-justering, jog-wheel/scratch, takt-scroll, dubbelklick-takt, LOOP on/off (stoppar DAW) | Funktion, stor |
| T5 | Perch-mejl (troligen reklam) | Verifierat: buggen är äkta, se A5 |
| T6 | Combined wave: zoom, panorera, tid/takter, helbredd; samma i Pattern-fliken (toggle) | UI |
| T7 | Live Code: autocomplete på engelska med exempel, `.chord`, `$:`, förslag i parametrar, editor-tema, autoformat | Stor |
| T8 | Design: tabbar + kontroller/knobbar/diagram enligt `kickbase_tab_navigation_hierarchy_skiss.html` | Design |
| T9 | Glide i pianoroll: gäller samtidiga toner, ritbara streck | Funktion + bugg |
| T10 | KRITISK: synthen spelar bara C5 (THE MOON) | Bugg |
| T11 | Kick pitch-env: visa tonnamn vid punkter, även för AI | UI |
| T12 | Highlighta spelande noter i pianorollen | UI |
| T13 | BUGG: pattern-längd vid olika spårlängder + Loop | Bugg |
| T14 | Undo/redo (Ctrl+Z, Ctrl+Shift+Z/Y), grupperat per gest | Funktion, stor |
| T15 | Nya presets: 10 clean-psytrance-kickar, 6 rolling-bass | Innehåll |

## Fas A: Buggar först
UPPSPELNINGSPAKETET = A1 + A2 + A3, ETT block för EN person: Arbetare 1. Klocka/fönsterkod: `StepScheduler::Window`, `updatePreviewWindow`, `handlePreviewRequest`, `TransportClock`.
Specialistens granskningspunkter: (a) när orsaken står i PROGRESS, före fixen (A1, A2); (b) när diffen är klar, före merge (A3). Kör Arbetare 1 fast i A2 tar specialisten över analysen.
- A1 (T10) C5-buggen. Trolig orsak (kodkontrollerad): pianorullens MIDI IN står default på INSTRUMENT (`PianoRoll.cpp:98`). När rullen för instr 1 visas sätts `midiInLayer` (`PianoRoll.cpp:224`), och då spelar `handleLiveMidi` värdens ton live på radens ljud i stället för att starta mönstret (`InstrumentProcessor.cpp:540-579, 995-996`). Omladdning av preset stänger rullen -> `midiInLayer = -1` (`PianoRoll.cpp:245`), därför "fixar" det. BESLUT (Tobias, via specialisten): när värden spelar startar värdens noter alltid mönstret; pianorullens MIDI IN (INSTRUMENT, live-spel på en rad) gäller bara när värden står still. `handleLiveMidi` får alltså inte kapa noten när `hostIsPlaying`.
- A2 (T13) Pattern-längd/Loop. Ledtråd från bilderna: kickens LENGTH står på AUTO "follow the notes (2 bars)" fast toner ligger i takt 3. Mönstret blir 3 takter, indikatorn säger 2, loopen 1 takt. Misstanke: AUTO-längden räknas olika i UI och scheduler, eller modulo tas på fel längd i en läsare (bugg 4). Börja med test (kick 2 takter, instr 1 takt, utöka kick till 4, Loop 1-2). Förväntat: total längd = längsta spåret eller manuell längd; kortare spår loopar internt; Loop håller sig strikt till intervallet. Orsaken skrivs i PROGRESS före fixen.
- A3 Enbartaktsloopen i FL: verifiera `docs/work/patch-preview-freerun-untested.diff` (`setForceFreeRun`) i bygge. Specialisten granskar före merge. Hänger ihop med A2.
- A4 (T9, buggdelen) Glide satt i samma takt som tonen gäller inte. Ska gälla alla samtidigt klingande toner. Före glide-ritverktyget (D1). Arbetare 1.
- A5 (T5) AiMemory: `appendWorkLesson` uppdaterar på plats (`AiMemory.cpp:333-334`) så en uppdaterad äldsta lektion kan raderas av `pruneToLimit` (rad 74) trots "saved". Fix: ta bort gamla posten och lägg den sist (som `appendFeedback`, rad 263-267). Test: fyll nära taket, uppdatera äldsta med längre recovery, kontrollera att den finns kvar. Logga i LESSONS. Arbetare 2, uppvärmning (isolerad kod).

FL-tester som krävs innan paketet mergas (Tobias/PL kör i FL):
- (a) Instr 1:s pianoroll öppen, 4 takters C5 från FL: mönstret ska spela, inte en sinuston.
- (b) Kick 2 takter, instr 1 1 takt, förläng kick till 4: totallängd och indikator rätt, takt 3-4 har alla 4 kickslag.
- (c) Loop 1-2 i både free-run och värduppspelning.
- (d) FL i PAT-läge med 1-takts pattern och ALL-preview: hela loopen spelar (A3).
- (e) Normal värduppspelning följer fortfarande FL-positionen.
- (f) Init-kontrollen i LESSONS: ladda Init, spela, byt instrument, spara och ladda.

## Fas B: Obligatoriskt UX
- B1 (T2) Preview per flik + previewknapp (högerjusterad). Arbetare 1, direkt efter paketet (rör `handlePreviewRequest`, `previewLayer`, `midiInLayer`, `InstrumentProcessor.cpp:540-579, 993-999`). Regler: motorflik = bara motorn; INSTR/FILTER/MIX = hela instrumentet utan patterns; PATTERN, Analyzer, AI-chat, Live Code = hela pattern; KICK = bara kicken. MIDI-tangentbord triggar detta.
- B2 (T3) Meny: byt namn, New Project (med spara-fråga), Ctrl+S med synlig bekräftelse, projektnamn i rubrik, senaste 3, EXPORT in i menyn. Arbetare 2.
- B3 (T12, T11) Highlight av spelande noter; tonnamn vid pitch-env-punkter + samma info i AI-tillståndet (`get_state`). Arbetare 1.
- B4 (T1) Dropdown-fokus och piltangenter, enligt pianorollens fokusmönster. Arbetare 2.
- B5 Småposter: HeaderBar-timern för `rollShown` triggar inte alltid så LISTEN PIANO ROLL-knappen kommer sent (PROGRESS-copilot p25). Arbetare 1.

## Fas C: Undo/redo (T14)
Arkitektur (specialisten): EN gemensam `juce::UndoManager` i PluginBase.
- Parametrar: skicka in i APVTS-konstruktorn (`PluginBase.h:239`) och anropa `beginNewTransaction` vid gestens start (mouseDown på knob/slider).
- Pattern: `UndoableAction` med före/efter-ögonblicksbild av berörda rader, via befintligt edit-API. En gest = en transaktion.
- Live Code-körning och AI-ändring: en transaktion vardera.
- Presetladdning (BESLUT, Tobias): att ladda huvudpresetet tömmer historiken (`clearUndoHistory`); att ladda ett del-preset (kick, instrument, motor osv.) är EN ångerbar transaktion. Specialisten tar med detta i `undo-spec.md`.
- Allt på meddelandetråden, ljudtråden oförändrad.
Specialisten skriver `docs/work/undo-spec.md` när fasen startar. Implementeras efter paketet och B, före POP OUT, slicer och glide-ritning. Pianorullen först, sedan parametrar. Arbetare 1.

## Fas D: Funktioner
- D1 (T9) Glide-streck: högerklick-dra ritar streck till de toner glide gäller; går att sätta före noten; MIDI-export approximeras med varning. Arbetare 1.
- D2 (T6) Combined wave: zoom, panorering (mittknapp/scrollbars), tid i ms + takter, helbredd; Pattern-fliken får toggle för projektets combined wave. Arbetare 2. Rör PatternTab/Editor-layout: samordna med Arbetare 1.
- D3 (T4) Transport. OMTOLKAD (Tobias OK 2026-10-27): en VST3 kan i praktiken inte styra värdens transport eller tempo (`AudioPlayHead::canControlTransport`/`transportPlay` stöds inte av FL; ett plugin kan inte sätta tempo). Därför:
  - Kickbases egen BPM, LOOP och takturval gäller bara i free-run/preview.
  - När värden spelar följer vi FL; LOOP visas inaktiv med tooltip "styrs av DAW".
  - Verifiera mot JUCE 8.0.15-dokumentationen: https://docs.juce.com/master/classjuce_1_1AudioPlayHead.html
  - Takt-hover-scroll och dubbelklick på takt (skriv in takt) gäller i free-run.
  - Takt-hover-scroll: musen över takten + scrollhjul spolar; musiken pausas tills man släpper. Dubbelklick: skriv in startakt.
  - D3a transportbasen. D3b jog-wheel/scratch (Tobias: väntar tills transporten är stabil): jog-wheel/LP i takt-rutan, miniatyr-LP-knapp bredvid restart öppnar dialog, två lägen (uppspelning framåt stannar / fortsätter), automation + LFO-koppling, scratch funkar oavsett DAW, automationen ska kunna tas bort och synas tydligt. Arbetare 1, efter A/B/C.
- D4 (T7) Live Code i steg: (1) engelska hjälptexter utan åäö, med exempel för `notes("` (hur man sätter ut noter, samtidiga noter/ackord, vad `* 4` gör m.m.), förslag vid `s("` (instr1 …) och `$:` på tom rad, (2) `.chord(CHORD, BASETONE, START, LENGTH)`, `$:` och parameterförslag, (3) editor (VS Code/Atom Material-tema, färgkodning, autoformat-knapp som lägger kedjan på egna rader och flyttar `s("instr")` först, aktivt instrument markerat med egen färg), (4) arrayer i `.notes()` ger flera röster. Arbetare 2.
- D5 POP OUT (pianorullen i eget fönster). Arbetare 1 (äger pianorullen). Efter undo.
- D6 GHOST NOTES i pianorullen. Arbetare 1.

- D7 (T15) Presets: 10 nya clean-psytrance-kickar och 6 rolling-bass-instrument. Använd skill kickbase-sound-design. Arbetare 2 (efter E1 eller när hen har lucka); lätt att ta i `content-*`-spåret.

## Fas E: Design och slicer
- E1 (T8) Designpass efter funktionsfaserna, helst ren LookAndFeel-ändring så layouten inte görs om två gånger, enligt skissen: tabbar först, sedan knobbar, kontroller, diagram. Arbetare 2. Snapshotsviten (`snapshots.ps1` + `pngdiff.py`) räcker som test.
- E2 Slicer-UI enligt `slicer-ui-spec.md`, steg 1-4. Arbetare 2. `fix-slicer-test` och `fix-matrix` mergas först.

## Fas F: Oinmergade grenar
Grenarna L, S och K finns inte. Enligt `git branch --no-merged main` i `-ai`:
- `content-d1`: 1 docs-commit.
- `content-ph`: 1 commit, MSVC-fix.
- `content-s2`: 5 commits, "ej byggt": måste byggas först.
- `fix-matrix`: 1 commit.
- `fix-slicer-test`: 1 commit (StateTests 4a).
Projektledaren bedömer och mergar efter test. T15 (presets) är D7.

## Fas H: Städning efter Copilot-granskningen (prio efter A och B)
Specialistens granskning (PROGRESS-specialist-2026-10-27.md): inga allvarliga fel, bara småsaker.
- H1 (C4, verifiera tidigt): Maximizer har MODE som knob 5 (Effects.cpp:506). Migreringen sätter inget standardvärde (Effects.cpp:528, PluginBase.cpp:1317), så gammal knob5 = 1.0 kan ge Smooth i stället för Classic. Test: ladda v2-projekt med maximizer och knob5 = 1.0.
- H2 (C3): full noteAuditionFifo (32 platser, PluginBase.h) tappar note-off och tonen fastnar. Fix: all-notes-off för raden när rullen stängs. Koordinera med A1 (audition med rowOverride >= 0 får inte blockeras när värden spelar).
- H3 (C5): InstrumentSynth.cpp:440-447 skriver fördröjningslinjen bara när width > 1, vilket ger klick när WIDTH dras upp från 1. Fix: skriv alltid line[pos] = x.
- H4 (C1/C2, kosmetiskt): ihopklistrad kommentar Params.h:120, indrag Params.cpp:421-437, `}    clock.beginBlock` på en rad i InstrumentProcessor.cpp:145.
- H5 (C6): fas F-grenarna har commits märkta "(ej byggt)" (ad768d0, 4dad3e2, d254e2d). Måste byggas och testas före merge.
- Ägare: Arbetare 2 (H1, H3, H4, H5); Arbetare 1 (H2, efter A1).

## Fas I: Resten av gamla planen (`plan-v2.md`), efter A-H
Klart och på main: R, M, P, L, LC (A-D, F1, FLOAT, autocomplete, Strudel-genvägar), Gemini, AN, MIX-flik, start offset, Low Multiplier, Shimmer, Maximizer läge 2, HALFTIME, pianorullens scrub/loop/audition/zoom. Ordning och detaljer: `plan-v2.md`.
Kvar, i grov ordning (projektledaren bedömer före varje fas; Opus bara efter hans bedömning):
- S: filter på/av per källa, brustyper, WARP ENV-källa, OSC-visning och OSC-presets (status oklart, kolla mot koden först).
- KB (kunskapsbas/skill, delvis), K (kick-verktyg, oklart), PH (fasverktyg, `content-ph`), S2/FM (`content-s2`, se fas F), S3 (unison/Reese).
- F2: sampler och slicer-UI (se E2).
- FX: Echo, Flanger, riktig Sidechain/Maximizer i slots, multiband, FX2; sidechain i två band (LOW och MID-HI).
- AI-leverantörer: Anthropic, OpenAI, Ollama. C: multisampling. BZ: Bézier-kick. E0/E: presetgranskning.
- IO: import/export, projekt, autosave.
- LC-rester: `.LFO()`/`.ENV()`, MCP `live_code_run`/`stop`, AGPL-banner och `THIRD-PARTY.md`, helpcheck.
- RING (Tobias 27 okt): ny WARP-läge "RING (B)" där OSC 2 multipliceras med OSC 1, på samma sätt som FM (B). Läggs sist i enum/val (ID:n byts inte). AM mot en egen sinus finns redan som WARP-läge och ringmodulator finns som FX. Granular och additiv tas inte med nu (additiv täcks delvis av harmonieditorn), och 3x FM är struket. Tillägg (Tobias 27 okt): det ska synas tydligt i UI (etikett/tooltip, t.ex. "OSC1 x OSC2") att det är detta som händer, och multiplikationen ska kunna väljas fritt mellan OSC 2 och OSC 3 (båda eller en av dem). Nya val/parametrar sist. (RING på OSC 2/3 övervägdes och avstods 27 okt: multiplikationen är symmetrisk, vinsten för liten.)
- JOG-mål och flera JOGS: STRUKET (Tobias 27 okt: överflödigt). Kvar är D3b med en gemensam jog (hjul, REC-läge, LP-design).
- Separata VST:er (VS) är struket. Tobias lägger in det själv när allt är klart.

Rapportering (Tobias 27 okt): arbetarna jobbar i långa pass och rapporterar inte förrän de är klara med sin lista, eller när det är helt nödvändigt (blockerare, beslut), eller när Tobias frågar. Specialisten är stängd tills en granskning behövs (fas C: `undo-spec.md`). Frågor till Tobias ställs alltid via projektledaren, aldrig direkt (Tobias sköter all kommunikation via projektledaren). Väntar en fråga eller blockerare på svar går arbetaren vidare med nästa punkt i listan och återkommer till den när svaret kommer. Detta sparar tokens.

## Fas G: Release
Releaser görs bara när stora sektioner är klara, för att hålla nere test och tokens (Tobias 27 okt). Första releasen: när allt från Tobias testlista (`Testar appen 2027-10-23.txt`, T1-T15, fas A-E och H) är fixat och testat i FL. Projektledaren mergar till main först då: efter Release-bygget, Tobias FL-test och eventuella rättelser. Projektledaren påminner Tobias och frågar innan mergen (Tobias 27 okt).
Tobias kör `installAll.bat [VST-config] [MCP-config]` (stäng DAW:en först, annars låser den .vst3) för att ersätta VST3-filerna och KickbaseMcp.exe efter ett Release-bygge. Hörtest i FL som fortfarande saknas: MIX-fliken, AP/START, HALFTIME, KEEP/COMPARE, shimmer, Maximizer MODE, Low Multiplier, autocomplete.
Efter varje release skriver projektledaren en PDF (publik) som sammanställer allt som ingår i releasen och allt som ska testas i FL och godkännas (Tobias 27 okt). Den ska vara läsbar utan internt sammanhang: inga rollnamn, interna filsökvägar, nycklar eller sloppy-loggar. Sparas i `docs/releases/`.

## Nästa release (rel-0.3, Tobias 27 okt kväll)
Arbetare 1: JOG (SCRUB/SPEED, RESET, broms A), ett enda uppspelningsläge (en aktiv källa, flikbyte stoppar bara previews), nav-polish (y-marginal, glasdisplay, breadcrumb med instrument och flik, MASTER VOL), preset-kategorier + tangentbordsfokus, kick SYNTH/SAMPLE MIX, TOP DECAY upp till kickens längd, FX multiselect, GHOST NOTES/FOLLOW/instrumentfärg, pop-out, glide-plus, SOUNDSTAGE. Därtill (Tobias): S3 Unison/Reese, multisampling, sample-editor (AMP/PITCH, på/av), klick-lager (3 st) och sampleverktyg, reverse per not, parameterlanes under pianorollen (resizebar). Plan först för sample-editor, klick-lager, reverse och parameterlanes.
Arbetare 2: slutför pågående presets (Psy Clean-kickar, PSY ROLLING BASS, PSY ROLLING, GLIDE BASSES, New Init 1-5), därefter programbygge: S3 Unison/Reese och multisampling (Tobias 27 okt). Resterande presets (SYNTH/MID BASS/HI BASS och genrer, max 3 per kategori) tas efter rel-0.3. Arbetare 1 har UI-delen, och därtill (Tobias 27 okt) förbättrad övertonsredigerare för wavetables (`HarmonicEditor.cpp`) samt kontroll och komplettering av ANALYZER-mätningarna enligt `plan-v2.md:258-271`. Förslag, ej beslutat: RING, Echo/Flanger, autosave/IO.
Beslut 28 okt: LOOP flyttad in i JOG-panelen (ingen PLAY-meny), BRAKE-kurvor VINYL/TAPE/HARD/SLOW (broms A), SOUNDSTAGE med bussuppdelning och flerval av källor nu. Senare idé: SAMPLE → SYNTH (resyntes av ett sample till synthparametrar).
Beslut 27 okt: broms B (BRAKE trigger) godkänd; fabriks-Init är bas för New Init 1-5; FX-kick-felet (b3cd633) följer med rel-0.3.
Övertonsredigerare i Serum-stil (Arbetare 1, 3 etapper, plan först): ta bort FUNDAMENTAL/EVEN/ODD ONLY/RESET-knapparna, större editor, levande vågform kopplad till spektrum, zoom, penslar, presets; formler senare.
AUTO FIX (Arbetare 1, troligen rel-0.4, plan först): välj instrument (multiselect) mot kick, deterministiska mätningar (kickens tailHz mot basens grundton per not, nivåskillnad, faskorrelation, överskott kring 200 Hz), åtgärder med före/efter-lista och kryssrutor, APPLY, ett ångra. Grundtonsborttagning bara där den krockar. Tobias förslag: ett instrument-spår som känner av kicken och dämpar grundtonen live, som en effekt (alternativ B: dämpning runt kickens tailHz, styrd av kickens envelope som synthen redan känner till; nya parametrar sist, crossfade). Alternativ A: instrumentkopia utan grundton (använder ett lager). Arbetare 1 väljer i planen.
FUNDAMENTAL DUCK (Tobias 27 okt): när den är byggd och testad i DAW ska den slås på i de nya presets som redan byggts och i de som byggs (basar som krockar med kicken). Gäller även PSY ROLLING BASS och GLIDE BASSES. Arbetare 2 gör genomgången efter rel-0.3-testet; rendersig för presets räknas om med avsikt.
Uppskjutet: multiband och sidechain i två band som FX-slot, AI-leverantörer, Fas J (preset-skola, AI-byggare), resterande presets.
OBS: omfattningen är stor; releasen kan delas om testrundan blir för tung.
Beslut 28 okt (ändrat): en enda release. Allt som tidigare låg i rel-0.4 (övertonsredigerare, sample-editor, klick-lager, reverse, parameterlanes, RING, Echo/Flanger, autosave/IO, AUTO FIX, resterande presets, Fas J) ingår i rel-0.3, inget mellanbygge. Arbetsordning och ansvar: `docs/work/plan-arbetsfordelning-rel03.md`. Delas bara om Tobias begär det.

## Fas J: Preset-skola och AI-presetbyggare (efter första releasen, Tobias 27 okt)
Arbetare 2 samlar genrekunskap och synthrecept i `docs/school/` (kick, rollbas, wavetables, MINIMAL, GOA, DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL; kategorier i presetmenyerna, GAMLA för äldre). När ett par genrer är klara: skill som läser `docs/school/` och bygger preset via render/`analyse_mix`-loopen (fas 2-3 i `docs/sound-design-roadmap.md`). Tobias öra i FL är facit; hans omdömen sparas som träningsdata.

## Städning
- `/STACK:8388608` i `CMakeLists.txt:243` rensas upp.
- Texten "15 types" i AGENT_GUIDE är föråldrad.
- Gamla worktrees är borttagna (27 okt, backup i `%LOCALAPPDATA%\Temp\claude\worktree-backup-2026-10-27\`). Grenarna finns kvar.
- `installRel.bat/.ps1` ligger i `.gitignore` (Tobias beslut 27 okt).

## Arbetsfördelning
- Arbetare 1 (pianoroll, preview, transport): A1-A4 (paketet), B1, B3, B5, C, D1, D5, D6, D3, H2.
- Arbetare 2 (meny, LookAndFeel, Live Code, wave): A5, B2, B4, D2, D4, E1, E2, H1, H3, H4, H5.
- Specialisten: granskar vid två punkter (A1/A2 orsak, A3 diff), skriver `undo-spec.md`, tar över A2-analysen vid behov.
- Projektledaren: mergar, F-grenar, kommunikation med Tobias.
- Alla: egen worktree, egen `PROGRESS-*.md`, aldrig push/merge, slarvfel i LESSONS.md.

## Beslut av Tobias (27 okt)
1. Värdens noter startar alltid mönstret när värden spelar (A1).
2. LOOP/BPM-omtolkningen godkänd (D3).
3. Huvudpreset tömmer undo-historiken, del-preset är en transaktion (fas C).
4. Copilot får pusha/mergea bara när Tobias uttryckligen säger det.
5. Gamla worktrees får tas bort (gjort).
6. Jog-wheel/scratch (D3b) väntar tills transporten är stabil.
7. `installRel.*` i `.gitignore`.
9. PREVIEW har företräde: medan PREVIEW är på ignoreras värdens noter (InstrumentProcessor.cpp:614 avbryter inte längre förhandsspelningen) och Kickbases loop gäller. Stängs PREVIEW tar värden över (A1 gäller i övrigt).
8. Separata VST:er är struket ur planen.

## Öppna frågor till Tobias
Inga just nu.
