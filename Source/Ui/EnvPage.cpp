#include "EnvPage.h"
#include "HelpDialog.h"

namespace
{
    // DURATION-knappens text, t.ex. "0.25 s" eller "2 s".
    juce::String secondsText (double seconds)
    {
        return juce::String (seconds, seconds < 1.0 ? 2 : 0).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " s";
    }

    // Envelopens form (0..1): attack linjärt, decay exponentiellt mot sustain, och efter `gateSeconds` release.
    float adsrAt (double seconds, float attackMs, float decayMs, float sustain, float releaseMs, double gateSeconds)
    {
        const auto attack = std::max (0.00005, attackMs * 0.001);
        const auto decay = std::max (0.0001, decayMs * 0.001);
        const auto release = std::max (0.0001, releaseMs * 0.001);
        const auto ln = std::log (0.001);

        auto held = [&] (double t)
        {
            if (t < attack)
                return t / attack;
            return sustain + (1.0 - sustain) * std::exp (ln * (t - attack) / decay);
        };
        if (seconds < gateSeconds)
            return (float) held (seconds);
        return (float) (held (gateSeconds) * std::exp (ln * (seconds - gateSeconds) / release));
    }

    juce::RangedAudioParameter* param (PluginBase& processor, int layer, InstrumentField field)
    {
        return processor.apvts.getParameter (ParamID::bass (layer, field));
    }
}

EnvPage::EnvPage (PluginBase& p) : processor (p), refs (p.apvts)
{
    instrumentLabel.setText ("INSTRUMENT", juce::dontSendNotification);
    instrumentLabel.setFont (Theme::font (11.0f, true));
    instrumentLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (instrumentLabel);
    instrumentBox.setTooltip ("Which instrument's envelopes you see and edit here.");
    instrumentBox.onChange = [this]
    {
        processor.selectedInstrument = instrumentBox.getSelectedId() - 1;
        rebuild();
    };
    addAndMakeVisible (instrumentBox);
    moreLabel.setText ("MORE SOURCES", juce::dontSendNotification);
    moreLabel.setFont (Theme::font (11.0f, true));
    moreLabel.setColour (juce::Label::textColourId, Theme::textDim);
    moreLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (moreLabel);
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::env, "the ENV tab (the four envelopes)");
    addAndMakeVisible (helpButton);

    rebuild();
    startTimerHz (4);
}

EnvPage::~EnvPage() = default;

EnvelopeView::NoteOff EnvPage::noteOffOf (int forLayer, double example) const
{
    const auto real = processor.typicalNoteSeconds (forLayer);
    return { real > 0.0 ? real : example, real > 0.0 };
}

void EnvPage::timerCallback()
{
    const auto wanted = juce::jlimit (0, processor.activeInstruments() - 1, processor.selectedInstrument);
    if (wanted != layer || processor.activeInstruments() != lastActive || processor.layerNamesVersion.load() != lastNamesVersion)
        rebuild();
    for (auto& tile : tiles)
        if (tile.onButton != nullptr)
            tile.view->setAlpha (tile.onButton->getToggleState() ? 1.0f : 0.35f);
}

