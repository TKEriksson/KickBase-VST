# PROGRESS Arbetare 2 (gren w2, worktree KickBase-VST-w2)

## 2026-10-27 A5: AiMemory.appendWorkLesson (klar, ej mergad)
- Orsak: `Source/Core/AiMemory.cpp:333` uppdaterade en befintlig lektion på plats (`lessons->set`); den låg kvar som äldsta post och `pruneToLimit` (rad 74, `kept.remove(0)`) kunde radera den vid taket.
- Fix: `lessons->remove(existing); lessons->add(record);` (som appendFeedback).
- Test: `chattest` i `Tools/Tool.cpp` (efter "inställningar sparas och läses tillbaka"): 1 MB-tak, fixtur nära taket, uppdatera äldsta med längre recovery, kontrollera att den ligger sist med occurrences 2. Bevisat: FEL utan fixen, ok med fixen.
- Byggt: RelWithDebInfo, mål KickbaseMcp (KickbaseTool.exe), `build/RelWithDebInfo` i w2.
- Obs: chattest slutar med "FEL: chatten misslyckades" (nätverks-/nyckeltest, ej undersökt om det är förväntat utan nyckel).
- Av: Arbetare 2 (Claude Sonnet 5.5).

## B4, B2, H1, H3, H4 (Arbetare 2, Sonnet 5.5, 2026-10-27)
- **B4**: dropdown-fokus, piltangenter, Enter i multiselect. Orsak: JUCE ComboBox hanterar tangenter själv (juce_ComboBox.cpp ~455-487); luckorna var synlig fokus, fokus efter klick och att multiselect öppnades utan markering. FilterPanel/MixerPage `withInitiallySelectedItem`, Components.cpp ~425, KickbaseLookAndFeel.cpp ~296.
- **B2**: MENU (gamla PROJECT): New (sparafråga), Open, 3 senaste, Save (Ctrl+S, grön bekräftelse), Save as, Export, projektnamn i rubriken. Modules.cpp/.h, PluginBase.h. Ctrl+S kräver fokus i editorfönstret. Ej visuellt kontrollerad.
- **H1**: Maximizer MODE: gammalt oanvänt värde på ratt 6 nollas vid fxKnobsV < 3 (Effects.cpp migrateLegacyKnobs). Test: MasterTests.cpp testMaximizerModeMigration.
- **H3**: fördröjningslinjen skrivs alltid (InstrumentSynth.cpp addMonoWide), annars klick när WIDTH dras upp från 1.
- **H4**: Params.h:120 kommentar, Params.cpp indrag + avslutande klammer, `}` / `clock.beginBlock` på två rader (Instrument/InstrumentProcessor.cpp:145).
- Debug-bygge och alla tester gick igenom.

## D2 – kombinerad vågform (Arbetare 2, Claude Sonnet 5.5)
- `Source/Ui/InstrMixPanel.*`: panorering (mittknapp-drag + rullningslist), zoom runt musen (0.1–32x), tidsaxel i ms, knappen WIDE/NARROW ger vågen hela bredden (ratter göms), dubbelklick återställer.
- Byggt i Debug, alla tester gick igenom. Ej visuellt testat i FL (Tobias vid release).
- Kvar: växel i Pattern-fliken för projektets sammanlagda våg (kräver mönsterrender, samordnas med Arbetare 1).

## D4 steg 1 – förslag i strängar (Arbetare 2, klar)
- `Source/Core/LiveCodeComplete.h`: `stringContext()` hittar funktionen före en öppen sträng; `s("` ger bd/kick och i1..i10, `note("`/`n("` ger tonnamn och minnotation-exempel (ackord, `*4`, `~`, `<>`, `@3`). `$:` kommer först på tom rad, plus en mall. Alla detaljtexter är nu engelska utan åäö.
- `Source/Ui/LiveCodePage.cpp:165`: komplettering öppnas också när `"` skrivs.
- Test: `testLiveCodeStringComplete` i `Tests/MasterTests.cpp`. Debug-bygget och alla tester gick igenom.

