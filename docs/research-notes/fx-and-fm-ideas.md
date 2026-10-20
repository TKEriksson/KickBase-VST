# FX and FM ideas (low-end shaper, FM squelch/lead, Psy Multi-FX)

Written by the project lead from general DSP knowledge, not from web pages: everything about third-party products is `[osäker]`. We write our own code from standard DSP (RBJ biquad formulas, JUCE `dsp` module); we copy no product's algorithm, so AGPL is not a problem. Priority 1 = do first, 3 = later. CPU: L low, M medium, H heavy.

## 1. Low-End Shaper / Multiplier (30–400 Hz)

What the known tools do `[osäker]`: Voxengo PHA-979 aligns phase between two channels (all-pass phase rotation, delay, correlation view). Kilohearts Disperser spreads phase with an all-pass cascade (frequency, amount, pinch). LFOTool is a drawn volume curve, which our DUCK curves already cover. Psylab Pro and PsyScope: we do not know their internals.

| Part | Simplest JUCE implementation | CPU | Prio |
|---|---|---|---|
| **Phase rotation of the fundamental** (0–360°) | Cascade of 2nd-order all-pass biquads (`IIR::Coefficients::makeAllPass`, one stage = −180° at its centre frequency). For our own kick/bass: set the oscillator start phase instead, which is free | L | 1 |
| **Disperser** (smear phase of the 100–2000 Hz range) | 4–16 all-pass stages, one frequency + spread knob; also a phase-shaper for PH | L | 1 |
| **Band split 30–400 Hz** | `dsp::LinkwitzRileyFilter` (LR4) at 2 crossovers (e.g. 90 and 250 Hz); sum is phase-flat | L | 1 |
| **Transient shaper** | Fast and slow envelope followers, gain = difference × amount; per band | L | 2 |
| **Sub-harmonic (octave down)** | Zero-crossing flip-flop on a low-passed copy, low-pass the square, follow the amplitude. Mono and one note at a time; fails on chords. Alternative: half-rate phase accumulator in our own bass oscillator (free, exact) | L–M | 2 |
| **Octave up / harmonic multiplier** | Full-wave rectifier or `x*|x|` waveshaper, then band-pass | L | 2 |
| **Auto phase check** | Per 16th: correlation and sum/difference energy between kick and bass in 30–150 Hz; part of AN/PH, no new DSP | L | 1 |
| **True phase rotation of arbitrary audio** | Hilbert transform (FIR ≥255 taps, latency ~127 samples) and `y = x·cosθ − H(x)·sinθ`. Latency breaks sample-exact ducking | M | 3 |

**Rule:** apply low-end processing in the instrument, before the FX channel, for the sources we generate ourselves. Only external audio needs the Hilbert path.
*Tool:* LOW-END page with band split, rotation knob per band, polarity, correlation meter. *Analyzer:* correlation per 16th, kick-bass overlap in 30–150 Hz.

## 2. FM squelch / lead (new source tab beside OSC 1–3, phase S2)

Typical psy squelch `[osäker]`: 2 operators (sometimes 3), carrier sine or saw, modulator at an integer or near-integer ratio, a fast pitch envelope (falling a few to 12+ semitones within roughly 30–200 ms), an envelope and an LFO on the modulation index in 1/16 steps, then a resonant filter and a delay. Most of the movement comes from index and filter, not from a rich operator count.

**Minimal structure:** 3 operators A (carrier), B, C, with 3 algorithms: (1) B→A, (2) C→B→A, (3) B+C→A. Operator ratio 0.25–16 plus fine ±50 cents, level/index, feedback on B (0–1), own AD envelope for B and C (A uses the instrument ADSR). Use phase modulation (`sin(phase + index·mod)`), not true frequency modulation, so notes stay in tune.
**Most important knobs, in order:** index B, ratio B, index C, pitch-envelope amount and time, feedback, then filter. Index and ratio must be modulatable by LFO 1–10 and the ENVs through the existing matrix, so no dedicated LFO is needed in the tab. A preset activates its own LFOs (decided in M).
**Implementation:** one class per voice, sine lookup (2048 points, linear interpolation) or a polynomial; fixed seed for start phases so renders stay deterministic. Aliasing at high index: 2× oversampling of the FM block only (`dsp::Oversampling`, adds latency, so test blocksizetest). Cost is about 3 sine evaluations per sample per voice and unison; high with unison 7 and VOICES 8 (63 per sample), so cap unison to 3 in this source, measure with perftest. CPU M, prio 2. Our wavetable list already has an FM table and a warp-FM mode, which cover simple cases first.

## 3. Psy Multi-FX against our list

We have 15 types: filter, distortion, eq, chorus, phaser, echo, reverb, compressor, gate, comb, ringMod, bitcrusher, autoFilter, maximizer, ducker (Source/Core/Effects.h:19).

| Wanted | Have? | Simplest implementation | CPU | Prio |
|---|---|---|---|---|
| Fast stereo / ping-pong / dotted 1/8 delay with EQ after | Echo exists (feedback modes, CUT filter) | Check that dotted and triplet sync and a post-EQ exist; add presets | L | 3 |
| **Multiband distortion** | No (SAKNAS) | LR4 split in 3 bands, existing distortion curve per band, drive per band; keeps sub clean (drive only above ~120 Hz) | M | 1 |
| **Disperser** | No | All-pass cascade as above; also a PH tool | L | 1 |
| **Widener** | SOUNDSTAGE width on master only | Mid/side, width 0–200%, side high-passed at 120 Hz, optional Haas 0–20 ms | L | 2 |
| Flanger | No (planned last in the type list) | Chorus code with short delay (0.1–10 ms) and feedback | L | 2 |
| Dynamic EQ | No | Band-pass sidechain detector + EQ band gain | M | 2 |
| Triggered / automated filter | autoFilter and LFO matrix | Add trigger from the pattern rows (retrigger envelope per note); mostly covered by ENV/LFO | L | 3 |
| Shimmer reverb | Reverb exists | Pitch shifter (+12 st) in the reverb feedback loop; granular two-tap shifter is grainy but light | M–H | 3 |
| Independent L/R pan mode | No | Two gains in stereo pan instead of balance | L | 3 |

New effect types go **last** in the type list (plan-v2 rule), with a state-load test.

## Tool ideas
- LOW-END page (band split, rotation per band, polarity, correlation meter).
- Disperser and multiband distortion as FX types 16–17.
- FM source tab with 3 operators and 3 algorithms.

## Analyzer ideas
- Kick-bass correlation per 16th in 30–150 Hz.
- Aliasing test for FM and multiband distortion (frequency sweep, folded-component level).
- Latency report per FX type, so ducking stays sample-exact.
