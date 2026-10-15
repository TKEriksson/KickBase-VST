#include "KickDisplay.h"

namespace
{
    constexpr float grabDistance = 9.0f;

    juce::String noteAndHz (double hz)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const auto midi = (int) std::round (69.0 + 12.0 * std::log2 (hz / 440.0));
        const auto octave = midi / 12 - 1; // C4 = 60 (som mittens C i de flesta program)
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (octave) + "  "
             + (hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz");
    }
}

KickDisplay::KickDisplay (PluginBase& p) : processor (p)
{
    pitchButton.setClickingTogglesState (false);
    ampButton.setClickingTogglesState (false);
    pitchButton.setColour (juce::TextButton::buttonOnColourId, Theme::text.withAlpha (0.6f));
    ampButton.setColour (juce::TextButton::buttonOnColourId, Theme::kick);
    pitchButton.setTooltip ("PITCH - Edit the kick's pitch sweep: how far above the tail note it starts and how it falls.");
    ampButton.setTooltip ("AMP - Edit the kick's volume over time. The last node decides how long the kick is.");
    pitchButton.onClick = [this] { setMode (false); };
    ampButton.onClick = [this] { setMode (true); };
    addAndMakeVisible (pitchButton);
    addAndMakeVisible (ampButton);
    setMode (false);

    setTooltip ("KICK - The waveform of the kick. Drag the nodes to shape pitch (PITCH) or volume (AMP); double-click to add a node, "
                "right-click a node to remove it, drag the small diamond on a segment to bend it.");
    lastHash = stateHash();
    renderWaveform();
    startTimerHz (20);
}

void KickDisplay::setMode (bool showAmp)
{
    amp = showAmp;
    pitchButton.setToggleState (! amp, juce::dontSendNotification);
    ampButton.setToggleState (amp, juce::dontSendNotification);
    editing.clear();
    dragged = draggedTension = hovered = -1;
    repaint();
}

void KickDisplay::resized()
{
    auto top = getLocalBounds().reduced (8, 6).removeFromTop (22);
    pitchButton.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (4);
    ampButton.setBounds (top.removeFromLeft (64));
}

juce::int64 KickDisplay::stateHash() const
{
    // Allt som påverkar kickens ljud: kickens parametrar, envelopernas version och klicksamplingen.
    juce::int64 hash = processor.kickPitchEnv.version.load() * 31 + processor.kickAmpEnv.version.load();
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter); withId != nullptr && withId->paramID.startsWith ("kick"))
        {
            const auto value = parameter->getValue();
            juce::int64 bits = 0;
            std::memcpy (&bits, &value, sizeof (value));
            hash = hash * 1000003 + bits;
        }
    return hash * 31 + processor.getClickSampleName().hashCode64();
}

void KickDisplay::renderWaveform()
{
    const auto params = processor.kickForDisplay (0);
    waveform = KickEngine::render (params, processor.clickForDisplay(), 44100.0);
    waveformSeconds = waveform.getNumSamples() / 44100.0;
}

void KickDisplay::timerCallback()
{
    const auto hash = stateHash();
    if (hash != lastHash)
    {
        lastHash = hash;
        renderWaveform();
        repaint();
    }
}

float KickDisplay::lengthScale() const
{
    return std::max (0.05f, processor.kickForDisplay (0).lengthScale);
}

float KickDisplay::viewSeconds() const
{
    if (fixedView > 0.0f)
        return fixedView;
    const auto length = processor.kickAmpEnv.endSeconds() * lengthScale();
    return juce::jlimit (0.06f, 3.0f, length * 1.12f);
}

juce::Rectangle<float> KickDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 12.0f).withTrimmedTop (22.0f);
}

juce::Point<float> KickDisplay::toScreen (float seconds, float value) const
{
    const auto area = plotArea();
    return { area.getX() + seconds * lengthScale() / viewSeconds() * area.getWidth(),
             area.getBottom() - juce::jlimit (0.0f, 1.0f, value / displayMax()) * area.getHeight() };
}

juce::Point<float> KickDisplay::fromScreen (juce::Point<float> p) const
{
    const auto area = plotArea();
    const auto seconds = (p.x - area.getX()) / area.getWidth() * viewSeconds() / lengthScale();
    const auto value = (area.getBottom() - p.y) / area.getHeight() * displayMax();
    return { seconds, value };
}

juce::Point<float> KickDisplay::tensionHandle (size_t segment) const
{
    const auto& points = editing.empty() ? active().getPoints() : editing;
    const auto& a = points[segment];
    const auto& b = points[segment + 1];
    const auto mid = NodeEnvelope::shape (0.5f, a.tension);
    return toScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * mid);
}

