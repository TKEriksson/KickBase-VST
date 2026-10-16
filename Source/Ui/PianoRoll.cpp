#include "PianoRoll.h"

namespace
{
    constexpr float keyWidth = 46.0f, rulerHeight = 16.0f, velocityHeight = 40.0f, edgeGrab = 6.0f;
    constexpr float maxPixelsPerTick = 3.0f;

    bool isBlackKey (int pitchClass)
    {
        return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
    }

    juce::String gridNameFor (int ticks)
    {
        for (int g = 0; g < Pattern::numGrids; ++g)
            if (Pattern::ticksPerStep ((BassGrid) g) == ticks)
                return Pattern::gridNames()[g];
        return juce::String (ticks) + " ticks";
    }
}

PianoRoll::PianoRoll (PluginBase& p, std::vector<juce::Colour> colours) : processor (p), layerColours (std::move (colours))
{
    setTooltip ("PIANO ROLL - Click in an empty spot to add a note (drag to move it), drag a note's right edge to change its length. "
                "Right-click a note to delete it, double-click it to turn SLIDE on/off (the little triangle: it glides into the next note). "
                "Scroll = pitch, Ctrl + scroll = zoom, Shift + scroll = move in time. The strip at the bottom is velocity. "
                "GRID (left) sets the snap: from 1/4 down to 1/64 and triplets.");
    startTimerHz (30);
}

void PianoRoll::setLayer (int newLayer)
{
    layer = juce::jlimit (0, Pattern::numLayers - 1, newLayer);
    repaint();
}

int PianoRoll::bars() const
{
    return Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
}

BassGrid PianoRoll::grid() const
{
    return (BassGrid) (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::grid))->load();
}

int PianoRoll::snapped (float tick) const
{
    const auto step = snapTicks();
    return juce::jlimit (0, totalTicks() - 1, (int) std::floor (tick / (float) step + 0.0001f) * step);
}

int PianoRoll::rootNote() const
{
    const auto key = processor.transportStatus.keyNote.load();
    return Params::bassRootMidiNote (key >= 0 ? key : 60);
}

int PianoRoll::rootPitchClass() const { return rootNote() % 12; }

float PianoRoll::ticksToX (float tick) const { return gridArea.getX() + (tick - scrollTicks) * pixelsPerTick; }
float PianoRoll::xToTicks (float x) const { return scrollTicks + (x - gridArea.getX()) / pixelsPerTick; }
float PianoRoll::pitchToY (int pitch) const { return gridArea.getY() + (topPitch - (float) pitch) * rowHeight(); }
int PianoRoll::yToPitch (float y) const { return (int) std::floor (topPitch - (y - gridArea.getY()) / rowHeight()); }

void PianoRoll::fitToWidth()
{
    pixelsPerTick = gridArea.getWidth() / (float) std::max (1, totalTicks());
    scrollTicks = 0.0f;
}

void PianoRoll::clampView()
{
    const auto fit = gridArea.getWidth() / (float) std::max (1, totalTicks());
    pixelsPerTick = juce::jlimit (fit, std::max (fit, maxPixelsPerTick), pixelsPerTick);
    const auto visibleTicks = gridArea.getWidth() / pixelsPerTick;
    scrollTicks = juce::jlimit (0.0f, std::max (0.0f, (float) totalTicks() - visibleTicks), scrollTicks);

    // Raderna: hela ±24-området ska gå att nå, men inte mer.
    const auto rows = (float) visibleRows();
    topPitch = juce::jlimit ((float) Pattern::minPitch + rows - 1.0f, (float) Pattern::maxPitch, topPitch);
    if (rows > (float) (Pattern::maxPitch - Pattern::minPitch + 1))
        topPitch = (float) Pattern::maxPitch;
}

void PianoRoll::resized()
{
    auto area = getLocalBounds().toFloat();
    keyArea = area.withWidth (keyWidth).withTrimmedTop (rulerHeight).withTrimmedBottom (velocityHeight + 4.0f);
    rulerArea = area.withTrimmedLeft (keyWidth).withHeight (rulerHeight);
    velocityArea = area.withTrimmedLeft (keyWidth).removeFromBottom (velocityHeight);
    gridArea = area.withTrimmedLeft (keyWidth).withTrimmedTop (rulerHeight).withTrimmedBottom (velocityHeight + 4.0f);

    const auto first = pixelsPerTick == 1.0f && scrollTicks == 0.0f;
    if (first)
    {
        fitToWidth();
        topPitch = (float) visibleRows() * 0.5f; // grundtonen i mitten
    }
    clampView();
}

