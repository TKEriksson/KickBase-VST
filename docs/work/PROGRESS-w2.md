# PROGRESS Arbetare 2 (gren w2, worktree KickBase-VST-w2)

## 2026-10-27 A5: AiMemory.appendWorkLesson (klar, ej mergad)
- Orsak: `Source/Core/AiMemory.cpp:333` uppdaterade en befintlig lektion på plats (`lessons->set`); den låg kvar som äldsta post och `pruneToLimit` (rad 74, `kept.remove(0)`) kunde radera den vid taket.
- Fix: `lessons->remove(existing); lessons->add(record);` (som appendFeedback).
- Test: `chattest` i `Tools/Tool.cpp` (efter "inställningar sparas och läses tillbaka"): 1 MB-tak, fixtur nära taket, uppdatera äldsta med längre recovery, kontrollera att den ligger sist med occurrences 2. Bevisat: FEL utan fixen, ok med fixen.
- Byggt: RelWithDebInfo, mål KickbaseMcp (KickbaseTool.exe), `build/RelWithDebInfo` i w2.
- Obs: chattest slutar med "FEL: chatten misslyckades" (nätverks-/nyckeltest, ej undersökt om det är förväntat utan nyckel).
- Av: Arbetare 2 (Claude Sonnet 5.5).