int KickDisplay::pointAt (juce::Point<float> p) const
{
    const auto& points = active().getPoints();
    for (int i = 0; i < (int) points.size(); ++i)
        if (toScreen (points[(size_t) i].x, points[(size_t) i].y).getDistanceFrom (p) < grabDistance)
            return i;
    return -1;
}

int KickDisplay::tensionHandleAt (juce::Point<float> p) const
{
    const auto& points = active().getPoints();
    for (size_t i = 0; i + 1 < points.size(); ++i)
        if (tensionHandle (i).getDistanceFrom (p) < grabDistance)
            return (int) i;
    return -1;
}

juce::String KickDisplay::describe (const NodeEnvelope::Point& point) const
{
    const auto ms = point.x * lengthScale() * 1000.0f;
    const auto time = juce::String (ms, ms < 100.0f ? 1 : 0) + " ms";
    if (amp)
        return time + "   " + juce::String (juce::roundToInt (point.y * 100.0f)) + " %";

    const auto params = processor.kickForDisplay (0);
    const auto hz = params.tailHz * std::exp2 (point.y * params.pitchAmount / 12.0f);
    return time + "   +" + juce::String (point.y, 1) + " st   " + noteAndHz (hz);
}

void KickDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = plotArea();
    const auto colour = amp ? Theme::kick : Theme::text;

    // Tidsrutnät: 10, 20, 50, 100 ... ms.
    g.setFont (Theme::font (9.0f));
    for (const auto ms : { 5, 10, 20, 50, 100, 200, 500, 1000 })
    {
        const auto x = area.getX() + (float) ms * 0.001f / viewSeconds() * area.getWidth();
        if (x > area.getRight() - 24.0f)
            break;
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (Theme::textDim.withAlpha (0.7f));
        g.drawText (ms >= 1000 ? juce::String (ms / 1000) + " s" : juce::String (ms) + " ms",
                    juce::Rectangle<float> (x + 2.0f, area.getBottom() - 12.0f, 40.0f, 11.0f), juce::Justification::left);
    }
    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    // Själva kickens vågform (min/max per bildpunkt), runt mittlinjen.
    if (waveform.getNumSamples() > 1)
    {
        const auto* data = waveform.getReadPointer (0);
        const auto samplesPerSecond = 44100.0;
        const auto secondsPerPixel = viewSeconds() / area.getWidth();
        juce::Path wave;
        std::vector<juce::Point<float>> lower;
        for (int px = 0; px < (int) area.getWidth(); ++px)
        {
            const auto from = (int) (px * secondsPerPixel * samplesPerSecond);
            const auto to = std::min (waveform.getNumSamples(), (int) ((px + 1) * secondsPerPixel * samplesPerSecond) + 1);
            if (from >= waveform.getNumSamples())
                break;
            auto lo = 0.0f, hi = 0.0f;
            for (int i = from; i < to; ++i)
            {
                lo = std::min (lo, data[i]);
                hi = std::max (hi, data[i]);
            }
            const auto x = area.getX() + (float) px;
            const auto half = area.getHeight() * 0.5f * 0.92f;
            const juce::Point<float> top { x, area.getCentreY() - hi * half };
            if (px == 0)
                wave.startNewSubPath (top);
            else
                wave.lineTo (top);
            lower.push_back ({ x, area.getCentreY() - lo * half });
        }
        for (auto it = lower.rbegin(); it != lower.rend(); ++it)
            wave.lineTo (*it);
        wave.closeSubPath();
        g.setColour (Theme::bass.withAlpha (0.55f));
        g.fillPath (wave);
    }

    // Envelopens kurva.
    const auto& points = editing.empty() ? active().getPoints() : editing;
    NodeEnvelope preview (active().getMaxSeconds(), active().getMaxValue(), points);
    juce::Path curve;
    constexpr int resolution = 240;
    const auto endX = points.back().x;
    for (int i = 0; i <= resolution; ++i)
    {
        const auto t = endX * (float) i / resolution;
        const auto screen = toScreen (t, preview.evaluate (t));
        if (i == 0)
            curve.startNewSubPath (screen);
        else
            curve.lineTo (screen);
    }
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    // Noder och spänningshandtag.
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        juce::Path diamond;
        diamond.addPolygon (tensionHandle (i), 4, 4.0f, juce::MathConstants<float>::pi / 4.0f);
        g.setColour (colour.withAlpha ((int) i == draggedTension ? 1.0f : 0.5f));
        g.fillPath (diamond);
    }
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto screen = toScreen (points[(size_t) i].x, points[(size_t) i].y);
        const auto active = i == hovered || i == dragged;
        g.setColour (Theme::knobBodyDark);
        g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (screen));
        g.setColour (colour);
        g.drawEllipse (juce::Rectangle<float> (active ? 13.0f : 11.0f, active ? 13.0f : 11.0f).withCentre (screen), 2.0f);

        // I PITCH-läget märks varje nod med ton och Hz, som i Kick 3.
        if (! amp && (active || points.size() <= 6))
        {
            g.setFont (Theme::font (9.5f, true));
            g.setColour (Theme::text.withAlpha (active ? 1.0f : 0.75f));
            const auto params = processor.kickForDisplay (0);
            const auto hz = params.tailHz * std::exp2 (points[(size_t) i].y * params.pitchAmount / 12.0f);
            g.drawText (noteAndHz (hz), juce::Rectangle<float> (screen.x - 5.0f, screen.y - 20.0f, 110.0f, 12.0f), juce::Justification::left);
        }
    }

    // Läsning av den nod som hålls eller dras.
    const auto shown = dragged >= 0 ? dragged : hovered;
    if (shown >= 0 && shown < (int) points.size())
    {
        g.setFont (Theme::font (11.0f, true));
        g.setColour (Theme::text);
        g.drawText (describe (points[(size_t) shown]), getLocalBounds().reduced (10, 6).removeFromTop (22).withTrimmedLeft (140),
                    juce::Justification::centredLeft);
    }
}

