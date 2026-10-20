# Low-end, FM and psychedelic FX ideas

Research handoff for implementation planning. “AGPL-compatible” means **reimplement the documented DSP principle**, never copy another vendor's code, UI, presets, branding, or samples. Verify any borrowed code's licence before use.

## 1. Low-End Shaper / Multiplier (30–400 Hz)

**Start with analysis, not automatic correction** (30–400 Hz; compare kick, INSTR and sum) — a phase checker should report correlation, peak/RMS and cancellation at the actual first-bass overlap; “best” is musical, not maximum correlation — *Our control/tool:* ANALYZER, offline render; SAKNAS → low-end compare panel — PHA-979 concept [verified]

**Use sample-accurate volume shaping for rhythmic ducking** (1/16-capable, host-sync) — LFOTool-style custom curves are a deterministic alternative to envelope-following compression — *Our control/tool:* existing SIDECHAIN custom curve and tempo LFOs — LFOTool [verified]

**Offer micro-time and polarity tests before phase rotation** (±0–10 ms candidate range; polarity 0/180°) — a time offset or flip is cheap, reversible and explains what changed — *Our control/tool:* SAKNAS → per-INSTR micro-nudge/polarity A/B — PHA-979 [verified]

**Keep a phase rotator separate from dispersion** (0–360° all-band rotation versus frequency-dependent group delay) — PHA-979 describes all-frequency rotation; Disperser is a stack of all-pass filters whose delay changes by frequency — *Our control/tool:* SAKNAS → two explicitly named FX — PHA-979; Disperser [verified]

**Use all-pass dispersion as colour, not “repair”** (low cutoff concentrates more group delay; high amount can reach hundreds of ms) — it can lower crest factor and turn clicks into zaps, but also smears bass timing — *Our control/tool:* SAKNAS → Disperser-style phase shaper after filter — Disperser [verified]

**Generate sub harmonics conservatively** (one octave below, band-limited to about 30–90 Hz) — octave division/waveshaping may add weight but can create DC, aliasing and uncontrolled headroom — *Our control/tool:* SAKNAS → sub generator, high-pass/DC blocker, output ceiling — implementation inference [uncertain]

| Candidate | Smallest JUCE implementation | CPU/latency | Licence/prio |
| --- | --- | --- | --- |
| Low-band analyser | `juce::dsp::FFT`, band filters, peak/RMS/correlation over a short history | low–medium; no added audio latency | original DSP is safe; **P1** |
| Time/polarity A/B | circular delay buffer plus sign switch; sample offset, not `delayLine` allocation in `processBlock` | low; no algorithmic latency | original DSP is safe; **P1** |
| Phase rotation | first-order all-pass sections; label as frequency-dependent unless a true linear-phase design is used | low; no latency | original DSP is safe; **P2** |
| Disperser-style shaper | cascaded all-pass biquads, frequency/Q/amount, dry-wet and gain compensation | low–medium; no fixed latency | principle safe; do not clone product; **P2** |
| Sub generator | low-band split → saturation/full-wave rectification → LPF + octave-down tracking/mix; 2–4× oversampling for nonlinearity | medium–high; oversampling adds latency | original DSP is safe; **P3** |
| PHA-979-style linear phase | overlap-save FFT phase rotation and latency reporting; correlation meter | high; fixed latency (PHA-979 documents 48 ms) | do not copy; **P3** |

**Product interpretation:** PsyScope is best treated as an oscilloscope/diagnostic target, not an audio process; PHA-979 supplies phase rotation, ± time shift, M/S and multiband correlation; Disperser supplies frequency-dependent all-pass group delay; LFOTool supplies editable, sample-accurate host-sync LFO shapes. “Psylab Pro” and generic “sub-bass multiplier” feature claims could not be verified from a primary product page, so they must remain [uncertain].

## 2. FM squelch / lead source

**Keep the first FM voice to two operators** (carrier + modulator, ratio 1:1–4:1) — it proves the audible workflow before exposing a matrix — *Our control/tool:* SAKNAS → `FM` tab beside OSC 1–3 — Sound Design Compendium; implementation inference [one source]

**Drive pitch and filter in opposing short envelopes** (0 ms attack; 40–80 ms zap or 200–400 ms acid contour) — upward/downward pitch movement plus resonant BP/LP sweep makes the squelch speak — *Our control/tool:* Pitch Env, Filter Env, BP/LP24/TB-303 filter — Squelches; Sound Design Compendium [one source]

**Modulate FM index, not only cutoff** (tempo LFO at 1/16, 1/8, or one-shot envelope) — index movement changes harmonic density while preserving the played note — *Our control/tool:* existing LFO routing; SAKNAS → FM-index destination — implementation inference [uncertain]

**Use 3–4 operators only as named roles** (carrier, bright modulator, sub/feedback modulator, optional noise/inharmonic modulator) — unlabelled free matrices become difficult to audition and automate — *Our control/tool:* SAKNAS → fixed-role operator panel — implementation inference [uncertain]

