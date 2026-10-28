#include "HelpDialog.h"
#include "Theme.h"

namespace
{
    // ---- Texterna (markdown-lik: "# " och "## " rubriker, "- " punkter, tomma rader mellan stycken) ----

    const char* pianoRollText = R"(# PIANO ROLL
Here you write the notes of one row (the kick or an instrument). Notes are placed on a grid; the strip at the bottom is velocity.

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
- **WIDE**: give the waveform the full width (the knobs are hidden). **NARROW** brings them back.
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
Cut the sample into pieces. Each slice gets its own "Slice N" row in the pattern; a note there plays that slice at its original speed.
- **Grid**: equal parts (the slider sets how many, max 128). **Auto**: cut at the hits (the slider sets the sensitivity). **Manual**: your own markers.
- **CREATE**: turn the Grid or Auto result into your own markers. Drag a boundary to move it, double-click to add one, right-click to remove one.
- **REVERSE**: plays every slice backwards.
- **SHOW SAMPLE / SLICES**: see the whole waveform, or every slice as its own box. Click a slice to hear it.
- Only the first 16 slices get a row in the pattern.
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

    struct TopicInfo { HelpDialog::Topic topic; const char* tab; const char* text; };
    const TopicInfo topics[] = {
        { HelpDialog::Topic::pianoRoll, "PIANO ROLL", pianoRollText },
        { HelpDialog::Topic::liveCode, "LIVE CODE", liveCodeText },
        { HelpDialog::Topic::soundstage, "SOUNDSTAGE", soundstageText },
        { HelpDialog::Topic::osc, "OSC", oscText },
        { HelpDialog::Topic::filter, "FILTER", filterText },
        { HelpDialog::Topic::fm, "FM", fmText },
        { HelpDialog::Topic::mix, "MIX", mixText },
        { HelpDialog::Topic::sample, "SAMPLE", sampleText },
        { HelpDialog::Topic::env, "ENV", envText },
        { HelpDialog::Topic::lfo, "LFO", lfoText },
        { HelpDialog::Topic::matrix, "MATRIX", matrixText },
        { HelpDialog::Topic::effects, "EFFECTS", effectsText },
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