void EnvPage::rebuild()
{
    using F = InstrumentField;
    const auto count = processor.activeInstruments();
    layer = juce::jlimit (0, count - 1, processor.selectedInstrument);
    lastActive = count;
    lastNamesVersion = processor.layerNamesVersion.load();

    instrumentBox.clear (juce::dontSendNotification);
    for (int i = 0; i < count; ++i)
        instrumentBox.addItem (processor.getLayerName (i), i + 1);
    instrumentBox.setSelectedId (layer + 1, juce::dontSendNotification);

    tiles.clear();
    // Notens velocity och ett slumpvärde per ton som källor: dra handtaget till en ratt.
    velocityHandle = std::make_unique<LfoDragHandle> (Modulation::instrumentSource (layer, 2), "VELOCITY");
    randomHandle = std::make_unique<LfoDragHandle> (Modulation::instrumentSource (layer, 3), "RANDOM");
    addAndMakeVisible (*velocityHandle);
    addAndMakeVisible (*randomHandle);
    const auto accent = Theme::layerColour (layer);
    const auto thisLayer = layer;

    auto addTile = [&] (const juce::String& title, const juce::String& hint, double seconds, int handleSource)
    {
        Tile tile;
        tile.title = title;
        tile.hint = hint;
        tile.colour = handleSource >= 0 && ! Modulation::isEnvSource (handleSource) ? Theme::sourceColour (handleSource) : accent;
        juce::ignoreUnused (seconds); // startvärdet står i durations
        const auto index = (int) tiles.size();
        tile.view = std::make_unique<EnvelopeView> (durations[(size_t) index]);
        // Tiderna i ms (Sync räknat från tempot), så att diagrammet visar det som låter. ON/OFF syns som en tonad ruta i stället.
        tile.view->onBeforePaint = [this, thisLayer] { current = refs.readInstrument (thisLayer, 36).withTempo (processor.transportStatus.bpm.load(), false); };
        tile.onButton = std::make_unique<juce::TextButton> ("ON");
        tile.onButton->setClickingTogglesState (true);
        tile.onButton->setColour (juce::TextButton::buttonOnColourId, tile.colour.withAlpha (0.6f));
        tile.onButton->setTooltip ("ON/OFF - " + title + (index == 0 ? ": off = every note plays at full level while it is held."
                                                        : index == 1 ? ": off = the filter stays at CUTOFF (WARP ENV still follows the shape)."
                                                                     : ": off = it gives 0, so the knobs it is dragged to stay where they are."));
        tile.onAttachment = std::make_unique<juce::ButtonParameterAttachment> (*param (processor, layer, ParamID::envOnField (index)), *tile.onButton);
        addAndMakeVisible (*tile.onButton);
        tile.durationButton = std::make_unique<juce::TextButton> (secondsText (durations[(size_t) index]));
        tile.durationButton->setTooltip ("DURATION - How many seconds the graph shows (click to step 0.25, 0.5, 1, 2, 4, 8 s). Only the view, not the sound.");
        tile.durationButton->onClick = [this, index]
        {
            static constexpr double steps[] { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
            auto& seconds = durations[(size_t) index];
            auto next = steps[0];
            for (auto step : steps)
                if (step > seconds + 1.0e-6) { next = step; break; }
            seconds = next;
            tiles[(size_t) index].view->setDisplaySeconds (seconds);
            tiles[(size_t) index].durationButton->setButtonText (secondsText (seconds));
        };
        addAndMakeVisible (*tile.durationButton);
        addAndMakeVisible (*tile.view);
        tile.listen = std::make_unique<ListenButton>();
        tile.listen->setAccent (tile.colour);
        tile.listen->setTooltip ("LISTEN - Hold to hear this instrument on the preview note (the PREVIEWS note at the top) for as long as you hold; "
                                 "it starts over if the sound ends. Double-click to keep it playing until you click again. Use it while you drag the handles.");
        tile.listen->onStart = [this, thisLayer] { hold.start (PluginBase::heldLayerAudition (thisLayer)); };
        tile.listen->onStop = [this] { hold.stop(); };
        addAndMakeVisible (*tile.listen);
        if (handleSource >= 0)
        {
            tile.handle = std::make_unique<LfoDragHandle> (handleSource, "DRAG");
            addAndMakeVisible (*tile.handle);
        }
        tiles.push_back (std::move (tile));
        return tiles.back().view.get();
    };

    // SYNC: tiderna som notvärden som följer tempot (på) eller ms (av), för ruta `index` (0 ENV 1, 1 FILTER ENV, 2-3 ENV 2-3).
    auto addModeButton = [&] (int index, const juce::String& tooltip)
    {
        auto& tile = tiles[(size_t) index];
        tile.modeButton = std::make_unique<juce::TextButton> ("SYNC");
        tile.modeButton->setClickingTogglesState (true);
        tile.modeButton->setColour (juce::TextButton::buttonOnColourId, tile.colour.withAlpha (0.6f));
        tile.modeButton->setTooltip (tooltip);
        tile.modeAttachment = std::make_unique<juce::ButtonParameterAttachment> (*param (processor, layer, ParamID::envTileModeField (index)), *tile.modeButton);
        addAndMakeVisible (*tile.modeButton);
    };
    // Handtagets x-led styr ms-ratten, eller i läget Sync notvärdet.
    auto pick = [this, thisLayer] (int index, F msField, F syncField)
    {
        return [this, thisLayer, index, msField, syncField]
        {
            const auto sync = processor.apvts.getRawParameterValue (ParamID::bass (thisLayer, ParamID::envTileModeField (index)))->load() >= 0.5f;
            return processor.apvts.getParameter (ParamID::bass (thisLayer, sync ? syncField : msField));
        };
    };

    // ENV 1: volym-envelopen (det som formar ljudet).
    {
        auto* view = addTile ("ENV 1  VOLUME", "Shapes the loudness of every note: attack, decay, sustain level and release. DRAG the handle onto a knob to let the same shape "
                                               "move it (source AMP ENV; 0 when ENV 1 is off).", 1.0, Modulation::instrumentSource (layer, Modulation::instrumentSources));
        const auto gate = [this, thisLayer] { return noteOffOf (thisLayer, 0.4).seconds; };
        view->setNoteOff ([this, thisLayer] { return noteOffOf (thisLayer, 0.4); });
        view->addCurve ({ [this, gate] (double t) { return adsrAt (t, current.attackMs, current.decayMs, current.sustain, current.releaseMs, gate()); }, accent, true, "VOLUME" });
        view->addHandle ({ [this] { return juce::Point<float> (current.attackMs * 0.001f, 1.0f); }, param (processor, layer, F::attack), nullptr, false, Theme::text, pick (0, F::attack, F::env1AttackSync), EnvelopeView::timeCapOf (processor.apvts, layer, F::attack) });
        view->addHandle ({ [this, gate]
                           {
                               const auto t = (current.attackMs + current.decayMs * 0.5f) * 0.001;
                               return juce::Point<float> ((float) t, adsrAt (t, current.attackMs, current.decayMs, current.sustain, current.releaseMs, gate())); },
                           param (processor, layer, F::decay), param (processor, layer, F::sustain), false, Theme::text, pick (0, F::decay, F::env1DecaySync), EnvelopeView::timeCapOf (processor.apvts, layer, F::decay) });
        view->addHandle ({ [this, gate] { return juce::Point<float> ((float) gate() + current.releaseMs * 0.001f, 0.0f); }, param (processor, layer, F::release), nullptr, false, Theme::text, pick (0, F::release, F::env1ReleaseSync), EnvelopeView::timeCapOf (processor.apvts, layer, F::release) });
        addModeButton (0, "ENV 1 TIME - On: attack, decay and release are note lengths (1/16, 1/4 ... 4 bars) that follow the tempo; off: milliseconds. The handles step through the note lengths.");
    }

    // Filter-envelopen: hur mycket och hur länge filtret öppnas i början av varje ton.
    {
        auto* view = addTile ("FILTER ENVELOPE", "Opens the filter at the start of every note and lets it close again: AMOUNT (how far, in octaves), ATTACK (how fast it opens) and DECAY (how fast it closes). DRAG the handle onto a knob "
                                                  "to let the same shape move it (source FILT ENV, 0..1 without AMOUNT; 0 when the filter envelope is off).", 0.5,
                              Modulation::instrumentSource (layer, Modulation::instrumentSources + 1));
        view->addCurve ({ [this] (double t) { return InstrumentSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f; }, accent, true, "FILTER" });
        view->addHandle ({ [this] { return juce::Point<float> (std::max (0.0015f, current.envAttackMs * 0.001f), current.envAmount / 8.0f); }, param (processor, layer, F::envAttack), param (processor, layer, F::envAmount), false, accent,
                           pick (1, F::envAttack, F::filterEnvAttackSync) });
        view->addHandle ({ [this]
                           {
                               const auto t = current.envAttackMs * 0.001 + current.envDecayMs * 0.001 / 3.0;
                               return juce::Point<float> ((float) t, InstrumentSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f); },
                           param (processor, layer, F::envDecay), nullptr, false, accent, pick (1, F::envDecay, F::filterEnvDecaySync), EnvelopeView::timeCapOf (processor.apvts, layer, F::envDecay) });
        addModeButton (1, "FILTER ENV TIME - On: attack and decay are note lengths (1/16, 1/4 ... 4 bars) that follow the tempo; off: milliseconds. The handles step through the note lengths.");
    }

    // ENV 2 och ENV 3: modulationskällor.
    for (int env = 0; env < 2; ++env)
    {
        const auto base = (int) F::env2Attack + env * 4;
        const auto source = Modulation::instrumentSource (layer, env);
        auto* view = addTile ("ENV " + juce::String (env + 2) + "  MODULATION", "An extra envelope that does not play the sound but moves other knobs. Drag the handle at the top onto a knob "
                                                                              "(in the window above), or set it up in the MATRIX tab.", 2.0, source);
        const auto gate = [this, thisLayer] { return noteOffOf (thisLayer, 0.7).seconds; };
        view->setNoteOff ([this, thisLayer] { return noteOffOf (thisLayer, 0.7); });
        // Tiderna i ms (current är redan räknad från tempot).
        auto read = [this, env] { return env == 0 ? current.env2 : current.env3; };
        // Handtagets x-led styr ms-ratten, eller i läget Sync notvärdet (attack, decay, release).
        auto timeParam = [this, env, thisLayer, base] (int which)
        {
            return [this, env, thisLayer, base, which]
            {
                const auto sync = processor.apvts.getRawParameterValue (ParamID::bass (thisLayer, ParamID::envModeField (env)))->load() >= 0.5f;
                const auto field = sync ? ParamID::envSyncField (env, which) : (InstrumentField) (base + (which == 0 ? 0 : which == 1 ? 1 : 3));
                return processor.apvts.getParameter (ParamID::bass (thisLayer, field));
            };
        };
        view->addCurve ({ [read, gate] (double t) { const auto e = read(); return adsrAt (t, e.attackMs, e.decayMs, e.sustain, e.releaseMs, gate()); }, Theme::sourceColour (source), true, "ENV " + juce::String (env + 2) });
        view->addHandle ({ [read] { return juce::Point<float> (read().attackMs * 0.001f, 1.0f); }, param (processor, layer, (F) base), nullptr, false, Theme::text, timeParam (0) });
        view->addHandle ({ [read, gate]
                           {
                               const auto e = read();
                               const auto t = (e.attackMs + e.decayMs * 0.5f) * 0.001;
                               return juce::Point<float> ((float) t, adsrAt (t, e.attackMs, e.decayMs, e.sustain, e.releaseMs, gate())); },
                           param (processor, layer, (F) (base + 1)), param (processor, layer, (F) (base + 2)), false, Theme::text, timeParam (1) });
        view->addHandle ({ [read, gate] { return juce::Point<float> ((float) gate() + read().releaseMs * 0.001f, 0.0f); }, param (processor, layer, (F) (base + 3)), nullptr, false, Theme::text, timeParam (2) });
        addModeButton (env + 2, "ENV " + juce::String (env + 2) + " TIME - On: attack, decay and release are note lengths (1/16, 1/4 ... 4 bars) that follow the tempo; off: milliseconds. The handles step through the note lengths.");
    }

    resized();
    repaint();
}

void EnvPage::paint (juce::Graphics& g)
{
    for (const auto& tile : tiles)
    {
        g.setColour (Theme::knobBodyDark.withAlpha (0.35f));
        g.fillRoundedRectangle (tile.bounds.toFloat(), 6.0f);
        g.setColour (tile.colour);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (tile.title, tile.bounds.withHeight (24).withTrimmedLeft (10), juce::Justification::centredLeft);
    }
}

void EnvPage::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (24);
    instrumentLabel.setBounds (top.removeFromLeft (90));
    instrumentBox.setBounds (top.removeFromLeft (170));
    top.removeFromLeft (8);
    helpButton.setBounds (top.removeFromLeft (44));
    if (randomHandle != nullptr)
    {
        randomHandle->setBounds (top.removeFromRight (84));
        top.removeFromRight (6);
        velocityHandle->setBounds (top.removeFromRight (84));
        top.removeFromRight (6);
        moreLabel.setBounds (top.removeFromRight (110));
    }
    area.removeFromTop (6);

    const auto count = (int) tiles.size();
    if (count == 0)
        return;
    const auto gap = 8;
    const auto width = (area.getWidth() - gap * (count - 1)) / count;
    for (auto& tile : tiles)
    {
        tile.bounds = area.removeFromLeft (width);
        area.removeFromLeft (gap);
        auto inner = tile.bounds.reduced (6, 4);
        // Två rader över grafen: titeln, och under den knapparna (SYNC till vänster, DRAG till höger). Alla fyra får samma rader
        // så att graferna ligger i höjd, och knapparna kan aldrig hamna över grafens tidslinje (SYNC och DRAG krockade förut).
        inner.removeFromTop (22); // titeln (ritas i paint)
        auto controls = inner.removeFromTop (22);
        if (tile.handle != nullptr)
            tile.handle->setBounds (controls.removeFromRight (72));
        if (tile.onButton != nullptr)
            tile.onButton->setBounds (controls.removeFromLeft (40).reduced (2, 0));
        if (tile.modeButton != nullptr)
            tile.modeButton->setBounds (controls.removeFromLeft (52).reduced (2, 0));
        if (tile.listen != nullptr)
            tile.listen->setBounds (controls.removeFromLeft (34).reduced (2, 0));
        if (tile.durationButton != nullptr)
            tile.durationButton->setBounds (controls.removeFromLeft (52).reduced (2, 0));
        inner.removeFromTop (2);
        tile.view->setBounds (inner);
    }
    repaint();
}
