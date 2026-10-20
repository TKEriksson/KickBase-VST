# Kick & bass school

Audience: the AI that designs presets through MCP. Written in our own words from lessons, transcripts and the research notes; all numbers are starting points to verify with `render_preview` / `analyse_mix` and by ear, never presets to copy. Rule format as in `docs/workers/README.md`. Labels: `[verified]` = several sources agree or we measured it, `[one source]`, `[unsure]`. A contradiction between sources is shown as a one-line note. Parameter names come from `docs/AGENT_GUIDE.md`; if no control exists the entry says SAKNAS (missing) and points to a tool idea.

Basics: 1/16 note = 15000 / BPM ms. 145 BPM → 103.4 ms. 1/8 = two sixteenths = 30000 / BPM ms (145 BPM → 206.9 ms, 144 BPM → 208 ms).

## 1. Kick: length and tail

**The kick ends at the second 16th, i.e. one 1/8 note** (30000 / BPM ms; ~206 ms at 145 BPM) — the bass needs the next 16th clear, and a longer kick pushes the bass into the tail — *Our control/tool:* `kickLength`, amp curve via `set_kick_envelopes` (up to 2 s, last node 0); `render_preview part="kick"` checks kick ≤ 1/8; SAKNAS → 1/16 and 1/8 markers drawn in the amp and pitch graphs → tool idea K1 — Kick & Bass School lesson 1, 7 Rules, project reference [verified]

**Faster tempo means a shorter kick** (recompute the end time at every BPM change) — the 1/8 window shrinks, ~214 ms at 140 BPM, ~200 ms at 150 BPM — *Our control/tool:* `kickLength`, amp envelope; the check above recomputes with BPM — Kick & Bass School lesson 1 [one source]

**The tail should settle at about 35–43 Hz** (end of the kick) — clearly above that the kick loses weight in the sub, and the lesson says fullness drops outside the range — *Our control/tool:* pitch curve via `set_kick_envelopes` (semitones above the tail, 0–72, up to 0.5 s) plus `kickTune`; the tail estimate is in the `render_preview` kick metrics (approximate under ~40 Hz); SAKNAS → shaded 35–43 Hz zone in the pitch graph → tool idea K2 — Kick & Bass School lesson 1 [one source]
- Contradiction: the Sweet Spot video proposes ~45–50 Hz as a translation-friendly tail; the lesson says 35–43 Hz. Treat both as hypotheses and A/B them; the small speakers of a club system lose very low tails.

**Do not tune the kick tail to the bass key by default** (an A tail locks the kick at 55 Hz and leaves nothing below) — psytrance kicks are short, so a key-matched tail brings little and costs sub weight — *Our control/tool:* `kickKeyFollow`=Fixed, `kickTune` — Kick & Bass School lesson 1 [one source]
- Contradiction: the project reference and other guides allow tail = bass root, or a few semitones above. Test the three alternatives (root / +2..3 semitones / fixed Hz) with the same A/B render; one source is never a verdict.

**Keep the pitch sweep untouched unless there is a reason** (a sweep of ~30–80 ms; tiny changes alter the purity of the sweep) — change one node at a time and re-measure — *Our control/tool:* `set_kick_envelopes` pitch curve, `kickPitchAmount` — Kick & Bass School lesson 1, Tweaking Kicks [one source]

**Always start the kick at the same phase** — identical hits keep identical low-end level — *Our control/tool:* `kickPhase` (fixed start), `kickSampleOffset` is not a kick/bass phase control — 7 Rules, project reference [verified]

**Last amp node is 0, check there is no click at the end** (end value below −60 dB) — a cut-off tail clicks in every bar — *Our control/tool:* `set_kick_envelopes` (enforces last node 0); SAKNAS → end-click detector → analyzer idea A2 — 7 Rules [verified]

## 2. Kick: amp envelope as a reverse EQ