**Ship macros/presets rather than a blank matrix** (Zap, Acid Squelch, Metallic Lead, FM Pluck; sine/triangle one-shot, ramp, stepped 1/16 LFO) — the existing three LFOs can recall the required shape with the preset — *Our control/tool:* LFO preset persistence; SAKNAS → per-preset LFO-shape storage — Squelches; project design [one source]

| Minimal FM-tab parameter | Recommended range/role | JUCE implementation | CPU/licence/prio |
| --- | --- | --- | --- |
| Carrier waveform/level | sine default; optional existing wavetable carrier | existing wavetable oscillator | low; original DSP; **P1** |
| Modulator ratio + level | ratio 0.25–8; index 0–100% | phase accumulator feeding carrier phase | low; original DSP; **P1** |
| Index envelope + LFO depth | 0–80 ms zap, synced 1/16/1/8 shapes | reuse envelope/LFO evaluation per sample | low; original DSP; **P1** |
| FM feedback | 0–30% soft range | delayed prior modulator sample, clamp/soft clip | low; original DSP; **P2** |
| Operator 3/4 routing | fixed named routes, not arbitrary graph | additional accumulators, optional modulation sums | low–medium; original DSP; **P2** |
| Oversampling | 2×/4× only when strong FM/drive aliases | `juce::dsp::Oversampling` around voice/drive | medium; latency/state complexity; **P3** |

**Preset LFO shapes:** fast exponential down/up for zaps; downward ramp or triangle for filter travel; stepped 1/16 for rhythmic chatter; slow sine (1–8 bars) for lead drift. Keep low end muted/high-passed for these lead presets so they do not compete with KICK/INSTR bass.

### Image review: additive instant phase shift

**Expose phase as a partial-level sound-design control** (image shows 32 partials, each with an amplitude bar and phase control) — the same harmonic amplitudes can produce radically different waveform/transient shapes when partial phases move; that is why it can sound like an “instant phase shift” of the fundamental — *Our control/tool:* SAKNAS → additive/partial-phase editor, separate from OSC start phase and Disperser — `plugintips_with_instant_phase_shift.bmp` [one source]

**Build a constrained version before a 32-partial editor** (fundamental + 2nd–8th partials, level and phase, phase-lock/reset switch) — it delivers the audible idea while avoiding a dense, hard-to-preset interface; retain the existing wavetable oscillator as the default source — *Our control/tool:* SAKNAS → “PARTIALS” subpanel/preset macro — image review; implementation inference [uncertain]

| Partial-phase option | Smallest implementation | CPU/licence/prio |
| --- | --- | --- |
| 8-partial additive oscillator | sum eight `sin(phase[n])` terms per sample; normalise and reset all accumulators per note | low; original DSP; **P2** |
| Phase macro | rotate selected harmonic groups (odd/even/high) with a 0–360° macro, retain exact per-partial values in preset | low; original DSP; **P2** |
| 32-partial editor | 32 sine accumulators with amplitude/phase arrays and a scrollable/graph editor | low CPU, high UI/preset complexity; original DSP; **P3** |

**Do not present it as automatic kick/bass phase correction** — partial phase changes the source waveform itself; it is best auditioned against the phase analyser and ducking controls, then saved as a timbre preset.

### AI, MCP and live-code workflow

**Let AI search bounded phase candidates, then keep the human decision** (e.g. 8-partial macro values, 4–16 candidates, fixed note/BPM/seed) — the agent can render the same phrase, compare transient/crest/correlation measures and return named A/B candidates; it must not silently overwrite a patch because “phase alignment” is also a timbre choice — *Our control/tool:* MCP `set_parameters`, `render_preview`, ANALYZER; SAKNAS → `set_partial_phases` and candidate/A-B snapshot API — implementation proposal [uncertain]

**Expose musical controls, not raw 32-value prompts** (`partial_phase_odd`, `partial_phase_even`, `partial_phase_high`, `partial_spread`, `phase_reset`) — chat can say “make the squelch sharper but retain sub stability”, while the plugin maps that request to bounded, inspectable macros — *Our control/tool:* SAKNAS → partial-phase macro parameters with normal automation — implementation proposal [uncertain]

**Use Strudel to automate a chosen macro, not to build unsafe DSP graphs** (0–1 phase-macro lanes at 1/16–1/4 resolution) — live code is ideal for repeating/transforming musical gestures; the VST remains the deterministic sound engine — *Our control/tool:* planned Strudel/live-code control and host automation; SAKNAS → documented parameter names and clamp ranges — implementation proposal [uncertain]

**Suggested agent loop** (analyse → generate candidates → render fixed 1–4 bars → report metrics and audible labels → user approves apply/save) — it is reproducible and permits rollback — *Our control/tool:* deterministic rendering, preset save/load, analyzer report; SAKNAS → candidate comparison record — implementation proposal [uncertain]

### Analyzer and saved-report requirement

**Turn phase/timbre issues into actionable findings** (per 1/16 note or first bass onset) — the analyzer must flag low-band cancellation, inconsistent repeat hits, excessive crest/peak change, or unstable mono correlation as “what to fix”, rather than merely drawing a scope — *Our control/tool:* ANALYZER and `analyse_mix`/`get_mix_report`; SAKNAS → partial-phase diagnostics — implementation proposal [uncertain]

