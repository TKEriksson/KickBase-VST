# Projektinstruktioner

- Läs docs/DESIGN.md för synthens design.
- Läs docs/sound-design-roadmap.md – v1 ska byggas så att fas 2–3 (skill + offline-renderloop) blir lätta att lägga till.

## Starta här (ny chatt / efter sessionsgräns)

När Tobias bara skriver "fortsätt": läs i den här ordningen, och fortsätt sedan där senaste raden i progressfilen slutar.

1. `docs/work/PROGRESS.md` – nuläge och nästa steg. Jobbar du i en worktree med egen fil (t.ex. `PROGRESS-lc.md`) läs den också.
2. `docs/plan-v2.md` – planen för v2 (faser, bl.a. R, M, LC).
3. `docs/work/LESSONS.md` – lärdomar och testregler (kör bara relevanta tester, hela sviten först vid fasens slut).
4. Specar för pågående fas, t.ex. `docs/work/m-spec.md`.

Arbetssätt:
- Svara på svenska. Tobias har ont om tid: korta svar, förklara orsaken till fel och peka ut `fil:rad`.
- Claude skriver all C++ (Tobias vill inte skriva kod här).
- Opus (projektledare, session `kickbase-vst-c3`) fördelar uppdrag och mergar godkända faser till dev. Andra sessioner är arbetare.
- Arbeta bara i den worktree/branch du fått. Rör aldrig huvudmappen från en annan worktree. Pusha och mergea aldrig utan att Tobias eller Opus sagt det.
- Inga nycklar eller tokens i repot.
- Håll sessioner korta: uppdatera progressfilen före sessionens slut och föreslå ny chatt per uppgift.
- Nya parametrar läggs sist i layouten och nya modulationskällor sist i källistan (`docs/ENGINEERING.md`), så att sparade projekt och automation förblir kompatibla.
