#include "StepSequencer.h"

namespace
{
    constexpr int knobsWidth = 46;                 // två små rattar (volym och FX) före namnet
    constexpr int labelWidth = knobsWidth + 62;
    constexpr int pixelsPerSemitone = 5;
}

StepSequencer::StepSequencer (PluginBase& p, juce::StringArray names, std::vector<juce::Colour> colours)
    : processor (p), layerNames (std::move (names)), layerColours (std::move (colours))
{
    setTooltip ("PATTERN - Click steps to turn notes on/off (drag to paint). Click a row's name to edit that layer's "
                "velocity, length or pitch in the lane below. A layer edited in the piano roll shows a small piano roll instead of steps: "
                "click it to open the piano roll, right-click to delete it. GATE mode edits the on/off rhythm that chops the sound.");
    addRowKnobs ("KICK", ParamID::kickLevel, ParamID::kickFxAmount, Theme::kick);
    for (int layer = 0; layer < numLayers(); ++layer)
        addRowKnobs (layerNames[layer], ParamID::bass (layer, BassField::level), ParamID::bass (layer, BassField::fxAmount), layerColours[(size_t) layer]);

    previousPageButton.setTooltip ("Previous four bars.");
    nextPageButton.setTooltip ("Next four bars.");
    previousPageButton.onClick = [this] { setPage (page - 1); };
    nextPageButton.onClick = [this] { setPage (page + 1); };
    addChildComponent (previousPageButton);
    addChildComponent (nextPageButton);

    startTimerHz (30);
}

void StepSequencer::setPage (int newPage)
{
    newPage = juce::jlimit (0, numPages() - 1, newPage);
    if (newPage == page)
        return;
    page = newPage;
    repaint();
}

void StepSequencer::addRowKnobs (const juce::String& name, const juce::String& levelID, const juce::String& fxID, juce::Colour colour)
{
    auto knobs = std::make_unique<RowKnobs>();
    auto setup = [&] (juce::Slider& slider, const juce::String& id, const juce::String& what, const juce::String& text)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setTooltip (name + " " + what + " - " + text);
        addAndMakeVisible (slider);
        if (auto* parameter = processor.apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    };
    setup (knobs->level, levelID, "VOLUME", "The volume of this row (the same as LEVEL on its page). Double-click to reset.");
    setup (knobs->fx, fxID, "FX SEND", "How much of this row goes through its effect channel (pick the channel in the top right corner of its tab). Double-click to reset.");
    knobs->levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, levelID, knobs->level);
    knobs->fxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, fxID, knobs->fx);
    rowKnobs.push_back (std::move (knobs));
}

juce::Rectangle<int> StepSequencer::knobBounds (juce::Rectangle<int> row, int index) const
{
    return juce::Rectangle<int> (22, std::min (22, row.getHeight())).withCentre ({ labelColumn.getX() + 12 + index * 23, row.getCentreY() });
}

void StepSequencer::selectLayer (int layer)
{
    layer = juce::jlimit (0, numLayers() - 1, layer);
    if (layer == selectedLayer)
        return;
    selectedLayer = layer;
    if (onLayerSelected)
        onLayerSelected (layer);
    repaint();
}

int StepSequencer::bars() const
{
    return Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
}

BassGrid StepSequencer::grid (int layer) const
{
    return (BassGrid) (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::grid))->load();
}

BassGrid StepSequencer::gateGrid (int layer) const
{
    return (BassGrid) juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateGrid))->load());
}

void StepSequencer::setMode (Mode newMode)
{
    mode = newMode;
    resized();
    repaint();
}

int StepSequencer::noteTickAt (int layer, int step) const
{
    const auto from = step * ticksPerStep (layer);
    return processor.pattern.firstNoteIn (layer, from, from + ticksPerStep (layer));
}

Pattern::Note* StepSequencer::noteAt (int layer, int step) const
{
    const auto tick = noteTickAt (layer, step);
    return tick >= 0 ? &processor.pattern.note (layer, tick) : nullptr;
}

