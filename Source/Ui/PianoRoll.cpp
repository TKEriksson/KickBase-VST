#include "PianoRoll.h"

namespace
{
    constexpr float keyWidth = 46.0f, toolbarHeight = 22.0f, rulerHeight = 16.0f, velocityHeight = 40.0f, edgeGrab = 6.0f, barSize = 11.0f;
    constexpr float maxPixelsPerTick = 3.0f;

    // jlimit kräver lägsta <= högsta; här kan intervallet vara tomt (då vinner det lägsta).
    int clampSafe (int value, int lowest, int highest) { return std::max (lowest, std::min (highest, value)); }

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
    setWantsKeyboardFocus (true);
    setTooltip ("PIANO ROLL - Click in an empty spot to add a note (drag to move it), drag a note's right edge to change its length. "
                "Shift + click selects several notes (or Shift + drag a box, or Ctrl + A for all): selected notes move, stretch and delete together. "
                "Right-click a note to delete it. With GLIDE on, the notes you draw are glide notes (pink): they glide the sounding note to their pitch "
                "during their own length, then the pitch stays. Double-click a note to turn glide on/off for it. "
                "Scroll = pitch, Ctrl + scroll = zoom, Shift + scroll = move in time. The strip at the bottom is velocity.");

    glideButton.setClickingTogglesState (true);
    glideButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
    glideButton.setTooltip ("GLIDE - On: the notes you draw are glide notes. A glide note does not play a new note: it glides the note that is "
                            "sounding to its own pitch during its own length (draw it shorter for a quick slide), and the pitch stays there "
                            "until the long note ends. Like slide notes in FL Studio.");

    snapButton.setClickingTogglesState (true);
    snapButton.setToggleState (true, juce::dontSendNotification);
    snapButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
    snapButton.setTooltip ("SNAP - On: notes snap to the grid next to it. Off: place and resize notes freely, tick by tick.");
    snapButton.onClick = [this] { snapEnabled = snapButton.getToggleState(); repaint(); };

    gridBox.addItemList (Pattern::gridNames(), 1);
    gridBox.setTooltip ("GRID - The grid the notes snap to, from 1/4 down to 1/64 and triplets. It is the same GRID as in the step view.");
    smallerButton.setTooltip ("Lower rows (see more notes at once).");
    biggerButton.setTooltip ("Taller rows.");
    smallerButton.onClick = [this] { changeRowHeight (-2.0f); };
    biggerButton.onClick = [this] { changeRowHeight (2.0f); };
    allButton.setTooltip ("SELECT ALL - Selects every note of this layer (Ctrl + A).");
    allButton.onClick = [this] { selectAll(); };
    deleteButton.setTooltip ("DELETE - Deletes the selected notes (Delete key).");
    deleteButton.onClick = [this] { deleteSelected(); };

    for (auto* c : std::initializer_list<juce::Component*> { &snapButton, &gridBox, &smallerButton, &biggerButton, &allButton, &deleteButton, &glideButton, &hBar, &vBar })
        addAndMakeVisible (c);
    hBar.addListener (this);
    vBar.addListener (this);
    hBar.setAutoHide (false);
    vBar.setAutoHide (false);
    hBar.setSingleStepSize (24.0);
    vBar.setSingleStepSize (1.0);

    attachGridBox();
    startTimerHz (30);
}

void PianoRoll::attachGridBox()
{
    gridAttachment.reset();
    gridAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, ParamID::bass (layer, BassField::grid), gridBox);
}

void PianoRoll::setLayer (int newLayer)
{
    newLayer = juce::jlimit (0, Pattern::numLayers - 1, newLayer);
    if (newLayer != layer)
    {
        layer = newLayer;
        selection.clear();
        attachGridBox();
    }
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

int PianoRoll::snapTicks() const { return snapEnabled ? Pattern::ticksPerStep (grid()) : 1; }

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

    updateScrollBars();
}

