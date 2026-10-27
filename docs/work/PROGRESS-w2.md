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