void StepSequencer::timerCallback()
{
    // Rita om bara när något synligt ändrats: markörerna, mönstret, mönsterlängden eller rutnäten.
    auto changed = false;
    auto update = [&changed] (int& last, int now) { if (last != now) { last = now; changed = true; } };

    update (lastKickStep, processor.transportStatus.kickStep.load());
    const auto barsBefore = lastBars;
    update (lastBars, bars());
    if (lastBars != barsBefore)
    {
        page = juce::jlimit (0, numPages() - 1, page);
        resized(); // sidknapparna visas bara när mönstret är längre än en sida
    }
    update (lastVersion, processor.patternVersion.load());
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        update (lastBassSteps[(size_t) layer], stepOfTick (layer, processor.transportStatus.bassSteps[(size_t) layer].load()));
        update (lastGrids[(size_t) layer], (int) grid (layer));
        update (lastGateGrids[(size_t) layer], (int) gateGrid (layer));
    }
    update (lastCustomBits, (processor.pattern.isCustom (0) ? 1 : 0) | (processor.pattern.isCustom (1) ? 2 : 0) | (processor.pattern.isCustom (2) ? 4 : 0));

    if (changed)
        repaint();
}

void StepSequencer::resized()
{
    auto area = getLocalBounds();
    labelColumn = area.removeFromLeft (labelWidth);

    // Sidväljaren (bara för mönster längre än fyra takter) ligger i en smal remsa överst.
    const auto paged = bars() > barsPerPage;
    pageArea = paged ? area.removeFromTop (18) : juce::Rectangle<int>();
    if (paged)
        area.removeFromTop (3);
    previousPageButton.setVisible (paged);
    nextPageButton.setVisible (paged);
    if (paged)
    {
        previousPageButton.setBounds (pageArea.withWidth (28).withX (area.getX()));
        nextPageButton.setBounds (pageArea.withWidth (28).withX (area.getX() + 32));
    }

    // Raderna tar den plats de behöver, och lanet (velocity m.m.) en låg remsa under dem.
    const auto rows = 1 + numLayers();
    const auto gap = 5;
    const auto laneSpace = mode == Mode::notes ? 70 + 3 : 0;
    const auto rowHeight = juce::jlimit (22, 54, (area.getHeight() - laneSpace - gap * rows) / rows);

    kickRow = area.removeFromTop (rowHeight);
    area.removeFromTop (gap);
    bassRows.clear();
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        bassRows.push_back (area.removeFromTop (rowHeight));
        area.removeFromTop (gap);
    }
    area.removeFromTop (3);
    laneArea = mode == Mode::notes ? area.removeFromTop (std::min (area.getHeight(), 100)) : juce::Rectangle<int>();

    // Rattarna (volym och FX) längst till vänster i varje rad.
    for (size_t i = 0; i < rowKnobs.size(); ++i)
    {
        const auto row = i == 0 ? kickRow : bassRows[i - 1];
        rowKnobs[i]->level.setBounds (knobBounds (row, 0));
        rowKnobs[i]->fx.setBounds (knobBounds (row, 1));
    }
}

juce::Rectangle<float> StepSequencer::stepBounds (juce::Rectangle<int> row, int step, int numSteps) const
{
    const auto width = (float) row.getWidth() / (float) numSteps;
    return { row.getX() + step * width + 1.0f, (float) row.getY(), std::max (1.0f, width - 2.0f), (float) row.getHeight() };
}

int StepSequencer::stepAt (int x, int numSteps) const
{
    const auto fraction = (float) (x - kickRow.getX()) / (float) kickRow.getWidth();
    return pageOffset (numSteps) + juce::jlimit (0, numSteps - 1, (int) std::floor (fraction * (float) numSteps)); // plats i mönstret
}

StepSequencer::Target StepSequencer::targetAt (juce::Point<int> point, int& layer) const
{
    layer = selectedLayer;
    const auto inLabels = point.x < kickRow.getX();

    if (point.y < kickRow.getBottom() + 2)
        return inLabels ? Target::none : Target::kick;

    for (int l = 0; l < (int) bassRows.size(); ++l)
    {
        if (point.y < bassRows[(size_t) l].getBottom() + 2)
        {
            layer = l;
            return inLabels ? Target::label : Target::bass;
        }
    }
    return inLabels ? Target::none : Target::lane;
}

