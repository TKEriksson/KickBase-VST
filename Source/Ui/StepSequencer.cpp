#include "StepSequencer.h"

namespace
{
    constexpr int labelWidth = 52;
    constexpr int rowHeight = 30;
    constexpr int rowGap = 6;
    constexpr int pixelsPerSemitone = 5;
}

StepSequencer::StepSequencer (PluginBase& p, juce::Colour bassAccent, const juce::String& bassNameIn)
    : processor (p), bassColour (bassAccent), bassName (bassNameIn)
{
    startTimerHz (30);
}

int StepSequencer::bars() const
{
    return Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
}

BassGrid StepSequencer::grid() const
{
    return (BassGrid) (int) processor.apvts.getRawParameterValue (ParamID::bassGrid)->load();
}

void StepSequencer::timerCallback()
{
    // Rita om bara när något synligt ändrats: markören, mönstret, mönsterlängden eller rutnätet.
    const auto kickStep = processor.transportStatus.kickStep.load();
    const auto bassStep = processor.transportStatus.bassStep.load();
    const auto currentBars = bars();
    const auto currentGrid = (int) grid();
    const auto version = processor.patternVersion.load();

    if (kickStep != lastKickStep || bassStep != lastBassStep || currentBars != lastBars || currentGrid != lastGrid
        || version != lastVersion)
    {
        lastVersion = version;
        lastKickStep = kickStep;
        lastBassStep = bassStep;
        lastBars = currentBars;
        lastGrid = currentGrid;
        repaint();
    }
}

void StepSequencer::resized()
{
    auto area = getLocalBounds();
    labelColumn = area.removeFromLeft (labelWidth);
    kickRow = area.removeFromTop (rowHeight);
    area.removeFromTop (rowGap);
    bassRow = area.removeFromTop (rowHeight);
    area.removeFromTop (rowGap);
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

StepSequencer::Target StepSequencer::targetAt (juce::Point<int> point) const
{
    if (point.x < kickRow.getX())
        return Target::none;
    if (point.y < kickRow.getBottom() + rowGap / 2)
        return Target::kick;
    if (point.y < bassRow.getBottom() + rowGap / 2)
        return Target::bass;
    return Target::lane;
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
                                    int playingStep, const std::function<bool (int)>& isOn) const
{
    for (int step = 0; step < numSteps; ++step)
    {
        const auto bounds = stepBounds (row, step, numSteps);
        const auto on = isOn (step);
        const auto playing = step == playingStep;

        if (on)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillRoundedRectangle (bounds.expanded (1.0f), 4.0f);
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.2f), 0.0f, bounds.getY(),
                                                     colour.darker (0.3f), 0.0f, bounds.getBottom(), false));
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
    const auto currentGrid = grid();
    const auto numSteps = bassSteps();
    const auto area = laneArea.toFloat();

    g.setColour (Theme::knobBodyDark.withAlpha (0.6f));
    g.fillRoundedRectangle (area, 4.0f);

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
        const auto& data = processor.pattern.bassStep (currentGrid, step);
        const auto on = data.on.load();
        const auto colour = on ? bassColour : Theme::textDim.withAlpha (0.25f);
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

    const auto currentGrid = grid();
    paintToggleRow (g, kickRow, kickSteps(), Theme::kick, processor.transportStatus.kickStep.load(),
                    [this] (int step) { return processor.pattern.isKickOn (step); });
    paintToggleRow (g, bassRow, bassSteps(), bassColour, processor.transportStatus.bassStep.load(),
                    [this, currentGrid] (int step) { return processor.pattern.bassStep (currentGrid, step).on.load(); });
    paintLane (g);

    // Etiketter till vänster.
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::kick);
    g.drawText ("KICK", labelColumn.withY (kickRow.getY()).withHeight (rowHeight), juce::Justification::centredLeft);
    g.setColour (bassColour);
    g.drawText (bassName, labelColumn.withY (bassRow.getY()).withHeight (rowHeight), juce::Justification::centredLeft);
    g.setColour (Theme::textDim);
    g.drawText (laneName(), labelColumn.withY (laneArea.getY()).withHeight (16), juce::Justification::centredLeft);

    if (hoverStep >= 0)
    {
        g.setFont (Theme::font (10.0f));
        g.setColour (Theme::text);
        g.drawFittedText (describeStep (hoverStep), labelColumn.withY (laneArea.getY() + 18).withHeight (44).withTrimmedRight (4),
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
    const auto& data = processor.pattern.bassStep (grid(), step);
    juce::String text = "Steg " + juce::String (step + 1) + "\n";

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
    const auto currentGrid = grid();

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
            auto& step = processor.pattern.bassStep (currentGrid, stepAt (point.x, bassSteps())).on;
            if (firstEvent)
                paintOn = ! step.load();
            step = paintOn;
            break;
        }
        case Target::lane:
        {
            auto& step = processor.pattern.bassStep (currentGrid, stepAt (point.x, bassSteps()));
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
        case Target::none:
            return;
    }

    edited = true;
    repaint();
}

void StepSequencer::mouseDown (const juce::MouseEvent& event)
{
    dragTarget = targetAt (event.getPosition());
    edited = false;

    // Högerklick i lanet återställer steget.
    if (event.mods.isPopupMenu() && dragTarget == Target::lane)
    {
        auto& step = processor.pattern.bassStep (grid(), stepAt (event.x, bassSteps()));
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
        hoverStep = stepAt (event.x, bassSteps());
}

void StepSequencer::mouseUp (const juce::MouseEvent&)
{
    if (edited)
        processor.patternChanged();

    dragTarget = Target::none;
}

void StepSequencer::mouseMove (const juce::MouseEvent& event)
{
    const auto step = targetAt (event.getPosition()) == Target::lane ? stepAt (event.x, bassSteps()) : -1;
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
    if (lane != Lane::pitch || targetAt (event.getPosition()) != Target::lane || wheel.deltaY == 0.0f)
        return;

    auto& step = processor.pattern.bassStep (grid(), stepAt (event.x, bassSteps()));
    step.pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, step.pitch.load() + (wheel.deltaY > 0.0f ? 1 : -1));
    processor.patternChanged();
    repaint();
}
