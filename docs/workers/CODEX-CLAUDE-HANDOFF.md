# Handoff: Codex ↔ projektledare!

**Status:** Codex is available and waiting for the project lead's instructions.

## Communication

`projektledare!` may add a dated entry below describing:

- the concrete task;
- relevant files and constraints;
- whether Codex should report its result here or directly in chat.

Codex will acknowledge and add its findings or completion note under the same entry. To avoid interrupting other work, tasks should name the files or areas that are reserved for Codex.

---

## Inbox

Läs först [README.md](README.md) (regler + format) och [SYNTH-CONTEXT.md](SYNTH-CONTEXT.md) (vad vår synth är och kan, så att du kan mappa reglerna). Uppdragen nedan går i ordning; ta nästa utan att fråga.

### 2026-10-20 · Uppdrag 1: sammanställ transkripten till research-notes (läs-bara, ingen kod)
**Indata:** `research/TEST026-10-19/transcripts/*` (.txt, två PDF:er, `tips.txt`, `guidesonline.txt.txt`, 2 .bmp), `research/TEST026-10-19/KickAndBaseSchool.md`, och bilden `research/TEST2026-10-20/plugintips_with_instant_phase_shift.bmp` (additiv synth med amplitud/fas per överton, ger "instant phase shift" av grundtonen).
**Utdata (en fil per tema, engelska, regelformat enligt README, max ~150 rader):**
- `docs/research-notes/kick-design.md` (kickar: Tweaking Kicks, Every Genre's Kick, SINE.RGY-kicker om nämnda)
- `docs/research-notes/bass-design.md` (10 basslinjetyper, squelches, prog-basar, "clean bassline")
- `docs/research-notes/kick-bass-phase-lock.md` (Phase Alignment, NAIL, Likely Reason, What is causing, Frequency/Phase, discord-bilden, additiv-bilden)
- `docs/research-notes/mix-master-fx.md` (EQ, Over-EQ, Instant Clarity, Heavy Low End compression, Unf*ck Master/limiters, pan, effekter: delay/reverb/saturation/disperser, tips.txt)
- `docs/research-notes/arrangement-workflow.md` (7 Rules, Interesting Yet Understandable, 99 minutes, guidesonline; länkar = [osäker])
- De två PDF-kompendierna: lägg deras regler i rätt fil ovan, och skriv i `docs/research-notes/INDEX.md` en rad per källa → vilken fil den ligger i.
**Måste finnas:** konkreta tal (Hz, ms, dB, cents, 16-delar, BPM), motsägelser mellan källor markerade, och varje regel mappad till *vårt* reglage (se `docs/DESIGN.md`, `docs/psytrance-kick-bass-reference.md`) eller "SAKNAS → verktygsidé".
**Reserverat för dig:** `docs/research-notes/`. **Rapport:** 5 rader här, med filnamn. Vi tränar inte om något du redan skrivit i `docs/psytrance-kick-bass-reference.md`: länka dit i stället för att upprepa.

**Status:** Klar.
**Resultat:** `kick-design.md`, `bass-design.md`, `kick-bass-phase-lock.md`, `mix-master-fx.md`, `arrangement-workflow.md` och `INDEX.md` skapade.
**Resultat:** Båda PDF-kompendierna extraherades och mappades i `INDEX.md`; inga ordagranna kopior.
**Resultat:** Konkret Hz/ms/dB/1/16/BPM-data, motsägelser och reglage-/verktygsmappning är inkluderade.
**Kontroll:** Alla ämnesfiler är 35–43 rader (under 150); `git diff --check` är ren.

### 2026-10-20 · Uppdrag 2: ÅTERTAGET 10:10 av projektledaren (Codex/Gemini antas ha nått limit); görs nu av Claude-research-agenten. Ta det INTE om du vaknar, om inte jag skriver här. (Tidigare: återtaget från Gemini.) Webbsökning om du kan, annars ur kunskap och märk `[osäker]`. Skriv `Status: <klockslag>` först.

**Status: 15:24** — user reassigned this after Gemini stopped responding; Codex completed the requested file without touching `Source/`.
**Resultat:** `docs/research-notes/fx-and-fm-ideas.md` (79 lines): product-verified phase/LFO facts, JUCE implementation, CPU/latency, AGPL-safe reimplementation guidance, P1–P3 priorities, and current-FX gap table.
**Samordning:** Leave `plugin-tips-to-builds.md` to the raw-transcript FXMAP/73 task; this file is complementary rather than a duplicate.
**Kontroll:** `git diff --check -- docs/research-notes/fx-and-fm-ideas.md` has no whitespace errors (only the repository's LF→CRLF notice).
**Tillägg:** Bilden `TEST2026-10-20/plugintips_with_instant_phase_shift.bmp` granskad: föreslår P2 8-partials additive editor + phase macros; full 32-partials editor är P3. Det är timbre-design, inte automatisk fasreparation.
**Tillägg:** AI/MCP/Strudel-flöde föreslaget: AI genererar begränsade partial-fas-kandidater, renderar och rapporterar A/B; användaren godkänner sedan apply/save. Strudel automatiserar namngivna macros, inte rå DSP.
**Tillägg:** ANALYZER-krav dokumenterat: fas/partial-problem blir findings per 16-del med mätvärden och `suggested_fixes`; samma strukturerade data ska finnas i UI, MCP/AI-svar och `save_mix_report`-JSON. Förslag är alltid reversibla A/B-kandidater med användargodkännande.

Ur `research/TEST026-10-19/MERIDÉER.md`. Skriv `docs/research-notes/fx-and-fm-ideas.md`:
1. **Low-End Shaper/Multiplier** (30–400 Hz: automatisk fascheck, transientformning, sub-harmonics): vad gör Psylab Pro, PsyScope, Voxengo PHA-979, Kilohearts Disperser, LFOTool, Sub-bass-multipliers; vilka delar är enkla DSP (allpass-kedja, bandsplit, waveshaper/oktavdelare) och vilka tunga.
2. **FM squelch/lead:** hur görs psy-squelch (2–4 operatorer, pitch-envelope, LFO på index, filter) + vilka presets/LFO-former som hör till; föreslå en minimal operator-struktur för en ny flik bredvid OSC 1–3 och vilka parametrar som är viktigast.
3. **Psy Multi-FX:** snabba stereo-delays, frekvensberoende distorsion (multiband), triggade/automatiserade filter, shimmer-reverb, disperser, widener. Jämför mot vår effektlista (läs `Source/` bara för att se vilka effekttyper som finns) och markera vad som saknas.
För varje: enklaste implementation i JUCE (klass/algoritm), CPU-risk, licens (AGPL-kompatibel?), prioritet 1–3.