## D4 steg 2–3 (Arbetare 2)
- Förslag i `lfo("`/`env("` ger parameternamn (`LiveCodeComplete.h`).
- Autoformat: `Source/Core/LiveCodeFormat.h` (ren funktion, test i `Tests/MasterTests.cpp`), FORMAT-knapp i `LiveCodePage.cpp` (en kedja per rad, `s("..")` först, satser med `//` rörs inte, ångra fungerar).
- Mörkt editortema (Atom One Dark-färger) via `setColourScheme` i `LiveCodePage.cpp`.
- Kvar: `.chord(...)` (väntar på svar via projektledaren), färg per aktivt instrument, flera röster via arrayer i `.notes()`. Flytande fönstret har inte nya temat/FORMAT än.
- Debug-bygget: alla tester gick igenom.

## D4 steg 2: `.chord(CHORD, BASETONE, START, LENGTH)` (Arbetare 2)
- `Resources/strudel/glue.js` (`modProto.chord`): staplar en `.note()` per ackordton, `.late(START)` och `.clip(LENGTH)`. Provkört under node: `s("i1").chord("major","c2",0,["1/4","4/4","2/4"])` ger C2 (clip 0.25), E2 (1), G2 (0.5).
- Tolkningar: START och LENGTH är bråkdelar av en cykel (tal eller "a/b"); standard LENGTH = 1/4; en LENGTH-array som är kortare än ackordet upprepas; BASETONE är notnamn eller tal (C2 = 0), standard c2; CHORD kan också vara en array med intervall.
- Ackordnamn (autokomplettering i första strängen): major, minor, dim, aug, sus2, sus4, power, major7, minor7, dom7, dim7, minor7b5, major9, minor9, dom9, add9, six, minor6.
- Test: `chord("maj` -> `major7` i `Tests/MasterTests.cpp`. Debug-bygget går igenom.

## E1 steg 1: flikar (Arbetare 2)
- Underflikar (verktygsraden EFFECTS/SIDECHAIN/LFO/…, `tools.setSubTabs()` i InstrumentEditor/FxEditor) ritas nu som segment: rundad rektangel, accentstreck under den valda, ingen LED. Huvudflikar är oförändrade pills. `TabbedPanel.h:77`, `KickbaseLookAndFeel.cpp` (drawButtonBackground/drawButtonText, egenskapen "subtab").
- Debug-bygge och tester gick igenom. Ingen skärmdump tagen än; Tobias ser det i FL vid release.
- Kvar i E1: rattar, kontroller, diagram.

## E2 steg 1: skivvyn (Arbetare 2)
- SHOW SAMPLE|SLICES (`Modules.cpp` SamplePage, `Modules.h:144`): SLICES ritar varje skiva som en färgad ruta med "Slice N" och tonen under; ett klick spelar bara den skivan via `postNoteAudition(layer, rowRootNote+MAP TO+index)`, släpps i `SampleView::mouseUp`.
- Nya tooltips på SHOW-knapparna och enhet på skivreglaget. Debug-bygge och tester gick igenom. Kvar i E2: steg 2 (OWN PIANO ROLL, ny parameter sist), 3 (dra gränser, zoom, MANUAL), 4 (AUTO SLICE-dialog).

## E2 steg 3: zoom och manuell redigering (Arbetare 2)
- `SampleView` (Modules.cpp/.h): hjulet zoomar mot musen, Skift+hjul scrollar; vågform, skivrutor, START och spelmarkör går via `normToX`/`xToNorm`.
- MANUAL + SLICES: dra en gräns flyttar den (`mouseDrag`), högerklick tar bort, **dubbelklick** lägger till (avviker från specens "klick": ett enkelklick provlyssnar skivan). Skift/Alt-klick fungerar som förut.
- Steg 2 (OWN PIANO ROLL) uppskjutet: rör PianoRoll (Arbetare 1) och kärnan, kräver beslut/samordning.
- Debug-bygge och tester OK. Ej provat i FL.

