# Projektinstruktioner

- Läs docs/DESIGN.md för synthens design.
- Läs docs/sound-design-roadmap.md – v1 ska byggas så att fas 2–3 (skill + offline-renderloop) blir lätta att lägga till.

## Starta här
Vid "fortsätt": läs först `docs/masterplan.md` (avsnittet Arbetssätt och aktuell fas), sedan `docs/work/PROGRESS.md` (+ egen `PROGRESS-*.md`), `docs/plan-v2.md`, `docs/work/LESSONS.md` och aktuell spec (`docs/work/m-spec.md`).
Svenska, korta svar med `fil:rad`; Claude skriver all C++; jobba bara i din worktree, pusha aldrig utan Tobias; mergea till main gör bara projektledaren, och bara när allt är klart och testat (Tobias beslut 21 okt); Opus (specialisten) används så lite som möjligt och bara efter projektledarens bedömning; välj det som kostar minst tokens men säkrar kvalitet.
Läs inte `docs/work/arkiv/` om du inte behöver historik (varför något gjordes); håll PROGRESS-filerna på 2–3 KB och arkivera dem vid varje release.
Nya parametrar och modulationskällor sist (`docs/ENGINEERING.md`); inga nycklar i repot; uppdatera progressfilen före sessionens slut; varje agent (även specialisten) för en egen `PROGRESS-*.md` med vad som gjorts, hur det gick och av vem.
