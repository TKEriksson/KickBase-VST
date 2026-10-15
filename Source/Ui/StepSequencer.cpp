#include "StepSequencer.h"

namespace
{
    constexpr int labelWidth = 64;
    constexpr int pixelsPerSemitone = 5;
}

StepSequencer::StepSequencer (PluginBase& p, juce::StringArray names, std::vector<juce::Colour> colours)
    : processor (p), layerNames (std::move (names)), layerColours (std::move (colours))
{
    setTooltip ("PATTERN - Click steps to turn them on/off (drag to paint). Click a row's name to edit that layer's "
                "velocity, gate, pitch or slide in the lane below. Right-click a step in the lane to reset it.");
    startTimerHz (30);
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

Pattern::BassStep& StepSequencer::selectedStep (int step) const
{
    return processor.pattern.bassStep (selectedLayer, grid (selectedLayer), step);
}

void StepSequencer::timerCallback()
{
    // Rita om bara när något synligt ändrats: markörerna, mönstret, mönsterlängden eller rutnäten.
    auto changed = false;
    auto update = [&changed] (int& last, int now) { if (last != now) { last = now; changed = true; } };

    update (lastKickStep, processor.transportStatus.kickStep.load());
    update (lastBars, bars());
    update (lastVersion, processor.patternVersion.load());
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        update (lastBassSteps[(size_t) layer], processor.transportStatus.bassSteps[(size_t) layer].load());
        update (lastGrids[(size_t) layer], (int) grid (layer));
    }

    if (changed)
        repaint();
}

void StepSequencer::resized()
{
    auto area = getLocalBounds();
    labelColumn = area.removeFromLeft (labelWidth);

    // Raderna får plats ovanför lanet, som tar resten (minst en tredjedel).
    const auto rows = 1 + numLayers();
    const auto gap = 5;
    const auto rowHeight = juce::jlimit (18, 30, (area.getHeight() * 2 / 3 - gap * rows) / rows);

    kickRow = area.removeFromTop (rowHeight);
    area.removeFromTop (gap);
    bassRows.clear();
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        bassRows.push_back (area.removeFromTop (rowHeight));
        area.removeFromTop (gap);
    }
    area.removeFromTop (3);
    laneArea = area;
}

juce::Rectangle<float> StepSequencer::stepBounds (juce::Rectangle<int> row, int step, int numSteps) const
{
    const auto width = (float) row.getWidth() / (float) numSteps;
    return { row.getX() + step * width + 1.0f, (float) row.getY(), std::max (1.0f, width - 2.0f), (float) row.getHeight() };
}

int StepSequencer::stepAt (int x, int numSteps) const
{
    const auto fraction = (float) (x - kickRow.getX()) / (float) kickRow.getWidth();
    return juce::jlimit (0, numSteps - 1, (int) std::floor (fraction * (float) numSteps));
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
    const auto beats = bars() * 4;
    const auto beatWidth = (float) area.getWidth() / (float) beats;
    for (int beat = 0; beat < beats; ++beat)
    {
        if (beat % 2 == 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRect (juce::Rectangle<float> (area.getX() + beat * beatWidth, (float) area.getY(), beatWidth, (float) area.getHeight()));
        }
    }

    for (int bar = 1; bar < bars(); ++bar)
    {
        g.setColour (Theme::textDim.withAlpha (0.5f));
        const auto x = area.getX() + bar * 4 * beatWidth;
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }
}