void PianoRoll::timerCallback()
{
    if (! isShowing())
        return;

    auto changed = false;
    auto update = [&changed] (int& last, int now) { if (last != now) { last = now; changed = true; } };
    update (lastVersion, processor.patternVersion.load());
    update (lastPlayTick, processor.transportStatus.bassSteps[(size_t) layer].load() / 6); // var 6:e tick räcker
    update (lastBars, bars());
    update (lastGrid, (int) grid());
    if (changed)
    {
        clampView();
        repaint();
    }
}

juce::Rectangle<float> PianoRoll::noteBounds (int tick) const
{
    const auto& n = processor.pattern.note (layer, tick);
    const auto x0 = ticksToX ((float) tick);
    const auto x1 = ticksToX ((float) (tick + n.length.load()));
    return { x0, pitchToY (n.pitch.load()), std::max (3.0f, x1 - x0), rowHeight() };
}

int PianoRoll::noteAtPoint (juce::Point<float> p, bool& onRightEdge) const
{
    onRightEdge = false;
    // Bakifrån, så att en ton som ligger överst (senare start) vinner.
    const auto first = std::max (0, (int) xToTicks (gridArea.getX()) - Pattern::ticksPerBar * 2);
    const auto last = std::min (totalTicks() - 1, (int) xToTicks (gridArea.getRight()) + 1);
    for (int tick = last; tick >= first; --tick)
    {
        if (! processor.pattern.note (layer, tick).on.load())
            continue;
        const auto bounds = noteBounds (tick);
        if (bounds.contains (p))
        {
            onRightEdge = p.x > bounds.getRight() - edgeGrab && bounds.getWidth() > edgeGrab * 1.5f;
            return tick;
        }
    }
    return -1;
}

int PianoRoll::noteForVelocity (float x) const
{
    auto best = -1;
    auto bestDistance = 7.0f;
    for (int tick = 0; tick < totalTicks(); ++tick)
    {
        if (! processor.pattern.note (layer, tick).on.load())
            continue;
        const auto distance = std::abs (ticksToX ((float) tick) + 1.5f - x);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = tick;
        }
    }
    return best;
}

juce::String PianoRoll::describe (int tick) const
{
    const auto& n = processor.pattern.note (layer, tick);
    const auto pitch = n.pitch.load();
    const auto beat = tick / Pattern::ticksPerQuarter;
    juce::String text = juce::MidiMessage::getMidiNoteName (rootNote() + pitch, true, true, 4)
                      + "  (" + (pitch > 0 ? "+" : "") + juce::String (pitch) + ")  "
                      + "bar " + juce::String (beat / 4 + 1) + "." + juce::String (beat % 4 + 1) + "  "
                      + "length " + gridNameFor (n.length.load()) + "  "
                      + "vel " + juce::String (juce::roundToInt (n.velocity.load() * 100.0f)) + " %";
    if (n.slide.load())
        text << "  SLIDE";
    return text;
}

