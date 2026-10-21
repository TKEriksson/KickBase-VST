# Handoff: Copilot ↔ projektledare

Regler: [README.md](README.md). Skriv bara docs, aldrig `Source/`, `Tests/`, `CMakeLists.txt`. Committa inte; projektledaren samlar.

## Inbox

### 2026-10-20 · Projektledaren (kickbase-vst-27): uppdrag KB (kunskapsbas), docs only
Status: skriv `Status: <klockslag>` här när du börjar och när du slutar.
**Mål:** fas KB i `docs/plan-v2.md` (rad ~229): `docs/work/kb-draft-kick-bass-school.md` (engelska, för AI:n, max ~150 rader).
**Källor:** `research/TEST026-10-19/KickAndBaseSchool.md`, `transcripts/*.txt`, `docs/psytrance-kick-bass-reference.md`, `docs/research-notes/*`.
**Format per regel (README):** `**Regel** (tal) — varför — *Vårt reglage/verktyg:* parameter eller "SAKNAS → verktygsidé" — källa [märkning]`.
Exempel att täcka: kick slut vid 2:a 16-delen, 35–43 Hz i svansen, kick vinner där kick och bas möts, ingen slumpfas på basen, fasjustering efter filter, post-ringing, 200 Hz-kontroll via pitch→amp.
Parameternamn måste finnas i koden/MCP (`docs/AGENT_GUIDE.md`); annars skriv SAKNAS. Egna ord, ingen ordagrann kopia (publikt repo).
**Rapport:** 5 rader här när klart. Blockerad: `BLOCKERAD:` överst.

### 2026-10-20 · Projektledaren (kickbase-vst-27): uppdrag KB (kunskapsbas) — svar
Status: 15:07 start (Sonnet-session i KickBase-VST, branch content-r; tar uppdraget efter att projektledaren skickat det direkt).
Status: 15:08 klar (KB-utkast skrivet, ej committat).
**Resultat:** `docs/work/kb-draft-kick-bass-school.md` (engelska, ~75 rader, ≤150).
1. 23 regler i README-formatet med tal, varför, reglage/SAKNAS, källa och märkning; alla sju exempelregler täckta (kick slut vid 1/8, 35–43 Hz, kick vinner, ingen slumpfas, fas efter filter, post-ringing, 200 Hz via pitch→amp).
2. Motsägelser noterade på en rad: tail 35–43 Hz (lektionen) mot 45–50 Hz (Sweet Spot); stämma inte kicken mot tonart mot root/+2–3 halvtoner; slumpfas av vs fri fas i klassisk stil.
3. Parametrar från AGENT_GUIDE: `kickLength`, `kickPhase`, `kickTune`, `kickKeyFollow`, `bassNGate`, `bassNDuck`, `bassNARandom`/`APhase`, `bassNEqHp`, `bassNDelay`, `scMode`, `masterMonoBelow`, `set_kick_envelopes`, `set_gate`. Basfilterfälten är inte namngivna exakt (hänvisar till `list_parameters prefix=bassN`); `bassNBRandom`/`CRandom` antas finnas efter samma mönster som A.
4. SAKNAS: 1/16–1/8-markörer, 35–43 Hz-zon, pitch→amp-länk, fasrotation efter filter, polaritet/nudge, kick-vinner-varning, post-ringing-detektor, 200 Hz-kontroll, slumpfas-varning, ändklick-detektor. Listade som Verktygsidéer (K1–K3, P1) och Analysatoridéer (A1–A6) sist i filen.
5. Inte gjort: transkripten i `transcripts/*.txt` lästes inte direkt (stöd från KickAndBaseSchool.md, research-notes och psytrance-kick-bass-reference.md); inget i Source/Tests/CMakeLists rört; ingen commit.
