#include "EnvPage.h"

namespace
{
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

    juce::RangedAudioParameter* param (PluginBase& processor, int layer, BassField field)
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

    rebuild();
    startTimerHz (4);
}

EnvPage::~EnvPage() = default;

void EnvPage::timerCallback()
{
    const auto wanted = juce::jlimit (0, processor.activeInstruments() - 1, processor.selectedInstrument);
    if (wanted != layer || processor.activeInstruments() != lastActive || processor.layerNamesVersion.load() != lastNamesVersion)
        rebuild();
}

void EnvPage::rebuild()
{
    using F = BassField;
    const auto count = processor.activeInstruments();
    layer = juce::jlimit (0, count - 1, processor.selectedInstrument);
    lastActive = count;
    lastNamesVersion = processor.layerNamesVersion.load();

    instrumentBox.clear (juce::dontSendNotification);
    for (int i = 0; i < count; ++i)
        instrumentBox.addItem (processor.getLayerName (i), i + 1);
    instrumentBox.setSelectedId (layer + 1, juce::dontSendNotification);

    tiles.clear();
    const auto accent = Theme::layerColour (layer);
    const auto thisLayer = layer;

    auto addTile = [&] (const juce::String& title, const juce::String& hint, double seconds, int handleSource)
    {
        Tile tile;
        tile.title = title;
        tile.hint = hint;
        tile.colour = handleSource >= 0 ? Theme::sourceColour (handleSource) : accent;
        tile.view = std::make_unique<EnvelopeView> (seconds);
        tile.view->onBeforePaint = [this, thisLayer] { current = refs.readBass (thisLayer, 36); };
        addAndMakeVisible (*tile.view);
        if (handleSource >= 0)
        {
            tile.handle = std::make_unique<LfoDragHandle> (handleSource, "DRAG TO A KNOB");
            addAndMakeVisible (*tile.handle);
        }
        tiles.push_back (std::move (tile));
        return tiles.back().view.get();
    };

    // ENV 1: volym-envelopen (det som formar ljudet).
    {
        auto* view = addTile ("ENV 1  VOLUME", "Shapes the loudness of every note: attack, decay, sustain level and release.", 1.0, -1);
        constexpr double gate = 0.4;
        view->addCurve ({ [this] (double t) { return adsrAt (t, current.attackMs, current.decayMs, current.sustain, current.releaseMs, gate); }, accent, true, "VOLUME" });
        view->addHandle ({ [this] { return juce::Point<float> (current.attackMs * 0.001f, 1.0f); }, param (processor, layer, F::attack), nullptr, false, Theme::text });
        view->addHandle ({ [this]
                           {
                               const auto t = (current.attackMs + current.decayMs * 0.5f) * 0.001;
                               return juce::Point<float> ((float) t, adsrAt (t, current.attackMs, current.decayMs, current.sustain, current.releaseMs, gate)); },
                           param (processor, layer, F::decay), param (processor, layer, F::sustain), false, Theme::text });
        view->addHandle ({ [this] { return juce::Point<float> ((float) gate + current.releaseMs * 0.001f, 0.0f); }, param (processor, layer, F::release), nullptr, false, Theme::text });
    }

    // Filter-envelopen: hur mycket och hur länge filtret öppnas i början av varje ton.
    {
        auto* view = addTile ("FILTER ENVELOPE", "Opens the filter at the start of every note and lets it close again: AMOUNT (how far, in octaves) and DECAY (how fast).", 0.5, -1);
        view->addCurve ({ [this] (double t) { return BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f; }, accent, true, "FILTER" });
        view->addHandle ({ [this] { return juce::Point<float> (0.0015f, current.envAmount / 8.0f); }, nullptr, param (processor, layer, F::envAmount), false, accent });
        view->addHandle ({ [this]
                           {
                               const auto t = current.envDecayMs * 0.001 / 3.0;
                               return juce::Point<float> ((float) t, BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f); },
                           param (processor, layer, F::envDecay), nullptr, false, accent });
    }

    // ENV 2 och ENV 3: modulationskällor.
    for (int env = 0; env < 2; ++env)
    {
        const auto base = (int) F::env2Attack + env * 4;
        const auto source = Modulation::instrumentSource (layer, env);
        auto* view = addTile ("ENV " + juce::String (env + 2) + "  MODULATION", "An extra envelope that does not play the sound but moves other knobs. Drag the handle at the top onto a knob "
                                                                              "(in the window above), or set it up in the MATRIX tab.", 2.0, source);
        constexpr double gate = 0.7;
        auto read = [this, env]
        {
            const auto& e = env == 0 ? current.env2 : current.env3;
            return e;
        };
        view->addCurve ({ [read] (double t) { const auto e = read(); return adsrAt (t, e.attackMs, e.decayMs, e.sustain, e.releaseMs, gate); }, Theme::sourceColour (source), true, "ENV " + juce::String (env + 2) });
        view->addHandle ({ [read] { return juce::Point<float> (read().attackMs * 0.001f, 1.0f); }, param (processor, layer, (F) base), nullptr, false, Theme::text });
        view->addHandle ({ [read]
                           {
                               const auto e = read();
                               const auto t = (e.attackMs + e.decayMs * 0.5f) * 0.001;
                               return juce::Point<float> ((float) t, adsrAt (t, e.attackMs, e.decayMs, e.sustain, e.releaseMs, gate)); },
                           param (processor, layer, (F) (base + 1)), param (processor, layer, (F) (base + 2)), false, Theme::text });
        view->addHandle ({ [read] { return juce::Point<float> ((float) gate + read().releaseMs * 0.001f, 0.0f); }, param (processor, layer, (F) (base + 3)), nullptr, false, Theme::text });
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
        auto header = inner.removeFromTop (22);
        if (tile.handle != nullptr)
            tile.handle->setBounds (header.removeFromRight (130));
        inner.removeFromTop (2);
        tile.view->setBounds (inner);
    }
    repaint();
}
