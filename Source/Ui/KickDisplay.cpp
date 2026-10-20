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
    listenButton.setAccent (Theme::kick);
    listenButton.setTooltip ("LISTEN - Plays the kick (you can also click an empty spot in the kick's picture). Use it while you shape PITCH and AMP.");
    listenButton.onStart = [this] { processor.requestAudition (0); };
    addAndMakeVisible (listenButton);
    setMode (false);

    setTooltip ("KICK - The waveform of the kick: click an empty spot to hear it, and watch the marker follow it when it plays. Drag the nodes to shape pitch (PITCH) or volume (AMP); double-click to add a node, "
                "right-click a node to remove it, drag the small diamond on a segment to bend it.");
    lastHash = stateHash();
    renderWaveform();
    startVisualTimer (Settings::Visual::other, 30);
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
    top.removeFromLeft (8);
    listenButton.setBounds (top.removeFromLeft (36));
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
    followVisualRate();
    // Spelmarkören: var kicken är nu, och en blixt när en ny kick slås an (från DAW, mönstret eller ett klick).
    const auto playTime = processor.transportStatus.kickPlayTime.load();
    const auto triggers = processor.transportStatus.kickTriggers.load();
    if (triggers != lastTriggerCount)
    {
        lastTriggerCount = triggers;
        triggerWallTime = juce::Time::getMillisecondCounterHiRes();
    }
    const auto flashing = juce::Time::getMillisecondCounterHiRes() - triggerWallTime < 600.0;
    if (playTime != markerSeconds || flashing)
    {
        markerSeconds = playTime;
        repaint();
    }

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
    return juce::jlimit (0.06f, 3.0f, length * 1.12f) / timeZoom;
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

float KickDisplay::hzAtValue (float value) const
{
    const auto params = processor.kickForDisplay (0);
    return params.tailHz * std::exp2 (value * params.pitchAmount / 12.0f);
}

float KickDisplay::valueAtHz (float hz) const
{
    const auto params = processor.kickForDisplay (0);
    return params.pitchAmount > 0.0f ? 12.0f * std::log2 (hz / params.tailHz) / params.pitchAmount : 0.0f;
}

float KickDisplay::secondsAtHz (float hz) const
{
    const auto& env = processor.kickPitchEnv;
    const auto end = env.endSeconds();
    constexpr int steps = 2000;
    for (int i = 0; i <= steps; ++i)
    {
        const auto t = end * (float) i / steps;
        if (hzAtValue (env.evaluate (t)) <= hz)
            return t * lengthScale();
    }
    return -1.0f;
}

float KickDisplay::kickSeconds() const
{
    return processor.kickAmpEnv.endSeconds() * lengthScale();
}

float KickDisplay::xOfSeconds (float seconds) const
{
    const auto area = plotArea();
    return area.getX() + seconds / viewSeconds() * area.getWidth();
}

