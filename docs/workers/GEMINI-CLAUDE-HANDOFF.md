# Handoff: Gemini ↔ projektledare

**ÅTERTAGET 20 okt:** Gemini svarade inte; Uppdrag 1 ligger nu hos Codex. Börjar Gemini senare: skriv `Status: <tid>` här så delar jag ut nytt (t.ex. Uppdrag 2 nedan).
**Status:** väntar på start. Skriv `Status: <klockslag>` här när du börjar och när du slutar ett delmål (se tidsgränsregeln i [README.md](README.md)).

Läs först [README.md](README.md) (regler + format) och [SYNTH-CONTEXT.md](SYNTH-CONTEXT.md) (vad vår synth är och kan).

---

## Inbox

### 2026-10-20 · Uppdrag 1: effekt- och synthidéer med webbsökning (flyttat från Codex Uppdrag 2)
Ur `research/TEST026-10-19/MERIDÉER.md`. Skriv `docs/research-notes/fx-and-fm-ideas.md` (engelska, regelformat enligt README, max ~150 rader):
1. **Low-End Shaper/Multiplier** (30–400 Hz: automatisk fascheck, transientformning, sub-harmonics): vad gör Psylab Pro, PsyScope, Voxengo PHA-979, Kilohearts Disperser, LFOTool, sub-bass-multipliers; vilka delar är enkla DSP (allpass-kedja, bandsplit, waveshaper/oktavdelare) och vilka tunga.
2. **FM squelch/lead:** hur görs psy-squelch (2–4 operatorer, pitch-envelope, LFO på index, filter) + vilka presets/LFO-former som hör till; föreslå en minimal operator-struktur för en ny flik bredvid OSC 1–3 och de viktigaste parametrarna.
3. **Psy Multi-FX:** snabba stereo-delays, frekvensberoende distorsion (multiband), triggade/automatiserade filter, shimmer-reverb, disperser, widener. Jämför mot vår effektlista (läs `Source/` bara för att se vilka effekttyper som finns) och markera vad som saknas.
För varje: enklaste implementation i JUCE (klass/algoritm), CPU-risk, licens (AGPL-kompatibel?), prioritet 1–3. Märk allt du inte kunnat verifiera `[osäker]`. Ingen ordagrann kopia av webbtexter.
**Reserverat för dig:** `docs/research-notes/fx-and-fm-ideas.md`. **Rapport:** 5 rader här, med filnamn.

### Uppdrag 2 (efter 1): verifiera länkarna i `research/TEST026-10-19/transcripts/guidesonline.txt.txt`
Öppna varje länk/källa, skriv i `docs/research-notes/guidesonline-check.md` en rad per länk: finns den, vad den säger i en mening, `[verifierad]`/`[osäker]`/`[död]`. Max 100 rader.