void PianoRoll::updateScrollBars()
{
    hBar.setRangeLimits (0.0, (double) std::max (1, totalTicks()), juce::dontSendNotification);
    hBar.setCurrentRange (scrollTicks, (double) (gridArea.getWidth() / std::max (0.01f, pixelsPerTick)), juce::dontSendNotification);

    vBar.setRangeLimits (0.0, (double) (Pattern::maxPitch - Pattern::minPitch + 1), juce::dontSendNotification);
    vBar.setCurrentRange ((double) (Pattern::maxPitch - topPitch), (double) visibleRows(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &hBar)
        scrollTicks = (float) newRangeStart;
    else
        topPitch = (float) Pattern::maxPitch - (float) newRangeStart;
    clampView();
    repaint();
}

void PianoRoll::commit()
{
    processor.pattern.setCustom (layer, true); // lagret visas nu som pianorulle i stället för stegknappar
    processor.patternChanged();
}

int PianoRoll::pitchBefore (int tick) const
{
    for (int t = std::min (tick, Pattern::maxTicks) - 1; t >= 0; --t)
        if (processor.pattern.note (layer, t).on.load())
            return processor.pattern.note (layer, t).pitch.load();
    return processor.pattern.note (layer, tick).pitch.load();
}

void PianoRoll::changeRowHeight (float delta)
{
    const auto centrePitch = topPitch - (float) visibleRows() * 0.5f;
    rowH = juce::jlimit (7.0f, 30.0f, rowH + delta);
    topPitch = centrePitch + (float) visibleRows() * 0.5f;
    clampView();
    repaint();
}

void PianoRoll::resized()
{
    auto area = getLocalBounds().toFloat();
    toolbarArea = area.removeFromTop (toolbarHeight);
    keyArea = area.withWidth (keyWidth).withTrimmedTop (rulerHeight).withTrimmedBottom (velocityHeight + 4.0f + barSize);
    rulerArea = area.withTrimmedLeft (keyWidth).withTrimmedRight (barSize).withHeight (rulerHeight);
    velocityArea = area.withTrimmedLeft (keyWidth).withTrimmedRight (barSize).removeFromBottom (velocityHeight);
    gridArea = area.withTrimmedLeft (keyWidth).withTrimmedRight (barSize).withTrimmedTop (rulerHeight).withTrimmedBottom (velocityHeight + 4.0f + barSize);
    vBar.setBounds (juce::Rectangle<float> (gridArea.getRight(), gridArea.getY(), barSize, gridArea.getHeight()).toNearestInt());
    hBar.setBounds (juce::Rectangle<float> (gridArea.getX(), gridArea.getBottom() + 1.0f, gridArea.getWidth(), barSize).toNearestInt());

    // Verktygsraden: SNAP, rutnät, radhöjd, markera alla, ta bort.
    auto tools = toolbarArea.toNearestInt().reduced (2, 1);
    snapButton.setBounds (tools.removeFromLeft (50));
    tools.removeFromLeft (4);
    gridBox.setBounds (tools.removeFromLeft (76));
    tools.removeFromLeft (10);
    smallerButton.setBounds (tools.removeFromLeft (24));
    tools.removeFromLeft (2);
    biggerButton.setBounds (tools.removeFromLeft (24));
    tools.removeFromLeft (10);
    allButton.setBounds (tools.removeFromLeft (38));
    tools.removeFromLeft (2);
    deleteButton.setBounds (tools.removeFromLeft (38));
    tools.removeFromLeft (10);
    glideButton.setBounds (tools.removeFromLeft (56));

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
    if (bars() != lastBars)
    {
        // Ny mönsterlängd: visa hela mönstret igen.
        if (lastBars != 0)
            fitToWidth();
        lastBars = bars();
        changed = true;
    }
    update (lastGrid, (int) grid());
    if (changed)
    {
        clampView();
        pruneSelection();
        repaint();
    }
}

PianoRoll::NoteMap PianoRoll::readNotes() const
{
    NoteMap notes;
    for (int tick = 0; tick < Pattern::maxTicks; ++tick)
    {
        const auto& n = processor.pattern.note (layer, tick);
        if (n.on.load())
            notes[tick] = { n.length.load(), n.pitch.load(), n.velocity.load(), n.slide.load() };
    }
    return notes;
}

void PianoRoll::applyNotes (const NoteMap& target)
{
    auto& pattern = processor.pattern;
    for (int tick = 0; tick < Pattern::maxTicks; ++tick)
    {
        auto& n = pattern.note (layer, tick);
        const auto found = target.find (tick);
        if (found == target.end())
        {
            if (n.on.load())
                n.on = false;
            continue;
        }

        const auto& d = found->second;
        if (n.length.load() != d.length) n.length = d.length;
        if (n.pitch.load() != d.pitch) n.pitch = d.pitch;
        if (n.velocity.load() != d.velocity) n.velocity = d.velocity;
        if (n.slide.load() != d.slide) n.slide = d.slide;
        if (! n.on.load())
            n.on = true;
    }
}

void PianoRoll::pruneSelection()
{
    for (auto it = selection.begin(); it != selection.end();)
        it = (*it < 0 || *it >= totalTicks() || ! processor.pattern.note (layer, *it).on.load()) ? selection.erase (it) : std::next (it);
}

void PianoRoll::selectAll()
{
    selection.clear();
    for (const auto& [tick, data] : readNotes())
        if (tick < totalTicks()) // bara det som hör till mönstret
            selection.insert (tick);
    repaint();
}

void PianoRoll::deleteSelected()
{
    if (selection.empty())
        return;
    for (const auto tick : selection)
        processor.pattern.removeNote (layer, tick);
    selection.clear();
    commit();
    repaint();
}

void PianoRoll::moveSelected (int deltaTicks, int deltaPitch)
{
    if (selection.empty())
        return;

    // Flytta alla markerade så långt det går utan att något hamnar utanför.
    auto notes = readNotes();
    auto minTick = totalTicks(), maxTick = 0, minPitch = Pattern::maxPitch, maxPitch = Pattern::minPitch;
    for (const auto tick : selection)
    {
        minTick = std::min (minTick, tick);
        maxTick = std::max (maxTick, tick);
        minPitch = std::min (minPitch, notes[tick].pitch);
        maxPitch = std::max (maxPitch, notes[tick].pitch);
    }
    deltaTicks = clampSafe (deltaTicks, -minTick, totalTicks() - 1 - maxTick);
    deltaPitch = clampSafe (deltaPitch, Pattern::minPitch - minPitch, Pattern::maxPitch - maxPitch);

    NoteMap target;
    for (const auto& [tick, data] : notes)
        if (selection.count (tick) == 0)
            target[tick] = data;
    std::set<int> moved;
    for (const auto tick : selection)
    {
        auto data = notes[tick];
        data.pitch += deltaPitch;
        target[tick + deltaTicks] = data;
        moved.insert (tick + deltaTicks);
    }
    applyNotes (target);
    selection = moved;
    commit();
    repaint();
}

juce::Rectangle<float> PianoRoll::noteBounds (int tick) const
{
    jassert (juce::isPositiveAndBelow (tick, Pattern::maxTicks));
    const auto& n = processor.pattern.note (layer, juce::jlimit (0, Pattern::maxTicks - 1, tick));
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
        text << "  GLIDE";
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

        // Tidslinjer: rutnät, slag och takter (rutnätet visas även när SNAP är av).
        const auto step = Pattern::ticksPerStep (grid());
        const auto firstTick = std::max (0, (int) xToTicks (gridArea.getX()) / step * step);
        for (int tick = firstTick; tick <= totalTicks() && ticksToX ((float) tick) <= gridArea.getRight(); tick += step)
        {
            const auto x = ticksToX ((float) tick);
            const auto isBar = tick % Pattern::ticksPerBar == 0, isBeat = tick % Pattern::ticksPerQuarter == 0;
            if (! isBar && ! isBeat && (float) step * pixelsPerTick < 4.0f)
                continue;
            g.setColour (isBar ? Theme::textDim.withAlpha (0.55f) : isBeat ? Theme::textDim.withAlpha (0.28f) : Theme::grid.withAlpha (snapEnabled ? 0.6f : 0.3f));
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
            const auto selected = selection.count (tick) > 0;
            const auto active = tick == hoverTick || tick == dragTick;
            auto c = colour.withMultipliedBrightness (0.55f + 0.45f * n.velocity.load());
            if (n.slide.load())
            {
                c = Theme::pattern.withMultipliedBrightness (0.7f + 0.3f * n.velocity.load());

                // Rampen: från tonhöjden som klingar till glide-tonens tonhöjd, under glide-tonens längd.
                const auto fromY = pitchToY (pitchBefore (tick)) + rowHeight() * 0.5f;
                g.setColour (Theme::pattern.withAlpha (0.9f));
                g.drawLine (b.getX(), fromY, b.getRight(), b.getCentreY(), 2.0f);
            }
            if (selected)
                c = c.brighter (0.45f).interpolatedWith (juce::Colours::white, 0.25f);
            g.setGradientFill (juce::ColourGradient (c.brighter (0.35f), b.getX(), b.getY(), c.darker (0.2f), b.getX(), b.getBottom(), false));
            g.fillRoundedRectangle (b, 2.5f);
            g.setColour (selected ? juce::Colour (0xffffe27a) : active ? juce::Colours::white : c.brighter (0.5f));
            g.drawRoundedRectangle (b, 2.5f, selected || active ? 1.6f : 1.0f);

            if (b.getWidth() > 24.0f && b.getHeight() > 9.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.setFont (Theme::font (std::min (9.0f, b.getHeight() - 2.0f), true));
                g.drawText (juce::MidiMessage::getMidiNoteName (rootNote() + n.pitch.load(), true, true, 4),
                            b.reduced (3.0f, 0.0f), juce::Justification::centredLeft);
            }

        }

        // Markeringsrutan.
        if (drag == Drag::box)
        {
            const auto rect = juce::Rectangle<float> (boxStart, boxEnd);
            g.setColour (juce::Colour (0xffffe27a).withAlpha (0.12f));
            g.fillRect (rect);
            g.setColour (juce::Colour (0xffffe27a).withAlpha (0.8f));
            g.drawRect (rect, 1.0f);
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
        g.setFont (Theme::font (std::min (9.0f, rowHeight() - 2.0f), true));
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

    // Linjalen: takt.slag.
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

    // Verktygsraden: vad musen är över, och antal markerade.
    g.setFont (Theme::font (10.0f, true));
    g.setColour (colour);
    g.drawText ("BASS " + juce::String (layer + 1), toolbarArea.withTrimmedLeft (toolbarArea.getWidth() * 0.62f).withWidth (60.0f), juce::Justification::centredLeft);
    g.setColour (Theme::text);
    const auto info = hoverTick >= 0 ? describe (hoverTick)
                    : selection.empty() ? juce::String()
                                        : juce::String ((int) selection.size()) + " selected";
    g.drawText (info, toolbarArea.withTrimmedLeft (toolbarArea.getWidth() * 0.62f + 62.0f).reduced (6.0f, 0.0f), juce::Justification::centredRight);

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
            const auto lit = selection.count (tick) > 0 || (tick == dragTick && drag == Drag::velocity);
            g.setColour (lit ? juce::Colour (0xffffe27a) : colour.withAlpha (0.85f));
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
    grabKeyboardFocus(); // för Ctrl + A, Delete och piltangenterna
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
            // Högerklick på en markerad ton tar bort hela markeringen, annars bara tonen.
            if (selection.count (tick) == 0)
                selection = { tick };
            deleteSelected();
        }
        else
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Select all", true);
            menu.addItem (2, "Clear selection", ! selection.empty());
            menu.addItem (3, "Delete selected", ! selection.empty());
            menu.addSeparator();
            menu.addItem (4, "Delete the piano roll (back to the step pattern)");
            juce::Component::SafePointer<PianoRoll> safeThis (this);
            menu.showMenuAsync ({}, [safeThis] (int result)
            {
                if (safeThis == nullptr)
                    return;
                if (result == 1) safeThis->selectAll();
                else if (result == 2) { safeThis->selection.clear(); safeThis->repaint(); }
                else if (result == 3) safeThis->deleteSelected();
                else if (result == 4)
                {
                    // Bekräfta först: det tar bort lagrets alla toner.
                    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                                      juce::MessageBoxIconType::WarningIcon, "Delete piano roll",
                                                      "Delete the piano roll? All the notes of this layer are removed and the layer goes back to an empty step pattern.",
                                                      "Delete", "Cancel"),
                                                  [safeThis] (int choice)
                    {
                        if (safeThis == nullptr || choice != 1)
                            return;
                        safeThis->selection.clear();
                        safeThis->processor.pattern.clearNotes (safeThis->layer); // nollställer pianorullsmarkeringen
                        safeThis->processor.patternChanged();
                        if (safeThis->onDeleted)
                            safeThis->onDeleted();
                    });
                }
            });
        }
        return;
    }

    if (tick >= 0)
    {
        if (tick != justCreatedTick)
            justCreatedTick = -1;

        // Skift lägger till eller tar bort tonen i markeringen utan att flytta något.
        if (event.mods.isShiftDown())
        {
            if (selection.count (tick) > 0)
                selection.erase (tick);
            else
                selection.insert (tick);
            repaint();
            return;
        }

        // En omarkerad ton blir ensam markerad; en markerad ton behåller hela markeringen (så att den flyttas med).
        if (selection.count (tick) == 0)
            selection = { tick };

        drag = edge ? Drag::resize : Drag::move;
        dragTick = tick;
        dragStartTick = tick;
        dragStartPitch = processor.pattern.note (layer, tick).pitch.load();
        grabOffsetTicks = xToTicks (event.position.x) - (float) tick;
        dragSnapshot = readNotes();
        dragSelection = selection;
        repaint();
        return;
    }

    // Skift + dra i tomt: markeringsruta.
    if (event.mods.isShiftDown())
    {
        drag = Drag::box;
        boxStart = boxEnd = event.position;
        boxBase = selection;
        return;
    }

    // Tom plats: en ny ton på steget under musen, i raden under musen.
    const auto start = snapped (xToTicks (event.position.x));
    const auto pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, yToPitch (event.position.y));
    const auto glide = glideButton.getToggleState();
    const auto length = ! glide && lastLength > 0 ? lastLength : Pattern::ticksPerStep (grid());
    processor.pattern.addNote (layer, start, length, pitch, 0.8f, glide);
    selection = { start };
    justCreatedTick = start;
    drag = Drag::move;
    dragTick = start;
    dragStartTick = start;
    dragStartPitch = pitch;
    grabOffsetTicks = 0.0f;
    dragSnapshot = readNotes();
    dragSelection = selection;
    edited = true;
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& event)
{
    if (drag == Drag::none)
        return;

    auto& pattern = processor.pattern;

    if (drag == Drag::box)
    {
        boxEnd = event.position;
        const auto rect = juce::Rectangle<float> (boxStart, boxEnd);
        selection = boxBase;
        for (int tick = 0; tick < totalTicks(); ++tick)
            if (pattern.note (layer, tick).on.load() && noteBounds (tick).intersects (rect))
                selection.insert (tick);
        repaint();
        return;
    }

    if (dragTick < 0)
        return;

    if (drag == Drag::velocity)
    {
        pattern.note (layer, dragTick).velocity = juce::jlimit (0.02f, 1.0f, 1.0f - (event.position.y - velocityArea.getY() - 3.0f) / (velocityArea.getHeight() - 6.0f));
        edited = true;
        repaint();
        return;
    }

    if (drag == Drag::resize)
    {
        // Alla markerade ändras lika mycket som den grepna.
        const auto step = snapTicks();
        const auto end = snapEnabled ? (int) std::round (xToTicks (event.position.x) / (float) step) * step : (int) std::round (xToTicks (event.position.x));
        const auto& grabbed = dragSnapshot[dragStartTick];
        const auto delta = end - (dragStartTick + grabbed.length);

        auto target = dragSnapshot;
        auto shortest = Pattern::maxTicks;
        for (const auto tick : dragSelection)
            shortest = std::min (shortest, dragSnapshot[tick].length);
        const auto limitedDelta = std::max (delta, step - shortest); // ingen ton kortare än ett steg
        for (const auto tick : dragSelection)
            target[tick].length = juce::jlimit (1, Pattern::maxTicks - tick, dragSnapshot[tick].length + limitedDelta);
        applyNotes (target);
        lastLength = target[dragStartTick].length;
        edited = true;
        repaint();
        return;
    }

    // Flytta: ny starttid (snap) och ny rad för den grepna tonen; resten av markeringen följer med.
    const auto newTick = snapped (xToTicks (event.position.x) - grabOffsetTicks);
    const auto newPitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, yToPitch (event.position.y));
    auto deltaTicks = newTick - dragStartTick, deltaPitch = newPitch - dragStartPitch;

    auto minTick = totalTicks(), maxTick = 0, minPitch = Pattern::maxPitch, maxPitch = Pattern::minPitch;
    for (const auto tick : dragSelection)
    {
        minTick = std::min (minTick, tick);
        maxTick = std::max (maxTick, tick);
        minPitch = std::min (minPitch, dragSnapshot[tick].pitch);
        maxPitch = std::max (maxPitch, dragSnapshot[tick].pitch);
    }
    deltaTicks = clampSafe (deltaTicks, -minTick, totalTicks() - 1 - maxTick);
    deltaPitch = clampSafe (deltaPitch, Pattern::minPitch - minPitch, Pattern::maxPitch - maxPitch);

    // Bygg om från början av draget: de omarkerade orörda, de markerade på sin nya plats (de vinner vid krock).
    NoteMap target;
    for (const auto& [tick, data] : dragSnapshot)
        if (dragSelection.count (tick) == 0)
            target[tick] = data;
    selection.clear();
    for (const auto tick : dragSelection)
    {
        auto data = dragSnapshot[tick];
        data.pitch += deltaPitch;
        target[tick + deltaTicks] = data;
        selection.insert (tick + deltaTicks);
    }
    applyNotes (target);
    dragTick = dragStartTick + deltaTicks;
    lastLength = target[dragTick].length;
    hoverTick = dragTick;
    edited = true;
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (edited)
        commit();
    drag = Drag::none;
    dragTick = -1;
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& event)
{
    bool edge = false;
    if (! gridArea.contains (event.position) || event.mods.isPopupMenu() || event.mods.isShiftDown())
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

    // Slide på/av för alla markerade (eller bara tonen om den inte är markerad).
    auto targets = selection.count (tick) > 0 ? selection : std::set<int> { tick };
    const auto turnOn = ! processor.pattern.note (layer, tick).slide.load();
    for (const auto t : targets)
        processor.pattern.note (layer, t).slide = turnOn;
    commit();
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if ((mods.isCtrlDown() || mods.isCommandDown()) && (key.getKeyCode() == 'A' || key.getKeyCode() == 'a'))
    {
        selectAll();
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelected();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        selection.clear();
        repaint();
        return true;
    }

    const auto step = snapEnabled ? Pattern::ticksPerStep (grid()) : 1;
    if (key == juce::KeyPress::upKey)    { moveSelected (0, mods.isShiftDown() ? 12 : 1); return true; }
    if (key == juce::KeyPress::downKey)  { moveSelected (0, mods.isShiftDown() ? -12 : -1); return true; }
    if (key == juce::KeyPress::leftKey)  { moveSelected (-step, 0); return true; }
    if (key == juce::KeyPress::rightKey) { moveSelected (step, 0); return true; }
    return false;
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
