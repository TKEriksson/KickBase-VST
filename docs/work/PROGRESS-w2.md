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

## Multisampling etapp 1 klar (Arbetare 2)
- Datamodell + laddare: `Source/Core/MultiSample.h/.cpp` (zoner, rotton ur filnamn, autoMapKeyRanges, omsampling, atomiskt bankbyte, persistens via ValueTree).
- Test: `KickbaseTool multisampletest` (rotton, zonval utan allokering, velocity-lager, gränser, round trip, banklivslängd). `build.bat Debug all` grön.
- Misstag: fel förväntat värde i testet (0,5 s vid 96 kHz = 48000 samplar, jag skrev 44100). Rättat.
- Nästa: etapp 2 (sampleSource sist i InstrumentField, zonval vid note-on, spelare).

## Multisampling etapp 2 – pausad, delvis (Arbetare 2, 27 okt)
- Klart: `sampleSource` sist i `InstrumentField` (`Params.h`, `Params.cpp`, fältnamn "SampleSource"), `sampleMulti`/`sampleBank` i `InstrumentParams`, zonval i `noteOn` och uppslag per sampel via `activeSample()` (`InstrumentSynth.cpp`), `instrumentMultiSamples` i `PluginBase`. MultiSample.cpp flyttad till CORE_SOURCES (testmålet länkade inte). `build.bat Debug all` grön.
- Spelartestet finns sist i `multisampletest` (`Tools/AudioTests.cpp`). Tonhöjdskontrollerna går igenom; kontrollen "tangent 76 väljer höga zonen" FALLERAR och är inte undersökt (misstänkt: nivå/sampleLevel eller zongränser i testuppsättningen, inte bekräftat). Bankbyte, tom bank och 0 allokeringar kom inte att köras före felet i resultatet, kolla utskriften.
- Nästa: skriv ut lowRms/highRms i testet, hitta orsaken, rätta; sedan etapp 3 (persistens: eget träd "BassMultiSamples", gamla projekt oförändrade), testrad i `fl-testlista.md`, UI/TYPE-väljare med Arbetare 1.
- Stoppad på PL:s order (sessionsgräns); S3 är klar (64bde98), etapp 1 klar (150fb30).

## FUNDAMENTAL DUCK i presets + merge play-pack (28 okt, Arbetare 2)
- `fundDuck` i `PresetData.inl` (före PSY ROLLING): på i Roll 1–4 och Glide 1–3 (9 dB/200 cent; Glide Sub 12/250). Bas-only-presets orörda.
- Audit före/efter: effekten under 0,1 dB i banden; render-signaturer för default-av oförändrade (Init 94f0a1b6bd8a3c7f).
- Tidstestet flaky: Debug-gräns 120 s. Build grön efter merge av play-pack (SFZ-kärnan b9c9859).
- Kvar: KickbaseTool-kommando för SFZ + parseSfz-tester, Init 1–5, alla övriga kategorier (max 3), Fas J.

## Duck Demo (28 okt, Arbetare 2, på Tobias begäran)
- `duckDemo()` i `PresetData.inl` (sist i fullPresets): D-kick (tone 2) och lång bas på x±1 / x+3, alltså ±100 cent från kickens grundton (inom fönstret 200 cent) och dissonant. Duck 12 dB, send 0.
- Build grön. Audit visar bara duck-på-värden; på/av-skillnaden är inte mätt, den ska lyssnas fram i DAW (testrad tillagd).

## New Init 1–5 (28 okt, Arbetare 2)
- `newInit1–5` i `PresetData.inl`, sist i fullPresets, byggda av `initPreset()`/initSub/initMid/initAcid. Init oförändrad (rendersig 94f0a1b6bd8a3c7f). Build grön, audit utan clip.
- Kvar: SYNTH, MID BASS, HI BASS, genrer (max 3 per kategori), Fas J (preset-skola).

## 2026-10-28 MID BASS / HI BASS / SYNTH
- 9 nya baspresets (3 per kategori) via `registerBass` i `PresetData.inl`, sist i `bassPresets()`; namnprefixregler i `PresetCategories.cpp`. Bygge grönt, audit utan klippning. Återstår: genrer (MINIMAL, GOA, DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL), preset-skolan.

## 2026-10-28 Genrepresets
- 7 kickar + 14 basar (namnprefix Minimal/Goa/Dark Psy/Dance/Trance/Techno/Empirikal), sist i `kickPresets()`/`bassPresets()`; genreregler och fem nya kategorier (DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL) i `PresetCategories.cpp`. Bygge grönt, audit utan klippning. Ej lyssnat. Återstår: Fas J, bygg vidare på `docs/school/` (a3c54e7), sedan skill.

## 2026-10-28 Genreutredning med källor
- Arbetare 2: `docs/school/` minimal, goa (omskrivna) plus nya dark-psy, dance, trance, techno, empirikal. Källor som länkar/titlar, kvalitet och "obekräftat" angivet. Tunt: minimal-psy (en BPM-källa), Dance (ingen egen källa), Empirikal (ingen specifik källa, Reese-underlag). Ingen kod ändrad. Finjustering väntar på Tobias lyssning i Lemon.

