#include "InstrTimingPanel.h"

#include "Help.h"
#include "HelpDialog.h"

namespace
{
    constexpr int columns = 4, rows = 2, gap = 6;
    constexpr int headerHeight = 14, choiceHeight = 38;
    constexpr double maxSpanSeconds = 8.0;
    bool focusView = true; // senast valda vy, kvar tills programmet stängs (UI-läge, ingen parameter)
    const char* const engineNames[] = { "OSC 1", "OSC 2", "OSC 3", "SAMPLE", "NOISE", "SUB", "FM" };

    juce::String formatSeconds (double s)
    {
        return s < 1.0 ? juce::String (juce::roundToInt (s * 1000.0)) + " ms" : juce::String (s, 2) + " s";
    }
}

InstrTimingPanel::InstrTimingPanel (PluginBase& p, int layerIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), accent (accentIn), refs (p.apvts), timeline (p, layerIn, accentIn, true)
{
    auto& apvts = processor.apvts;
    for (int e = 0; e < ParamID::numTimingEngines; ++e)
    {
        auto cell = std::make_unique<Cell>();
        cell->name = engineNames[e];
        const auto id = [&] (int which) { return ParamID::bass (layer, ParamID::timingField (e, which)); };
        const auto extra = [&] (int which) { return ParamID::bass (layer, ParamID::timingExtraField (e, which)); };

        cell->mode = std::make_unique<ChoiceComponent> (apvts, extra (0), "MODE");
        cell->delaySync = std::make_unique<ChoiceComponent> (apvts, id (1), "SYNC");
        cell->lengthSync = std::make_unique<ChoiceComponent> (apvts, extra (1), "SYNC");
        cell->delay = std::make_unique<KnobComponent> (apvts, id (0), "POS DELAY", accent);
        cell->length = std::make_unique<KnobComponent> (apvts, id (2), "LENGTH", accent);
        cell->curve = std::make_unique<KnobComponent> (apvts, id (3), "CURVE", accent);
        setupCount (cell->delayCount, cell->delayCountAttachment, extra (2));
        setupCount (cell->lengthCount, cell->lengthCountAttachment, extra (3));

        // Samma varning som förut: POS DELAY längre än tonens volym-envelope (bedöms inte med SYNC eller hållen ton).
        const auto layerIndex = layer;
        cell->delay->setWarning ([&apvts, layerIndex, e]() -> juce::String
        {
            const auto read = [&] (InstrumentField f) { auto* raw = apvts.getRawParameterValue (ParamID::bass (layerIndex, f)); return raw != nullptr ? raw->load() : 0.0f; };
            if ((int) read (ParamID::timingField (e, 1)) > 0 || read (InstrumentField::sustain) > 0.001f)
                return {};
            const auto noteMs = read (InstrumentField::attack) + read (InstrumentField::decay) + read (InstrumentField::release);
            return read (ParamID::timingField (e, 0)) > noteMs ? "POS DELAY is longer than the note" : juce::String();
        });

        for (juce::Component* c : { (juce::Component*) cell->mode.get(), (juce::Component*) cell->delaySync.get(), (juce::Component*) cell->lengthSync.get(),
                                    (juce::Component*) cell->delay.get(), (juce::Component*) cell->length.get(), (juce::Component*) cell->curve.get(),
                                    (juce::Component*) &cell->delayCount, (juce::Component*) &cell->lengthCount })
            addAndMakeVisible (c);
        cells.push_back (std::move (cell));
    }
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::timing, "the TIMING tab (POS DELAY, LENGTH, SYNC, MODE, CURVE)");
    addAndMakeVisible (helpButton);
    addAndMakeVisible (timeline);
    timeline.onSelect = [this] (int engine) { selectedEngine = engine; applyViewMode(); resized(); repaint(); };
    viewButton.setTooltip ("Switch between the time diagram with one engine (FOCUS) and all engines at once (OVERVIEW).");
    viewButton.onClick = [this] { focusView = ! focusView; applyViewMode(); resized(); repaint(); };
    addAndMakeVisible (viewButton);
    refreshParams();
    for (size_t e = 0; e < cells.size(); ++e)
        updateVisibility (*cells[e], e);
    applyViewMode();
    startTimerHz (30);
}

void InstrTimingPanel::applyViewMode()
{
    viewButton.setButtonText (focusView ? "OVERVIEW" : "FOCUS");
    timeline.setVisible (focusView);
    timeline.setSelected (selectedEngine);
    for (size_t e = 0; e < cells.size(); ++e)
    {
        const auto show = ! focusView || (int) e == selectedEngine;
        for (juce::Component* c : { (juce::Component*) cells[e]->mode.get(), (juce::Component*) cells[e]->delaySync.get(), (juce::Component*) cells[e]->lengthSync.get(),
                                    (juce::Component*) cells[e]->curve.get() })
            c->setVisible (show);
        updateVisibility (*cells[e], e);
    }
}