void KickDisplay::commit (std::vector<NodeEnvelope::Point> points)
{
    active().setPoints (std::move (points));
    processor.markStateChanged();
    repaint();
}

void KickDisplay::mouseMove (const juce::MouseEvent& event)
{
    const auto index = pointAt (event.position);
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void KickDisplay::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void KickDisplay::mouseDown (const juce::MouseEvent& event)
{
    auto points = active().getPoints();
    const auto point = pointAt (event.position);

    // Högerklick tar bort en nod (utom första och sista).
    if (event.mods.isPopupMenu())
    {
        if (point > 0 && point < (int) points.size() - 1)
        {
            points.erase (points.begin() + point);
            commit (std::move (points));
        }
        return;
    }

    dragged = point;
    draggedTension = point < 0 ? tensionHandleAt (event.position) : -1;
    if (draggedTension >= 0)
    {
        tensionStart = points[(size_t) draggedTension].tension;
        dragStartY = event.position.y;
    }
    editing = points;
    fixedView = viewSeconds();
}

void KickDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (editing.empty())
        return;

    if (dragged >= 0)
    {
        const auto value = fromScreen (event.position);
        auto& point = editing[(size_t) dragged];
        const auto last = (int) editing.size() - 1;

        if (dragged > 0)
        {
            // Noden får inte passera grannarna; den sista kan dras fritt framåt (längden).
            const auto upper = dragged < last ? editing[(size_t) dragged + 1].x : active().getMaxSeconds();
            point.x = juce::jlimit (editing[(size_t) dragged - 1].x + 0.0002f, upper, value.x);
        }
        point.y = juce::jlimit (0.0f, active().getMaxValue(), value.y);
        if (amp && dragged == last)
            point.y = 0.0f; // volymen slutar på noll, annars knäpper det
        repaint();
    }
    else if (draggedTension >= 0)
    {
        const auto& a = editing[(size_t) draggedTension];
        const auto& b = editing[(size_t) draggedTension + 1];
        const auto direction = b.y >= a.y ? 1.0f : -1.0f;
        editing[(size_t) draggedTension].tension =
            juce::jlimit (-1.0f, 1.0f, tensionStart + direction * (event.position.y - dragStartY) / 80.0f);
        repaint();
    }
}

void KickDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! editing.empty() && (dragged >= 0 || draggedTension >= 0))
        commit (editing);

    editing.clear();
    dragged = draggedTension = -1;
    fixedView = 0.0f;
    repaint();
}

void KickDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (pointAt (event.position) >= 0 || ! plotArea().contains (event.position))
        return;

    auto points = active().getPoints();
    if ((int) points.size() >= NodeEnvelope::maxPoints)
        return;

    const auto value = fromScreen (event.position);
    if (value.x <= 0.0f || value.x >= points.back().x)
        return;
    points.push_back ({ value.x, juce::jlimit (0.0f, active().getMaxValue(), value.y), 0.0f });
    commit (std::move (points));
}