## 2026-10-28 Slutaudit alla fabrikspresets (efter merge av play-pack, build grönt)
- Arbetare 2: `audit kick|bass|full all` (30 kickar, 40 basar, 26 fulla). Ingen klippning (peak ≤ −0,3 dB), DC ≤ 0,004. Ej lyssnat.
- **Avvikelser (kick):** Dark Forest Boom och Clean Warm peak −0,3 dB (nära tak); Dark Forest Boom DC 0,004 (högst). Svans utanför 35–50 Hz: Psy Clean C och C Tight 33 Hz, Psy Clean A Punch 56 Hz.
- **Avvikelser (full):** newest 2/5/6 och Init/New Init 1–2 peak −0,3 dB; newest 2 DC 0,004. Init och newest 1 har side −37 dB (stereo i lågt band), övriga −120.
- **Basar:** alla peak −5…−12,4 dB, DC 0. Genrebasarna −9…−12 dB.
- Inget ändrat; preset-index orörda. Finjustering efter Tobias lyssning i Lemon.

## 2026-10-28 AUTO FIX, mätdelen (Arbetare 2)
- Plan: `docs/work/autofix-matdel-plan.md` (alternativ B, FUNDAMENTAL DUCK; A förkastad). Godkänd av projektledaren.
- Byggt: `Source/Core/AutoFix.h/.cpp` (ren funktion `AutoFix::analyse`, bara std): grundton mot kickens tailHz i cent per ton, fönster 100 cent, nivå bas-kick, korrelation, 200 Hz-överskott; åtgärdslista med före/efter (engelsk text), allvarligast först, deterministisk. Inga nya parametrar, inget UI.
- Tester: `Tests/AutoFixTests.cpp` (cent, krock/fönsterkant, redan på, vidgat fönster, fas/nivå/200 Hz, tomt, determinism).
- Slarv fångat av testet: fönsterkanten (100 cent) föll på flyttalsfel; fixat med 0,01 cent marginal.
- För Arbetare 1: mappa `Action::param` till UI/APPLY; fyll `PhaseInfo` ur `MixAnalysis::PhaseMetrics` (hjälpare `fromPhaseMetrics` byggs när UI-kopplingen behöver den).
- Build `Debug all` grönt.

## 2026-10-28 – fromPhaseMetrics och FX-kärnan (Arbetare 2)
- `AutoFix::fromPhaseMetrics` (AutoFixMix.h) + test, commit 084cad0.
- FX-kärnan RING/Echo/Flanger enligt `docs/work/fx-ring-echo-flanger-plan.md` (godkänd av PL): Flanger fick SHAPE (Sine/Triangle) och SPREAD (standard 0.25 = gamla ljudet) sist i tabellen, `newKnobDefault` legacyCount 5 (`Effects.cpp`). Nya `Tests/FxTests.cpp` (tabell, Flanger, Echo-tid, RING). Bygget grönt, alla tester gick igenom.
- Echo ms-tid parkerad (alla 10 knobplatser används; PL frågar Tobias om knobsPerSlot).
- Kvar för Arbetare 1: UI-etiketter SHAPE/SPREAD.

## 2026-10-28 – Echo TIME MS och knobsPerSlot 11 (Arbetare 2)
- `knobsPerSlot` 10 → 11 (`Source/Core/Effects.h:25`), `knobsVersion` 3 → 4 så att gamla projekt får P11 satt. ID:n P1–P10, typindex och presetindex orörda.
- Echo: ny sista ratt TIME MS (0–3300 ms, standard 0 = följer TIME/SYNC = gamla ljudet). Över 0 ersätter den synken (`EchoFx::process`).
- Tester: `testEchoMs` i `Tests/FxTests.cpp` (layout har P11 men inte P12, legacy-standard 0, 0 ms behåller synk, 250 ms hamnar rätt). `Debug all` grönt.
- Ekots ms-tid är därmed inte längre parkerad.

## 2026-10-28 – KickbaseTool sfz + parseSfz-tester (Arbetare 2)
- Redan i w2 (14e31ea): `KickbaseTool sfz <fil.sfz>` (`Tools/Tool.cpp:4061`) skriver ut regionerna och hur många zoner `addSfz` kan läsa; `parseSfz`-tester i `Tools/AudioTests.cpp:1677` (arv mellan nivåerna, tonnamn, default_path, kommentarer, tom text). Ingen UI rörd.
- `Debug all` grönt efter Echo TIME MS (a601d2e) inkluderar dessa. S3 Unison/Reese-presets väntar tills Tobias ber om dem.

## 2026-10-28 – S3 Unison/Reese-presets (Arbetare 2)
- Tre baspresets sist i `bassPreset`-listan (`PresetData.inl`, efter Empirikal Growl) och ny kategori UNISON (`PresetCategories.cpp`, prefix "Unison "). Inga nya parametrar, ID/index/hash orörda. `Debug all` grönt, audit utan klippning. Ej lyssnat.
- Nästa: planer + kärnor. Arbetare 1 vill ha sample-editor-kärnan först (hon lägger DELAY/LENGTH/CURVE efter den i `InstrumentSynth.cpp` sample-grenen).

## 2026-10-28 – Sample-editor-kärna (Arbetare 2)
- Plan: `docs/work/plan-sample-editor.md`. Sju nya parametrar sist i `InstrumentField` (SampleAmpOn/Ms/Shape, SamplePitchOn/Semis/Ms/Shape), alla av som standard = bit-identiskt gamla ljudet.
- Motor: `sampleAge` + `InstrumentSynth::sampleCurve` i sample-grenen (`InstrumentSynth.cpp`). AMP gäller alla röster, PITCH inte slice-röster.
- Test: `testSampleEditor` (`Tests/EngineTests.cpp`): av = identiskt, AMP tyst efter ms, PITCH högre i början och tillbaka efteråt. `Debug all` grönt.
- Arbetare 1: lägg DELAY/LENGTH/CURVE efter denna kärna; `samplePosition` ska stå still under DELAY.