void StepSequencer::paintGrid (juce::Graphics& g, juce::Rectangle<int> area) const
{
    // Varannat slag lite ljusare, och tydliga taktstreck.
    const auto beats = viewBars() * 4;
    const auto beatWidth = (float) area.getWidth() / (float) beats;
    for (int beat = 0; beat < beats; ++beat)
    {
        if (beat % 2 == 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRect (juce::Rectangle<float> (area.getX() + beat * beatWidth, (float) area.getY(), beatWidth, (float) area.getHeight()));
        }
    }

    for (int bar = 1; bar < viewBars(); ++bar)
    {
        g.setColour (Theme::textDim.withAlpha (0.5f));
        const auto x = area.getX() + bar * 4 * beatWidth;
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }
}

void StepSequencer::paintToggleRow (juce::Graphics& g, juce::Rectangle<int> row, int numSteps, int offset, juce::Colour colour,
                                    int playingStep, const std::function<bool (int)>& isOn, bool dimmed) const
{
    for (int step = 0; step < numSteps; ++step)
    {
        const auto bounds = stepBounds (row, step, numSteps).reduced (0.5f, 1.0f);
        const auto on = isOn (offset + step);
        const auto playing = offset + step == playingStep;
        constexpr float corner = 3.5f;

        if (on)
        {
            const auto c = dimmed ? colour.withMultipliedSaturation (0.6f).withMultipliedAlpha (0.65f) : colour;

            // Mjuk yttre neon-glöd
            g.setColour (c.withAlpha (0.30f));
            g.fillRoundedRectangle (bounds.expanded (1.5f), corner + 1.0f);

            // 3D-knapp med topp-diffus gradient
            g.setGradientFill (juce::ColourGradient (c.brighter (0.40f), bounds.getX(), bounds.getY(),
                                                     c.darker (0.25f), bounds.getX(), bounds.getBottom(), false));
            g.fillRoundedRectangle (bounds, corner);

            // Fasad toppkant (specular highlight)
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawLine (bounds.getX() + 2.0f, bounds.getY() + 0.5f, bounds.getRight() - 2.0f, bounds.getY() + 0.5f, 1.0f);

            // Skarp kant
            g.setColour (c.brighter (0.15f));
            g.drawRoundedRectangle (bounds, corner, 1.0f);
        }
        else
        {
            // Nedsänkt mörk sockel
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121420), bounds.getX(), bounds.getY(),
                                                     juce::Colour (0xff181b29), bounds.getX(), bounds.getBottom(), false));
            g.fillRoundedRectangle (bounds, corner);

            g.setColour (juce::Colour (0xff24283c));
            g.drawRoundedRectangle (bounds, corner, 1.0f);

            // Taktstreck-markering (var 4:e steg är subtilt ljusare)
            if (step % 4 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (bounds, corner);
            }
        }

        if (playing)
        {
            g.setColour (juce::Colours::white.withAlpha (on ? 0.95f : 0.65f));
            g.drawRoundedRectangle (bounds.expanded (0.5f), corner + 0.5f, 1.5f);
        }
    }
}

