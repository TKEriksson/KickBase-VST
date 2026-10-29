#include "HelpDialog.h"
#include "Theme.h"

namespace
{
    // ---- Texterna (markdown-lik: "# " och "## " rubriker, "- " punkter, tomma rader mellan stycken) ----

    const char* pianoRollText = R"(# PIANO ROLL
Here you write the notes of one row (the kick or an instrument). Notes are placed on a grid; the strip at the bottom is velocity (VEL). **+ LANE** (under the keys) adds more lanes below it, up to four; click a lane's name to pick its parameter or remove it, and drag the thin bar above a lane to change its height. In an extra lane (CUTOFF, PAN or FX SEND) click or drag to give a note its own value; **Alt + click** removes it, so the note follows the knob again. Values copy, cut, paste and move with the note, and one drag is one undo step. Stacked notes use their first note's values.

## Drawing notes
- **PENCIL**: click an empty spot to add a note, drag it to move it, drag its right edge to change its length.
- Click another pitch at the same tick to stack a note (up to four).
- **ERASER**: click or drag over notes to remove them. Right-click a note also deletes it.
- **SNAP** and **GRID**: notes snap to the grid (1/4 down to 1/64 and triplets). SNAP off = place freely, tick by tick.

## Selecting and editing
- **SELECT**: drag a box in an empty spot. Shift + drag does this with any tool.
- Selected notes move, stretch and delete together.
- **COPY** / **PASTE**: Ctrl + C, Ctrl + V. Choose where the copy goes, then OK (Enter) or CANCEL (Esc).
- Arrow keys move the selected notes (Shift = octave). Delete removes them, Esc clears the selection.

## Keys
- Hold **Alt** = eraser, hold **Shift** = select box.
- **Ctrl + A** select all, **Ctrl + C / X / V** copy / cut / paste.
- Scroll = pitch, **Ctrl + scroll** = zoom, **Shift + scroll** = move in time.

## Chords
- Select notes, right-click an empty spot and pick a chord.
- The chord notes show as outlines above or below the note. They play only with VOICES 2 or more.

## Glide
- With **GLIDE** on, the notes you draw are glide notes (pink). A glide note glides the sounding note to its pitch during its own length, then the pitch stays.
- Double-click a note to turn glide on or off for it.
- **STROKE**: right-drag on an empty spot to draw a glide from left to right. A right-click without dragging opens the menu.

## Row length and view
- **LENGTH** sets how long the row is; **+ BAR** adds an empty bar. Clicking in the shaded bar after the end also grows the row.
- **FOLLOW** flips the view to the bars that play. **GHOST NOTES** shows other rows faintly behind this one (display only).
- **POP OUT** opens the piano roll in its own window.

## Listening, presets, MIDI
- **LISTEN**: hold to hear just this row on the preview note, looping while you hold.
- **MIDI IN**: what the keyboard plays while the piano roll is open.
- **PRESET** saves or loads this row's notes. **MIDI** exports or imports a MIDI file.
)";

    const char* liveCodeText = R"(# LIVE CODE
Write patterns as text and hear them on the kick and the instruments. It is based on Strudel, with Kickbase's own sounds and parameters.

## Play and stop
- **AUTO** (the first button): when it is on, valid code runs by itself after each edit. Nothing runs when a project loads. **STOP** turns it off and stops.
- **CREATE** writes the pattern from your code into the piano rolls (it asks before overwriting rows that already have notes).
- **CODE ONLY** solos the rows Live Code has written, so you hear only them.
- **LOCK ROW**: a locked row (K = kick) is never written by Live Code, so your own notes stay.
- **FLOAT** puts the editor in its own window. **FORMAT** puts one call per line (Ctrl + Z undoes it).
- Errors and `console.log` output show in the console tab.

## Blocks
Each line starting with `$:` is one pattern that plays on its own.
- `$: s("bd")` plays the kick. `$: s("i1")` plays instrument 1 (i1 to i10).
- Mini-notation: `s("bd ~ bd bd")` is four steps, `~` is a rest. `[a b]` squeezes two steps into one. `a*2` repeats, `a/2` slows down.
- `<a b>` alternates every cycle. `a, b` plays both at once.

## Notes
- `note("c2 e2 g2")` plays pitches. In Kickbase **C2 is note 0**.
- Chords: `note("c2'major")`, and so on (see the autocomplete list for all chord names).
- Sharps and flats: `c#2`, `eb2`.

## Changing sounds
- `.lfo(...)` and `.env(...)` move a parameter over time.
- `slider(...)` gives a knob you can drag while the code plays.
- `setEffect(...)` picks an effect type for an FX slot.
- Parameters are set once per run: edit the code (or restart AUTO) to apply a new value.

## Autocomplete
- Type a word and a list of methods, globals and chord names appears. Use the arrow keys and Enter (or Tab) to pick one, Esc to close it.

## More examples
- See docs/live-code-exempel.md in the project for longer examples.
)";

    const char* soundstageText = R"(# SOUNDSTAGE
The sound stage: stereo width, bass in the centre and a live view of frequencies and stereo image.