## 2026-10-28 – Reverse per not (Arbetare 2)
- Plan: `docs/work/plan-sample-reverse.md`. Ny sista parameter `sampleReverse` (av = gamla ljudet). Motor: speglad startposition i `noteOn` och bakåtsteg i sample-grenen (`InstrumentSynth.cpp`); LOOP loopar från slutet. Slicer har kvar sin `slicerReverse`.
- Test: `testSampleReverse` (`Tests/EngineTests.cpp`). `Debug all` grönt.

## 2026-10-28 – Klick-lager 2/3 + sampleverktyg (Arbetare 2)
- Plan: `docs/work/plan-click-layer-sample-tools.md`. 13 nya kickparametrar sist (kickClick2/3 Level/Length/Tone/Delay/Mode, kickSampleStart/Reverse/Fade), alla av = gamla ljudet.
- Motor: extra klick i `KickEngine.cpp` (bara om nivå > 0), sampleverktyg i sample-grenen. Test: `testKickClickLayersAndSampleTools`.
- Arbetare 1: UI + hjälptexter behövs (se planen).

## 2026-10-28 – UI + hjälptexter för mina tre kärnor (Arbetare 2)
- Bas, `SamplePage` (`Source/Ui/Modules.cpp`, `Modules.h`): REVERSE-knapp, AMP (TIME/SHAPE) och PITCH (SEMIS/TIME/SHAPE) med på/av-knapp under vågformen. Slice-knappen heter nu SLICES REVERSE.
- Kick, `KickModule`: ny rad CLICK 2 / CLICK 3 (LEVEL/LENGTH/TONE/DELAY/MODE) i SYNTH-vyn; SAMPLE-vyn har raden TOOLS (START, REVERSE, FADE). Radhöjderna räknas från antalet rader, så gamla layouten är kvar.
- SYNTH/SAMPLE-mixen i kicken fanns redan (`synthToggle`/`sampleToggle`, LEVEL = balans).
- Hjälptexter: ett block i `Source/Ui/Help.cpp` (kickClick2/3*, kickSample*, bassSampleAmp*/Pitch*/Reverse). Arbetare 1:s tillfälliga rader för bassSampleAmp*/Pitch* finns bara i hennes gren: ta bort dubbletter vid merge.

## 2026-10-28 – Undersökning: FX send-matris, ljudväg och 132 sendparametrar (Arbetare 2, ingen kod)
Underlag: `docs/work/plan-fx-send-matrix.md` (main). Radnummer nedan är main (`InstrumentProcessor.cpp`), i w2 ligger samma kod ~76 rader tidigare.

### Hur ljudvägen är idag
- Per källa (`InstrumentProcessor.cpp:915-926`, kick `931-941`): `Mixer::mix` (`Mixer.cpp:108-135`) lägger `ljud × fader × pan × dryGain` på master-bussen och skriver **samma** `ljud × fader × pan` (utan `dryGain`) till `sendBus`. Sedan `effects.send(kanal, sendBus, amount)` (`Effects.cpp:2417`) som *adderar* `amount × sendBus` till kanalens ingång.
- **INSERT** (standard): `dryGain = 1 - amount` på bara den valda kanalen. **SEND** (`fxSend`): `dryGain = 1`, torrt orört, wet läggs på. Alltså: INSERT = torrt/blött-delning, SEND = additiv.
- Skickningen sker efter duckning, EQ, fader och pan, före kanalens effekter. Kanalen mixas tillbaka efter `effects.process` med nivå 1.0 (`:956-962`); kanalens egen nivå/duck ligger i `EffectRack::process` (`Effects.cpp:2473-2483`).
- Flera källor till samma kanal summeras redan idag (addFrom). En kanal utan skick och utan aktiv effekt är `idle` och hoppas över (`Effects.cpp:2442`) = CPU-kravet är redan uppfyllt.
- `EffectRack::isActive` är alltid `true` (`Effects.cpp:2399` sätter den, ingen sätter den falsk), så `routed` betyder i praktiken `kanal >= 0`.

### Förslag: ljudväg med flera sends
1. **Matrisen är alltid SEND (additiv).** De nya 132 beloppen `send[källa][kanal]` ändrar aldrig torrsignalen. Bara den gamla `fxChannel`/`fxAmount`/`fxSend` behåller INSERT-beteendet (`dryGain = 1 - fxAmount` på sin kanal). Skälet: med flera kanaler finns ingen entydig "1 - summan" att dra av, och gamla presets måste låta likadant.
2. **Effektivt belopp per kanal:** `amount_c = (c == fxChannel-1 ? fxAmount : 0) + send[källa][c]`. Summan (inte "matrisen ersätter") gör att ett gammalt preset med matris = 0 är bit-identiskt och att man kan lägga till en andra kanal utan att den första ändras. UI: reglaget för den gamla kanalen binds till `fxAmount`, övriga till matrisparametrarna; då nollar användaren inget oväntat om hen byter kanal i dropdownen.
3. **Implementation (en loop, `sendBus` återanvänds):** `Mixer::mix` skriver redan `sendBus` en gång per källa. Efter det: den gamla `effects.send(...)` oförändrad, sedan `for c in 0..numChannels: if (send[c] > 0) effects.send(c, sendBus, send[c])`. Inga extra buffertar, inga nya mix-anrop. `sent[c]` sätts bara av ett verkligt anrop, så kanaler utan skick och effekt förblir `idle`.
4. **Ordning/bit-identitet:** addition av flyttal är inte associativ. Låt den gamla sendens `addFrom` komma först och matrisens efter, och källorna i samma ordning som idag (kick, instr 1-10). Då är "alla sends 0" exakt samma operationer som förut (inga extra anrop).
5. **Gain-staging:** linjär 0..1, ingen normalisering (normalisering skulle ändra gammalt ljud). Det betyder att en källa med 1.0 till tre kanaler ger 3× signal in totalt; det är avsiktligt (SEND är en parallell väg) och regleras av kanalens egen nivå (`gains[ch]`, `Effects.cpp:2407`). Föreslå **hjälptext** "sends add up; lower the channel level if the return is too hot" och en mjuk markering i UI när en källas summa > 2. Ingen auto-gain.
6. **Mjukning:** dagens `amount` rampas inte (konstant per block, `addFrom`). Matrisen är automatiserbar, så använd `addFromWithRamp` med förra blockets värde (132 flyttal i processorn) för matrisen. Gamla vägen lämnas ouppmjukad (bit-identisk).
7. **Sidechain på kanal 12 / egen kanal:** inget ändras i sendvägen; kanalens `SIDECHAIN`-effekt läser `SourceSignals` (`:947-954`), som är oberoende av sends.