void PianoRoll::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = layerColours[(size_t) juce::jlimit (0, (int) layerColours.size() - 1, layer)];

    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    // Raderna: svarta tangenter mörkare, grundtonen markerad.
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (gridArea.toNearestInt());
        const auto rootPc = rootPitchClass();
        for (int row = 0; row <= visibleRows() + 1; ++row)
        {
            const auto pitch = (int) std::floor (topPitch) - row;
            if (pitch < Pattern::minPitch || pitch > Pattern::maxPitch)
                continue;
            const auto y = pitchToY (pitch);
            const auto pc = ((rootPc + pitch) % 12 + 12) % 12;
            g.setColour (isBlackKey (pc) ? juce::Colours::black.withAlpha (0.28f) : juce::Colours::white.withAlpha (0.03f));
            g.fillRect (juce::Rectangle<float> (gridArea.getX(), y, gridArea.getWidth(), rowHeight()));
            if (pitch == 0)
            {
                g.setColour (colour.withAlpha (0.14f));
                g.fillRect (juce::Rectangle<float> (gridArea.getX(), y, gridArea.getWidth(), rowHeight()));
            }
            g.setColour (Theme::grid.withAlpha (0.7f));
            g.drawHorizontalLine ((int) y, gridArea.getX(), gridArea.getRight());
        }

        // Tidslinjer: snap, slag och takter.
        const auto step = snapTicks();
        const auto firstTick = std::max (0, (int) xToTicks (gridArea.getX()) / step * step);
        for (int tick = firstTick; tick <= totalTicks() && ticksToX ((float) tick) <= gridArea.getRight(); tick += step)
        {
            const auto x = ticksToX ((float) tick);
            const auto isBar = tick % Pattern::ticksPerBar == 0, isBeat = tick % Pattern::ticksPerQuarter == 0;
            if (! isBar && ! isBeat && step * pixelsPerTick < 4.0f)
                continue;
            g.setColour (isBar ? Theme::textDim.withAlpha (0.55f) : isBeat ? Theme::textDim.withAlpha (0.28f) : Theme::grid.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, gridArea.getY(), gridArea.getBottom());
        }

        // Tonerna.
        const auto first = std::max (0, (int) xToTicks (gridArea.getX()) - Pattern::ticksPerBar * 2);
        const auto last = std::min (totalTicks() - 1, (int) xToTicks (gridArea.getRight()) + 1);
        for (int tick = first; tick <= last; ++tick)
        {
            const auto& n = processor.pattern.note (layer, tick);
            if (! n.on.load())
                continue;

            const auto b = noteBounds (tick).reduced (0.0f, 0.5f);
            const auto active = tick == hoverTick || tick == dragTick;
            const auto c = colour.withMultipliedBrightness (0.55f + 0.45f * n.velocity.load());
            g.setGradientFill (juce::ColourGradient (c.brighter (0.35f), b.getX(), b.getY(), c.darker (0.2f), b.getX(), b.getBottom(), false));
            g.fillRoundedRectangle (b, 2.5f);
            g.setColour (active ? juce::Colours::white : c.brighter (0.5f));
            g.drawRoundedRectangle (b, 2.5f, active ? 1.6f : 1.0f);

            if (b.getWidth() > 24.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.setFont (Theme::font (9.0f, true));
                g.drawText (juce::MidiMessage::getMidiNoteName (rootNote() + n.pitch.load(), true, true, 4),
                            b.reduced (3.0f, 0.0f), juce::Justification::centredLeft);
            }

            if (n.slide.load())
            {
                // Triangeln i hörnet, som i FL Studio.
                juce::Path triangle;
                triangle.addTriangle (b.getRight() - 1.0f, b.getBottom() - 1.0f, b.getRight() - 1.0f, b.getBottom() - 8.0f, b.getRight() - 8.0f, b.getBottom() - 1.0f);
                g.setColour (juce::Colours::white);
                g.fillPath (triangle);
            }
        }

        // Uppspelningsmarkören.
        const auto playTick = processor.transportStatus.bassSteps[(size_t) layer].load();
        if (playTick >= 0)
        {
            g.setColour (juce::Colour (0xffffe27a));
            g.fillRect (juce::Rectangle<float> (ticksToX ((float) playTick) - 0.5f, gridArea.getY(), 1.5f, gridArea.getHeight()));
        }
    }

    // Pianotangenterna till vänster: tonnamn, grundtonen markerad.
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (keyArea.toNearestInt());
        const auto rootPc = rootPitchClass();
        g.setFont (Theme::font (9.0f, true));
        for (int row = 0; row <= visibleRows() + 1; ++row)
        {
            const auto pitch = (int) std::floor (topPitch) - row;
            if (pitch < Pattern::minPitch || pitch > Pattern::maxPitch)
                continue;
            const auto y = pitchToY (pitch);
            const auto black = isBlackKey (((rootPc + pitch) % 12 + 12) % 12);
            const auto key = juce::Rectangle<float> (keyArea.getX(), y, keyArea.getWidth() - 2.0f, rowHeight()).reduced (0.0f, 0.5f);
            g.setColour (black ? juce::Colour (0xff23263a) : juce::Colour (0xffc4c8de));
            g.fillRoundedRectangle (key, 2.0f);
            if (pitch == 0)
            {
                g.setColour (colour);
                g.fillRoundedRectangle (key.withWidth (4.0f), 2.0f);
            }
            g.setColour (black ? Theme::textDim : juce::Colour (0xff23263a));
            g.drawText (juce::MidiMessage::getMidiNoteName (rootNote() + pitch, true, true, 4), key.reduced (7.0f, 0.0f), juce::Justification::centredLeft);
            g.setColour (black ? Theme::textDim.withAlpha (0.7f) : juce::Colour (0xff5b607c));
            g.drawText ((pitch > 0 ? "+" : "") + juce::String (pitch), key.reduced (3.0f, 0.0f), juce::Justification::centredRight);
        }
    }

    // Linjalen: takt.slag och ett fält som visar vad musen är över.
    g.setFont (Theme::font (9.5f, true));
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (rulerArea.toNearestInt());
        for (int beat = 0; beat < bars() * 4; ++beat)
        {
            const auto x = ticksToX ((float) (beat * Pattern::ticksPerQuarter));
            if (x < rulerArea.getX() - 20.0f || x > rulerArea.getRight())
                continue;
            g.setColour (beat % 4 == 0 ? Theme::text : Theme::textDim);
            g.drawText (beat % 4 == 0 ? juce::String (beat / 4 + 1) : juce::String (beat / 4 + 1) + "." + juce::String (beat % 4 + 1),
                        juce::Rectangle<float> (x + 3.0f, rulerArea.getY(), 40.0f, rulerArea.getHeight()), juce::Justification::centredLeft);
        }
    }
    g.setColour (Theme::text);
    g.drawText (hoverTick >= 0 ? describe (hoverTick) : juce::String(), rulerArea.withTrimmedLeft (rulerArea.getWidth() * 0.35f).reduced (6.0f, 0.0f),
                juce::Justification::centredRight);
    g.setColour (colour);
    g.drawText (gridNameFor (snapTicks()), juce::Rectangle<float> (4.0f, 0.0f, keyWidth - 6.0f, rulerHeight), juce::Justification::centredLeft);

    // Velocity.
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (velocityArea);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (9.0f, true));
    g.drawText ("VEL", juce::Rectangle<float> (4.0f, velocityArea.getY(), keyWidth - 6.0f, velocityArea.getHeight()), juce::Justification::centredLeft);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (velocityArea.toNearestInt());
        for (int tick = 0; tick < totalTicks(); ++tick)
        {
            const auto& n = processor.pattern.note (layer, tick);
            const auto x = ticksToX ((float) tick);
            if (! n.on.load() || x < velocityArea.getX() - 4.0f || x > velocityArea.getRight())
                continue;
            const auto height = n.velocity.load() * (velocityArea.getHeight() - 6.0f);
            g.setColour (colour.withAlpha (tick == dragTick && drag == Drag::velocity ? 1.0f : 0.85f));
            g.fillRect (juce::Rectangle<float> (x, velocityArea.getBottom() - 3.0f - height, 3.0f, height));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x + 1.5f, velocityArea.getBottom() - 3.0f - height }));
        }
    }
}