## How it works
On the SOUNDSTAGE page there are 16 buttons under "AFFECTS" (kick, instruments 1-10, FX A-E). A source that is switched off skips WIDTH, BASS MONO and ENHANCE. It still gets the master EQ, drive and limiter. All on is the default and sounds as before.

## The knobs
- **WIDTH**: 0 % = mono, 100 % = as it is, 200 % = extra wide.
- **BASS MONO**: removes the side signal below this frequency, so the bass stays tight and in the centre.
- **ENHANCE**: makes mono sound wider, and is mono compatible.
)";

    const char* oscText = R"(# OSC 1 / 2 / 3
One oscillator of the bass layer. It plays a wavetable: a row of waveforms that POS moves along.

## Choosing the sound
- **TABLE**: the wavetable. Click for categories, or load your own with **WAV** (2048-sample cycles become up to 16 waveforms).
- **SIN / TRI / SAW / SQR**: the classic waveforms in one click (wavetables are in the TABLE list).
- **MORPH**: on = POS fades smoothly between waveforms, off = it jumps between them.
- **WARP** and **AMOUNT**: bends the waveform. With RING (B) on OSC 1 a menu picks what OSC 1 is multiplied by (OSC 2, OSC 3 or both).

## The picture
Click and hold on the picture to hear this oscillator alone. The small button in the corner switches between wave, scope and spectrum.

## The knobs
- **POS**, **LEVEL**, **SEMI**, **FINE** (pitch), **PHASE**.
- **UNISON**, **DETUNE**, **BLEND**, **STEREO**, **PAN**: stacked voices, how far apart they are tuned, and how they are spread in the stereo image.
- **RAND**: random start phase.
- Hover any knob for its own explanation.

## Presets and filter
- **PRESET**, **<** **>**, **SAVE**, **RESET**: only this oscillator changes, not the rest of the sound.
- **FILTER**: on = this oscillator goes through Filter 1 and 2 (set on the FILTER tab).
)";

    const char* filterText = R"(# FILTER
Two filters per bass layer, with a curve that shows what they do to the sound.

## The curve
Low frequencies on the left, high on the right. The line in the middle is 0 dB.

## The switches
- **FILTER ON**: off = the whole instrument bypasses both filters (the amp envelope still works).
- **Sources button**: tick which sources go through the filters (OSC 1-3, SAMPLE, NOISE). Unticked sources bypass both.

## Filter 1 and Filter 2
- **MODEL** and **TYPE**: the filter design and its shape (low-pass, high-pass and so on). The two knobs P1 and P2 change name with the model and are dimmed when a model does not use them.
- **CUTOFF**, **RESONANCE**: where the filter acts and how much it emphasises that point.
- **KEYTRK** (Filter 1): the cutoff follows the note. **MIX** (Filter 2): how much of Filter 2 you hear.
- Filter 2 has its own on/off. Off, it costs no CPU and the sound is exactly as with Filter 1 alone.

## Routing and allpass
- **ROUTING**: SERIAL = filter 1, then filter 2. PARALLEL = both filter the same sound and MIX blends them.
- **ALLPASS** (FREQ and AMOUNT): a phase-shifting stage after the filters. AMOUNT 0 = off.
- Hover any knob for its own explanation.
)";

    const char* fmText = R"(# FM
A sine modulator bends the phase of a sine carrier. Few knobs, a lot of timbres.

## How it works
- The **carrier** is the tone you hear, at the note's pitch.
- The **modulator** is a second sine that you do not hear directly. It wobbles the carrier's phase and adds overtones.

## The knobs
- **PRESET**: fills in the knobs (sometimes also the pitch envelope or Filter 2). Touch a knob afterwards and the menu jumps back to Custom.
- **LEVEL**: how loud the FM engine is in the instrument.
- **RATIO**: modulator frequency relative to the carrier. Whole numbers (1, 2, 3) sound harmonic, other values sound metallic or bell-like.
- **INDEX**: how hard the modulator bends the carrier. Higher = brighter and rougher.
- **ENV AMT**: how much extra INDEX each note starts with.
- **ENV TIME**: how fast that extra brightness falls away after the note starts. Short = a click or pluck, long = a slow sweep.
- **SEMIS**: transposes the engine in semitones.
- **FILTER** (checkbox): send the FM engine through the filters, or let it bypass them.

## Tip
Start from a preset, then move INDEX and ENV TIME. For a deep bass keep RATIO at 1 or 2 and INDEX low.
)";

    const char* mixText = R"(# MIX
The balance of one bass layer: one strip per source, and the combined waveform on the left.

## The strips
- **OSC 1-3, SAMPLE, NOISE, SUB, FM**: one strip each.
- **LEVEL**: how loud that source is.
- **PAN** and **WIDTH**: where it sits and how wide it is. SUB has neither: it is always mono, in the centre.
- **START** (SUB and FM): the phase the source starts at on each note.
- **The meter** in each strip shows that source alone, after the filters, envelope, pan and width. Red = above -3 dB.

