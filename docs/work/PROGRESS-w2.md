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
