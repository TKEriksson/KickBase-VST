# Gaps to specs (missing measurement/tool → phase → minimal spec)

Written by the project lead from the "Saknade mätningar" table in `docs/psytrance-kick-bass-reference.md` and the tool/analyzer idea lists in the other research-notes. Phases are from `docs/plan-v2.md`: AN = analyzer, PH = phase tools, K = kick, FX = effects, S2 = FM source, LE = low-end, LC = render loop. Prio 1 = first, 3 = later. All third-party facts are `[osäker]`; the specs are our own design.

**Rule for every item:** measure first, never auto-fix. Report fact (number) separately from hypothesis. Deterministic offline rendering is the base (same seed, before/after).

## A. Measurements (analyzer, AN)

| # | Gap | Phase | Minimal spec | Prio |
|---|---|---|---|---|
| A1 | Kick–bass phase/correlation at first bass onset | AN/PH | Render kick and bass stems sample-aligned. Per kick hit: Pearson correlation and sum/difference energy in 30–150 Hz over the first 1/16; peak of the summed low band vs the larger single peak (ratio in dB). MCP: `analyse_mix` adds `lowband.correlation[16th]`, `lowband.sum_vs_max_db`. Fact only, no auto phase choice | 1 |
| A2 | Hit-to-hit consistency (phase, peak, zero-end click) | AN | Per hit: peak, phase at the first zero crossing, last-sample value. Report variance over 16 hits and flag > 1 dB peak spread or end value > −60 dB (click). Same for bass notes (sub peak variance) | 1 |
| A3 | Stereo width and mono compatibility per band | AN | M/S level and L/R correlation in 4 bands (< 120, 120–500, 0.5–4k, > 4k Hz) per bar. Warn only if correlation < 0.3 below 120 Hz or mono sum loses > 3 dB | 1 |
| A4 | Loudness: LUFS-I / short-term, true peak, crest | AN | ITU-R BS.1770 K-weighting + gating (we write it ourselves from the published filter coefficients), 4× oversampled true peak, crest = peak − RMS. Per source and master | 2 |
| A5 | Transients shorter than the FFT window | AN | Envelope timeline at 0.5 ms resolution plus click detector (sample-to-sample jump > 6 dB above local RMS). Used by A2 | 2 |
| A6 | Aliasing from warp, FM, drive | AN/S2 | Offline sweep 50 Hz–5 kHz sine through the source/effect at 48 and 192 kHz; report folded-component level (dBFS) above the 48 kHz-vs-192 kHz difference. Test target for FM and multiband distortion | 2 |
| A7 | FX return per source | AN | Render each FX return solo and sum in 30–300 Hz and 200–500 Hz; report leakage from reverb/echo into the low end and the source that feeds it. We have per-channel renders already; this adds the band numbers | 2 |
| A8 | Latency report per FX type | AN | One test renders an impulse through each effect type; report the delay in samples. Ducking stays sample-exact only if it is 0 | 1 |
| A9 | Arrangement density per band/width over time; 16-bar repeat detector | AN | Band energy and width per bar as a timeline; diff bar n vs n−16 and say which of notes, automation, FX, level changed | 3 |
| A10 | External DAW tracks in the same analysis | LC/IO | Host bounce or file import aligned on a bar line; same reports as above. Needs the IO phase | 3 |

## B. Tools (PH, K, LE, FX, S2)

| # | Gap | Phase | Minimal spec | Prio |
|---|---|---|---|---|
| B1 | Per-layer polarity, phase 0–360°, micro-nudge | PH | Per instrument: polarity switch, start phase 0–360° (free, sets the oscillator phase on note-on), nudge ±2 ms in 0.05 ms steps. A/B snapshot undo. Works only for our own sources (no latency). MCP: `set_instrument` fields `polarity`, `phase_deg`, `nudge_ms` | 1 |
| B2 | Low-band all-pass chain (labelled coloration, not correction) | LE/FX | 1–4 all-pass biquads at 40–200 Hz in the instrument, before the FX channel; show phase curve. Not enabled by default | 2 |
| B3 | Kick tail tuning helper | K | Tail fundamental readout in Hz and note, warning when it falls below 45 Hz (translation) — a warning only. Overlay of tail end vs first bass onset at the current BPM. Testing the three alternatives (exact root / few semitones above / fixed Hz) is a user choice via A/B render | 2 |
| B4 | Clean roll macro (bass) | LE | One macro sets phase reset on, mono sub below 120 Hz, 1/16 envelope, first-note velocity; stores nothing the user cannot undo | 3 |
| B5 | Squelch macro | S2 | Links pitch-envelope depth inversely to filter cutoff through the existing matrix (two routes, not new DSP) | 3 |
| B6 | Multiband distortion | FX | New effect type, last in the list: LR4 split at 120 and 500 Hz, existing distortion curve per band, drive per band; sub band can stay clean. State-load test for old projects | 1 |
| B7 | Disperser | FX/PH | New effect type: 4–16 all-pass stages, frequency and spread. Also a phase-shaper in PH | 1 |
| B8 | Stereo-pan mode and widener | FX | Pan: independent L/R gains. Widener: mid/side 0–200% with side high-passed at 120 Hz | 2 |
| B9 | Dynamic EQ, flanger | FX | Dynamic EQ: band-pass detector driving one EQ band's gain; flanger: chorus code with 0.1–10 ms delay and feedback | 2 |
| B10 | Transient shaper, octave-down/up | LE | Transient: fast/slow follower difference per band. Octave down: half-rate phase accumulator in our own bass oscillator (exact); octave up: `x·|x|` plus band-pass | 2 |
| B11 | FM source (3 operators, 3 algorithms) | S2 | See `fx-and-fm-ideas.md` section 2 (phase modulation, unison cap 3, 2× oversampling of the FM block only) | 2 |
| B12 | Pattern-variation lane every 4/8/16 bars; scale guide in piano roll | P/L | Per-section deltas for gate, velocity, filter, FX send; scale guide highlights keys. Fits the L phase | 3 |
| B13 | Additive/harmonic editor with per-partial level and phase | S2 | Up to 32 partials with amplitude and phase; phase 0 on retrigger gives a deterministic "instant phase" start. Heavy in the UI, light in DSP (table built on edit) | 3 |

## Suggested order
1. AN: A1, A2, A3, A8 (all four are plain offline maths on renders we already make).
2. PH: B1 (free in the oscillator), then B7 (disperser, shared with FX).
3. FX: B6, B7 as new types; A6 as their test.
4. K/LE: B3, B10; S2: B11 after A6 exists.

## Verktygsidéer
- Everything in section B above.

## Analysatoridéer
- Everything in section A above.