## The combined waveform
Everything this layer plays, a few cycles after the note starts (110 Hz).
- **Mouse wheel**: zoom in time. **Shift + wheel**: zoom the height.
- **Middle button drag** or the **scroll bar**: move along the note.
- **Double-click**: reset the view.
)";

    const char* timingText = R"(# TIMING
The **TIMING** tab (next to MIX) shows the timing of every source in its own panel, with a plot of its level over time. The playhead follows the last note and a line marks when it was released.
- **MODE**: GATE = the source runs silently and is let in after POS DELAY (as in earlier versions). SHIFT = the source also starts from scratch there (OSC 1-3 and FM restart their own envelopes too; the pitch envelope is shared and keeps running).
- **POS DELAY**: the source is silent this long (0-4000 ms) after each note starts. A yellow warning appears when it is longer than the note's volume envelope.
- **SYNC** (under POS DELAY): note values at the project tempo (1/64 to 4 bars) instead of milliseconds. With SYNC on, the knob is hidden and a **x** field sets how many note values (1-8). Fixed when the note starts.
- **LENGTH**: how long the source sounds after its POS DELAY before it has faded out. 0 = the whole note. It has the same **SYNC** and **x** choices.
- **CURVE**: the shape of that fade: 0 % = straight, above = fast first, below = slow first.
- Everything is off by default, so existing sounds are unchanged.
)";

    const char* sampleText = R"(# SAMPLE
A recorded sound (WAV, AIFF or FLAC) as a source for this bass layer, next to the oscillators.

## Loading
- **LOAD**: choose a file. You can also drop a file on the page. The sample is turned on for you.
- **X**: remove the sample. **HEAR**: play it.
- **Single / Multi**: Single uses one sample for all notes. In Multi, LOAD takes many files as zones; name a file like Piano_C3 to set its root note.
- Turn the oscillators off (the power buttons on the OSC tabs) to hear only the sample.

## The knobs
- **LEVEL**: how loud the sample is. **START**: where in the sample each note begins.
- **ROOT**: the note at which the sample plays at its original pitch. **LOOP** and **KEY**: loop mode and whether the sample follows the keyboard.
- **FILTER** (checkbox): send the sample through Filter 1 and 2. Off (default), it bypasses them and keeps its full sound.

## Slices
Cut the sample into pieces. The pattern gets one "Slices" row that opens as a piano roll where each key is a slice ("Slice N"); a note on a key plays that slice at its original speed.
- **Grid**: equal parts (the slider sets how many, max 128). **Auto**: cut at the hits (the slider sets the sensitivity). **Manual**: your own markers.
- **CREATE**: turn the Grid or Auto result into your own markers. Drag a boundary to move it, double-click to add one, right-click to remove one.
- **REVERSE**: plays every slice backwards.
- **SHOW SAMPLE / SLICES**: see the whole waveform, or every slice as its own box. Click a slice to hear it.
)";

    const char* envText = R"(# ENV
Four envelopes for the selected instrument. Drag the white handles in a graph to change the shape.

## The four graphs
- **ENV 1 VOLUME**: the loudness of every note: attack, decay, sustain level and release. Off = every note plays at full level while it is held.
- **FILTER ENVELOPE**: opens the filter at the start of each note and lets it close again (AMOUNT, ATTACK, DECAY). Off = the filter stays at CUTOFF.
- **ENV 2 and ENV 3**: extra envelopes that make no sound themselves. They move other knobs.

## Buttons on each graph
- **ON**: turns the envelope on or off.
- **SYNC**: attack, decay and release become note lengths (1/16 ... 4 bars) that follow the tempo. Off = milliseconds.
- **LISTEN**: hold to hear the instrument on the preview note while you drag. Double-click to keep it playing.
- **DURATION**: how many seconds the graph shows. Only the view, not the sound.
- **DRAG**: drag the handle onto any knob (or set it in the MATRIX tab) to let that envelope move the knob.

## More sources
**VELOCITY** (how hard the note is played) and **RANDOM** (a new random value per note) work the same way: drag them onto a knob.
)";

    const char* lfoText = R"(# LFO
An LFO is a slow wave that moves a knob up and down by itself. Each LFO is a card; **+ ADD LFO** adds the next free one.

## Connecting
- Drag the **DRAG** handle of a card onto any knob (kick, instrument or effects). The knob now follows the LFO.
- Connections are listed on the card. Right-click one to remove it.

## The card
- **ON**: turns the LFO on or off. Its connections stay.
- **SHAPE**: the wave form. **EDIT POINTS** turns it into your own curve: drag points, double-click for a new one, right-click to remove one.
- **RATE MODE**: Sync = note values of the tempo (RATE). Hz = its own speed (FREE RATE).
- **SNAP** (Hz mode): when the free rate comes close to a note value it locks to it, so the LFO sits on the beat. **RANGE** sets how close.

## Cards
- The arrow folds a card to one line. The X or right-click removes it (you are asked first if it moves a knob).
- **SHOW ONLY SELECTED** shows just the selected card. The preset button saves and loads LFO setups.
)";

    const char* matrixText = R"(# MATRIX
Every connection between a modulation source and a knob, in one list. A route says: this source moves that knob by this much.

