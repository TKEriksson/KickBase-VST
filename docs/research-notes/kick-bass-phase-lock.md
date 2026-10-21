# Kick–bass phase lock

Phase is an optimisation at the overlap, not a license to overlap everything. See [the project reference](../psytrance-kick-bass-reference.md) for the existing workflow.

## Rules

**Make source phase repeatable first** (random oscillator phase = 0%) — a changing start point makes identical notes produce different sums with the kick — *Our control/tool:* OSC random phase and phase knobs — Likely Reason [one source]

**Adjust phase by listening to the combined low end** (0–360° search) — the useful setting is the most stable, weighty sum, not necessarily the prettiest visual alignment — *Our control/tool:* OSC phase, Analyzer, A/B — Sweet Spot [one source]

**Duck before fine alignment** (duck most bass during kick; inspect the small residual overlap) — phase only matters where signals coexist — *Our control/tool:* per-layer DUCK and editable sidechain curve — Likely Reason [one source]

**Nudge the bass only when the envelope needs separation** (a few ms) — a micro-delay can avoid tail/first-transient collision; it changes groove, so audition in context — *Our control/tool:* per-layer note offset; SAKNAS → micro-nudge parameter — Sound Design Compendium [one source]

**Use phase rotation per band with restraint** (all-pass / crossover-based adjustment) — rotating selected low bands can alter the perceived “spongy” transient without EQ magnitude changes — *Our control/tool:* SAKNAS → phase-shaper FX; existing FX routing — Sweet Spot; Mixing & Effects Compendium [one source]

**Do not confuse polarity with alignment** (180° flip is one candidate; phase is frequency/time dependent) — whole-signal inversion may help or harm different bands — *Our control/tool:* SAKNAS → polarity invert and phase sweep per layer — Sound Design Compendium [one source]

**Re-check when pitch changes** (every new bass note can require a different relationship) — oscillator period and overlap move with pitch — *Our control/tool:* phase automation/LFO mapping; piano-roll note data — Likely Reason [one source]

**Inspect limiter damage at waveform scale** (release longer than an oscillation cycle when needed) — rapid gain switching can leave repeated waveform kinks and audible distortion — *Our control/tool:* MASTER limiter release/lookahead and scope — What is causing [one source]

**Keep sub mono when validating** (below 100–120 Hz; correlation +0.8 to +1.0 proposed) — stereo differences obscure cancellation and reduce translation — *Our control/tool:* SOUNDSTAGE mono-under-frequency, correlation meter — Mixing & Effects Compendium [one source]

**Treat “kick owns 30–90 Hz, bass 90–200/250 Hz” as a mix template** — it reduces conflict, but does not replace waveform/timing tests — *Our control/tool:* FX EQ, analyzer bands — both PDF compendia [one source]

## Image-derived reminders

**Read the Discord waveform as evidence, not a preset** (zoom to individual cycles) — the supplied discussion image illustrates that processing order can create irregular bass/kick shapes — *Our control/tool:* offline waveform zoom; SAKNAS → render comparison history — discord discussion bitmap [one source]

**Use additive partial phase deliberately** (phase per harmonic can reshape a fundamental instantly) — the supplied additive-synth image implies phase is a sound-design variable as well as a repair tool — *Our control/tool:* SAKNAS → per-partial phase/additive oscillator experiment — instant-phase-shift bitmap [one source]

## Tool ideas

- Per-layer polarity, 0–360° phase, and micro-nudge controls with undoable A/B snapshots.
- Optional low-band all-pass chain, clearly labelled as coloration rather than “phase correction”.

## Analyzer ideas

- Combined kick+bass waveform at the first bass onset, with variance across 16 hits.
- Correlation and low-band peak delta with phase/duck bypass comparison.