void PianoRoll::setHover (int tick)
{
    if (tick != hoverTick)
    {
        hoverTick = tick;
        repaint();
    }
}

void PianoRoll::removeAt (int tick)
{
    processor.pattern.removeNote (layer, tick);
    edited = true;
}

void PianoRoll::mouseMove (const juce::MouseEvent& event)
{
    bool edge = false;
    const auto tick = gridArea.contains (event.position) ? noteAtPoint (event.position, edge) : -1;
    hoverEdge = edge;
    setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    setHover (tick);
}

void PianoRoll::mouseExit (const juce::MouseEvent&) { setHover (-1); }

void PianoRoll::mouseDown (const juce::MouseEvent& event)
{
    edited = false;
    drag = Drag::none;
    dragTick = -1;

    if (velocityArea.contains (event.position))
    {
        const auto tick = noteForVelocity (event.position.x);
        if (tick >= 0)
        {
            drag = Drag::velocity;
            dragTick = tick;
            mouseDrag (event);
        }
        return;
    }
    if (! gridArea.contains (event.position))
        return;

    bool edge = false;
    const auto tick = noteAtPoint (event.position, edge);

    if (event.mods.isPopupMenu())
    {
        if (tick >= 0)
        {
            removeAt (tick);
            processor.patternChanged();
            repaint();
        }
        return;
    }

    if (tick >= 0)
    {
        if (tick != justCreatedTick)
            justCreatedTick = -1;
        drag = edge ? Drag::resize : Drag::move;
        dragTick = tick;
        grabOffsetTicks = xToTicks (event.position.x) - (float) tick;
        return;
    }

    // Tom plats: en ny ton på steget under musen, i raden under musen.
    const auto start = snapped (xToTicks (event.position.x));
    const auto pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, yToPitch (event.position.y));
    const auto length = lastLength > 0 ? lastLength : juce::roundToInt (Pattern::defaultGate * (float) snapTicks());
    processor.pattern.addNote (layer, start, length, pitch);
    justCreatedTick = start;
    drag = Drag::move;
    dragTick = start;
    grabOffsetTicks = 0.0f;
    edited = true;
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& event)
{
    if (drag == Drag::none || dragTick < 0)
        return;

    auto& pattern = processor.pattern;

    if (drag == Drag::velocity)
    {
        pattern.note (layer, dragTick).velocity = juce::jlimit (0.02f, 1.0f, 1.0f - (event.position.y - velocityArea.getY() - 3.0f) / (velocityArea.getHeight() - 6.0f));
        edited = true;
        repaint();
        return;
    }

    if (drag == Drag::resize)
    {
        const auto step = snapTicks();
        const auto end = (int) std::round (xToTicks (event.position.x) / (float) step) * step;
        const auto length = juce::jlimit (step, pattern.maxTicks - dragTick, end - dragTick);
        pattern.note (layer, dragTick).length = length;
        lastLength = length;
        edited = true;
        repaint();
        return;
    }

    // Flytta: ny starttid (snap) och ny rad.
    const auto newTick = snapped (xToTicks (event.position.x) - grabOffsetTicks);
    const auto newPitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, yToPitch (event.position.y));
    auto& current = pattern.note (layer, dragTick);

    if (newTick != dragTick)
    {
        // En ton per tick: flyttar man över en annan ersätter man den.
        const auto length = current.length.load();
        const auto velocity = current.velocity.load();
        const auto slide = current.slide.load();
        pattern.removeNote (layer, dragTick);
        pattern.addNote (layer, newTick, length, newPitch, velocity, slide);
        dragTick = newTick;
        edited = true;
    }
    else if (newPitch != current.pitch.load())
    {
        current.pitch = newPitch;
        edited = true;
    }
    lastLength = pattern.note (layer, dragTick).length.load();
    hoverTick = dragTick;
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (edited)
        processor.patternChanged();
    drag = Drag::none;
    dragTick = -1;
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& event)
{
    bool edge = false;
    if (! gridArea.contains (event.position) || event.mods.isPopupMenu())
        return;
    const auto tick = noteAtPoint (event.position, edge);
    if (tick < 0)
        return;

    // Ett dubbelklick på en plats där första klicket just skapade tonen ska inte sätta slide på den.
    if (tick == justCreatedTick)
    {
        justCreatedTick = -1;
        return;
    }

    // Det första klicket i dubbelklicket kan ha flyttat tonen: toggla på den ton som ligger under musen nu.
    auto& note = processor.pattern.note (layer, tick);
    note.slide = ! note.slide.load();
    processor.patternChanged();
    repaint();
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY == 0.0f && wheel.deltaX == 0.0f)
        return;

    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        // Zooma kring musen: tiden under musen ligger kvar.
        const auto anchorTick = xToTicks (event.position.x);
        pixelsPerTick *= wheel.deltaY > 0.0f ? 1.2f : 1.0f / 1.2f;
        clampView();
        scrollTicks = anchorTick - (event.position.x - gridArea.getX()) / pixelsPerTick;
    }
    else if (event.mods.isShiftDown() || wheel.deltaX != 0.0f)
    {
        const auto delta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        scrollTicks -= delta * 240.0f / std::max (0.2f, pixelsPerTick) * 0.25f;
    }
    else
    {
        topPitch += wheel.deltaY > 0.0f ? 2.0f : -2.0f;
    }
    clampView();
    repaint();
}