## Sources
- **LFO**: a slow wave (LFO tab).
- **MACRO 1-4**: a knob of your own, here on the left. Turn it by hand or automate it from the DAW. One macro can move many knobs.
- **ENV 2 / ENV 3**, **VELOCITY** and **RANDOM** of an instrument.

## Making a route
- Drag a **DRAG** handle onto any knob, or press **+ ADD ROUTE** and pick a knob (an LFO is used as the source; change it in the list).
- **SOURCE**: click to pick another source.
- **AMOUNT**: how far the source moves the knob, as a share of the knob's travel. Negative = the other way. Double-click for 25 %.
- **DESTINATION**: click to pick another knob.
- The X on a row removes the route. **CLEAR ALL** removes every route (it asks first).
)";

    const char* effectsText = R"(# EFFECTS
Effects live on **FX channels** (FX1-FX11), like mixer sends. An instrument sends part of its sound to a channel with its FX SEND knob; the channel runs five effects in a row.

## The page
- **FX1-FX11** (left): pick the channel to edit. Each instrument chooses its channel and how much goes through it.
- **EQ / COMP / FX**: three views of the selected channel.
- **FX**: the five slots in a row. Click a slot, then pick its effect type in the box and turn its knobs. Slots run left to right.
- **EQ**: a parametric equalizer with a live view of the sound reaching the channel. Flat by default.
- **COMP**: a compressor that evens out the channel level. Off (1:1) by default.
- **LEVEL**: volume of the channel's effect output.
- **DUCK**: how deeply the kick pushes the channel down, tails included, so reverb and echo don't blur the kick. 0 % = no ducking.

## Presets
**FX Preset** saves or loads the selected channel: its five effects, EQ, compressor, level and duck. Other channels are not touched, and who sends to the channel is not part of it.
)";

    const char* masterText = R"(# MASTER
The last stage before the output. Three views: **MIXER**, **SIDECHAIN** and **MAXIMIZER** (the FX version has SIDECHAIN and MAXIMIZER).
## SIDECHAIN
The kick ducks the other sounds. It has its own help button inside the view (see the SIDECHAIN topic).

## MIXER
One strip per sound source: the kick, the instruments and the FX channels.
- **Fader**: gain of the source.
- **Pan**: left/right position.
- **Meter**: level of the source.
- **SC**: whether the kick's sidechain ducks this source. On by default.
- **MAX**: whether the source goes through the maximizer. On by default.

## MAXIMIZER
Drive, clip and a limiter on the whole mix.
- **DRIVE**: pushes more level into the clipper and limiter. Louder and denser.
- **CLIP**: how much the soft clipper rounds off the peaks.
- **CEILING**: the loudest level that gets out. The limiter never lets peaks pass it.
- **RELEASE**: how fast the limiter lets go after a peak.
- **OUTPUT**: final volume after everything.
- **OUT / GR meters**: output level and how much the limiter turns down.
- **Scopes**: white = the signal into the limiter, red = what comes out. Where white sticks out above red, the limiter is working.
- **THROUGH THE MAXIMIZER** (bottom): pick which sources go through it.
- The power switch (top left) turns the maximizer on and off. **Master Preset** saves its settings.
)";

    const char* sidechainText = R"(# SIDECHAIN
The kick pushes the other sounds down for a moment, so the kick and the bass don't fight for the same space.

## The controls
- **MODE**: *Kick envelope* = the sounds duck exactly as loud as the kick is. *Curve* = you draw the ducking shape yourself.
- **LENGTH**: how long the drawn curve lasts after each kick (Kick = as long as the kick). Only used in Curve mode.
- **Curve editor**: draw the volume after each kick. Low = ducked, high = full volume.
- **HALFTIME**: the scope shows 8 beats instead of 4. Only changes the view, not the sound.
- **DUCK / DEPTH**: how deeply the kick pushes each instrument down. 0 % = no ducking, 100 % = all the way down. One row per active instrument.
- **Scope**: orange = the kick, white outline = instruments before the sidechain, blue = after it. The blue is what gets through.

## FUNDAMENTAL DUCK
Dips only the fundamental of each engine while the kick sounds, so the kick's low end and the bass fundamental don't fight, while the rest of the bass stays.
- **DEPTH**: how many dB the fundamental is cut.
- **WINDOW**: how close (in cents) the kick's pitch must be to the engine's fundamental for the dip to apply fully.
- **SOURCE**: what makes it dip: the kick, another instrument, or any of them.
- Per engine, **INHERIT** follows the common values, **OFF** never dips, **OWN** uses its own depth, window and source.

## SIDECHAIN AFFECTS
At the bottom: which sources get ducked. The same SC switches exist in the MASTER mixer. All on is the default.
)";

    const char* settingsText = R"(# SETTINGS
Window size, drawing speed, the AI link and where Kickbase keeps its files.

