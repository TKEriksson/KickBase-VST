# Kick design

Use this beside, not instead of, [the project kick/bass reference](../psytrance-kick-bass-reference.md). Numbers below are source claims, not universal presets.

## Core rules

**Reserve the sub foundation** (kick 30–90 Hz; common peak 45–60 Hz) — a deeper, louder tail requires a lighter or higher bass part — *Our control/tool:* Kick pitch envelope, KEY/Fixed, TUNE, Analyzer — Every Genre's Kick; Sound Design Compendium [one source]

**Choose tail pitch by translation, not a doctrine** (45–50 Hz is proposed as a system-friendly target; F1–G1 is 44–49 Hz) — tail/key matching can reinforce musical coherence, but a fixed 45–50 Hz kick may translate better across keys — *Our control/tool:* KEY/Fixed and TUNE; analyzer A/B — Sweet Spot; project reference [contradiction]

**Keep the kick inside the available gap** (at 144 BPM, 1/8 ≈ 208 ms) — bass needs a predictable entry after the tail — *Our control/tool:* Kick amplitude envelope and bass DUCK curve — 7 Rules; project reference [verified]

**Shape attack separately from body** (pitch sweep about 30–80 ms; body roughly 100–200 Hz) — short high-frequency motion supplies click without making the sub tail bright — *Our control/tool:* Kick pitch envelope, Click layer, Drive — Tweaking Kicks; project reference [one source]

**Reset kick phase on every hit** (fixed start phase) — repeated kicks retain the same waveform and low-end level — *Our control/tool:* Kick oscillator phase reset — 7 Rules; project reference [verified]

**Use a controlled post-transient dip** (about −3 to −9 dB, 10–30 ms after attack) — a “fishtail” amplitude contour can clear boxy 200–600 Hz before the tail — *Our control/tool:* editable Kick amplitude envelope — project reference [one source]

**End at zero, or fade the rendered end** (last sample/envelope value = 0) — avoids clicks and overlapping tail artefacts — *Our control/tool:* Kick envelope end node; SAKNAS → zero-crossing/end-click warning — 7 Rules; project reference [verified]

**Balance kick and bass as one budget** (e.g. a 40 Hz kick leaves bass above it; 50–60 Hz kick can dominate low end) — level, tail length and bass octave compensate for each other — *Our control/tool:* Analyzer, MASTER meters, DUCK — Every Genre's Kick [one source]

**Avoid non-kick occupants below the low-end boundary** (HPF other musical parts around 100–120 Hz) — preserves headroom and kick/bass definition — *Our control/tool:* FX-channel EQ and MASTER EQ — 7 Rules; Mixing & Effects Compendium [one source]

**Keep kick/sub centred** (mono below 100–120 Hz) — stereo phase differences weaken club playback — *Our control/tool:* SOUNDSTAGE mono-under-frequency control — Mixing & Effects Compendium [one source]

## Verification

**Compare time and spectrum together** (per-hit waveform plus 30–120 Hz spectrum) — a visually neat kick can still overtake the bass, while a short/quiet kick may need more bass energy — *Our control/tool:* live scope, spectrum and offline render — Every Genre's Kick [one source]

**Do not copy a genre contour blindly** (kick shapes vary from narrow/tonal to broad/round) — arrangement and bass allocation determine the useful contour — *Our control/tool:* preset A/B and analyzer snapshots — Every Genre's Kick [one source]

## Tool ideas

- Tail-length overlay showing the first bass onset and 1/8-note boundary at current BPM.
- Kick-tail fundamental tracker with a 45–50 Hz translation warning, not a hard rule.

## Analyzer ideas

- Per-hit phase/peak consistency score and zero-end/click detector.
- Low-end allocation display: kick, bass, and all other channels from 30–200 Hz.