void StepSequencer::paintToggleRow (juce::Graphics& g, juce::Rectangle<int> row, int numSteps, juce::Colour colour,
                                    int playingStep, const std::function<bool (int)>& isOn, bool dimmed) const
{
    for (int step = 0; step < numSteps; ++step)
    {
        const auto bounds = stepBounds (row, step, numSteps);
        const auto on = isOn (step);
        const auto playing = step == playingStep;

        if (on)
        {
            const auto c = dimmed ? colour.withMultipliedSaturation (0.6f).withMultipliedAlpha (0.65f) : colour;
            g.setColour (c.withAlpha (0.25f));
            g.fillRoundedRectangle (bounds.expanded (1.0f), 4.0f);
            g.setGradientFill (juce::ColourGradient (c.brighter (0.2f), 0.0f, bounds.getY(),
                                                     c.darker (0.3f), 0.0f, bounds.getBottom(), false));
            g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
        }
        else
        {
            g.setColour (Theme::knobBodyDark);
            g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
            g.setColour (Theme::grid);
            g.drawRoundedRectangle (bounds.reduced (1.0f), 3.0f, 1.0f);
        }

        if (playing)
        {
            g.setColour (juce::Colours::white.withAlpha (on ? 0.9f : 0.5f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.5f);
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
        const auto& data = selectedStep (step);
        const auto on = data.on.load();
        const auto colour = on ? colourOn : Theme::textDim.withAlpha (0.25f);
        const auto column = stepBounds (laneArea, step, numSteps).reduced (1.0f, 3.0f);

        switch (lane)
        {
            case Lane::velocity:
            case Lane::gate:
            {
                const auto value = lane == Lane::velocity ? data.velocity.load() : data.gate.load();
                const auto bar = column.withTop (column.getBottom() - value * column.getHeight());
                g.setColour (colour.withAlpha (on ? 0.85f : 0.4f));
                g.fillRoundedRectangle (bar, 2.0f);
                break;
            }
            case Lane::pitch:
            {
                const auto pitch = data.pitch.load();
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
                if (data.slide.load())
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

void StepSequencer::paint (juce::Graphics& g)
{
    paintGrid (g, juce::Rectangle<int>::leftTopRightBottom (kickRow.getX(), kickRow.getY(), kickRow.getRight(), laneArea.getBottom()));

    paintToggleRow (g, kickRow, kickSteps(), Theme::kick, processor.transportStatus.kickStep.load(),
                    [this] (int step) { return processor.pattern.isKickOn (step); }, false);
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        const auto layerGrid = grid (layer);
        paintToggleRow (g, bassRows[(size_t) layer], bassSteps (layer), layerColours[(size_t) layer],
                        processor.transportStatus.bassSteps[(size_t) layer].load(),
                        [this, layer, layerGrid] (int step) { return processor.pattern.bassStep (layer, layerGrid, step).on.load(); },
                        layer != selectedLayer);
    }
    paintLane (g);

    // Etiketter till vänster; det valda lagret markeras.
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::kick);
    g.drawText ("KICK", labelColumn.withY (kickRow.getY()).withHeight (kickRow.getHeight()), juce::Justification::centredLeft);
    for (int layer = 0; layer < numLayers(); ++layer)
    {
        const auto row = labelColumn.withY (bassRows[(size_t) layer].getY()).withHeight (bassRows[(size_t) layer].getHeight());
        const auto colour = layerColours[(size_t) layer];
        if (layer == selectedLayer && numLayers() > 1)
        {
            g.setColour (colour.withAlpha (0.18f));
            g.fillRoundedRectangle (row.toFloat().reduced (0.0f, 1.0f).withTrimmedRight (6.0f), 4.0f);
        }
        g.setColour (layer == selectedLayer ? colour : colour.withAlpha (0.6f));
        g.drawText (layerNames[layer], row.withTrimmedLeft (4), juce::Justification::centredLeft);
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    g.drawText (laneName(), labelColumn.withY (laneArea.getY()).withHeight (14), juce::Justification::centredLeft);
    if (numLayers() > 1)
    {
        g.setColour (layerColours[(size_t) selectedLayer]);
        g.drawText (layerNames[selectedLayer], labelColumn.withY (laneArea.getY() + 14).withHeight (14), juce::Justification::centredLeft);
    }

    if (hoverStep >= 0)
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
        case Lane::gate:     return "GATE";
        case Lane::pitch:    return "PITCH";
        case Lane::slide:    return "SLIDE";
    }
    return {};
}

juce::String StepSequencer::describeStep (int step) const
{
    const auto& data = selectedStep (step);
    juce::String text = "Step " + juce::String (step + 1) + "\n";

    switch (lane)
    {
        case Lane::velocity: return text + juce::String (juce::roundToInt (data.velocity.load() * 100.0f)) + " %";
        case Lane::gate:     return text + juce::String (juce::roundToInt (data.gate.load() * 100.0f)) + " %";
        case Lane::pitch:    return text + (data.pitch.load() > 0 ? "+" : "") + juce::String (data.pitch.load()) + " st";
        case Lane::slide:    return text + (data.slide.load() ? "slide" : "-");
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
            auto& step = processor.pattern.bassStep (dragLayer, grid (dragLayer), stepAt (point.x, bassSteps (dragLayer))).on;
            if (firstEvent)
                paintOn = ! step.load();
            step = paintOn;
            break;
        }
        case Target::lane:
        {
            auto& step = selectedStep (stepAt (point.x, bassSteps (selectedLayer)));
            switch (lane)
            {
                case Lane::velocity: step.velocity = laneValueAt (point.y); break;
                case Lane::gate:     step.gate = std::max (0.05f, laneValueAt (point.y)); break;
                case Lane::pitch:
                    // Dra uppåt/neråt från startpunkten; samma tonhöjd målas över de steg man drar över.
                    if (firstEvent)
                    {
                        dragStartPitch = step.pitch.load();
                        dragStartY = point.y;
                    }
                    dragPitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch,
                                              dragStartPitch + (dragStartY - point.y) / pixelsPerSemitone);
                    step.pitch = dragPitch;
                    break;
                case Lane::slide:
                    if (firstEvent)
                        paintOn = ! step.slide.load();
                    step.slide = paintOn;
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

    // Högerklick i lanet återställer steget.
    if (event.mods.isPopupMenu() && dragTarget == Target::lane)
    {
        auto& step = selectedStep (stepAt (event.x, bassSteps (selectedLayer)));
        step.velocity = 0.8f;
        step.gate = 0.7f;
        step.pitch = 0;
        step.slide = false;
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

    auto& step = selectedStep (stepAt (event.x, bassSteps (selectedLayer)));
    step.pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, step.pitch.load() + (wheel.deltaY > 0.0f ? 1 : -1));
    processor.patternChanged();
    repaint();
}
