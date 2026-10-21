# Bass design

For default rolling-bass values and patterns, see [the project reference](../psytrance-kick-bass-reference.md); this note captures additional source guidance.

## Rolling and alternative basses

**Start a rolling bass from a saw** (harmonic-rich waveform) — a 24 dB/oct low-pass envelope exposes the pluck then leaves fundamental weight — *Our control/tool:* OSC waveform, LP24, Filter Env — Sweet Spot; Sound Design Compendium [one source]

**Use envelope-shaped 1/16 motion** (0 ms attack/release; cited decay 100 ms or 120–180 ms) — hard retriggered edges make a tight rolling line — *Our control/tool:* ADSR and per-note Filter Env — Sound Design Compendium [one source]

**Treat exact 1/16 gate as a choice, not safety** (full 1/16 versus a shorter non-overlap gate) — one source favours no overlap; another finds full-length 1/16 notes can add full-on weight — *Our control/tool:* GATE row, Note end = Cut, DUCK — project reference; Sweet Spot [contradiction]

**Keep the main low bass deterministic** (random phase 0%; choose start phase) — each strike has repeatable low-end interaction with the kick — *Our control/tool:* OSC phase and random-phase controls — Likely Reason [one source]

**Separate width from weight** (sub/main oscillator fixed and centred; stereo layer high-passed with 2-voice unison) — unison/random phase can widen a higher layer without destabilising the sub — *Our control/tool:* per-OSC pan/spread, unison, HP filter — Strong Prog Basses [one source]

**Build progressive bass in layers** (sub + harmonic main + filtered stereo saw) — a weak fundamental in a processed wavetable may need a separate sub, while the upper layer provides bite — *Our control/tool:* OSC 1–3, sub octave, LP filter — Strong Prog Basses [one source]

**Sync modulation to note rate** (1/8 or 1/16 one-shot LFO/envelope) — retriggered modulation is easier to align to the pattern than a free-running LFO — *Our control/tool:* tempo-sync LFO, Filter Env — Strong Prog Basses [one source]

**Use slides for a distinct line, not the sub foundation** (pitch/glide per note) — a simple warm waveform can retain weight while notes connect — *Our control/tool:* piano-roll slide flag and glide — Strong Prog Basses [one source]

**Make “clean” observable** (no kinks or level drift at the bass waveform) — over-aggressive processing can create cycle-by-cycle limiter distortion — *Our control/tool:* analyzer waveform; MASTER limiter release — What is causing [one source]

## Squelch and acid-bass bridge

**Squelch = pitch motion plus band-pass motion** (24 dB/oct BP; modest resonance) — opposed pitch/filter trajectories create the vocal, elastic sweep — *Our control/tool:* LP/BP filter, Filter Env, Pitch Env — Squelches [one source]

**Use no-attack short squelches for rhythmic zaps** (very short decay) — they articulate a sequence without becoming a pad — *Our control/tool:* ADSR, Filter Env, 1/16 grid — Squelches [one source]

**Use longer attack/no decay for one-shots** (then dotted 1/8 ping-pong delay) — a rising gesture fills a transition rather than every bass step — *Our control/tool:* ADSR, FX send delay/reverb — Squelches [one source]

**Remove low end from squelch FX** (high-pass before mix return) — preserves the kick/bass ownership of the floor — *Our control/tool:* FX-channel EQ — Squelches; Mixing & Effects Compendium [one source]

## Tool ideas

- “Clean roll” preset macro: phase reset, mono sub, 1/16 envelope and first-note velocity.
- Squelch macro linking pitch-envelope depth inversely to band-pass cutoff.

## Analyzer ideas

- Note-to-note sub peak variance and phase-start display.
- Layered-bass mono/correlation and low-layer-versus-wide-layer spectrum view.