**Read the amp envelope as an EQ over the pitch sweep** — the sweep runs high → low, so the amp level at time t is the level of the frequency the pitch has reached at t; a low amp early cuts highs/click, a low amp late cuts sub — *Our control/tool:* `set_kick_envelopes` (pitch and amp together); SAKNAS → clicking a frequency in the pitch graph marks the matching time in the amp graph → tool idea K3 — Kick & Bass School lesson 1 [one source]

**Control the ~200 Hz region by the amp level at the moment the pitch passes 200 Hz** — a bump there makes the kick boxy and covers the bass body; a controlled dip (about −3 to −9 dB, 10–30 ms after the transient, "fishtail") clears 200–600 Hz — *Our control/tool:* amp curve via `set_kick_envelopes`, `kickLowCut`, `kickEq*`; SAKNAS → 200 Hz excess check and a time marker for the 200 Hz crossing → analyzer idea A4, tool idea K3 — Kick & Bass School lesson 1; project reference [one source]

**Do not push the kick mids to be heard in a full mix** (clicky/plastic top, loses warmth) — a kick that is too loud in the mids pushes percussion and leads up with it; solo kick+bass can be fatter than the full track — *Our control/tool:* `kickClick*`, `kickLevel`, `kickDrive`; judge with `render_preview` of the whole mix — Kick & Bass School lesson 1 [one source]

## 3. Bass note and roll

**Every bass note is at most one 16th and ends before the next note** (about 40–80 % of a 16th as a gate start) — overlapping notes smear the pluck and the next kick — *Our control/tool:* `bassNGate` (note length scaler), `set_gate` (`bassNGateOn/GateGrid/GateDepth/GateSmooth`), `set_pattern` — project reference, 7 Rules [verified]

**First bass note after the kick is lower in velocity** (about 60–85 %) — the ear then hears the kick as the downbeat and the three bass notes as a roll — *Our control/tool:* velocity in `set_pattern` — project reference [one source]

**Where kick and bass meet, the kick wins** — the first bass 16th starts while the kick tail is still audible; bass energy there masks the tail and the sum gets louder or weaker unpredictably — *Our control/tool:* `bassNDuck`, `scMode`=Kick envelope, `scLength`, `set_sidechain_curve`; SAKNAS → "kick wins" warning comparing kick and bass level at the meeting point → analyzer idea A1 — Kick & Bass School lesson 2, project reference [verified]

**Control summed loudness by velocity or level, not by more master limiting** — if kick+bass is too loud at the meeting point, lower the first bass note, not the master — *Our control/tool:* `bassNLevel`, velocity in `set_pattern`, `analyse_mix` limiter reduction — Unf*ck Master, Kick & Bass School lesson 2 [one source]

## 4. Bass phase

**No random start phase on the bass** (`Random` = 0) — a random phase gives a different attack and different low-end level on every note, and phase alignment to the kick then cannot hold — *Our control/tool:* `bassNARandom`, `bassNBRandom`, `bassNCRandom` = 0; SAKNAS → warning when Random > 0 on a bass layer → analyzer idea A5 — Kick & Bass School lesson 2, 7 Rules [verified]
- Contradiction: the project reference allows free phase for an organic 90s feel (classic style). Keep it as a deliberate style choice, never a default for tight full-on.

**Align the bass fundamental to the kick sine, measured after the filter** — a saw is a sum of harmonics, and the filter shifts the phase of the fundamental, so a start phase set before the filter does not mean the fundamental is aligned — *Our control/tool:* `bassNAPhase` / `BPhase` / `CPhase` (start phase before the filter); SAKNAS → phase rotation of the fundamental after the filter, polarity invert, nudge in samples → tool idea P1; SAKNAS → low-band correlation kick/bass per hit → analyzer idea A1 — Kick & Bass School lesson 2, Phase Alignment video [one source]

