# AUTO FIX, mätdelen utan UI: plan (Arbetare 2, 2026-10-28)

Uppdrag: `docs/masterplan.md:135-136`. Mätningar per instrument mot kicken som ren funktion + åtgärdslista (före/efter) som Arbetare 1 senare kopplar till UI och APPLY. Inget UI här.

## Rekommendation: alternativ B (finns redan), mätdelen föreslår den
| | A: instrumentkopia utan grundton | B: FUNDAMENTAL DUCK (kickens envelope dämpar basens grundton) |
|---|---|---|
| Status | finns inte | **byggd** av Arbetare 1 (`Source/Core/FundamentalDuck.h`, `InstrumentProcessor.cpp:410-428,939-984`, `docs/school/fundamental-duck.md`) |
| Kostnad | tar ett av 3 instrumentlager (`Params.h:25`), ändrar mönstret | inget lager, inget mönster; bara parametrar |
| Underhåll | kopian driver från originalet när presetet ändras | följer originalet, av = bit-identisk bypass |
| Precision | hel grundton borta även där den inte krockar | filter på tonens grundton, djup följer kickens envelope och fönster (cent) |
| Risk | ändrar ljudkaraktären (basen tappar tyngd när kicken inte spelar) | smal peaking (Q 1,4), liten ljudförändring utanför kickslaget |

Slutsats: B. A skulle bryta mot "fundamental borttagen bara där den krockar" (masterplan.md:135) och kosta ett lager. Samma val som Arbetare 1 gjorde (`PROGRESS-w1.md:280-291`).

## Vad som byggs (nytt, rent core, inga nya parametrar)
- `Source/Core/AutoFix.h/.cpp`, namnrymd `AutoFix`. Bara std (inga JUCE-typer), så att det testas på påhittade data.
- **Indata** (`AutoFix::Input`): kickens `tailHz` (mätt, annars design), per instrument: noter (starttid, längd, grundton Hz, velocity), nuvarande inställning (duck på/djup, FUNDAMENTAL DUCK på/djup/fönster, fas-/rotationsvärde) och mått från `MixAnalysis::PhaseMetrics` (medelkorrelation, kick-/basnivå per 16-del) samt överskott runt 200 Hz (dB mot grannband).
- **Mått** (`AutoFix::Measurement`, per instrument):
  1. grundton mot kickens tailHz per ton, avstånd i cent, `collides` = inom fönstret (standard 100 cent, samma som FUNDAMENTAL DUCK WINDOW);
  2. nivåskillnad bas minus kick (dB) i lågbandet;
  3. fas: viktad medelkorrelation kick/bas och andel 16-delar under −0,3 (släcker ut);
  4. överskott runt 200 Hz (dB), flaggas över 3 dB.
- **Åtgärder** (`AutoFix::Action`): `id`, instrument, `param` (symboliskt namn), `before`, `after`, `reason`, `severity`. Förslag:
  - krock + FUNDAMENTAL DUCK av → slå på, djup efter krockens storlek, fönster efter spridning i cent;
  - nivå: bas över kick i lågbandet → sänk velocity/nivå på krockande toner (före/efter i dB);
  - negativ korrelation → PHASE ROTATION-förslag;
  - 200 Hz-överskott → smal sänkning (EQ-förslag, `after` i dB);
  - sidechain-djup för litet för att nå målet → höj djup.
  Allt är **kandidater**, inget ändras. Funktionen är deterministisk (samma indata = samma lista) och ordnar åtgärderna efter allvar.
- `Tests/AutoFixTests.cpp` (registreras i `CMakeLists.txt` bredvid övriga tester), påhittade data: krock/ingen krock, fönsterkant, tom indata, determinism, att redan påslagen duck inte föreslås igen, ordningen.

## Gränser (v1, ärligt)
- "Hörbar krock" är ett mått, inte ett omdöme; glide, LFO-pitch och ackord mäts bara vid nominell ton; krock via 2:a övertonen hanteras inte.
- Fas-/200 Hz-måtten kommer från en analyskörning (`MixAnalysis`); kopplingen från körning till `Input` gör Arbetare 1 tillsammans med UI:t. Jag levererar en liten hjälpare `fromPhaseMetrics`.

## Arbetssteg
1. Plan klar och rapporterad (denna fil).
2. Bygga `AutoFix.h/.cpp` + tester, `build.bat Debug all` grönt.
3. Logga i `PROGRESS-w2.md`, commit, ingen push.