## WINDOW SIZE
- **Slider**: scales the whole plugin window from 50 % to 160 %. Use a small value on a small screen. Dragging the corner of the window does the same.
- **SMALL / MEDIUM / LARGE / HUGE**: quick sizes. The size applies to every Kickbase window and is remembered.
- **OPTIMIZATIONS**: how often each view (spectrum, scopes, meters, playhead) is redrawn, whether filled areas are drawn, and REDUCE ANIMATIONS. Lower rates use less CPU.
- **ABOUT**: the version, the licence (AGPL-3.0), where the source code is, and the licences of the software Kickbase is built with.

## AI LINK
Lets an AI assistant that you run yourself control Kickbase. It is off until you switch it on.
- The **access key** is what the assistant needs to connect. **SHOW** reveals it, **COPY** puts it on the clipboard, **CLEAR** removes it.
- Never share the key with anyone you don't trust: whoever has it can change your sounds.

## FOLDERS
Where Kickbase looks for and saves things: PRESETS, EXPORTS, WAVETABLES and LIBRARY.
- **CHANGE** picks another folder, **RESET** goes back to the default, **OPEN** shows the folder in your file browser.
- Changing the library folder rescans it, so new sounds show up in the kick and sample lists.

## WAVETABLE PACK
Optional free wavetables (AKWF). Nothing is downloaded until you press the install button (about 20 MB).

## Kickbase FX
The effect version has no oscillators, so the wavetable and pack parts are hidden there.
)";

    const char* kickText = R"(# KICK
The kick has two sources you can use alone or together: a **SYNTH** kick (a sine body with a pitch drop) and a **SAMPLE** kick (a WAV from the library or your own file). The SYNTH / SAMPLE tabs in the header switch the view.

## SYNTH view
- **TUNING / TUNE**: the pitch the kick settles on.
- **ENVELOPES**: **PITCH AMT** = how far the pitch drops at the start, **LENGTH** = how long the kick lasts. The two envelopes are drawn in the display.
- **BODY**: **PHASE** = where the wave starts, **HARMONICS** = overtones added to the body, **H. DECAY** = how fast they fade, **H. TILT** = low or high overtones first.
- **CLICK**: the short transient at the start. **LEVEL**, **LENGTH** and **TONE**.
- **DRIVE**: **MODE** = the kind of clipping, **AMOUNT** = how hard.
- **OUT**: **HIGH CUT** removes top end, **LEVEL** is the kick's volume.
- **SHAPE**: **WAVE** is the body waveform, **SUB** adds a sine one octave down, **FM** / **RATIO** / **FM DECAY** bend the phase at the start (punchier, more metallic).
- **TOP**: a noisy layer on top. **NOISE** = amount, **DECAY** = how long it rings, **TONE** = its high-pass.
- **CRUSH**: bit reduction. **COMP**: a compressor after drive and crush, 0 = off. **FILTER**: **LOW CUT**, and **VEL>PITCH** (harder hits start higher).
- **KEY**: whether the kick follows the played note.

## SAMPLE view
- **Library** (left): click a kick to load it. Type in the search box to filter, **RESCAN** reads the library folder again. The folder is set in SETTINGS.
- **LOAD** picks a WAV/AIFF/FLAC file, or drop a file on the kick module. **X** goes back to the synth click. **HEAR SAMPLE** plays just the sample.
- **MODE**: *Click* = only the first moments of the sample, *Layer* = the whole sample on top of the synth kick, *Replace* = the sample is the kick.
- **LEVEL** and **TUNE** apply to Layer and Replace. **OFFSET** moves the sample against the synth kick (plus = later, minus = starts a few ms into the sample), so the two don't cancel in phase.

## Sources
The **SYNTH** and **SAMPLE** switches under the controls turn each source on or off. With both on, LEVEL (SAMPLE view) balances them.
)";

    const char* instrText = R"(# INSTRUMENT (INSTR tab)
The INSTR tab is the main page of an instrument layer: the envelope graph on the left and the knobs that are not part of one source on the right. The other tabs (OSC 1-3, SAMPLE, FILTER, FM, MIX) hold the sources and the filters.

## Envelope graph
Drag the handles. White = the **volume envelope** (ATTACK, DECAY, SUSTAIN, RELEASE), the layer colour = the **filter envelope**, yellow = the **pitch envelope**. The vertical line is where the note ends. The speaker button (**LISTEN**) plays the instrument on the preview note while you hold it; double-click to keep it playing.

## Knob groups
- **PITCH**: **OCTAVE** and **TUNE** move the whole instrument. **ENV AMT** is how far the pitch envelope bends the start (up or down), **ENV DEC** how fast it settles.
- **SUB**: a plain oscillator under the sources. **LEVEL**, **WAVE** and **OCTAVE**. The little scope glows while the instrument sounds.
- **NOISE**: **LEVEL**, **TONE** and **TYPE**. The **FILTER** switch sends the noise through the filters on the FILTER tab.
- **FILTER ENVELOPE**: **AMOUNT** is how much the envelope opens the filters, **ATTACK** and **DECAY** its shape, **VEL** how much note velocity changes the amount.
- **VOLUME ENVELOPE**: the same four stages as the white curve.
- **WARP ENV**: moves the WARP of OSC 1 over time (needs a WARP mode on OSC 1). **SOURCE** picks what drives it, **DIRECT DEC** how fast it falls.
- **DRIVE**: **AMOUNT** and **MODE** (the kind of clipping).
- **LAYER OUT**: **LEVEL** is the layer volume, **DUCK** how much the sidechain lowers it, **LENGTH** the note length (how much of the pattern step the note holds) and **VOICES** how many notes can play at once.
- **PHASE**: **POLARITY** flips the wave, **START** is where each note's wave begins, **NUDGE** shifts the layer a few ms in time. Use them to make the kick and the bass add up instead of cancelling.