### Förslag: 132 parametrar utan att ändra rendersig
- Namn: `fxSend<Källa><Kanal>`, t.ex. via `ParamID`-hjälpare: kick + `bass(layer, InstrumentField::…)` kan inte användas rakt av (matrisen är 11×12, inte per fält). Enklast: ett fält-block i `InstrumentField` för bas-lagren (12 fält: `fxSendTo1..12`, **sist** i enum) + 12 `kickFxSendTo1..12` sist bland kick-parametrarna. 10×12 + 12 = 132, samma mönster som `fxSend` (`Params.cpp:417`, `hasBass`).
- Standard 0, `NormalisableRange<float>(0, 1, 0.001)`, vanliga `AudioParameterFloat` (automatiserbara). Saknas parametern i ett gammalt preset gäller standard 0 = av.
- **Lagt sist i layouten**, efter alla befintliga parametrar (efter `Params.cpp:417` för lagren; kicken sist i `addKickToggleParams`). Obs: mixer-/effektparametrar för FX-kanal 12 (nya `fxL…`, `Mixer FxL …`) måste *också* läggas sist i hela layouten, inte i sin kanalloop, annars flyttas ordningen för parametrar efter kanal 11. **Att kolla först:** om presetindex/hash räknas på ordning eller id; id-baserat räcker att ID:na är nya.
- `Params` laddar bas-fälten per lager i en loop med `get(F::x)`; matrisen kan läsas som en array `sendAmount[12]` i samma `getBass`-struct (`Params.cpp:817-824`-mönstret) så ljudtråden inte slår upp strängar.
- helpcheck: 132 hjälpnycklar. Bas: `bassFxSendTo<N>` ×12 (en nyckel per fält räcker om helpcheck matchar fältnamn, ej lager) + `kickFxSendTo<N>` ×12. Då blir det 24 rader, inte 132. **Att verifiera** hur helpcheck räknar lager.
- Mixer/FX har hårt 11 på: `Effects.h:23`, `PluginBase.cpp:13` (static_assert fxA-fxK, 5 platser), `Mixer.h:15` (strips = 1 + 10 + kanaler, fx sist, så en 12:e strip hamnar sist = ofarligt), `PluginBase.cpp:97-104, 605, 715, 1507, 1684`, `PresetButton.cpp:259-268`, `EffectsPage.h:79`, `fxPresetNames`. Inget av det rör sendvägen.

### Tester (förslag)
- Alla sends 0: rendersig oförändrad (existerande testet).
- Två sends från samma källa till kanal A och B = ljudet var för sig summerat (linjär, tolerans nära 0).
- Gammalt preset (`fxChannel`+`fxAmount`, matris 0) bit-identiskt; INSERT-dry fortfarande `1 - amount`.
- Matrissend på samma kanal som den gamla: effektivt belopp = summan.
- Blocksizetest 1/64/512 med matris på; CPU: kanaler utan skick och effekt förblir `idle`.

### Risker / beslut åt Tobias/PL
- Summerande sends kan låta för högt om flera ligger på 1.0 (inget auto-gain, medvetet).
- Kanal 12 + `ParamID`-ordning: lägg sist.
- `fxAmount` vs matris för samma kanal: förslaget är summa; alternativ är "matrisen vinner", men det kräver migrering av gamla presets.

### Svar på de öppna frågorna (kodkoll, 2026-10-28)
- **Presetindex och hash: ordning i preset-listan, inte parameterordning.** Indexet är platsen i `PresetData.inl` (kommentarerna "sist så att äldre index och hashar är oförändrade", rad 443/446/638/680). Rendersig (`Tools/Tool.cpp:1574-1605`) hashar renderade samples per presetindex. Parametrar sparas och laddas på **ID** (APVTS), så nya 132 ID:n sist ändrar inte index. Parameterordningen spelar bara roll för värdens automationslista, och där hamnar de nya sist. Rendersig förblir oförändrad så länge alla sends är 0 (inga extra `send`-anrop).
- **Helpcheck: på ID med regler, inte en nyckel per parameter.** `Tools/StateTests.cpp:43-57` går igenom alla parametrar och hoppar över lager >0 (`Presets::instrumentLayerOf`), effektkanaler utom `A`, `mixBass` utom `mixBass1` och `mixFx` utom `mixFxA`. `Help::forParameter` (`Help.cpp:10-11`) strippar `bass<lager>`-prefixet. Alltså: basmatrisen behöver **12 texter** (ett per `bassFxSendTo<N>`-fält, lager 1), kicken 12 egna ID:n = **24 texter**. Kanalerna är bara ett suffix; en gemensam regel i `forParameter` (t.ex. `contains("FxSendTo")`) kan ge alla 24 samma mall-text med kanalens bokstav.
- **Kanal 12 i helpcheck:** nytt ID `fxL…` hoppas över av regeln `id[2] != 'A'`, och `mixFxL…` av `mixFx`-regeln, så inga fler texter behövs där.
- **Beslut (PL):** matris + gamla `fxAmount` på samma kanal = **summa (additivt)**. **FX-pluginet får ingen matris nu.** Ingen kod i rel-0.3.