void StepSequencer::paintLane (juce::Graphics& g) const
{
    const auto numSteps = bassSteps (selectedLayer);
    const auto area = laneArea.toFloat();
    const auto colourOn = layerColours[(size_t) selectedLayer];

    g.setColour (Theme::knobBodyDark.withAlpha (0.6f));
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (colourOn.withAlpha (0.35f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);

    if (isCustom (selectedLayer))
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (11.0f));
        g.drawFittedText ("This layer is edited in the piano roll (note velocity, length, pitch and glide are set there). "
                          "Click its row to open the piano roll, or right-click it to delete the piano roll and use steps again.",
                          laneArea.reduced (14, 8), juce::Justification::centred, 3);
        return;
    }

    if (lane == Lane::pitch)
    {
        // Nollinje i mitten och svaga linjer vid ±12.
        g.setColour (Theme::grid);
        for (const auto semis : { -12, 12 })
            g.drawHorizontalLine ((int) (area.getCentreY() - semis * area.getHeight() / 48.0f), area.getX(), area.getRight());
        g.setColour (Theme::textDim.withAlpha (0.5f));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    }

    for (int step = 0; step < numSteps; ++step)
    {
        const auto* note = noteAt (selectedLayer, pageOffset (numSteps) + step);
        const auto on = note != nullptr;
        const auto colour = on ? colourOn : Theme::textDim.withAlpha (0.25f);
        const auto column = stepBounds (laneArea, step, numSteps).reduced (1.0f, 3.0f);
        if (! on && lane != Lane::slide)
            continue;

        switch (lane)
        {
            case Lane::velocity:
            case Lane::gate:
            {
                const auto value = lane == Lane::velocity ? note->velocity.load()
                                                          : juce::jlimit (0.05f, 1.0f, (float) note->length.load() / (float) ticksPerStep (selectedLayer));
                const auto bar = column.withTop (column.getBottom() - value * column.getHeight());
                g.setColour (colour.withAlpha (on ? 0.85f : 0.4f));
                g.fillRoundedRectangle (bar, 2.0f);
                break;
            }
            case Lane::pitch:
            {
                const auto pitch = note->pitch.load();
                const auto y = column.getCentreY() - pitch * column.getHeight() / 48.0f;
                const auto bar = juce::Rectangle<float>::leftTopRightBottom (column.getX(), std::min (y, column.getCentreY()),
                                                                            column.getRight(), std::max (y, column.getCentreY()));
                g.setColour (colour.withAlpha (on ? 0.85f : 0.4f));
                g.fillRect (bar);
                g.fillRoundedRectangle (column.withHeight (4.0f).withCentre ({ column.getCentreX(), y }), 2.0f);

                if (pitch != 0 && column.getWidth() >= 14.0f)
                {
                    g.setColour (on ? Theme::text : Theme::textDim);
                    g.setFont (Theme::font (9.0f, true));
                    const auto textY = pitch > 0 ? y - 12.0f : y + 2.0f;
                    g.drawText ((pitch > 0 ? "+" : "") + juce::String (pitch),
                                juce::Rectangle<float> (column.getX() - 4.0f, textY, column.getWidth() + 8.0f, 10.0f),
                                juce::Justification::centred);
                }
                break;
            }
            case Lane::slide:
            {
                const auto box = juce::Rectangle<float> (std::min (12.0f, column.getWidth()), 12.0f).withCentre (column.getCentre());
                if (note != nullptr && note->slide.load())
                {
                    g.setColour (colour);
                    g.fillRoundedRectangle (box, 3.0f);

                    // Pil mot nästa steg.
                    const auto next = stepBounds (laneArea, (step + 1) % numSteps, numSteps);
                    if (step + 1 < numSteps)
                        g.drawArrow (juce::Line<float> (box.getRight(), box.getCentreY(), next.getX() + 2.0f, box.getCentreY()), 1.5f, 5.0f, 5.0f);
                }
                else
                {
                    g.setColour (colour.withAlpha (0.5f));
                    g.drawRoundedRectangle (box, 3.0f, 1.0f);
                }
                break;
            }
        }
    }
}