// Verktyg ovanpå grafen: 1/16- och 1/8-markörer ur BPM, svansens slutfrekvens, 35-43 Hz-zonen och den följda frekvensen.
void KickDisplay::paintTools (juce::Graphics& g)
{
    const auto area = plotArea();
    const auto bpm = processor.transportStatus.bpm.load();
    const auto amber = juce::Colour (0xffffb43c);
    g.setFont (Theme::font (9.0f, true));

    const auto verticalLine = [&] (float seconds, juce::Colour c, const juce::String& label, bool dashed)
    {
        const auto x = xOfSeconds (seconds);
        if (x < area.getX() || x > area.getRight())
            return;
        g.setColour (c);
        if (dashed)
        {
            const float dash[] { 4.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (x, area.getY(), x, area.getBottom()), dash, 2, 1.0f);
        }
        else
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.drawText (label, juce::Rectangle<float> (x + 2.0f, area.getY(), 90.0f, 11.0f), juce::Justification::left);
    };

    if (bpm > 20.0)
    {
        const auto sixteenth = (float) (60.0 / bpm / 4.0);
        verticalLine (sixteenth, Theme::textDim.withAlpha (0.8f), "1/16", true);
        const auto tooLong = amp && kickSeconds() > sixteenth * 2.0f * 1.02f;
        verticalLine (sixteenth * 2.0f, tooLong ? amber : Theme::textDim.withAlpha (0.8f),
                      amp ? "1/8 = end of 2nd 16th  " + juce::String (juce::roundToInt (sixteenth * 2000.0f)) + " ms" : juce::String ("1/8"), true);
        if (tooLong)
        {
            g.setColour (amber.withAlpha (0.9f));
            g.drawText ("tail runs " + juce::String (juce::roundToInt ((kickSeconds() - sixteenth * 2.0f) * 1000.0f)) + " ms past the 2nd 16th",
                        area.withTrimmedTop (area.getHeight() - 26.0f).removeFromTop (12.0f), juce::Justification::centredRight);
        }
    }

    if (! amp)
    {
        // Svansens slutfrekvens (grön i 35-43 Hz-zonen, annars gul) och zonen själv.
        const auto tailHz = hzAtValue (0.0f);
        const auto zoneTop = toScreen (0.0f, valueAtHz (43.0f)).y, zoneBottom = toScreen (0.0f, valueAtHz (35.0f)).y;
        if (zoneBottom - zoneTop >= 1.0f)
        {
            g.setColour (juce::Colours::limegreen.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (area.getX(), zoneTop, area.getWidth(), zoneBottom - zoneTop));
            g.setColour (juce::Colours::limegreen.withAlpha (0.7f));
            g.drawText ("35-43 Hz", juce::Rectangle<float> (area.getX() + 2.0f, zoneTop - 11.0f, 60.0f, 11.0f), juce::Justification::left);
        }
        g.setColour ((tailHz >= 35.0f && tailHz <= 43.0f ? juce::Colours::limegreen : amber).withAlpha (0.9f));
        g.drawText ("tail ends at " + noteAndHz (tailHz), juce::Rectangle<float> (area.getRight() - 170.0f, toScreen (0.0f, 0.0f).y - 13.0f, 168.0f, 11.0f),
                    juce::Justification::right);
    }

    // Följd frekvens: Ctrl + klick i PITCH; samma tidpunkt markeras i båda graferna (amp-kurvan som "omvänd EQ").
    if (markedHz > 0.0f)
    {
        const auto t = secondsAtHz (markedHz);
        const auto label = noteAndHz (markedHz) + (t >= 0.0f ? "  @ " + juce::String (t * 1000.0f, t < 0.1f ? 1 : 0) + " ms" : juce::String ("  never reached"));
        const auto c = juce::Colour (0xff5ad2ff);
        if (! amp)
        {
            const auto y = toScreen (0.0f, valueAtHz (markedHz)).y;
            g.setColour (c.withAlpha (0.7f));
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        }
        if (t >= 0.0f)
            verticalLine (t, c.withAlpha (0.9f), label, false);
        else
        {
            g.setColour (c);
            g.drawText (label, area.withHeight (11.0f), juce::Justification::centred);
        }
    }
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
        g.setColour (Theme::bass.withAlpha (markerSeconds >= 0.0f ? 0.32f : 0.55f));
        g.fillPath (wave);

        // Det som redan spelats lyses upp bakom markören: ljudvågen "fylls i" medan kicken klingar.
        if (markerSeconds >= 0.0f)
        {
            const auto markerX = area.getX() + markerSeconds / viewSeconds() * area.getWidth();
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (area.withRight (std::min (area.getRight(), markerX)).toNearestInt());
            g.setColour (Theme::bass.brighter (0.5f).withAlpha (0.95f));
            g.fillPath (wave);
        }
    }

    paintTools (g);

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
        if (! amp && (active || points.size() <= 12))
        {
            g.setFont (Theme::font (9.5f, true));
            g.setColour (Theme::text.withAlpha (active ? 1.0f : 0.75f));
            const auto params = processor.kickForDisplay (0);
            const auto hz = params.tailHz * std::exp2 (points[(size_t) i].y * params.pitchAmount / 12.0f);
            g.drawText (noteAndHz (hz), juce::Rectangle<float> (screen.x - 5.0f, screen.y - 20.0f, 110.0f, 12.0f), juce::Justification::left);
        }
    }

    // Spelmarkören: ett lysande streck där kicken är just nu, och en kort blixt över hela rutan när den slås an.
    const auto sinceTrigger = juce::Time::getMillisecondCounterHiRes() - triggerWallTime;
    if (sinceTrigger < 600.0)
    {
        g.setColour (Theme::kick.withAlpha (0.16f * (float) std::exp (-sinceTrigger / 140.0)));
        g.fillRoundedRectangle (bounds, 6.0f);
    }
    if (markerSeconds >= 0.0f)
    {
        const auto markerX = area.getX() + markerSeconds / viewSeconds() * area.getWidth();
        if (markerX <= area.getRight())
        {
            g.setColour (Theme::kick.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (markerX - 3.0f, area.getY(), 6.0f, area.getHeight()));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (markerX - 0.75f, area.getY(), 1.5f, area.getHeight()));
        }
    }

    // Glöd när musen är över rutan.
    if (hoverDisplay)
    {
        g.setColour (Theme::kick.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 4.0f);
        g.setColour (Theme::kick.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 1.5f);
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

    // Läsning av musens plats: tid och envelopens värde (nivå eller ton och Hz), uppe till höger.
    if (cursorSeconds >= 0.0f)
    {
        const auto ms = cursorSeconds * 1000.0f;
        const auto value = amp ? juce::String (juce::roundToInt (cursorValue * 100.0f)) + " %" : noteAndHz (hzAtValue (cursorValue));
        g.setFont (Theme::font (11.0f));
        g.setColour (Theme::textDim);
        g.drawText (juce::String (ms, ms < 100.0f ? 1 : 0) + " ms   " + value, getLocalBounds().reduced (10, 6).removeFromTop (22).removeFromRight (230),
                    juce::Justification::centredRight);
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
    if (plotArea().contains (event.position))
    {
        const auto at = fromScreen (event.position);
        cursorSeconds = at.x * lengthScale();
        cursorValue = active().evaluate (at.x);
        repaint();
    }
    else if (cursorSeconds >= 0.0f)
    {
        cursorSeconds = -1.0f;
        repaint();
    }
    const auto index = pointAt (event.position);
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }

    // Högtalare över tomma platser (klicka för att lyssna), vanlig pekare över noder och handtag.
    setMouseCursor (index < 0 && tensionHandleAt (event.position) < 0 ? speakerCursor : juce::MouseCursor::NormalCursor);
}