## E2 steg 4: CREATE (Arbetare 2)
- `SamplePage` (Modules.cpp ~1372): knappen CREATE (syns i Grid/Auto) gör de beräknade skivgränserna till manuella markörer, byter till Manual och visar SLICES, så att man kan dra/lägga till/ta bort dem. Etiketter och tooltips uppdaterade.
- Slidrarna verkställer fortfarande direkt (live), CREATE är "frys och redigera". Avvikelse från specens dialog: ingen separat dialog, allt ryms på raden.
- Debug-bygge och tester OK. Ej provat i FL.

## E1 omgjord som synligt designpass (27 okt, Arbetare 2)
- Efter skissen `research/TEST2026-10-23/kickbase_tab_navigation_hierarchy_skiss.html`: platta mörka rattar med mono-avläsning, huvudflikar som piller med glöd och stamtriangel, underflikar som platt remsa med etikett, chip-flikar (FX1–FX11, EQ/COMP/FX), arbetsytans rubrikpanel med brödsmula, diagram med prickraster på #07090e, platt combobox.
- Ändrat: Theme.h, TabbedPanel.h, Components.h/.cpp, KickbaseLookAndFeel.cpp, EffectsPage.cpp, Modules.cpp:928 plus diagrambakgrunder.
- Ej omgjort: toggle-knappen (neonring) och linjära reglage (metallhandtag).
- Bilder: build\snap-e1b\ (kick, pattern, tab20, tab21, fx, ai). Bygge + tester gröna.
- E1 steg 2 (27 okt): toggle-knappen (mjukare glöd, mörk fylld skiva när av) och vertikala fadrar (mörkt spår med kant, glödande fyllning, platt handtag med accentkant) i samma stil. KickbaseLookAndFeel.cpp. Bilder: build\snap-e1c\. Bygge + tester gröna.

## E2 steg 2: egen pianorulle, en rad per skiva (27 okt, Arbetare 2)
- Spec: `docs/work/own-piano-roll-spec.md`. Ny radsort `Kind::slice` (rad "Slice N", de 16 första skivorna, `RowRegistry::maxSliceRows`), händelsetyp `Type::slice` i `StepScheduler.cpp`, skivargument i `InstrumentSynth::noteOn`, `PluginBase::syncSliceRows` (en undo-transaktion). Gamla modellen (SLICER-rad, MAP TO, `slicerOn`/`slicerMapTo`-UI, MCP-argumenten) borttagen; parameter-ID:n kvar men oanvända. UI: "Only the first 16 slices have rows" vid fler än 16.
- Tester: syncSliceRows (lägg till/ta bort/tak/undo/redo) och "Slice 3 mot direkt skivindex 2" i `Tools/StateTests.cpp`. persisttest grön, rendersig-trion, blocksizetest, audiotest och undotest gröna efter den sista ljudkodsändringen (testfilen och etiketten ändrades därefter).
- Slice 3-testet: först skillnad 0,036 pga att jämförelsen låg 2 samplar fel (`barSamples` avrundad nedåt); efter exakt start 0,0016 = anslagsskillnaden 102/127 mot 0,8, toleransen är 1 % av peak.
- audiotest: 4312 allokationer finns redan på ren play-pack-HEAD (gammal skuld enligt Arbetare 1); min kod ökar inte talet.
- persisttest föll en gång ("CODE-modulering är aktiv före sparning", StateTests.cpp ~345) och gick igenom i alla senare körningar. Orsak ej klarlagd, troligen tidsberoende; ej åtgärdad.
- Gamla filer: ingen versionshanterad fil använder gamla slicerformatet, inget `git rm`-ades (se spec).
- Bilder: `docs/work/snapshots-w2/` (slices-pattern, slices-pattern20, slices-sample, slices-over16). Skissen (flikhierarki) har inga skivrader, så skivradernas utseende är inte jämfört mot den, bara flikarna/ramen oförändrade.
- Ej provat i FL. Redigering av Slice-rader i pianorullen/stegsekvenserna är inte testad separat.