void StepSequencer::paintMiniature (juce::Graphics& g, juce::Rectangle<int> row, int layer, juce::Colour colour, int playTick, bool dimmed) const
{
    // Ett lager som redigerats i pianorullen visas som en liten pianorulle, som i FL Studios kanalrack.
    const auto area = row.toFloat().reduced (0.0f, 1.0f);
    g.setColour (juce::Colour (0xff121420));
    g.fillRoundedRectangle (area, 4.0f);

    const auto totalTicks = bars() * Pattern::ticksPerBar;
    auto lowest = Pattern::maxPitch, highest = Pattern::minPitch;
    for (int tick = 0; tick < totalTicks; ++tick)
        if (const auto& n = processor.pattern.note (layer, tick); n.on.load())
        {
            lowest = std::min (lowest, n.pitch.load());
            highest = std::max (highest, n.pitch.load());
        }

    const auto span = std::max (8, highest - lowest + 1);
    const auto bottomPitch = (lowest <= highest ? lowest : 0) - (span - std::max (1, highest - lowest + 1)) / 2;
    const auto rowHeight = (area.getHeight() - 4.0f) / (float) span;
    const auto alpha = dimmed ? 0.55f : 1.0f;

    for (int tick = 0; tick < totalTicks; ++tick)
    {
        const auto& n = processor.pattern.note (layer, tick);
        if (! n.on.load())
            continue;
        const auto x0 = area.getX() + 2.0f + (area.getWidth() - 4.0f) * (float) tick / (float) totalTicks;
        const auto x1 = area.getX() + 2.0f + (area.getWidth() - 4.0f) * (float) std::min (totalTicks, tick + n.length.load()) / (float) totalTicks;
        const auto y = area.getBottom() - 2.0f - (float) (n.pitch.load() - bottomPitch + 1) * rowHeight;
        const auto note = juce::Rectangle<float> (x0, y, std::max (2.0f, x1 - x0), std::max (2.0f, rowHeight - 0.5f));
        g.setColour ((n.slide.load() ? colour.interpolatedWith (juce::Colours::white, 0.55f) : colour).withAlpha (alpha * (0.5f + 0.5f * n.velocity.load())));
        g.fillRoundedRectangle (note, 1.5f);
    }

    g.setColour (colour.withAlpha (dimmed ? 0.25f : 0.55f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
    g.setFont (Theme::font (8.5f, true));
    g.setColour (Theme::textDim.withAlpha (0.8f));
    g.drawText ("PIANO ROLL", area.reduced (5.0f, 2.0f), juce::Justification::topRight);

    if (playTick >= 0)
    {
        g.setColour (juce::Colour (0xffffe27a));
        g.fillRect (juce::Rectangle<float> (area.getX() + 2.0f + (area.getWidth() - 4.0f) * (float) playTick / (float) totalTicks, area.getY() + 1.0f, 1.5f, area.getHeight() - 2.0f));
    }
}

void StepSequencer::showCustomMenu (int layer)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Open piano roll");
    menu.addItem (2, "Delete piano roll (back to the step pattern)");

    juce::Component::SafePointer<StepSequencer> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withMinimumWidth (240), [safeThis, layer] (int result)
    {
        if (safeThis == nullptr)
            return;
        if (result == 1 && safeThis->onOpenPianoRoll)
            safeThis->onOpenPianoRoll (layer);
        else if (result == 2)
        {
            // Bekräfta först: det tar bort lagrets alla toner.
            juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                              juce::MessageBoxIconType::WarningIcon, "Delete piano roll",
                                              "Delete the piano roll? All the notes of this layer are removed and the layer goes back to an empty step pattern.",
                                              "Delete", "Cancel"),
                                          [safeThis, layer] (int choice)
            {
                if (safeThis == nullptr || choice != 1)
                    return;
                safeThis->processor.pattern.clearNotes (layer); // nollställer också pianorullsmarkeringen
                safeThis->processor.patternChanged();
            });
        }
    });
}

void StepSequencer::paint (juce::Graphics& g)
{
    const auto gridBottom = mode == Mode::notes ? laneArea.getBottom() : bassRows.back().getBottom();
    paintGrid (g, juce::Rectangle<int>::leftTopRightBottom (kickRow.getX(), kickRow.getY(), kickRow.getRight(), gridBottom));

    paintToggleRow (g, kickRow, kickSteps(), pageOffset (kickSteps()), Theme::kick, processor.transportStatus.kickStep.load(),
                    [this] (int step) { return processor.pattern.isKickOn (step); }, false);
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        const auto& row = bassRows[(size_t) layer];
        const auto colour = layerColours[(size_t) layer];
        const auto playTick = processor.transportStatus.bassSteps[(size_t) layer].load();

        if (mode == Mode::gate)
        {
            // GATE: en egen av/på-rytm med eget rutnät. Raden dämpas när lagrets gate är avstängd.
            const auto gateTicks = Pattern::ticksPerStep (gateGrid (layer));
            const auto gateIsOn = processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateOn))->load() >= 0.5f;
            paintToggleRow (g, row, gateSteps (layer), pageOffset (gateSteps (layer)), colour, playTick < 0 ? -1 : playTick / gateTicks,
                            [this, layer] (int step) { return processor.pattern.isGateStepOn (layer, step); },
                            layer != selectedLayer || ! gateIsOn);
        }
        else if (isCustom (layer))
            paintMiniature (g, row, layer, colour, playTick, layer != selectedLayer);
        else
            paintToggleRow (g, row, bassSteps (layer), pageOffset (bassSteps (layer)), colour, stepOfTick (layer, playTick),
                            [this, layer] (int step) { return noteTickAt (layer, step) >= 0; }, layer != selectedLayer);
    }
    if (mode == Mode::notes)
        paintLane (g);

    // Etiketter till vänster; det valda lagret markeras.
    const auto nameColumn = labelColumn.withTrimmedLeft (knobsWidth);
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::kick);
    g.drawText ("KICK", nameColumn.withY (kickRow.getY()).withHeight (kickRow.getHeight()), juce::Justification::centredLeft);
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        const auto row = nameColumn.withY (bassRows[(size_t) layer].getY()).withHeight (bassRows[(size_t) layer].getHeight());
        const auto colour = layerColours[(size_t) layer];
        if (layer == selectedLayer && numLayers() > 1)
        {
            g.setColour (colour.withAlpha (0.18f));
            g.fillRoundedRectangle (row.toFloat().reduced (0.0f, 1.0f).withTrimmedRight (6.0f), 4.0f);
        }
        g.setColour (layer == selectedLayer ? colour : colour.withAlpha (0.6f));
        g.drawText (layerNames[layer], row.withTrimmedLeft (4), juce::Justification::centredLeft);
    }
    if (bars() > barsPerPage)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.0f, true));
        g.drawText ("BARS " + juce::String (page * barsPerPage + 1) + "-" + juce::String (page * barsPerPage + viewBars()) + " OF " + juce::String (bars()),
                    pageArea.withTrimmedLeft (68), juce::Justification::centredLeft);
    }
    if (mode == Mode::gate)
        return;

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    g.drawText (laneName(), labelColumn.withY (laneArea.getY()).withHeight (14), juce::Justification::centredLeft);
    if (numLayers() > 1)
    {
        g.setColour (layerColours[(size_t) selectedLayer]);
        g.drawText (layerNames[selectedLayer], labelColumn.withY (laneArea.getY() + 14).withHeight (14), juce::Justification::centredLeft);
    }

    if (hoverStep >= 0 && ! isCustom (selectedLayer))
    {
        g.setFont (Theme::font (10.0f));
        g.setColour (Theme::text);
        g.drawFittedText (describeStep (hoverStep), labelColumn.withY (laneArea.getY() + 32).withHeight (40).withTrimmedRight (4),
                          juce::Justification::topLeft, 3);
    }
}