void InstrTimingPanel::setupCount (juce::Slider& slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment, const juce::String& paramID)
{
    slider.setSliderStyle (juce::Slider::LinearBar);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    slider.textFromValueFunction = [] (double v) { return "x" + juce::String ((int) v); };
    slider.setScrollWheelEnabled (true);
    const auto help = Help::forParameter (paramID);
    slider.setTooltip ("x - " + (help.isNotEmpty() ? help : juce::String ("How many note values.")));
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramID, slider);
}

void InstrTimingPanel::refreshParams()
{
    bpm = processor.transportStatus.bpm.load();
    if (bpm <= 0.0)
        bpm = 120.0;
    params = refs.readInstrument (layer, 36).withTempo (bpm, false);
    double span = 0.0;
    for (int e = 0; e < ParamID::numTimingEngines; ++e)
    {
        const auto delay = InstrumentSynth::engineDelaySeconds (params, e, bpm);
        const auto length = InstrumentSynth::engineLengthSeconds (params, e, bpm);
        span = juce::jmax (span, delay + length);
    }
    spanSeconds = juce::jlimit (0.5, maxSpanSeconds, span * 1.15);
}

void InstrTimingPanel::updateVisibility (Cell& cell, size_t engine)
{
    cell.delayIsSync = params.delaySync[engine] > 0;
    cell.lengthIsSync = params.lengthSync[engine] > 0;
    const auto show = ! focusView || (int) engine == selectedEngine;
    cell.delay->setVisible (show && ! cell.delayIsSync);
    cell.delayCount.setVisible (show && cell.delayIsSync);
    cell.length->setVisible (show && ! cell.lengthIsSync);
    cell.lengthCount.setVisible (show && cell.lengthIsSync);
}

void InstrTimingPanel::timerCallback()
{
    if (! isShowing())
        return;
    if (tick++ % 8 == 0)
    {
        refreshParams();
        for (size_t e = 0; e < cells.size(); ++e)
        {
            const auto wasDelay = cells[e]->delayIsSync, wasLength = cells[e]->lengthIsSync;
            updateVisibility (*cells[e], e);
            if (wasDelay != cells[e]->delayIsSync || wasLength != cells[e]->lengthIsSync)
                resized();
        }
    }
    if (! focusView)
        for (auto& cell : cells)
            repaint (cell->plot);
}

void InstrTimingPanel::layoutCell (Cell& cell, bool withPlot)
{
    auto area = cell.frame.reduced (4);
    area.removeFromTop (headerHeight);
    cell.plot = withPlot ? area.removeFromTop (juce::jmax (30, (int) (area.getHeight() * 0.34f))) : juce::Rectangle<int>();
    area.removeFromTop (2);
    cell.mode->setBounds (area.removeFromTop (choiceHeight).withWidth (juce::jmin (area.getWidth(), 110)));
    // tre kolumner: DELAY, LENGTH, CURVE; valrutan för SYNC sitter under ratten
    const auto colWidth = area.getWidth() / 3;
    const auto column = [&] (juce::Rectangle<int> r, KnobComponent& knob, juce::Slider& count, ChoiceComponent* sync)
    {
        r = r.reduced (2, 0);
        if (sync != nullptr)
            sync->setBounds (r.removeFromBottom (choiceHeight));
        knob.setBounds (r);
        count.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 56), 22));
    };
    auto r = area;
    column (r.removeFromLeft (colWidth), *cell.delay, cell.delayCount, cell.delaySync.get());
    column (r.removeFromLeft (colWidth), *cell.length, cell.lengthCount, cell.lengthSync.get());
    cell.curve->setBounds (r.reduced (2, 0));
}

void InstrTimingPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    if (focusView)
    {
        // Överst knappar, sedan tidsdiagrammet, och under det den valda motorns reglage.
        auto strip = area.removeFromTop (22);
        viewButton.setBounds (strip.removeFromLeft (90));
        helpButton.setBounds (strip.removeFromRight (70));
        area.removeFromTop (gap);
        timeline.setBounds (area.removeFromTop ((int) (area.getHeight() * 0.55f)));
        area.removeFromTop (gap);
        for (size_t e = 0; e < cells.size(); ++e)
        {
            cells[e]->frame = (int) e == selectedEngine ? area : juce::Rectangle<int>();
            layoutCell (*cells[e], false);
        }
        legendArea = {};
        return;
    }
    const auto cellWidth = (area.getWidth() - gap * (columns - 1)) / columns;
    const auto cellHeight = (area.getHeight() - gap * (rows - 1)) / rows;
    const auto cellBounds = [&] (int index)
    {
        return juce::Rectangle<int> (area.getX() + (index % columns) * (cellWidth + gap), area.getY() + (index / columns) * (cellHeight + gap), cellWidth, cellHeight);
    };
    for (size_t e = 0; e < cells.size(); ++e)
    {
        cells[e]->frame = cellBounds ((int) e);
        layoutCell (*cells[e]);
    }
    legendArea = cellBounds ((int) cells.size());
    auto buttons = legendArea.reduced (8).removeFromBottom (24);
    helpButton.setBounds (buttons.removeFromLeft (70));
    buttons.removeFromLeft (6);
    viewButton.setBounds (buttons.removeFromLeft (90));
}

void InstrTimingPanel::paintPlot (juce::Graphics& g, const Cell& cell, size_t engine) const
{
    const auto plot = cell.plot.toFloat();
    g.setColour (Theme::chartBackground);
    g.fillRoundedRectangle (plot, 3.0f);

    const auto delay = InstrumentSynth::engineDelaySeconds (params, (int) engine, bpm);
    const auto length = InstrumentSynth::engineLengthSeconds (params, (int) engine, bpm);
    const auto curve = params.curve[engine];
    const auto inner = plot.reduced (3.0f, 4.0f);
    const auto xOf = [&] (double t) { return inner.getX() + (float) juce::jlimit (0.0, 1.0, t / spanSeconds) * inner.getWidth(); };

    juce::Path path;
    const auto steps = juce::jmax (2, (int) inner.getWidth());
    path.startNewSubPath (inner.getX(), inner.getBottom());
    for (int i = 0; i <= steps; ++i)
    {
        const auto t = spanSeconds * (double) i / (double) steps;
        path.lineTo (inner.getX() + inner.getWidth() * (float) i / (float) steps,
                     inner.getBottom() - inner.getHeight() * InstrumentSynth::gateAt (t, delay, length, curve));
    }
    path.lineTo (inner.getRight(), inner.getBottom());
    path.closeSubPath();
    g.setColour (accent.withAlpha (0.28f));
    g.fillPath (path);
    g.setColour (accent);
    g.strokePath (path, juce::PathStrokeType (1.2f));

    const auto noteOff = (double) processor.noteOffTime[(size_t) layer].load (std::memory_order_relaxed);
    const auto playTime = (double) processor.notePlayTime[(size_t) layer].load (std::memory_order_relaxed);
    if (playTime >= 0.0 && playTime <= spanSeconds)
    {
        g.setColour (Theme::text.withAlpha (0.85f));
        g.drawVerticalLine ((int) xOf (playTime), plot.getY() + 1.0f, plot.getBottom() - 1.0f);
    }
    if (noteOff >= 0.0 && noteOff <= spanSeconds)
    {
        g.setColour (Theme::fuchsia.withAlpha (0.8f));
        const auto x = xOf (noteOff);
        for (float y = plot.getY() + 1.0f; y < plot.getBottom() - 1.0f; y += 4.0f)
            g.drawVerticalLine ((int) x, y, juce::jmin (y + 2.0f, plot.getBottom() - 1.0f));
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (9.0f));
    g.drawText (formatSeconds (spanSeconds), cell.plot.reduced (4, 1), juce::Justification::bottomRight);
}

void InstrTimingPanel::paint (juce::Graphics& g)
{
    for (size_t e = 0; e < cells.size(); ++e)
    {
        const auto& cell = *cells[e];
        if (focusView && (int) e != selectedEngine)
            continue;
        g.setColour (Theme::insetStrip);
        g.fillRoundedRectangle (cell.frame.toFloat(), 4.0f);
        g.setColour (Theme::panelBorderDim);
        g.drawRoundedRectangle (cell.frame.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (11.0f, true));
        g.drawText (cell.name, cell.frame.reduced (6, 4).withHeight (headerHeight), juce::Justification::centredLeft);
        if (! focusView)
            paintPlot (g, cell, e);
    }
    if (focusView)
        return;
    auto legend = legendArea.reduced (8);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f));
    g.drawFittedText ("Level of each source over time. The time scale is the same in every plot.\n\n"
                      "White line: the last note. Dashed line: when it was released.\n\n"
                      "GATE: the source is silent until POS DELAY has passed.\n"
                      "SHIFT: it also starts from the beginning there.",
                      legend.withTrimmedBottom (30), juce::Justification::topLeft, 12);
}