## Steg 3: send-matrisen – skriven plan (2026-10-28, Arbetare 2, ingen kod)
**Rättelse:** `addFromWithRamp` ovan (rad 268) är en JUCE-metod på `AudioBuffer`, inte en egen funktion i `Source/Core`. Ingen wrapper finns i `EffectRack`; den görs i steg 3 (se Ramp). Radnummer här är w2.

### Filer och funktioner (i ordning)
1. `Source/Core/Params.h:145` – nya `InstrumentField::fxSendTo1..fxSendTo12` direkt efter `fxSend` (före `sampleMode`, annars flyttas inget: enumen är bara index, ID:n styr sparning). Kick: 12 nya ID:n `kickFxSendTo1..12` bredvid `kickFxSend` (`Params.h:68`). Hjälpfunktion `fxSendToField (int channel)` så loopen slipper switch.
2. `Source/Core/Params.cpp` – `bassFxSendTo<N>` registreras efter `fxSend` (l.420) men *i lagerloopen, så sist av instrumentfälten som ID*; kickens i `addKickToggleParams` sist. `floatParam` 0..1 steg 0.001, standard 0, text "%". **Obs:** parameterordningen i layouten påverkar bara värdens automationslista, aldrig preset/hash (se svaren ovan).
3. `Source/Core/Params.h:270` `hasBass` – FX-pluginet saknar fälten (ingen matris där), loopen måste gå via `hasBass` eller läsas in i `InstrumentParams` bara för processorn.
4. `Source/Instrument/InstrumentProcessor.cpp:990-1016` – send-looparna (nedan).
5. `Source/Core/Effects.h/.cpp` – ny `EffectRack::sendRamped (channel, L, R, n, fromAmount, toAmount)` bredvid `send` (`Effects.cpp:2427`).
6. `Source/Core/Presets.cpp:269` – lägg `fxSendTo*` i undantagslistan (`isLayerField`), så instrumentpresets inte skriver över routingen.
7. `Source/Core/PluginBase.cpp:92-104` `fxChannelUsageMask` – kanal räknas som använd om `fxSendTo<c> > 0`. `:1315` AI-namnkartan. `:1694-1730` `loadInstrumentPreset` (länkning) – kolla att matrisen inte rörs. `:1845` `turnOff` – ingen ändring (matrisen berörs inte av fxAmount-avstängning), men besluta med PL om "ta bort FX" ska nolla matrisen.
8. `Source/Ui/Help.cpp:10` – en regel `contains("FxSendTo")` med mall-text per kanal (24 ID:n, 1 regel). `Components.cpp:478-499` tooltips, `Modules.cpp:923` (kick) och `:1193` (bas): `FxSendControl` blir en lista med ett reglage per synlig kanal (UI sist, efter att ljudet är testat).

### Loopen (InstrumentProcessor)
Den gamla `effects.send (channel, sendBus…, amountToChannel)` ligger kvar **först och orörd** (bit-identitet). Direkt efter, per källa:
```cpp
for (int c = 0; c < Effects::numEffectChannels; ++c)   // aldrig hårdkodad 11/12
{
    const auto to   = params.bass (layer, fxSendToField (c));
    auto&      from = lastSendTo[(size_t) sourceIndex][(size_t) c];
    if (to > 0.0f || from > 0.0f)                      // båda 0 = inga anrop = rendersig oförändrad
        effects.sendRamped (c, sendBus.getReadPointer (0), sendBus.getReadPointer (1), numSamples, from, to);
    from = to;
}
```
Kicken: samma loop med `sourceIndex = Mixer::kickSource` och kickens ID:n; `sendBus` är kickens (mono kopieras till båda i `mix`, som idag; null höger återanvänder vänster i `send`).
- `sendBus` fylls av `mixer.mix` en gång per källa → flera sends delar samma buffer (ingen extra mixning). Summa av A+B = A + B eftersom `addFrom` är linjär.
- Gamla vägen + matris på samma kanal = **summa**: två `addFrom` efter varandra. Flyttalsordning fast (gamla först), så det är deterministiskt.
- INSERT-läget (`dryGain = 1 - amount`) gäller bara gamla kanalen. Matrisen är alltid SEND (torrt orört). Beslut att skriva i hjälptexten.
- `isActive (c)`-kollen behålls: kanaler utan effekter och utan sends kostar inget (`process` hoppar `idle`).

### Ramp
`sendRamped`: `sent[channel] = true; buffer.addFromWithRamp (ch, 0, src, n, from, to)` (JUCE, linjär ramp över blocket), höger via samma pekare om null. `lastSendTo[11][numEffectChannels]` är en medlem i processorn (`std::array<std::array<float,12>,11>`, 132 flyttal), nollas i `prepare`/reset. Första blocket efter laddning: `from = 0` ger en mjuk start, inget klick. Gamla vägen rampas inte (oförändrad). Stor blockstorlek 1: ramp över 1 sample = `to`; ok.