void KickDisplay::mouseEnter (const juce::MouseEvent&)
{
    hoverDisplay = true;
    repaint();
}

void KickDisplay::mouseExit (const juce::MouseEvent&)
{
    cursorSeconds = -1.0f;
    hovered = -1;
    hoverDisplay = false;
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

    // Ctrl + klick: i PITCH följs den frekvensen (samma tidpunkt markeras i PITCH och AMP), i AMP tas markeringen bort.
    if (event.mods.isCtrlDown() && point < 0 && plotArea().contains (event.position))
    {
        markedHz = amp ? 0.0f : hzAtValue (std::max (0.0f, fromScreen (event.position).y));
        repaint();
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

    // Ett klick på en tom plats provlyssnar kicken.
    if (point < 0 && draggedTension < 0)
        processor.requestAudition (0);
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

void KickDisplay::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Ctrl + hjul: zooma i tid. Skift + hjul: zooma i frekvens (PITCH). Annars som vanligt.
    const auto step = wheel.deltaY > 0.0f ? 1.25f : 0.8f;
    if (wheel.deltaY != 0.0f && event.mods.isCtrlDown())
        timeZoom = juce::jlimit (1.0f, 16.0f, timeZoom * step);
    else if (wheel.deltaY != 0.0f && event.mods.isShiftDown() && ! amp)
        freqZoom = juce::jlimit (1.0f, 8.0f, freqZoom * step);
    else
    {
        Component::mouseWheelMove (event, wheel);
        return;
    }
    repaint();
}