## H5: fas F-grenarna byggda och testade (27 okt, Arbetare 2)
- `content-s2` (ad768d0, 4dad3e2 m.fl.): `build.bat Debug all` gick igenom (enhetstesterna gröna), persisttest och blocksizetest gröna.
- `fix-slicer-test` (d254e2d): bygge gick igenom, persisttest grön ("Sparandetestet gick igenom").
- undotest finns inte på de gamla grenarna (verktyget saknas), så exit 1 där är inte ett fel.
- OBS till projektledaren: båda grenarna ligger på en gammal main. Slicertesterna i 4dad3e2/d254e2d gäller den gamla MAP TO-slicern som E2 steg 2 tog bort i w2, så de krockar med eller ersätts av E2 vid merge. Merga E2 först och släng/skriv om de två slicertesterna.
- w2 ombyggd efteråt behövs innan nästa w2-test (build\Debug höll fix-slicer-test-bygget).
- Svar på projektledarens fråga (H5, de tre övriga grenarna):
  - `content-d1` (a84e569): bara `docs/content-d1-report.md`, ingen kod, inget bygge behövs.
  - `content-ph` (3447d16): `juce::Identifier`-nycklar i `applyEnvelope`. Samma ändring finns redan i w2 (`ToolLayer.cpp:222-232`), som byggs och testas gröna, men commiten är inte förfader till w2. Överflödig vid merge (ger konflikt eller tom commit).
  - `fix-matrix` (bc3e9f8): `maxInstruments` utan `Modulation::`. Samma ändring finns redan i w2 (`Tool.cpp:1785`). Överflödig på samma sätt.
