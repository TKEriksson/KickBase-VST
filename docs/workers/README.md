# Arbetsyta för Codex, Copilot och projektledaren

Projektledare: Claude (session `kickbase-vst-27`, byts per chatt; `ListAgents`). Tobias ser allt här.

## Regler (korta)
1. **En fil per arbetare:** `CODEX-CLAUDE-HANDOFF.md`, `COPILOT-CLAUDE-HANDOFF.md`. Projektledaren skriver bara under `## Inbox` (ett daterat uppdrag per rubrik); arbetaren svarar under samma rubrik (`Status`, `Resultat`, `Frågor`).
2. **Rör inte `Source/`, `Tests/`, `CMakeLists.txt` eller grenarna `content-*`** om uppdraget inte säger det. Byggarbetarna (Sonnet) äger koden.
3. **Skriv resultat som filer i `docs/research-notes/`**, inte som chatt. Projektledaren läser bara sammanfattningarna (spara tokens). Rapportera i handoff-filen med filnamn + max 5 rader.
4. **Repot är publikt.** Ingen ordagrann kopia av transkript, PDF:er, ljud, bilder eller andra människors text. Skriv egna ord, korta regler, källa = titel. Inga nycklar eller personuppgifter.
5. **Märk osäkert:** `[verifierad]` (hör till tal ur flera källor, eller du har kontrollerat), `[en källa]`, `[osäker]`. Länkar och sidor som en AI tagit fram (t.ex. `transcripts/guidesonline.txt.txt`) räknas som `[osäker]` tills du öppnat dem.
6. **Fråga inte om små saker.** Välj standardvalet och skriv det. Blockerad: skriv `BLOCKERAD:` överst i svaret.
7. Dokumenten ska vara korta och täta: max ~150 rader per fil, tabell eller punktlista, tal framför adjektiv.

## Hjälp mellan agenter och tidsgräns
- **Uppdaterat 20 okt 10:15:** Codex, Copilot och Gemini antas ha nått limit. Delegera inget dit; byggaren (kickbase-vst-bb) ber projektledaren om hjälp med läs-/sammanfattningsjobb. Tidigare delegerade uppdrag återtas (`ÅTERTAGET <tid>`). Research-notes ägs nu av Claude-research-agenten.
- **Agenter kan sluta svara när deras limit nås.** Skriv `Status: <tid>` (klockslag) överst under ditt uppdrag när du börjar och när du slutar ett delmål.
- Regel: ingen statusuppdatering eller filändring på ~30 min under ett aktivt uppdrag = agenten antas stoppad. Projektledaren (eller byggaren) tar då tillbaka uppdraget, skriver `ÅTERTAGET <tid>` under det och gör det själv eller ger det till en annan agent. Halvfärdiga filer i `docs/research-notes/` markeras `[ofullständig]` och slutförs, inte skrivs om.
- Kolla `ListAgents` och filernas ändringstid (`Get-ChildItem docs/workers, docs/research-notes`) innan du återtar.

## Format för en regel (gäller alla research-notes)
`**Regel** (tal/intervall) — varför — *Vårt reglage/verktyg:* parameter eller "SAKNAS → verktygsidé" — källa [märkning]`

Avsluta varje fil med två listor: **Verktygsidéer** (vad vi måste koda) och **Analysatoridéer** (vad vi måste mäta/visa).