## Warnings
A yellow mark on **VOICES** or **WARP ENV** means the setting is heavy on the CPU or does nothing; hover it to read why.
)";

    const char* analyzerText = R"(# ANALYZER
The ANALYZER tab listens to your sound and shows where it works and where it does not. Pick a view with the buttons at the top.

## Views
- **KICK+BASS**: how the kick and the bass add up. Waveform, phase and correlation per 16th note, focused on 20-300 Hz. If they cancel each other, try POLARITY, START or NUDGE on the instrument (INSTR tab).
- **COLLISIONS**: a list of places where sounds collide or cancel each other, including phase problems between kick and bass. The band and the bars are marked so you can find them.
- **SPECTRUM**: the live spectrum of the kick, the instruments and the master, with harmonics and the EQ.
- **REPORT**: analyses the whole pattern and writes a report: what to look at, and what an AI suggests or has fixed.
)";

    const char* aiChatText = R"(# AI CHAT
Talk to an AI that can read and change your sound. It uses your own API key, which is stored encrypted on this computer and only sent to the provider you picked.

## Getting started
Press **KEY**, choose a provider (Gemini has a free tier, OpenAI, Claude, or Ollama on your own computer), paste the key and save it. Then pick a **MODEL**.

## Header
- **COST**: how many tokens the chat may use. *Frugal* = short answers and few steps, *Normal* = the everyday balance, *Rich* = long answers and most of the history kept.
- **UNDO** undoes the AI's last change to the sound.
- **NEW CHAT** starts a fresh conversation (the sound stays as it is).
- **CHATS** saves this chat, opens a saved one or exports it to a text file. API keys are never written to these files.
- **MEM** sets what the AI may remember: local feedback and work lessons, what it may load, and storage limits.

## While it works
**STOP** ends the run after the step that is running. If the AI stops early, **CONTINUE** (or Enter in an empty field) tells it to go on. Some steps ask you to confirm before they run.
)";

    const char* gateText = R"(# GATE
The GATE tab (Kickbase FX only) shapes the sound of the synth that Kickbase FX sits on. Put Kickbase FX on the mixer track of your instrument or bass synth.

## How it works
The synth is cut into pieces by the gate steps and ducked under the kick. When the DAW is stopped, audio passes through untouched.

## Controls
- **Picture**: one gate opening of 100 ms with its attack and release. Drag the two handles to change ATTACK and RELEASE.
- **KEY**: what opens the gate.
- **ATTACK**: how fast the gate opens, in milliseconds.
- **RELEASE**: how fast it closes again.
- **LEVEL**: the volume of the gated sound.
)";

    const char* historyText = R"(# HISTORY
MENU > History lists everything you have done, newest at the top, so you can go back to any step.

## Reading the list
- The line **Now** is where you are. Steps below it are done.
- Grey steps above **Now** are undone. They can be redone until you make a new change.

## Buttons
- **Undo** takes one step back (Ctrl+Z). **Redo** takes one step forward (Ctrl+Shift+Z or Ctrl+Y).
- **Restore** goes to the selected step: everything between it and **Now** is undone or redone. Double-clicking a row does the same.

The list is cleared when the plugin is reloaded, and a note at the bottom tells you when older steps were dropped.
)";

    const char* aboutText = R"(# ABOUT
SETTINGS > ABOUT shows the version and the licences.

## What is here
- **Version**: the Kickbase version you are running.
- **Licence**: Kickbase is free software under the GNU Affero General Public License 3.0 (AGPL-3.0). The AGPL gives you the right to the source code.
- **THIRD-PARTY**: the software and content Kickbase is built with or can download, and their licences.
- **LICENSE**: the full text of the AGPL-3.0.

**BACK** returns to the other settings.
)";

    const char* optimizationsText = R"(# OPTIMIZATIONS
SETTINGS > OPTIMIZATIONS lets you trade smooth pictures for less CPU. All settings are remembered and apply to every Kickbase window.

## Global
- **REDUCE ANIMATIONS**: halves every redraw rate and turns the logo's glow off. The quickest way to save CPU everywhere.
- **COLLISION GRADIENT**: the colour field on ANALYZER > COLLISIONS. Off draws only the list and the markings, which is faster.

## Per view
Each row is one view. The menu sets how many times per second it is redrawn (5 to 40; the default is marked). Lower uses less CPU, higher moves more smoothly.
- **FILLED**: fills the areas under curves and waves. Filled areas are the most expensive part of drawing, so turning it off saves the most.
- **ANALYZER** also has an analyses-per-second menu: how often the sound is analysed. It is the same as SPEED on the ANALYZER tab.