**Check phase in the sum, not on a waveform picture** — the target is the first bass 16th: the summed low band should not be smaller than the larger of the two single peaks; try one change at a time (phase, start time, polarity, pitch) and level-match A/B — *Our control/tool:* `render_preview` whole / `part="kick"` / `part="bass"`; the analysis is mono and does not prove phase cancellation (AGENT_GUIDE section 9); SAKNAS → sum-vs-max readout → analyzer idea A1 — Kick & Bass School lesson 2, Phase Alignment video, project reference [verified]

**Cheap phase fixes, in this order** — (1) remove the fundamental from the first bass note (low-cut / sub off), (2) delay the bass start a few ms, (3) invert one element, (4) shift tempo/pitch so the bass meets another point in the tail — *Our control/tool:* `bassNEqHp`, `bassNSub`, `bassNDelay`; SAKNAS → polarity invert (P1) — project reference, Phase Alignment video [one source]

## 5. Post-ringing and filter

**No part of the bass may ring into the next kick transient** — a resonant filter or long release keeps sounding after the note and lands on the kick attack; cut it before the next kick — *Our control/tool:* `bassNGate`, `bassNGateSmooth`, the bass filter decay/resonance fields (see `list_parameters prefix=bassN`), `bassNDuck`; SAKNAS → post-ringing detector (bass level in the last 5–10 ms before each kick) → analyzer idea A3 — Kick & Bass School lesson 2 (the lesson does not say how the cut is made; we assume a gate/volume shape) [one source]

**Pluck from the filter decay, not release** (cutoff ~150–400 Hz, env amount +2..+4 oct, decay 30–90 ms; shorter at higher BPM) — the open filter at the attack gives harmonics, the closed filter keeps the body — *Our control/tool:* bass filter Cutoff / EnvAmount / EnvDecay (names via `list_parameters`) — project reference [one source]

**Keep the bass below ~120 Hz mono** — wide low end collapses in mono club systems — *Our control/tool:* `masterMonoBelow`, `bassNSub` (mono) — project reference, Mixing & Effects Compendium [verified]

## 6. Measure before changing

**Change one thing, render the same seed, read the number, ask for listening** — the analysis is mono with a ~171 ms window, cannot prove phase problems and measures instruments before FX sends — *Our control/tool:* `analyse_mix`, `get_mix_report`, `add_report_note`, `get_state`/`undo` — AGENT_GUIDE section 9, project reference [verified]

**Remove processing before adding EQ** — mute layers, bypass FX, then act on what is audibly or measurably wrong — *Our control/tool:* layer on/off, FX bypass, `set_effect` — What is causing this Kick & Bass to Sound Bad; Over EQ'ing [one source]

## Verktygsidéer
- K1: markers for 1/16 and 1/8 (from BPM) in the kick amp and pitch graphs.
- K2: shaded 35–43 Hz zone and a tail readout (Hz and note) in the pitch graph; warning only.
- K3: click a frequency in the pitch graph and see the same instant in the amp graph (and the reverse).
- P1: per-layer polarity switch, fundamental phase rotation after the filter, micro-nudge in samples (see gaps-to-specs B1, B7).
- Macro "clean roll": phase reset on, Random 0, mono sub, 1/16 gate, lower first note (gaps-to-specs B4).
- Optional post-ring cut: a fast volume shape that ends the bass before each kick, with a visible curve.

## Analysatoridéer
- A1: low-band correlation and sum-vs-max level (dB) at the first bass 16th of every kick; "kick wins" warning (gaps-to-specs A1).
- A2: per-hit consistency (peak, phase, end value) and end-click detector (gaps-to-specs A2, A5).
- A3: post-ringing detector: bass level shortly before each kick vs the kick attack.
- A4: 200 Hz excess check on the kick and the time at which the pitch sweep crosses 200 Hz.
- A5: random-phase warning (any bass layer with Random > 0, or phase differing between notes).
- A6: tail frequency readout accurate below 40 Hz (longer window or time-domain estimator).
