---
name: kickbase-sound-design
description: Use when designing, tuning or judging a psytrance kick or bass sound in Kickbase (presets, parameters, render_preview checks, analyse_mix loop, kick/bass mixing).
---

# Kickbase sound design

Start with `get_guide` (MCP): it is the tool manual and workflow, always current, and covers kick/bass phase (`get_mix_report` with `include lowband`), Live Code, LFO 1-10 and stacked-note polyphony. The topics `get_guide topic=school` (kick and bass school) and `topic=psytrance` (numeric reference) go deeper. Read the recipes in guide section 9 before fixing a finding.

You can only measure, not listen: the numbers and the spectrogram are your ears. Tell the user what you changed and why, and ask them to listen.

## The loop (render, analyse, adjust, again)

Goal: no `problem` or `warning` finding left that you can act on, and the checks in `render_preview` pass.

1. `list_instances` / `use_instance` if the user wants it heard in the DAW; otherwise `headless`. `get_state` once to know the starting point (and `save_preset` a backup if the user cares about it).
2. `render_preview` full, then `part=kick` and `part=bass`. Note the metrics (kick tail frequency 40-55 Hz, kick length, DC, peak, band energy) and the checks that fail.
3. `analyse_mix` (starts in the background), then `get_mix_report` until it is done. Do not change anything while it runs. Read the findings (problem, then warning) with `sources` and `bars`.
3b. To choose a value instead of guessing, `sweep_parameter` (e.g. `kickDrive` at 3-5 values, `part=kick`) gives a table of metrics per value and puts the sound back; then set the best value with `set_parameters`.
3c. With a reference recording, `score_vs_reference reference_file=... name=r2` gives a distance (0 = same) and the terms behind it (bands, RMS, centroid, kick tail/length). Render the same `part` as the reference kind. It shows which way a change moved the sound; it is not a quality score.
4. Pick ONE finding, the most severe. Change one thing (at most two knobs that belong together) with the matching recipe from the guide. Never tune several unrelated things in one round: you could not tell which one helped.
5. `render_preview` (give each render a `name`, e.g. `r1`, `r2`) and `analyse_mix` again. `compare_renders before=r1 after=r2` gives the metric deltas and the checks that changed. Compare with the previous round: finding gone, still there, or a new one.
   - Better: keep it, go to the next finding.
   - Same or worse: `undo`, try a different fix for the same finding (once).
6. Repeat from 4.

Stop when ANY of these holds:
- no `problem`/`warning` findings and the render checks pass (success);
- 6 rounds done (a round = steps 4-5), report what is left;
- the same finding is still there after two different fixes: undo, stop, tell the user what you tried and ask;
- a fix would need a musical decision (retune the key, change the pattern, a different kick): stop and ask.

Rules for the loop:
- Do not "fix" `info` findings unless the user asks. Do not overclaim what a finding proves (see guide section 9, especially phase and tuning).
- Change levels, EQ and sidechain depth before pattern, notes or the sound's character. Never touch the pattern or Live Code rows the user locked.
- Renders are meant to be repeatable (same state, same numbers); if a number moves when you changed nothing, say so instead of chasing it.
- Finish with `add_report_note` per finding (`applied: true` if changed) and a short summary: what changed (knob, from, to), which findings are gone, which remain, what you could not judge by numbers.