juce::String StepSequencer::laneName() const
{
    switch (lane)
    {
        case Lane::velocity: return "VELOCITY";
        case Lane::gate:     return "LENGTH";
        case Lane::pitch:    return "PITCH";
        case Lane::slide:    return "SLIDE";
    }
    return {};
}

juce::String StepSequencer::describeStep (int step) const
{
    const auto* note = noteAt (selectedLayer, step);
    juce::String text = "Step " + juce::String (step + 1) + "\n";
    if (note == nullptr)
        return text + "(no note)";

    switch (lane)
    {
        case Lane::velocity: return text + juce::String (juce::roundToInt (note->velocity.load() * 100.0f)) + " %";
        case Lane::gate:     return text + juce::String (juce::roundToInt (100.0f * (float) note->length.load() / (float) ticksPerStep (selectedLayer))) + " %";
        case Lane::pitch:    return text + (note->pitch.load() > 0 ? "+" : "") + juce::String (note->pitch.load()) + " st";
        case Lane::slide:    return text + (note->slide.load() ? "slide" : "-");
    }
    return text;
}

float StepSequencer::laneValueAt (int y) const
{
    return juce::jlimit (0.0f, 1.0f, 1.0f - (float) (y - laneArea.getY()) / (float) laneArea.getHeight());
}

void StepSequencer::editAt (juce::Point<int> point, bool firstEvent)
{
    switch (dragTarget)
    {
        case Target::kick:
        {
            auto& step = processor.pattern.kickStep (stepAt (point.x, kickSteps()));
            if (firstEvent)
                paintOn = ! step.load();
            step = paintOn;
            break;
        }
        case Target::bass:
        {
            if (mode == Mode::gate)
            {
                auto& gateStep = processor.pattern.gateStep (dragLayer, stepAt (point.x, gateSteps (dragLayer)));
                if (firstEvent)
                    paintOn = ! gateStep.load();
                gateStep = paintOn;
                break;
            }
            if (isCustom (dragLayer))
                return; // pianorulle: stegen redigeras inte här

            const auto step = stepAt (point.x, bassSteps (dragLayer));
            const auto tps = ticksPerStep (dragLayer);
            if (firstEvent)
                paintOn = noteTickAt (dragLayer, step) < 0;

            if (paintOn)
            {
                if (noteTickAt (dragLayer, step) < 0)
                    processor.pattern.addNote (dragLayer, step * tps, juce::roundToInt (Pattern::defaultGate * (float) tps));
            }
            else
            {
                // Tar bort alla toner som börjar inom steget.
                for (auto tick = noteTickAt (dragLayer, step); tick >= 0; tick = noteTickAt (dragLayer, step))
                    processor.pattern.removeNote (dragLayer, tick);
            }
            break;
        }
        case Target::lane:
        {
            if (mode == Mode::gate || isCustom (selectedLayer))
                return;
            auto* note = noteAt (selectedLayer, stepAt (point.x, bassSteps (selectedLayer)));
            if (note == nullptr)
                return; // ett steg utan ton har inget att redigera
            switch (lane)
            {
                case Lane::velocity: note->velocity = laneValueAt (point.y); break;
                case Lane::gate:
                    note->length = std::max (1, juce::roundToInt (std::max (0.05f, laneValueAt (point.y)) * (float) ticksPerStep (selectedLayer)));
                    break;
                case Lane::pitch:
                    // Dra uppåt/neråt från startpunkten; samma tonhöjd målas över de steg man drar över.
                    if (firstEvent)
                    {
                        dragStartPitch = note->pitch.load();
                        dragStartY = point.y;
                    }
                    dragPitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch,
                                              dragStartPitch + (dragStartY - point.y) / pixelsPerSemitone);
                    note->pitch = dragPitch;
                    break;
                case Lane::slide:
                    if (firstEvent)
                        paintOn = ! note->slide.load();
                    note->slide = paintOn;
                    break;
            }
            break;
        }
        case Target::label:
        case Target::none:
            return;
    }

    edited = true;
    repaint();
}

