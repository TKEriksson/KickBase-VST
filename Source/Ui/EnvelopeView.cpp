#include "EnvelopeView.h"

namespace
{
    constexpr float handleRadius = 5.0f;
    constexpr float grabDistance = 12.0f;
    constexpr float dragSensitivity = 1.0f; // hela bredden/höjden = hela parameterns omfång
}

EnvelopeView::EnvelopeView (double displaySecondsIn) : displaySeconds (displaySecondsIn)
{
    startVisualTimer (Settings::Visual::other, 30); // följer med när parametrar ändras via rattar eller automation
}

void EnvelopeView::timerCallback()
{
    followVisualRate();
    // Rita bara om när kurvorna ändrats (via rattar, handtag eller automation).
    if (onBeforePaint)
        onBeforePaint();

    const auto current = signature();
    if (current != lastSignature)
    {
        lastSignature = current;
        repaint();
    }
}

float EnvelopeView::signature() const
{
    auto sum = 0.0f;
    for (const auto& handle : handles)
    {
        const auto p = handle.position();
        sum += p.x * 7919.0f + p.y * 104729.0f;
    }
    if (noteOff)
        sum += (float) noteOff().seconds * 977.0f;
    for (const auto& curve : curves)
        for (const auto fraction : { 0.0025, 0.01, 0.05, 0.25, 1.0 })
            sum += curve.valueAt (fraction * displaySeconds) * 31.0f;
    return sum;
}

juce::Rectangle<float> EnvelopeView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 10.0f);
}

float EnvelopeView::timeToX (double seconds) const
{
    const auto area = plotArea();
    const auto fraction = std::sqrt (juce::jlimit (0.0, 1.0, seconds / displaySeconds));
    return area.getX() + (float) fraction * area.getWidth();
}

float EnvelopeView::valueToY (float value) const
{
    const auto area = plotArea();
    return area.getBottom() - juce::jlimit (0.0f, 1.0f, value) * area.getHeight();
}

juce::Point<float> EnvelopeView::handlePoint (const Handle& handle) const
{
    const auto p = handle.position();
    return { timeToX (p.x), valueToY (p.y) };
}

void EnvelopeView::paint (juce::Graphics& g)
{
    if (onBeforePaint)
        onBeforePaint();

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 6.0f);

    // Rutnät: lodräta linjer vid jämna tider.
    const auto area = plotArea();
    g.setFont (Theme::font (9.0f));
    auto labelsEnd = -1.0e9f; // högerkanten på förra etiketten: en etikett som skulle hamna ovanpå den hoppas över (linjen ritas ändå)
    for (const auto ms : { 10, 50, 100, 200, 400, 800, 1600 })
    {
        if (ms / 1000.0 > displaySeconds)
            break;

        const auto x = timeToX (ms / 1000.0);
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        const auto label = juce::String (ms) + "ms";
        const auto labelWidth = (float) juce::GlyphArrangement::getStringWidthInt (Theme::font (9.0f), label);
        if (x + 2.0f < labelsEnd + 3.0f)
            continue;
        g.setColour (Theme::textDim.withAlpha (0.6f));
        g.drawText (label, (int) x + 2, (int) area.getY() - 9, 40, 10, juce::Justification::left);
        labelsEnd = x + 2.0f + labelWidth;
    }
    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) area.getBottom(), area.getX(), area.getRight());

    // När tonen släpps (note off): det är inte en inställning här, den kommer från mönstret. Därefter följer release.
    if (noteOff)
    {
        const auto marker = noteOff();
        const auto x = timeToX (marker.seconds);
        juce::Path line;
        line.startNewSubPath (x, area.getY());
        line.lineTo (x, area.getBottom());
        juce::Path dashed;
        const float dashes[] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);
        g.setColour (Theme::text.withAlpha (marker.real ? 0.55f : 0.35f));
        g.fillPath (dashed);

        const auto text = juce::String (marker.real ? "NOTE OFF" : "NOTE OFF (example)");
        g.setFont (Theme::font (9.0f, true));
        const auto width = juce::GlyphArrangement::getStringWidthInt (Theme::font (9.0f, true), text) + 4;
        const auto room = area.getRight() - x - 3.0f;
        if (room >= (float) width)
            g.drawText (text, (int) x + 3, (int) area.getY() + 1, width, 10, juce::Justification::left);
        else
            g.drawText (text, (int) x - width - 2, (int) area.getY() + 1, width, 10, juce::Justification::right);
    }

    // Kurvorna. Samplas tätare i början där skalan är utdragen.
    constexpr int numPoints = 240;
    for (const auto& curve : curves)
    {
        juce::Path path;
        for (int i = 0; i <= numPoints; ++i)
        {
            const auto fraction = (double) i / numPoints;
            const auto seconds = fraction * fraction * displaySeconds;
            const juce::Point<float> point { timeToX (seconds), valueToY (curve.valueAt (seconds)) };

            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        if (curve.filled)
        {
            auto fill = path;
            fill.lineTo (area.getRight(), area.getBottom());
            fill.lineTo (area.getX(), area.getBottom());
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (curve.colour.withAlpha (0.35f), 0.0f, area.getY(),
                                                     curve.colour.withAlpha (0.02f), 0.0f, area.getBottom(), false));
            g.fillPath (fill);
        }

        g.setColour (curve.colour.withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (curve.colour);
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Kurvornas namn nere till höger (tidsetiketterna ligger överst).
    auto legend = getLocalBounds().reduced (10, 12).removeFromBottom (12);
    g.setFont (Theme::font (10.0f, true));
    for (auto it = curves.rbegin(); it != curves.rend(); ++it)
    {
        if (it->name.isEmpty())
            continue;
        g.setColour (it->colour);
        g.drawText (it->name, legend.removeFromRight (44), juce::Justification::right);
    }

    // Handtagen.
    for (int i = 0; i < (int) handles.size(); ++i)
    {
        const auto point = handlePoint (handles[(size_t) i]);
        const auto active = i == hoveredHandle || i == draggedHandle;
        const auto radius = active ? handleRadius + 2.0f : handleRadius;

        g.setColour (handles[(size_t) i].colour.withAlpha (active ? 0.35f : 0.2f));
        g.fillEllipse (juce::Rectangle<float> (radius * 3.0f, radius * 3.0f).withCentre (point));
        g.setColour (Theme::knobBodyDark);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (point));
        g.setColour (handles[(size_t) i].colour);
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (point), 2.0f);
    }

    // Värdet för handtaget under musen.
    const auto shown = draggedHandle >= 0 ? draggedHandle : hoveredHandle;
    if (shown >= 0)
    {
        const auto text = describe (handles[(size_t) shown]);
        const auto point = handlePoint (handles[(size_t) shown]);
        g.setFont (Theme::font (11.0f, true));
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (Theme::font (11.0f, true), text) + 12.0f;
        auto box = juce::Rectangle<float> (width, 18.0f).withCentre (point.translated (0.0f, -18.0f));
        box = box.constrainedWithin (bounds.reduced (2.0f));
        g.setColour (Theme::knobBodyDark.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (handles[(size_t) shown].colour);
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (Theme::text);
        g.drawText (text, box, juce::Justification::centred);
    }
}