If Kickbase feels heavy, try REDUCE ANIMATIONS first, then lower the ANALYZER rate and turn FILLED off.
)";

    const char* harmonicsText = R"(# HARMONIC EDITOR
Shapes the sound of one oscillator by its harmonics. Open it from the oscillator's table menu.

## What is a wavetable?
A wavetable is a row of waveforms, called **frames**. POS on the oscillator moves along the row. Each frame is one cycle of a wave, and every wave is a sum of harmonics: harmonic 1 is the fundamental (the pitch you hear), 2 is an octave up, and so on.

## Frames
- **All frames**: your edit is applied to every frame of the table.
- **Frame 1..n**: only that frame changes. The picture shows this frame.

## The bars
- The **dB** area has one bar per harmonic. The middle line is 0 dB (unchanged), up is louder (+12 dB at most), and a bar dragged to the bottom is removed.
- The **PHASE** area moves where each harmonic starts, from -180 to +180 degrees.
- The grey part is how it looked when the window opened.
- **Remove the fundamental**: drag bar 1 in the dB area to the bottom. Useful when a kick already supplies the low note.
- Double-click a bar to reset it. Double-click the waveform to reset everything.

## Tools
- **DRAW**, **LINE**, **SMOOTH**, **RANDOM**: how the mouse changes the bars.
- **PRESET**: shapes the bars into a classic spectrum (saw, square, ...). You can keep editing afterwards.
- **16 / 32 / 64 harmonics**: how many bars are shown. Ctrl + mouse wheel changes it too.

## The phase
Phase changes the shape of the wave, but each harmonic keeps its level, so a single note sounds almost the same. It matters when waves are layered (for example kick and bass) or when the wave is bent by the filter or warp.