### Tester
- `testFxSendMatrix` (ny, `Tools/AudioTests.cpp`): (a) A=0.5 och B=0.3 samtidigt = render(A ensam) + render(B ensam) inom 1e-6 på respektive kanals `fxL`/bus; (b) matris på samma kanal som gamla `fxAmount` = summa; (c) blocksize 1/64/512 ger samma resultat inom 1e-5 (rampen ändrar bara över blockgräns, jämför med konstant belopp); (d) kick (mono) och instrument.
- Gammalt preset bit-identiskt: `rendersigwide`/`rendersig` oförändrade (alla nya 0 → inga `sendRamped`-anrop); kör före och efter och jämför hashar.
- Idle-CPU: kanal utan effekter och utan sends ger 0 extra anrop (räknare i testbygget eller `process` idle-flagga).
- `paramtest` (alla 132 nya går att sätta/läsa, standard 0) och `helpcheck` (24 texter via regeln).
- `Debug all` grön före commit. DAW-test i `docs/work/fl-testlista.md`: flera sends från kicken, kanal 12 sidechain, gamla presets, automation av ett sendbelopp utan klick.

### Samordning med Arbetare 1 (steg 1, `fx12`)
| Arbetare 1 (`fx12`) | Arbetare 2 (steg 3) |
|---|---|
| `Effects.h:23` `numEffectChannels`, `PluginBase.cpp:13` static_assert, `Mixer.h:15`, `EffectsPage.h:79`, `PluginBase.h:616`, `fxChannelChoices` (`Params.cpp:68`), `MixerPage`, `fxL`-ID:n, `FxProcessor.cpp:144-150` | `Params.h:145`+`:68`, `Params.cpp` (efter l.420 och i kickens funktion), `InstrumentProcessor.cpp:990-1016`, `Effects.cpp:2427` (ny `sendRamped`), `Presets.cpp:269`, `PluginBase.cpp:92-104/1315`, `Help.cpp`, `Components.cpp`/`Modules.cpp` |
- Enda gemensamma yta: `Params.cpp`/`Params.h` (båda lägger till sist) och `Presets.cpp:269` (hårdkodad 11, Arbetare 1 ändrar siffran, jag lägger till fält på samma rad → **konflikt troligt**; föreslår att Arbetare 1 tar den raden och jag rebasar).
- Loopgränsen använder `numEffectChannels`, så koden fungerar med både 11 och 12. Steg 3 byggs ovanpå `fx12` (rebasa efter att den är mergad) eller mergas i ordning; PL beslutar.
- `lastSendTo`-storleken följer `numEffectChannels` (ingen egen siffra).

### Öppet (PL)
1. Ska "ta bort FX"/`turnOff` också nolla matrisen? (förslag: ja för vald kanal, annars ligger sends kvar mot en tom kanal.)
2. Rampen: 1 block (~10 ms) räcker; vill PL ha egen tid?

### Besked från PL (2026-10-28) – beslut som läggs in i planen
- **turnOff (`PluginBase.cpp:1845`):** nollar matrisen (`fxSendTo<c>`) för vald kanal och källa, men bara när användaren aktivt tar bort FX. Aldrig vid laddning av preset.
- **Ramp:** ett block räcker om blocksizetestet (1/64/512) inte visar klick. Visar det något: förläng till 2-3 block.
- **Presets.cpp:269:** Arbetare 1 tar raden i steg 1. Jag rebasar steg 3 på `fx12` efter hennes steg 1.
- **Summa:** Tobias har inte bekräftat. Anta summa men håll det lätt att ändra (en enda plats: ordningen/anropet efter den gamla `send` i `InstrumentProcessor.cpp`).
- **Start:** ingen kod i steg 3 före PL:s besked (steg 1 klart först). Ingen push.

### Steg 3 – kod klar, byggs (2026-10-28, Arbetare 2)
Gren `fx-matrix` (från `pl-presets`), ingen UI, `numEffectChannels` = 11 tills `fx12` rebasas in.
- Params: `fxSendTo1..12` per lager (`Params.h`), `kickFxSendToIds[]`, `fxMuteIds[]`; registreras sist i `Params::addFxMatrixParams` (`Params.cpp`, anropas sist i `createLayout`).
- Ljud: `InstrumentProcessor::sendMatrix` (efter den gamla sendet, summa på samma kanal – ändras på ett ställe), `EffectRack::sendRamped` (1 block), mute i `EffectRack::process` (~5 ms glidning, kanalen kör vidare).
- `PluginBase.cpp`: `fxChannelUsageMask` räknar matrissends; `copyTrack`/spårpreset tar med `fxSendTo<N>`; "export utan effekter" nollar matrisen. `turnOff` i UI:t kommer med UI-steget (ingen UI nu).
- `Help.cpp`: regel för `FxSendTo` (24 ID) och `fxMute` (12 ID).
- Test: `KickbaseTool fxmatrixtest` (`Tools/AudioTests.cpp`). Rebase på `fx12` + exkluderingslistan i `Presets.cpp:269` återstår.

## Steg 3 – klar (28 okt)
Debug all grönt; fxmatrixtest, helpcheck, paramtest ok. Rendersig-jämförelse mot main avförd (Tobias bygger nya presets; rendersig varierar dessutom mellan körningar). Kvar: UI-turnOff, AI-namnkarta (~PluginBase.cpp:1318), rebase på fx12 + matris-ID:n i Presets.cpp:269-listan.