juce::String EnvelopeView::describe (const Handle& handle) const
{
    juce::StringArray parts;
    for (auto* parameter : { xOf (handle), handle.yParam })
        if (parameter != nullptr)
            parts.add (parameter->getName (32).fromLastOccurrenceOf ("Kick ", false, false)
                                            .fromLastOccurrenceOf ("Instr ", false, false)
                       + " " + parameter->getCurrentValueAsText());
    return parts.joinIntoString ("   ");
}

int EnvelopeView::handleAt (juce::Point<float> point) const
{
    auto best = -1;
    auto bestDistance = grabDistance;
    for (int i = 0; i < (int) handles.size(); ++i)
    {
        const auto distance = handlePoint (handles[(size_t) i]).getDistanceFrom (point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

void EnvelopeView::mouseMove (const juce::MouseEvent& event)
{
    const auto hovered = handleAt (event.position);
    if (hovered != hoveredHandle)
    {
        hoveredHandle = hovered;
        setMouseCursor (hovered >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EnvelopeView::mouseExit (const juce::MouseEvent&)
{
    hoveredHandle = -1;
    repaint();
}

void EnvelopeView::mouseDown (const juce::MouseEvent& event)
{
    draggedHandle = handleAt (event.position);
    if (draggedHandle < 0)
        return;

    const auto& handle = handles[(size_t) draggedHandle];
    dragStart = event.position;
    startX = xOf (handle) != nullptr ? xOf (handle)->getValue() : 0.0f;
    startY = handle.yParam != nullptr ? handle.yParam->getValue() : 0.0f;

    for (auto* parameter : { xOf (handle), handle.yParam })
        if (parameter != nullptr)
            parameter->beginChangeGesture();
}

void EnvelopeView::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedHandle < 0)
        return;

    const auto& handle = handles[(size_t) draggedHandle];
    const auto area = plotArea();
    const auto delta = event.position - dragStart;
    const auto fine = event.mods.isShiftDown() ? 0.2f : 1.0f; // shift = finjustering

    if (xOf (handle) != nullptr)
        xOf (handle)->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startX + delta.x / area.getWidth() * dragSensitivity * fine));

    if (handle.yParam != nullptr)
    {
        const auto dy = -delta.y / area.getHeight() * dragSensitivity * fine * (handle.invertY ? -1.0f : 1.0f);
        handle.yParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startY + dy));
    }

    repaint();
}

void EnvelopeView::mouseUp (const juce::MouseEvent&)
{
    if (draggedHandle < 0)
        return;

    const auto& handle = handles[(size_t) draggedHandle];
    for (auto* parameter : { xOf (handle), handle.yParam })
        if (parameter != nullptr)
            parameter->endChangeGesture();

    draggedHandle = -1;
    repaint();
}

void EnvelopeView::mouseDoubleClick (const juce::MouseEvent& event)
{
    // Dubbelklick på ett handtag återställer dess parametrar.
    const auto index = handleAt (event.position);
    if (index < 0)
        return;

    const auto& handle = handles[(size_t) index];
    for (auto* parameter : { xOf (handle), handle.yParam })
    {
        if (parameter != nullptr)
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    }
}