**Put the same findings in every report surface** (UI, MCP/AI result, and `save_mix_report` JSON) — an agent cannot propose a trustworthy fix from a graph it cannot read, and an exported report must preserve why a decision was made — *Our control/tool:* existing analyzer report/export; SAKNAS → structured `phase_findings` and `suggested_fixes` fields — source inspection [verified]

**Report a fix as a reversible candidate** (never an unexplained mutation) — each suggestion must name target, safe range, expected effect, risk, confidence and before/after metrics; applying it creates an A/B snapshot and records `applied: true` only after approval — *Our control/tool:* preset snapshots and MCP actions; SAKNAS → analyzer candidate store/apply endpoint — implementation proposal [uncertain]

Suggested serialized shape:

```json
{
  "phase_findings": [{
    "severity": "warning", "source": ["Kick", "Instr 1"],
    "when": "bar 1, 1.2", "band_hz": [30, 120],
    "evidence": {"correlation": -0.31, "sum_peak_delta_db": -4.2, "repeat_variance_db": 2.1},
    "message": "Low-band cancellation at the first bass onset"
  }],
  "suggested_fixes": [{
    "id": "phase-ab-01", "target": "Instr 1 partial-phase macro",
    "candidates": [{"label": "A: current", "values": {"even_phase": 0}}, {"label": "B: tighten transient", "values": {"even_phase": 35}}],
    "expected": "more stable 30–120 Hz sum", "risk": "changes timbre", "confidence": "medium",
    "approval_required": true, "applied": false
  }]
}
```

**Only recommend phase changes after separating timing and level causes** (check duck amount, note overlap, polarity and nudge first) — an all-pass/partial solution can hide an envelope or gain problem — *Our control/tool:* SIDECHAIN, Note end, micro-nudge/polarity A/B, Analyzer — implementation proposal [uncertain]

## 3. Psy Multi-FX gap analysis

**Existing:** Filter, Distortion, EQ, Chorus, Phaser, Echo, Reverb, Compressor, Gate, Comb, Ring Mod, Bitcrusher, Auto Filter, Maximizer and Ducker; plus per-channel EQ/compressor, sends, tempo modulation and 5 slots × 11 FX channels. This already covers basic stereo delay, triggered filtering, gating, modulation and width-by-chorus.

| Requested capability | Existing coverage | Missing/recommended addition | Smallest JUCE approach | CPU/licence/prio |
| --- | --- | --- | --- | --- |
| Fast stereo delay | Echo likely covers base delay | **Verify** independent L/R time, ping-pong, diffusion and filtered feedback; add only gaps | two delay lines, cross-feedback, HP/LP feedback filters | low; original DSP; **P2** |
| Frequency-dependent distortion | Distortion plus EQ; no explicit multiband effect | SAKNAS → 2/3-band distortion with crossover audition/solo | IIR/Linkwitz–Riley split, per-band waveshaper, sum; oversample nonlinear path | medium–high; original DSP; **P2** |
| Triggered/automated filter | Auto Filter, Filter, Gate, LFO routing | likely present; add envelope follower / MIDI-note trigger only if absent | band/ladder filter + envelope follower with attack/release | low; original DSP; **P2** |
| Shimmer reverb | Reverb only | SAKNAS → shimmer send effect, default HP input/LP feedback | pitch-shifted (+12 semitone) feedback loop into reverb; limit feedback | medium–high; beware runaway feedback; original DSP; **P3** |
| Disperser | Phaser is not an all-pass group-delay shaper | SAKNAS → phase-shaper FX | cascaded all-pass filters, frequency/Q/amount/dry-wet | low–medium; original DSP; **P2** |
| Widener | Chorus may widen, SOUNDSTAGE observes width | SAKNAS → M/S high-band widener with mono-safe low split | M/S encode; high-pass Side; width gain; correlation limiter | low; original DSP; **P2** |

**Prioritise safe composition over new aliases** (FX send → EQ → compressor → SIDECHAIN) — existing FX routing already permits clean psychedelic chains; add a preset/template first, then a new DSP type only where the table says SAKNAS — *Our control/tool:* FX1–FX11, 5 slots, always-on channel EQ/compressor — source inspection [verified]

## Suggested build order

1. **P1:** low-end analyser/compare, phase/polarity/time A/B, two-operator FM source with index routing and four presets.
2. **P2:** disperser-style all-pass, M/S high-band widener, multiband distortion; fill any Echo trigger/ping-pong gaps found in an audit.
3. **P3:** guarded sub generator, shimmer reverb and true linear-phase rotation only after latency/UI/measurement decisions.

## Sources

- [PHA-979 product and user guide](https://www.voxengo.com/product/pha979/) [verified]
- [Kilohearts Disperser documentation](https://kilohearts.com/docs/disperser) [verified]
- [Xfer LFOTool product page](https://www.xferrecords.com/products/lfo-tool) [verified]
- `research/TEST026-10-19/MERIDÉER.md`, Sound Design and Mixing/Effects PDF compendia, and `Source/Core/Effects.h` [one source]