The result becomes the oscillator's User table.
)";

    struct TopicInfo { HelpDialog::Topic topic; const char* tab; const char* text; };
    const TopicInfo topics[] = {
        { HelpDialog::Topic::pianoRoll, "PIANO ROLL", pianoRollText },
        { HelpDialog::Topic::liveCode, "LIVE CODE", liveCodeText },
        { HelpDialog::Topic::soundstage, "SOUNDSTAGE", soundstageText },
        { HelpDialog::Topic::osc, "OSC", oscText },
        { HelpDialog::Topic::harmonics, "HARMONICS", harmonicsText },
        { HelpDialog::Topic::filter, "FILTER", filterText },
        { HelpDialog::Topic::fm, "FM", fmText },
        { HelpDialog::Topic::mix, "MIX", mixText },
        { HelpDialog::Topic::timing, "TIMING", timingText },
        { HelpDialog::Topic::sample, "SAMPLE", sampleText },
        { HelpDialog::Topic::env, "ENV", envText },
        { HelpDialog::Topic::lfo, "LFO", lfoText },
        { HelpDialog::Topic::matrix, "MATRIX", matrixText },
        { HelpDialog::Topic::effects, "EFFECTS", effectsText },
        { HelpDialog::Topic::master, "MASTER", masterText },
        { HelpDialog::Topic::sidechain, "SIDECHAIN", sidechainText },
        { HelpDialog::Topic::settings, "SETTINGS", settingsText },
        { HelpDialog::Topic::kick, "KICK", kickText },
        { HelpDialog::Topic::instr, "INSTR", instrText },
        { HelpDialog::Topic::analyzer, "ANALYZER", analyzerText },
        { HelpDialog::Topic::aiChat, "AI CHAT", aiChatText },
        { HelpDialog::Topic::gate, "GATE", gateText },
        { HelpDialog::Topic::history, "HISTORY", historyText },
        { HelpDialog::Topic::about, "ABOUT", aboutText },
        { HelpDialog::Topic::optimizations, "OPTIMIZATIONS", optimizationsText },
    };

    // ---- Rendering ----

    // Lägger till `text` med **fet** och `kod` som egna stycken i strängen.
    void appendInline (juce::AttributedString& out, const juce::String& text, float size, juce::Colour colour, bool boldAll)
    {
        bool bold = false, code = false;
        juce::String run;
        auto flush = [&]
        {
            if (run.isEmpty())
                return;
            const auto f = code ? Theme::monoFont (size, false) : Theme::font (size, bold || boldAll);
            out.append (run, f, code ? Theme::kick : colour);
            run.clear();
        };
        for (int i = 0; i < text.length(); ++i)
        {
            if (text.substring (i, i + 2) == "**" && ! code)
            {
                flush();
                bold = ! bold;
                ++i;
            }
            else if (text[i] == '`')
            {
                flush();
                code = ! code;
            }
            else
                run << text[i];
        }
        flush();
    }

    class MarkdownView : public juce::Component
    {
    public:
        void setText (const juce::String& markdown)
        {
            blocks.clear();
            for (auto line : juce::StringArray::fromLines (markdown))
            {
                Block b;
                if (line.startsWith ("## "))      { b.kind = h2; b.text = line.substring (3); }
                else if (line.startsWith ("# "))  { b.kind = h1; b.text = line.substring (2); }
                else if (line.startsWith ("- "))  { b.kind = bullet; b.text = line.substring (2); }
                else if (line.trim().isEmpty())   continue;
                else                              { b.kind = para; b.text = line; }
                blocks.push_back (std::move (b));
            }
            relayout();
        }

        void resized() override { relayout(); }

        void paint (juce::Graphics& g) override
        {
            for (auto& b : blocks)
            {
                if (b.kind == bullet)
                {
                    g.setColour (Theme::kick);
                    g.fillEllipse ((float) pad + 3.0f, (float) b.y + 7.0f, 4.0f, 4.0f);
                }
                b.layout.draw (g, juce::Rectangle<float> ((float) b.x, (float) b.y, (float) b.w, (float) b.h));
            }
        }

    private:
        enum Kind { h1, h2, bullet, para };
        struct Block { Kind kind = para; juce::String text; juce::TextLayout layout; int x = 0, y = 0, w = 0, h = 0; };
        static constexpr int pad = 14;

        void relayout()
        {
            const auto width = std::max (120, getWidth());
            int y = pad;
            for (auto& b : blocks)
            {
                const auto size = b.kind == h1 ? 20.0f : b.kind == h2 ? 15.0f : 13.5f;
                const auto colour = b.kind == h1 ? Theme::text : b.kind == h2 ? Theme::kick : Theme::text;
                juce::AttributedString as;
                as.setWordWrap (juce::AttributedString::byWord);
                appendInline (as, b.text, size, colour, b.kind == h1 || b.kind == h2);
                b.x = b.kind == bullet ? pad + 16 : pad;
                b.w = width - b.x - pad;
                if (b.kind == h2)
                    y += 8;
                b.layout.createLayout (as, (float) b.w);
                b.y = y;
                b.h = (int) std::ceil (b.layout.getHeight()) + 1;
                y += b.h + (b.kind == bullet ? 3 : 7);
            }
            const auto height = y + pad;
            if (height != getHeight())
                setSize (getWidth(), height);
            repaint();
        }

        std::vector<Block> blocks;
    };

    class HelpPanel : public juce::Component
    {
    public:
        HelpPanel()
        {
            int i = 0;
            for (auto& t : topics)
            {
                auto& button = tabs[(size_t) i];
                button.setButtonText (t.tab);
                button.setClickingTogglesState (true);
                button.setRadioGroupId (4711);
                button.setColour (juce::TextButton::buttonOnColourId, Theme::kick);
                button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
                const auto topic = t.topic;
                button.onClick = [this, topic] { select (topic); };
                addAndMakeVisible (button);
                ++i;
            }
            viewport.setViewedComponent (&view, false);
            viewport.setScrollBarsShown (true, false);
            addAndMakeVisible (viewport);
            setSize (560, 520);
        }

        void select (HelpDialog::Topic topic)
        {
            int i = 0;
            for (auto& t : topics)
            {
                if (t.topic == topic)
                {
                    tabs[(size_t) i].setToggleState (true, juce::dontSendNotification);
                    view.setSize (viewport.getMaximumVisibleWidth(), 10);
                    view.setText (juce::String::fromUTF8 (t.text));
                    viewport.setViewPosition (0, 0);
                }
                ++i;
            }
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            // Flikarna radbryts: högst sex per rad, så att många ämnen inte blir smala.
            constexpr int perRow = 6;
            const auto count = (int) std::size (topics);
            const auto rows = (count + perRow - 1) / perRow;
            const auto each = area.getWidth() / std::min (count, perRow);
            for (int i = 0; i < count; ++i)
                tabs[(size_t) i].setBounds (area.getX() + (i % perRow) * each, area.getY() + (i / perRow) * 26, each, 24);
            area.removeFromTop (rows * 26);
            area.removeFromTop (6);
            viewport.setBounds (area);
            view.setSize (viewport.getMaximumVisibleWidth(), std::max (10, view.getHeight()));
        }

    private:
        std::array<juce::TextButton, std::size (topics)> tabs;
        MarkdownView view;
        juce::Viewport viewport;
    };
}

namespace HelpDialog
{
    void show (juce::Component* centreAround, Topic topic)
    {
        static juce::Component::SafePointer<juce::DialogWindow> open;
        if (open != nullptr)
        {
            if (auto* panel = dynamic_cast<HelpPanel*> (open->getContentComponent()))
                panel->select (topic);
            open->toFront (true);
            return;
        }
        auto* panel = new HelpPanel();
        panel->select (topic);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (panel);
        o.dialogTitle = "Help";
        o.dialogBackgroundColour = Theme::panel;
        o.componentToCentreAround = centreAround;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = true;
        open = o.launchAsync();
    }

    void setupButton (juce::TextButton& button, juce::Component& owner, Topic topic, const juce::String& what)
    {
        button.setButtonText ("HELP");
        button.setTooltip ("HELP - Opens the help window on " + what + ". The window has a tab for each page, and you can keep it open while you work.");
        button.onClick = [&owner, topic] { show (&owner, topic); };
    }
}