## Steg 4 – popup-UI, klart (28 okt)
Debug all grönt (inkl. helpcheck, fxmatrixtest). `Components.cpp/.h` `FxSendControl` + `FxSendPopup` (sammanfattning, rad per kanal, på/av, %, INS/SEND-rad, `+ add FX channel`). `Effects.cpp` `visibleChannels/addChannel/removeLastChannel` (global `fxChannelCount` på apvts.state, inte i preset). `EffectsPage` rullista + ta bort-fråga. `MixerPage` 12 kanaler + M-knapp (`fxMute1..12`). Matris-ID undantagna i `Presets.cpp`. Kvar: `PresetButton.cpp:369-378` loopar `numLegacyEffectChannels` (FX12 kan inte väljas; Arbetare 1:s kod). Plan för parameterrader: `docs/work/plan-pianoroll-param-lanes.md`.

## Fas 2 – parameterrader, data (28 okt)
Debug all grönt. `Pattern::Note::lock[3]` (cutoff, pan, fxsend; -1 = osatt) med `setLock/clearLock/getLock`; sparas som valfri `locks`-egenskap (Pattern version 8) och `lockN` i .roll (version 4); undo täcks via toValueTree. MCP: `set_pattern` tar `cutoff/pan/fxsend` per ton, `repeat_to` kopierar, `get_pattern` visar `locks [tick, parameter, value]`. Test: `testNoteLocks` (EngineTests.cpp). Även `PresetButton.cpp:369-378` rättad (`numEffectChannels`). Kvar: fas 1 (UI, efter Arbetare 1:s PianoRoll-commit), fas 3 (ljud).

## Fas 3 – parameterrader, ljud (28 okt)
Debug all grönt. Lås gäller från anslaget tills nästa ton ändrar dem: cutoff exakt vid anslaget (`InstrumentVoices::setCutoffLock`), pan via `Mixer::setPanLock` (20 ms-ramp, ingen klick), FX-send per block med glidning (`InstrumentProcessor.cpp` `applyNoteLocks` / `finishBlock`). Ljudtråden läser bara atomics; allt förberäknas i `prepareToPlay`. Nytt `locktest` (`Tools/AudioTests.cpp`): cutoff-lås = samma cutoff som parameter (skillnad 0), pan-lås 0 = pan -1 som parameter, send-lås 0,3 = fxAmount 0,3, inget hopp. Rendersig-trion (rendersig, wide, tb303), audiotest, fxmatrixtest oförändrade. Kvar: fas 1 (UI), blocksizetest-felet vid röststöld (PL-order).

## blocksizetest – röststöld (28 okt)
Debug all grönt. 20/20 sekventiella körningar (lägen 2,7) utan fel. Fel syntes bara i parallella körningar, vid kallstart och en gång i build.bat. Orsak ej bevisad; misstänkt: `Wavetables::get` (Wavetable.cpp:631) lämnar Basic tills `TableBuilder` är klar. `Tools/AudioTests.cpp` `blockSizeTest`: filter `KB_BS_MODE`, determinismkontroll (block 32 två gånger), och `Wavetables::ensureAll()` före första renderingen (ab929cc). Fynd på vägen: `ParamRefs::readKick` allokerade på ljudtråden (186b570). Jämförelse mot main/57ddb1a avförd (PL). Kör blocksizetest sekventiellt.

## 0.4 – plan (28 okt, main inmergad i fx-matrix)
Punkter från PL: (1) gamla FX-länken, (2) ta bort FX-kanal från popupen med ångra, (3) PATTERN-raderna / "N x FX", (4) FX-lane väljer FX-kanal.
1. Gamla länken. Motorn: INSERT är standard (`fxSend` av): torrt minskar med beloppet (`InstrumentProcessor.cpp:1010`). Matrisen är bara SEND (linjär summa, `:1092`). INS har ingen motsvarighet, så att flytta INS till en matrisrad skulle ändra ljudet. Beslut: vid laddning flyttas bara länkar i SEND-läge (kanal N, belopp a, fxSend på): `fxSendToN += a` (max 1), `fxChannel=0`, `fxAmount=0`, `fxSend=av`. INS-länkar lämnas orörda och visas i popupen som en egen rad "INS FXn" (inte som dubblett av matrisraden). Inga ID:n ändras; migreringen sker på värden i apvts efter laddning (en gemensam funktion, anropas från setStateInformation och preset-laddning), inget nytt i hash. Test: SEND-länk -> matrisrad, samma ljud (diff 0 mot gamla vägen); INS -> oförändrat.
2. Ta bort kanal från popupen: `Effects::removeLastChannel` finns (bara sista). Lägg en knapp på sista raden i popupen med bekräftelse (finns på FX-sidan) och ångra: `Effects` tar en ögonblicksbild av kanalen (effektslots, EQ, kompressor, nivå, alla sends i alla källor) före borttagning och återställer vid ångra. Ångra som knapp "UNDO REMOVE" i popupen/FX-sidan så länge sessionen lever.
3. PATTERN: ta bort FX-ratten (`StepSequencer.cpp:33,159`) efter kontroll att inget annat läser raden; VOL-ratten större. FX-knappen visar "N x FX" / "FX" / "None"; tooltip listar kanalerna (`Components.cpp:706`).
4. FX-lane (steg b): lanen får ett kanalval; lane-värdet ersätter matrissenden för den kanalen. Kräver pianorullkod och lås-format: samordnas med Arbetare 1 (kickbase-vst-b6) innan jag rör något; jag rör inte PianoRoll.cpp.
Ordning: 1, 3, 2, sedan 4 efter svar från Arbetare 1. Ljudkod (InstrumentProcessor) ändras bara för punkt 4; PL får veta först.