- Slicertesterna i 4dad3e2 (+2 rader) och d254e2d (0,8 s-WAV, peak per takt) gäller den gamla MAP TO-slicern. Beslut: släng dem. E2 steg 2 har nya tester (syncSliceRows, Slice 3 mot skivindex 2) i `Tools/StateTests.cpp`. Vid merge: ta E2:s version av StateTests, ta inte de två commitarna.
- Egen lista mot loggen: A5, B2, B4, D2, D4, H1, H3, H4 och E2 är klara. Kvar är bara E1 (designlistan som Tobias underkände).
- E1 kontroll (27 okt): w2 ombyggd (build.bat Debug all grönt), nya bilder `build\snap-e1d\` (inst_0 = PATTERN, inst_1) samt `build\snap-e1c\` (kick, pattern, lfo, env, matrix, master, sidechain, soundstage, settings, fx, ai). Jämfört mot skissen (`research/TEST2026-10-23/kickbase_tab_navigation_hierarchy_skiss.html`): huvudflikar som piller med glöd, brödsmula/rubrikpanel, platt underflikremsa, mörka rattar, chip-flikar och diagram på mörk bakgrund stämmer i uppbyggnad. Skillnad: skissens rattar har metallisk yttre/inre gradient (.knob-outer/.knob-inner, skissen rad 39-46), mina är plattare med glödande båge, och skissens underflikar har stamtriangel (.tab-stem, rad 56-79) som jag bara har på huvudflikarna. Inget är jämnt pixel för pixel; Tobias avgör.
- E1 steg 3 (27 okt, Arbetare 2): skissens luckor stängda. Rattar får skuggan (2px 2px 5px svart 0,6) och högdagern (-1px -1px 3px vit 0,05) från `.knob-outer` (`KickbaseLookAndFeel.cpp`, drawRotarySlider, efter `dir`-raden). Underflikar får stamtriangel under vald flik (`TabbedPanel.h`, subTabs-grenen i paint). Bara ritkod, ingen ljudpåverkan. `build.bat Debug all` grönt ("Alla tester gick igenom"). Inga nya bilder skickade (Tobias tittar i Release-bygget). E1 klar.

## D7 / T15: 10 rena kickar och 6 rullande basar (27 okt, Arbetare 2)
- Presets ligger kompilerade i `Source/Core/PresetData.inl` (sist i kick- och basListan, commit 858f72f); inga nya parametrar eller ID:n, äldre index/hashar orörda.
- Mätt med `KickbaseTool audit kick` / `audit bass` (kan inte lyssna): alla 10 kickar toppar -3,2…-2,6 dB utom Clean Warm (-0,3 dB, mjukklipp läge 2, dc 0,002). Svans 38–45 Hz (Clean Sub 38, Clean Deep 39: strax under 40–55-målet, medvetet djupa). Längd 136–263 ms. Baserna toppar -0,3 dB, rms -10,4…-11,3, tyngdpunkt 81–95 Hz, inget dc.
- Namnen tillagda i `docs/work/fl-testlista.md` (en rad per grupp). Tobias måste lyssna i FL: sitter kick+bas ihop, och är Clean Warm för het.

## newest 1–10: tio nya fulla huvudpresets (27 okt, Arbetare 2)
- Kompilerade i `Source/Core/PresetData.inl` (`newest1()`…`newest10()`, hjälpare `newest`/`sendTo`), sist i `fullPresets()` efter Empty så att äldre index och hashar är oförändrade. Inga nya parametrar eller ID:n.
- Varje preset = kick (mönstermall + pitch/amp-envelope) + baslager (mönster via `extras.pattern`) + FX på kanal A som send.
- Stilar: 1 full-on, 2 dark forest, 3 progressive (reese på offbeat), 4 hi-tech (zap), 5 Zenonesque (pluck), 6 hård mörk, 7 acid goa (slides + LFO→cutoff), 8 deep reese (sub + reese), 9 minimal natt, 10 twilight (3 lager + LFO→cutoff).
- Mätt med `KickbaseTool audit full` (kan inte lyssna): toppar -3,1…-0,3 dB, dc ≈ 0, ingen klippning utöver Inits egna -0,3 dB. Newest 4 var för ljus (centroid 244) → cutoff 380, nivå 0,50 → centroid 172. Newest 5 och 6 är ljusa (mellan -10,6/-11,4 dB), avsiktligt för stilen.
- `build.bat Debug all`: alla tester gick igenom.
- Tobias måste lyssna i FL (rad i `docs/work/fl-testlista.md`) och säga vilka som ska finjusteras.

## Del 1 av T-kick/rollbas: Psy Clean-kickar (27 okt, Arbetare 2, Claude Sonnet 5.5)
- Psy140 Clean G är bra för att den är ren sinus (0 övertoner/klick/drive/EQ), har ett brant 70-halvtoners svep på ~120 ms (hårt framtungt: 50 st vid 2 ms, 17 st vid 30 ms) och nästan platt volym (0,9 vid 170 ms, slut 200 ms, tension 0,3), nivå 0,85.
- Nytt: hjälparen `psyCleanKick` + konstanten `psyCleanCategory` ("PSY CLEAN") och 8 kickar sist i `kickPresets()` (`Source/Core/PresetData.inl`, efter Clean Modern). Inga index flyttade, inga nya parametrar/ID:n. Tonen är `kickKeyFollow` = Fixed (C1 32,7 Hz + tune), så tailHz är känd: `Params.cpp:462-469`, `Params.cpp:561-567`.
- Mätt (`audit kick`): toppar -3,4…-3,1 dB, dc 0, längd 152–216 ms, svans C 33, D 37–38, F 45/50, A 56, G 50 Hz. F Sub/A Punch/G har högre centroid (107–130) än C/D (77–90) eftersom svansen ligger högre.
- Kategori PSY CLEAN (kickar): Psy Clean C, Psy Clean C Tight, Psy Clean D, Psy Clean D Long, Psy Clean F Soft, Psy Clean F Sub, Psy Clean A Punch, Psy Clean G.
- Kategori PSY ROLLING BASS (basar): inga än (efter godkänd utredning). PSY ROLLING (huvudpresets): inga än.
- `build.bat Debug all`: alla tester gick igenom. Tobias lyssnar i FL (rad i `fl-testlista.md:65`).

## Del 2 – rollbas-utredning + docs/school (Arbetare 2, Sonnet 5.5, 2026-10-27)
- Skrivit `docs/school/` (README, kick, rollbas, wavetables, kick-bass-samspel, mixning-fx-sidechain, minimal, goa, presets-logg). `docs/work/rollbas-utredning.md` pekar dit.
- Mätt: 6 Rolling-basar är nästan identiska, saknar mellanregister (se school/rollbas.md §3).
- Kvar: tabellspektrum (mäts innan bygge), genreutredning MINIMAL/GOA. Inget byggt, väntar på PL:s ok.

## Del 3: PSY ROLLING BASS (27 okt, Arbetare 2)
- Byggt: `psyRollBass` + 4 basar sist i `bassPresets()` (index 14–17): FULL-ON ROLL, DEKEL GROOVE, FOREST 1/32, WIDE MID (`PresetData.inl`). Gamla index oförändrade.
- Mätt (`audit bass`, E1 41 Hz): centroid 112–122 (gamla 81–97), 250–1k −12,0..−12,8 dB (gamla −13,3..−14,5), <60 −6..−8,5. Målet (centroid 150–300, 250–1k −6..−9) **nåddes inte**.
- Fynd: audit-siffrorna rör sig knappt när cutoff ändras 120→1400 Hz, envAmount 0→3 och OSC A pos 0.2→1.0. Gamla Rolling-varianter är också identiska. Misstanke: audit-spelningen (`Tools/PresetAudit.inl:79`) eller filterkedjan/band-måttet är okänsligt. Behöver utredas innan fler basar kan ställas in mot siffror.
- Wide Mid saknar fortfarande unison/bredd. Inget kategorinamn i huvudpreset än.

## Lager 1-2 och bas-preset (27 okt, Arbetare 2)
Läst i koden (inget ändrat): bas-preset laddar bara valt lager (`Presets.cpp:267`); lager 1-2 har default på/0.7 (`Params.cpp:227`) men inga toner i standardmönstret (`Pattern.cpp:426-428`), så de är troligen tysta i pluginet. Audit spelade en MIDI-ton i alla lager: därför dränktes skillnaderna där. Audit fixad (`Tools/PresetAudit.inl:74-76`); gamla basmått ogiltiga. Förslag: ingen ändring av laddningen. Verifiering i DAW av Tobias: ladda ny instans + bas-preset, kolla att bara rad 1 ljuder.

## Del 4: PSY ROLLING huvudpresets (27 okt, Arbetare 2)
- Byggt: `psyRolling1..4` (Roll Full-On, Roll Wide, Roll Dekel, Roll Forest) sist i `fullPresets()` efter newest10 (`PresetData.inl`), kategori `psyRollingCategory`. PSY CLEAN-kick + ny rollbas, FX-send, Duck, `instrumentCount` 1 (lager 1–2 avstängda).
- Mätt (`audit full`): peak −2,9..−3,3, RMS ca −11, DC 0, centroid 82–131 Hz. Alla ändliga. `build.bat Debug all` grön. Gamla index/hashar rörs ej.
- Jag kan bara mäta, inte lyssna: Tobias provar i DAW (testlistan).
- Nästa: GLIDE BASSES, sedan New Init 1–5.

## Del 5: GLIDE BASSES (27 okt, Arbetare 2)
- Läst: glide är en flagga per ton, glidtiden = tonens längd (`InstrumentProcessor.cpp:1061`, `Presets.cpp:166`); `F::glide` oanvänd. Recept i `docs/school/glide-bass.md`.
- GLIDE BASE (Tobias fil): glide-tonerna var 12 ticks (~80 ms) och hördes knappt → förlängda till 60; backup `.mofp.bak`.
- Byggt: `glideBass1..3` (Glide Sub, Glide Acid, Glide Dive), kategori `glideBassesCategory`, sist i `fullPresets()`. Mätt: peak −0,3..−2,0 (Glide Sub sänkt till bas-nivå 0,50), RMS ca −11, DC 0. Bygg grönt. Ej lyssnat.
- Nästa: New Init 1–5 (utgår från fabriks-Init; rendersig 94f0a1b6bd8a3c7f ändras ej). Max 3 presets per ny genre (Tobias).

## Kvar (presets pausade efter rel-0.2, beslut Tobias 27 okt)
Tas efter nästa release, max 3 presets per ny kategori: New Init 1–5 (utgår från fabriks-Init, rendersig 94f0a1b6bd8a3c7f ändras ej), MINIMAL, GOA, DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL, SYNTH, MID BASS, HI BASS, samt presets för S3 Unison/Reese när funktionen finns. Utredningsdokument per genre (`docs/school/*.md`) skrivs vid behov före bygge.
Nu: programmet, rel-0.3 motornära punkter: 1) S3 Unison/Reese, 2) multisampling (Arbetare 1 har UI).

## PLAN rel-0.3 motornära: S3 Unison/Reese och multisampling (27 okt, Arbetare 2) - väntar på PL:s ok
**S3 Unison/Reese: finns redan.** Unison per OSC (1..7 röster, detune, blend, slumpfas, pan, spridning, bredd) för OSC A/B/C; UI varnar vid RÖSTER × unison > 24 (`Modules.cpp:1273`). Ingen ombyggnad.
- Etapp S3-1: mäta, inte bygga. Renderar Reese-recept (2 OSC saw, unison 5-7, detune 8-25 cent) i `audiotest`/`perftest`: CPU per röst, stereo/mono-kompatibilitet (lowSide), DC, 0 allokeringar, blocksizetest.
- Etapp S3-2: bara om mätningen visar ett verkligt gap (t.ex. faslåsning, för hög CPU). Då nya parametrar sist i `InstrumentField` (före `count`), standard = dagens ljud. Annars inga kodjusteringar; Reese-presets tas efter releasen.
**Multisampling (Fas C, `docs/content-plan.md:175`)**
- Lagring: ny `MultiSampleSet` (zoner: tangent lo/hi, rot, velocity lo/hi, loop, ClickSample per zon) bredvid `SampleSlot` (`SampleSlot.h:10`). Laddas och resamplas på meddelandetråden; ljudtråden läser en atomisk pekare; gammalt släpps via `DeferredRelease` (samma mönster som `SampleSlot`).
- Ljudtråd: vid note-on väljer synten zon (tangent + velocity) och sätter `params.sampleData` till zonens data (`InstrumentSynth.cpp:178`); resten av spelaren (loop, nyckelföljning, start) återanvänds. Inga allokeringar, inga lås. Valet görs en gång per röst; rösten håller zonen till release (pekaren skyddas av DeferredRelease).
- Persistens/hash: nytt barn i state-trädet (`BassMultiSample`, som `BassSample` i `PluginBase.cpp:1900`). Saknas det laddar gamla projekt/presets som förut. Biblioteks-id + hash eller inbäddat. Inga ID-/hash-ändringar.
- Parametrar: en ny val-parameter `sampleSource` (0 Enkel = dagens, 1 Multi) sist i `InstrumentField` (före `count`, `Params.h`), standard 0. UI-väljaren (TYPE) är Arbetare 1:s; jag ger bara parametern.
- Etapper: (1) datamodell + laddare, (2) zonval + spelare (Hermite som val senare), (3) persistens, (4) SFZ-delmängd som import i KickbaseTool, (5) tester: zonval, loop, saknad fil, hash-mismatch, rundtur, 0 allokeringar, blocksizetest, CPU med 8 röster.
- Risk: polyfoni (`InstrumentVoices`) måste hålla zonpekare per röst; round-robin/layers utanför v1.

### S3 klar (27 okt, Arbetare 2)
- Godkänd av PL. `KickbaseTool reesetest` tillagd (`Tools/AudioTests.cpp`, `Tools/Tool.cpp`). 0 alloc, DC 0, blocksizetest ok, CPU oförändrat med unison. Inget gap, ingen motorkod ändrad. Recept: `docs/school/reese.md`.
- Nästa: multisampling etapp 1–3.
