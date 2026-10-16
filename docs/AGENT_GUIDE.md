# Kickbase – guide for the AI agent

You control **Kickbase**, a psytrance kick + bass synth, through MCP tools. This is the manual. The companion `get_guide topic=psytrance` explains what a good psytrance kick and bass is (read it before designing sounds). Facts here describe how *this* synth works; where the two documents disagree, this one wins.

## 1. The loop

1. `get_guide` (this), `get_guide topic=psytrance` once per session.
2. `list_presets` / `load_preset` for a starting point, or `reset` for the Init sound.
3. Change things: `set_parameters`, `set_effect`, `set_kick_envelopes`, `set_pattern`, `set_sidechain_curve`, `set_modulation`.
4. `render_preview` (full, then `part=kick` and `part=bass` separately). Read the **metrics**, the **checks** and look at the **spectrogram image**. You cannot hear: the numbers and the picture are your ears.
5. Adjust and repeat. Use `undo` freely. Save with `save_preset` (it appears in the plugin's preset menus; ask the user to listen).
6. Always tell the user what you changed and why, and that you could only measure, not listen.

Instances: `list_instances` shows `headless` (private, no DAW, always available) and any Kickbase in a DAW whose **AI button** (top bar) is on. `use_instance` switches. Changes in a DAW instance are audible immediately; `render_preview` still renders offline from a copy.

## 2. What the synth is

One plugin: **kick** + **3 bass layers** + sidechain + **5 FX channels (A–E)** + mixer + master.

- **Key**: the incoming MIDI note sets the key and *also the bass octave*. MIDI note 52 (E4) gives bass root E1 = 41 Hz and kick tail near E1; note 60 (C5 in FL) gives C2 = 65 Hz. Psytrance bass roots are 41–49 Hz (E1–G1), so the usual keys are MIDI 52–55. `render_preview` takes `note` (default 52).
- **Kick tail**: `kickKeyFollow` = Follow puts the tail on the key note within A#0..A1 (29–55 Hz), `kickTune` ±12 semitones moves it. Fixed = C1 + tune.
- **Pattern**: the kick plays on a 16-step-per-bar grid. Each bass layer has free-timed notes (96 ticks per quarter note, 384 per bar) with pitch ±24 semitones from the key, velocity, length and an optional **glide** flag. A glide note works like an FL Studio slide note: it plays no new note, it glides the note that is sounding to its own pitch during its own length, and the pitch stays there until that note ends (a 2-bar note with a quarter-long glide note an octave up half-way through: it rises an octave over that quarter and stays up).
- **Gate**: separate from the notes. Each bass layer can have a GATE: an on/off rhythm with its own grid (`bassNGateGrid`), depth (`bassNGateDepth`) and edge smoothing (`bassNGateSmooth`) that chops the layer's sound while `bassNGateOn` is true. It does not change the notes; notes can sound outside the gate. Use `set_gate`.
- **Piano roll layers**: a layer whose notes were edited in the piano roll (or written with `notes`, pitches or glide) is shown as a small piano roll in the plugin instead of step buttons, as in FL Studio. Pattern length 1, 2 or 4 bars (`patternBars`); Note End (`noteEnd`) Cut/Finish bar/Finish pattern is what happens when the held key ends.
- **Piano roll files**: `save_piano_roll` / `load_piano_roll` keep a layer's notes (with glide flags, pattern length and grid) as `.roll` presets in `Documents\Kickbase\Presets\Piano Roll`, listed by `list_presets` (section Pattern) and by the PRESET button in the plugin's piano roll. They can be loaded into any layer. `export_midi` / `import_midi` write and read standard MIDI files (`note` = the key that sets the bass root, default 52); glide notes are exported as ordinary notes, and an imported file is moved by octaves to fit within ±24 semitones.
- **Signal**: layer → ducked by the kick (per-layer depth) → optionally through an FX channel (`bassNFxChannel`, `bassNFxAmount`) → mixer strip (gain, pan, SC on/off, MAX = through the maximizer or not) → master (width, mono-below, drive, clip, limiter).
- Rendering is deterministic (seeded randomness): the same sound renders bit-identically.

## 3. Units and parameter ids

Values are in the parameter's own unit: Hz, ms, dB, semitones, octaves; percentages are 0..1. Choice parameters take the option name or index; switches true/false. `list_parameters` (with `prefix`) tells you id, range, default, current value.

- **kick**: `kickOn`, `kickKeyFollow`, `kickTune` (st), `kickPitchAmount` (scales the pitch envelope), `kickLength` (stretches both envelopes), `kickPhase`, `kickHarmonics` + `kickHarmDecay` (ms) + `kickHarmTilt`, `kickClick` (level) + `kickClickLength` (ms) + `kickClickTone` (Hz), `kickClipMode` (Soft/Hard/Tube), `kickDrive`, `kickHighCut` (Hz), `kickLevel`, `kickFxChannel` (None, A–E), `kickFxAmount`.
- **bass layer N** (`bass1…bass3`): `bassNOn`, `bassNLevel`, `bassNGate` (LENGTH: scales note lengths), `bassNDuck` (sidechain depth), `bassNGrid`, `bassNFxChannel`, `bassNFxAmount`, `bassNOctave` (−2..1), `bassNTune`.
  - Oscillators `bassNA…` / `bassNB…`: `Table`, `Pos` (position in the wavetable), `Level`, `Semi`, `Fine` (cents), `Unison` (1–7), `Detune` (cents), `Blend`, `Warp` (Off, Sync, Bend, PWM, Mirror, FM (B) – FM only on A, OSC B modulates A even when its level is 0), `WarpAmount`, `Phase`, `Random` (random start phase; 0 = identical attack every note), `Pan`, `Stereo` (unison spread).
  - Tables (index): Basic, PWM, Sync, Harmonics, FM, Growl, Fold, Formant, Digital, Phase Dist, **Classic Sine (10), Classic Triangle (11), Classic Saw (12), Classic Square (13)**, User WAV. Classic tables ignore `Pos`.
  - `bassNWarpEnv` (extra warp on A at the start of each note), `bassNSub` + `SubOctave` (sine sub, mono), `bassNNoise`.
  - Filter: `FilterType` (LP 24, LP 12, Band, High), `KeyTrack`, `Cutoff` (Hz), `Resonance`, `EnvAmount` (octaves the envelope opens the filter), `EnvDecay` (ms), `VelToEnv` (accent: velocity opens the filter more).
  - Amp: `Attack`, `Decay`, `Sustain`, `Release` (ms), `Drive`. (`Glide` is no longer used: a glide note glides during its own length.)
  - Gate: `GateOn`, `GateGrid`, `GateDepth` (0..1), `GateSmooth` (ms).
- **sidechain**: `scMode` (Kick envelope | Curve), `scLength` (Kick, 1/16, 1/8, 3/16, 1/4, 1/2), plus `bassNDuck` per layer and `fxXDuck` per FX channel. Use `set_sidechain_curve` for curve points.
- **FX channels** `fxA…fxE`: `fxALevel` (0..1.5), `fxADuck`, EQ (`fxAEqHp`, `fxAEqLp`, `fxAEq1F/1G/1Q … 4F/4G/4Q`), compressor (`fxACompThr/Ratio/Att/Rel/Makeup`; ratio 1 = off). Slots with effects: `set_effect`.
- **mixer**: `mix<Kick|Bass1|Bass2|Bass3|FxA…FxE><Gain|Pan|Duck|Max>` (Gain in dB −60..+12; Duck = sidechain applies; Max = goes through the maximizer).
- **master**: `masterOn`, `masterDrive` (dB), `masterClip`, `masterCeiling` (dB), `masterWidth`, `masterEnhance`, `masterMonoBelow` (Hz), `masterRelease`, and `gain` (output).
- **LFOs**: `lfo1…lfo3` `On`, `Shape`, `Rate` (tempo synced); routes with `set_modulation`.

## 4. Kick envelopes

`set_kick_envelopes` takes nodes `[time_seconds, value, tension]`.
- **pitch**: semitones above the tail pitch, 0..72, up to 0.5 s. Example psy kick: `[[0,38,-0.65],[0.010,17,-0.45],[0.035,4,-0.3],[0.085,0.4,-0.2],[0.12,0,0]]` (start ~38 semitones above the tail, settles within ~100 ms).
- **amp**: volume 0..1, up to 2 s. The kick ends at the last node, which must be 0. Example: `[[0,0,0],[0.0005,1,0],[0.03,1,0],[0.075,0.78,0.15],[0.19,0,0.35]]`.
- Tension: a segment moves as `t^(4^tension)`. Negative = fast first, then slow; positive = holds, then moves late.
- Keep the whole kick ≤ one eighth note (≈ 207 ms at 145 BPM). `render_preview part=kick` measures length and tail.

## 5. The pattern language (`set_pattern`)

Bass layer: `bars` = one text per bar (repeated to fill four bars), `grid` = step resolution ('1/16', '1/8', '1/8 T', '1/16 T', '1/32', '1/64', '1/4'). Words separated by spaces or `|`:
- `.` rest
- `g` ghost (42 %), `m` soft (62 %), `x` normal (80 %), `X` accent (100 %) velocity
- then optionally pitch (`+7`, `-5` semitones from the key), `~` makes it a **glide note** (no new note: it glides the sounding note to this pitch during its own length), `:50` length as percent of the step

Examples (grid 1/16, 16 words per bar): classic rolling `". m x x . m x x . m x x . m x x"`; with a melodic turn in beat 4: `". m x x . m x x . m x x . m+3 x+7 X+12"`; hypnotic gaps `". m . x . m x . . m . x . m x ."`. Triplet K-B-B: grid `1/8 T`, `". x x . x x . x x . x x"`. Offbeat: grid `1/8`, `". x . x . x . x"`.
Or exact `notes`: `[{"tick":0,"length":24,"pitch":0,"velocity":0.8,"slide":false}]`.
Kick: layer `"kick"`, `bars` of 16 characters `x`/`.`: `["x...x...x...x..."]`.
A glide note needs a sounding note to glide: write a long note, then a short `~` note later inside it, e.g. a 1/16 grid with a one-bar note and `~` words with pitch: use `notes` for exact control (the long note `{"tick":0,"length":384}` and the glide `{"tick":192,"length":96,"pitch":12,"slide":true}`). Gate steps: `set_gate` with `bars` of `x` (open) and `.` (closed).

## 6. Effects

`list_effects` gives all 13 types and their knobs. Use `set_effect` with channel A–E, slot 1–5, type and knobs by name. Sources reach a channel through their sends (`bassNFxChannel` = 1 for A … 5 for E; `bassNFxAmount` = how much goes through, the rest stays dry); the channel returns with its `Level` and `Duck`. For parallel effects (reverb, echo) send a moderate amount and give the channel an EQ high-pass (`fxAEqHp` ≈ 150–300 Hz) so the low end stays clean, and `fxADuck` so tails pump with the kick.
The Echo is modelled on FL's Fruity Delay 2 (TIME, OFFSET, IN PAN, IN VOL, FB MODE Normal/Invert/Ping-pong, FEEDBACK, CUT, DRY VOL; echoes are IN VOL × input, the dry sound DRY VOL).

## 7. What `render_preview` tells you

Metrics: peak (dBFS), RMS, DC offset, spectral centroid (Hz), energy per band relative to the total (<60, 60–250, 250–1k, 1–5k, >5k Hz), side-vs-mid overall and below 150 Hz, and for `part=kick` the kick length (ms) and the tail frequency (Hz). **Checks**: finite audio, no clipping (peak ≤ −0.3 dBFS), small DC, bass mono below 150 Hz, kick ≤ 1/8 note, kick tail within about two semitones of the bass root.
The spectrogram has a log frequency axis 20 Hz–20 kHz, bright = loud, vertical lines are beats (numbers = bars). For a kick close-up the time axis is the first 0.4 s: look for the pitch sweep falling from the top to a steady line at the tail frequency, and for the click as a short broadband smear at the very start. Bass: look for the filter envelope (brightness opening and closing each note) and a clean, steady fundamental.

Also check yourself: the first bass note after a kick is a little softer (rolling feel); notes do not overlap (lower `bassNGate`, or shorten notes); the bass fundamental and the kick tail agree (or the bass sits an octave above); wide/unison sounds stay narrow in the low end (`masterMonoBelow` helps, but keep `Stereo` modest on low layers).

## 8. Pitfalls

- A pattern note beyond the pattern length (`patternBars`) is not played but is kept.
- Do not confuse note LENGTH (`bassNGate`, how long notes sound) with the GATE (an on/off chopping rhythm). A glide note is not a normal note: with nothing sounding it plays as a normal note.
- `bassNLevel` is the layer's own volume; the mixer gain (`mixBass1Gain`) comes after it. Use one of them consistently.
- Presets `section=Bass` store a layer's sound (not its on/off, gate, duck, grid, sends); `Full` stores everything. User presets are plain files in `Documents\Kickbase\Presets\<Section>`.
- FX knob parameters (`fxA1P1…`) are normalized 0..1 and depend on the slot's type: always use `set_effect`.
- The user wants **intuitive, clear** sounds with good names; do not leave a project in a half-edited state without saying so; never claim to have heard a sound.