### 0.4 punkt 1 – klar (28 okt, Arbetare 2)
- `Effects::migrateSendLinks` (Effects.cpp) flyttar SEND-länkar till `fxSendToN` vid laddning (PluginBase.cpp:828). INS orörd, visas som "INS FXn" i popupen. Inga ID/hash ändrade (presets räknar inte fxChannel/Amount/Send).
- Test: `testSendLinkMigration` (MasterTests.cpp). `Debug all` grönt.
- Känt: FX-lås (`fxSendLock`) styr bara gamla länken; migrerade länkar tappar låset tills punkt 4 är klar.

### 0.4 punkt 3 – klar (Arbetare 2)
- FX-ratten borttagen från PATTERN-raderna (`StepSequencer.cpp` addRowKnobs, RowKnobs, rubriken "FX"); VOL-ratten större (34 px, 30 i låga rader), layouten i låga rader flyttad vänster (labelWidth 226).
- FX-knappen visar "None", "FX" (en kanal) eller "N x FX"; tipset listar kanalerna med procent (`Components.cpp` FxSendControl::updateSummary).
- Debug all grönt. Ingen ljudkod ändrad.

## 0.4 punkt 2 – klar (Arbetare 2)
- FX-popupen har knappen '- remove FXn' (Components.cpp, FxSendPopup::askRemoveLast). Bekräftelse om kanalen har effekter eller sends; Ctrl+Z återställer kanalen, effekter, EQ, kompressor, nivå och sends.
- Effects.cpp removeLastChannel: egen undo-transaktion, fxChannelCount skrivs via undo-managern. Test: 'ta bort FX-kanal och ångra' (MasterTests.cpp). Debug all grönt.

## 0.4 punkt 4 – PAUSAD (projektledaren, 28 okt)
- Skriven men obyggd och ocommittad i worktree `KickBase-VST-w2`, gren `fx-matrix` (git status visar ändringarna): `Pattern.h` (lockFxChannel, fxChannelToLock/lockToFxChannel), `PianoRoll.h:131,259`, `PluginBase.h:433`, `InstrumentProcessor.h/.cpp` (fxSendLockChannel/Amount, applyNoteLocks, sendMatrix, static_assert), `ToolLayer.cpp`, `Tests/EngineTests.cpp` (FX-kanallås-test).
- Kvar när den återupptas: bygg Debug all, kör rendersig/rendersigreal/rendersigwide mot baseline, meddela Arbetare 1 om API:t, commit.

## 0.4 (r) – harmoniska editorn: förklaringar (Arbetare 2)
- Tooltips per fält (WAVEFORM / dB / PHASE) via `HarmonicEditor::getTooltip` (HarmonicEditor.cpp:~71, juce::TooltipClient), HELP-knapp i fönstret, ny hjälpflik HARMONICS (HelpDialog.cpp `harmonicsText`, Topic::harmonics sist i enumen). Bara UI-text. Debug all grönt.

## 0.4 (u) – en provlyssning i taget (Arbetare 2)
- `InstrumentProcessor.cpp` (audition-dispatchen i processChunk + `handleLiveMidi`): lokal provlyssning (kick, källa, instrument 0/5/200–599) stoppar huvud-PREVIEW; ny PREVIEW släpper hållen/solo-provlyssning; en tangent/MIDI-ton stoppar PREVIEW via `stopPreview()` (släpper även basrösterna). Knapparna släcks via `previewingLayer`, inga UI-filer rörda.
- `Debug all` grön. Ingen ny test; ingen render-ändring i vanlig uppspelning.

## 0.4 (h) – OSC-diagrammet visar utsignalen (Arbetare 2)
- Ny delad hjälp `Source/Ui/OscRender.h` (`OscRender::render`): motorns utsignal med warp, phase, unison, detune och blend. Återanvänds av INSTR-diagrammet (f steg 2).
- `OscPanel.cpp` drawCurrent: SCOPE-läget heter nu OUTPUT och använder hjälpen (2 perioder, 24 vid unison > 1). Vyvalet är UI-läge (MODE-etiketten), ingen parameter, sparas inte. Modules.cpp orörd.
- Debug all grönt.

## 0.4 (l) – Tidsdiagrammet i TIMING-fliken (Arbetare 2)
- Ny komponent `Source/Ui/TimelineChart.h`: en bana per motor på gemensam tidsaxel + gemensam kontur (volym-envelopen) längst ner. NUDGE flyttar banornas start, POLARITY vänder konturen. Spelhuvud/släpplinje som i de små diagrammen.
- Vald bana är 3x högre och markerad; drag flyttar POS DELAY (vänsterkanten) eller LENGTH (högerkanten, närmast kanten), inte med SYNC på. Klick på annan bana väljer motor. `editable=false` ger skrivskyddat läge för INSTR (klick anropar bara `onSelect`).
- `InstrTimingPanel`: knapp OVERVIEW/FOCUS (FOCUS först, valet kvar tills programmet stängs, UI-läge, ingen parameter). FOCUS = diagram + vald motors reglage.
- INSTR-placeringen (f) är kvar: komponenten är klar att läggas in med `editable=false` (kräver små hunkar i Modules.cpp, Arbetare 1).
- `Debug all` grön.