void StepSequencer::mouseDown (const juce::MouseEvent& event)
{
    dragTarget = targetAt (event.getPosition(), dragLayer);
    edited = false;

    // Ett lagers namn eller steg väljer lagret för lanet.
    if (dragTarget == Target::label || dragTarget == Target::bass)
        selectLayer (dragLayer);
    if (dragTarget == Target::label)
    {
        dragTarget = Target::none;
        return;
    }

    // Ett lager med pianorulle: klicka för att öppna den, högerklicka för att öppna eller ta bort den.
    if (dragTarget == Target::bass && mode == Mode::notes && isCustom (dragLayer))
    {
        dragTarget = Target::none;
        if (event.mods.isPopupMenu())
            showCustomMenu (dragLayer);
        else if (onOpenPianoRoll)
            onOpenPianoRoll (dragLayer);
        return;
    }
    if (dragTarget == Target::lane && (mode == Mode::gate || isCustom (selectedLayer)))
    {
        dragTarget = Target::none;
        return;
    }

    // Högerklick i lanet återställer steget.
    if (event.mods.isPopupMenu() && dragTarget == Target::lane)
    {
        if (auto* note = noteAt (selectedLayer, stepAt (event.x, bassSteps (selectedLayer))))
        {
            note->velocity = 0.8f;
            note->length = juce::roundToInt (Pattern::defaultGate * (float) ticksPerStep (selectedLayer));
            note->pitch = 0;
            note->slide = false;
        }
        processor.patternChanged();
        dragTarget = Target::none;
        repaint();
        return;
    }

    editAt (event.getPosition(), true);
}

void StepSequencer::mouseDrag (const juce::MouseEvent& event)
{
    editAt (event.getPosition(), false);
    if (dragTarget == Target::lane)
        hoverStep = stepAt (event.x, bassSteps (selectedLayer));
}

void StepSequencer::mouseUp (const juce::MouseEvent&)
{
    if (edited)
        processor.patternChanged();

    dragTarget = Target::none;
}

void StepSequencer::mouseMove (const juce::MouseEvent& event)
{
    int layer = 0;
    const auto step = targetAt (event.getPosition(), layer) == Target::lane ? stepAt (event.x, bassSteps (selectedLayer)) : -1;
    if (step != hoverStep)
    {
        hoverStep = step;
        repaint();
    }
}

void StepSequencer::mouseExit (const juce::MouseEvent&)
{
    hoverStep = -1;
    repaint();
}

void StepSequencer::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Skrollhjulet ändrar tonhöjden en halvton i taget i pitch-lanet.
    int layer = 0;
    if (lane != Lane::pitch || targetAt (event.getPosition(), layer) != Target::lane || wheel.deltaY == 0.0f)
        return;

    if (auto* note = noteAt (selectedLayer, stepAt (event.x, bassSteps (selectedLayer))))
    {
        note->pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, note->pitch.load() + (wheel.deltaY > 0.0f ? 1 : -1));
        processor.patternChanged();
        repaint();
    }
}
