# Plugin and effect tips → builds (FXMAP)

Every plugin, effect or technique named in `transcripts/*.txt`, both PDF compendia and the research-notes, mapped to what we already have. Own words; source = title; plugins named are inspiration only, we write our own DSP from standard formulas (AGPL is no issue, nothing is copied).
Read directly 20 okt: all effect-heavy transcripts and both PDFs (`pdftotext`; the PDFs' article links were **not** opened). `tips.txt`, `guidesonline.txt.txt` and both PDFs read like AI-compiled lists → `[osäker]` unless a transcript says the same.
**Ours:** HAVE = exists (effect type in `Source/Core/Effects.h:19`: filter, distortion, eq, chorus, phaser, echo, reverb, compressor, gate, comb, ringMod, bitcrusher, autoFilter, maximizer, ducker; 11 FX channels × 5 slots); B/A-nr = `gaps-to-specs.md`; NEW = no row there. Verdict: BYGG / SKIPPA / SENARE. Prio 1 = do first.

## Phase, low end
| Tip / plugin | Source [label] | What it does | Ours | Verdict, reason | P |
|---|---|---|---|---|---|
| Disperser / all-pass spread (Kilohearts) | Sweet Spot; tips; both PDFs [verifierad] | Smears transient phase; "spongy", even bass | B7, B2 | BYGG: biquad all-pass, trivial CPU, core psy trick | 1 |
| Phase rotation of the whole bass 0–360° (PHA-979) | Sweet Spot; Phase Alignment [verifierad] | Finds the angle where kick+bass fuse; must sit after filters | B1 (start phase only) | BYGG via B7 first; true rotation (Hilbert, latency) SENARE | 2 |
| Crossover phase tweak between lowest bands (Quadrafuzz, multiband) | Sweet Spot [en källa] | Shifts phase at the first split | B2 + B6 | SENARE: needs B6 split first | 3 |
| EQ high-pass stack as phase shifter to hit zero crossing | Phase Alignment [en källa] | Cascaded HPF moves the transient to zero crossing | HAVE FX eq | SKIPPA: B2/B7 do it cleaner; document only | 3 |
| Polarity flip snare/clap/hat vs kick | Instant Clarity [en källa] | Stops cancellation, kick+snare level jump ≈3 dB | B1 | BYGG inside B1 (switch per instrument) | 1 |
| Nudge/micro-delay of bass | both PDFs; guidesonline [osäker] | Avoids tail/onset clash | HAVE `bassNDelay`; B1 adds 0.05 ms steps | done / in B1 | – |
| Start phase on retrigger, random off | 7 Rules; Sweet Spot [verifierad] | Same waveform every hit | HAVE `kickPhase`, `bassNAPhase`, `bassNARandom` | done | – |
| Transient shaper / transient rack | NAIL; Mixing PDF [en källa] | Shapes click vs body | B10 | BYGG: two envelope followers, cheap | 2 |
| Octave down/up sub layer | project notes [osäker] | Half-rate oscillator / `x·abs(x)` | B10 | BYGG with B10 (own oscillator, exact) | 2 |
| Linear-phase HP/LP for kick+bass only | Is your Bassline clean; Waves PDF [en källa] | Removes phase shift, adds pre-ringing | NEW | SKIPPA now: latency (A8), FFT cost; revisit after A8 | 3 |
| Avoid resonant low-pass on kick/bass in outros | Interesting Yet [en källa] | Resonance boosts the sub | HAVE filter resonance | SKIPPA code; tooltip/analyzer warning only | 3 |
| Check each plugin's phase shift on a scope (SysScope-type) | NAIL; What is causing [en källa] | Cleanliness check | HAVE scope; A8 latency, B2 phase curve | in AN: show phase curve per FX | 2 |
| Additive synth, per-partial level+phase | image BMP [osäker] | "Instant phase shift" of the fundamental | B13 | SENARE: heavy UI | 3 |

## Distortion, dynamics, loudness
| Tip / plugin | Source [label] | What it does | Ours | Verdict, reason | P |
|---|---|---|---|---|---|
| Multiband saturation above ~2 kHz (Saturn, Quadrafuzz) | tips; Mixing PDF; Sweet Spot [en källa] | Presence without dirtying the sub | B6 | BYGG: LR4 3-band + existing curve | 1 |
| Soft clip / tape / tube on kick click and mid-bass | tips; both PDFs [osäker] | Peaks rounded, harmonics | HAVE distortion, maximizer CLIP (check drive curves via `list_effects`) | SKIPPA new; add a tape curve only if list lacks it | 3 |
| Bitcrush / downsample on squelch | Squelches [en källa] | Digital grit | HAVE bitcrusher | done | – |
| Dynamic EQ (TDR Nova) | Mixing PDF; mix-master-fx [osäker] | Cuts 200–500 Hz only when the lead plays | B9 | BYGG: band-pass detector + one band | 2 |
| Compressor with high-pass in the detector | Heavy Low End [en källa] | Kick/bass stop driving the pump on the master | NEW | BYGG: one HPF in the detector, no latency | 1 |
| Group glue ~1.5:1 | Mixing PDF [osäker] | Gentle bus glue | HAVE compressor | done | – |
| Parallel compression (dry/wet) | Mixing PDF [osäker] | Weight without losing transients | HAVE compressor (check a MIX knob in `list_effects`) | SENARE: add MIX if missing | 2 |
| Linear-phase multiband compressor glue | Strong Prog Basses [en källa] | Glue with no phase shift | NEW | SKIPPA: CPU+latency, B6 split covers most | 3 |
| Limiter delta listen + low-band compare | Unfck Master; What is causing [en källa] | Hear what the limiter removes; test modes on <100 Hz | NEW (A4 covers LUFS only) | BYGG in AN/LC: render with/without, diff, low-passed | 1 |
| Alternative limiter characters (Ozone eARC, Pro-L styles) | Unfck Master [en källa] | Different low-end behaviour | HAVE maximizer (one algorithm) | SENARE: second mode after the delta tool shows need | 3 |
| Clipper/limiter on kick+snare group only | Instant Clarity [en källa] | Reduces only the combined peak | HAVE maximizer on one channel (`kickFxChannel`) | done; add coincidence check to AN | 2 |
| Timed ducking / volume shaper (LFOTool, VolumeShaper) | tips; both PDFs; NAIL; Phase Alignment [verifierad] | Repeatable pump instead of slow detector | HAVE Sidechain kick curve, `bassNDuck`, ducker | done | – |
| Multiband sidechain shape (ShaperBox) | 99 Minutes [en källa] | Different pump for low vs high band | NEW | SENARE: reuse B6 split; prio after B6 | 2 |

## Stereo, space, motion
| Tip / plugin | Source [label] | What it does | Ours | Verdict, reason | P |
|---|---|---|---|---|---|
| Widener / Imager / Hyper Dimension (highs only, no lows) | tips; Strong Prog Basses [en källa] | Width without mono loss | B8 | BYGG: M/S, side HPF 120 Hz | 2 |
| Widen only transients/highest band (M-stereo generator) | Strong Prog Basses [en källa] | Wide attack, mono body | B8 variant | SENARE: option in B8 once it exists | 3 |
| Split stereo pan vs balance pan | How to Pan; 99 Minutes [verifierad] | Moves L and R independently | B8 | BYGG: independent L/R gains | 2 |
| Left-only/right-only channel select (Utility) for mono hat | 99 Minutes [en källa] | Mono source with no comb effect | NEW (small) | SENARE: add to B8 as a pan mode | 3 |
| Kick/sub mono below 100–120 Hz; mono button | many [verifierad] | Club translation | HAVE `masterMonoBelow`; A3 measures | done | – |
| Two random-phase stereo layers vs mono precise phase | Strong Prog Basses [en källa] | Wide layer without phase problems | HAVE random-phase switch, OSC pan/spread | done | – |
| Flanger | tips; Mixing PDF [osäker] | Sweeping comb | B9 (chorus code, 0.1–10 ms) | BYGG: cheap | 2 |
| Phaser / chorus / LFO filter sweeps (16–32 bars) | tips; Mixing PDF; 7 Rules [osäker] | Slow motion in pads | HAVE phaser, chorus, autoFilter, LFO 1–10 | done | – |
| Gated chops / triggered filter / 1/16 motion | Mixing PDF; Squelches [osäker] | Rhythmic movement | HAVE gate, autoFilter, `set_gate`, lanes | done | – |
| Shimmer reverb (+12 st in feedback) | tips; Mixing PDF [osäker] | Floating pad tails | NEW (Psy Multi-FX list in plan-v2, no B-nr) | SENARE: pitch shifter cost; low value for kick+bass | 3 |
| Reverb on send, pre-delay 20–50 ms, return HPF 300 Hz / LPF 6 kHz | tips; Mixing PDF; 7 Rules [verifierad] | Keeps dry transient and floor clean | HAVE FX send + eq (check a pre-delay knob on reverb) | done / verify pre-delay | 2 |
| Delay: dotted 1/8 ping-pong, 1/16, EQ after (cut <300 Hz, >6 kHz) | tips; Squelches; 7 Rules [verifierad] | Moving answer | HAVE echo (feedback modes, CUT); check dotted/triplet sync | verify; SKIPPA new work | 2 |
| Long delay/reverb tails to bridge sections, A/B | 7 Rules; Interesting Yet [en källa] | Section flow | HAVE echo/reverb, automation | done | – |
| Reese / detuned unison saws (darkpsy) | Sound Design PDF [osäker] | Wide dark bass | OSC 1–3 only; no unison control found in AGENT_GUIDE | SENARE: ask Tobias if wanted | 3 |
| FM 3 operators; band-pass squelch + pitch env | Squelches; Sound Design PDF [en källa] | Metallic leads, squelch | B11, B5 | BYGG B11 after A6; B5 is routing only | 2 |
| HPF others at 100–120 Hz; mud cut 200–500 Hz | 7 Rules; tips; PDFs [verifierad] | Low-end headroom | HAVE FX eq; A7 measures leaks | done | – |
| EQ by frequency zones (print bus) | Over EQ'ing [en källa] | Zone-by-zone workflow | – | Analyzer view only (below) | 3 |

## Top 5 with benefit for psytrance kick+bass but no phase yet
1. **Compressor detector HPF** (master + FX compressor): fixes pumping from kick/bass, no latency; one filter in existing code.
2. **Limiter delta + low-band loudness compare** (AN/LC): shows whether the limiter eats the kick; offline diff, no new DSP.
3. **Per-band ducking** (ShaperBox-style): different pump low vs high; cheap once B6's LR4 split exists.
4. **Compressor MIX / parallel compression** (check `list_effects` first): one knob, big effect on group energy.
5. **Kick+snare coincidence check** (AN): flags ≈3 dB jump where they hit together; pairs with B1 polarity and the group maximizer.
None of these has a B/A-number in `gaps-to-specs.md`; 2 and 5 are new A-items, 1/3/4 new B-items (FX).

## Verktygsidéer
- Detector HPF knob on compressor; MIX knob if missing (FX).
- Per-band ducker (reuses B6 split); second maximizer mode.
- Mono-L/R select as a B8 pan mode; transient-only widening as a B8 option.
- Hilbert phase rotation of audio (later); shimmer reverb (later).

## Analysatoridéer
- Limiter delta wave, low-passed (<100 Hz) loudness with/without limiter.
- Kick+snare coincidence level jump and group peak.
- Phase curve per FX slot (all-pass, filters) next to A8 latency.
- Frequency-zone view (30–120, 120–250, 250–500 Hz…) on the master/returns.
