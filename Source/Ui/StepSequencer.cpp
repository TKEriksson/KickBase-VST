#include "StepSequencer.h"

namespace
{
    constexpr int headerHeight = 16;
    constexpr int pixelsPerSemitone = 5;
}

StepSequencer::StepSequencer (PluginBase& p, juce::StringArray names, std::vector<juce::Colour> colours)
    : processor (p), layerNames (std::move (names)), layerColours (std::move (colours))
{
    setTooltip ("PATTERN - Click steps to turn notes on/off (drag to paint). Click the round button by a row's name to select it (double-click the name to rename it): "
                "the selected row's grid, length, FILL and piano roll show on the left and its "
                "velocity, length or pitch in the lane below. A layer edited in the piano roll shows a small piano roll instead of steps: "
                "click it to open the piano roll, right-click to delete it. GATE mode edits the on/off rhythm that chops the sound.");
    addRowKnobs ("KICK", ParamID::kickLevel, ParamID::kickFxAmount, Theme::kick);
    for (int layer = 0; layer < totalLayers(); ++layer)
        addRowKnobs (layerNames[layer], ParamID::bass (layer, InstrumentField::level), ParamID::bass (layer, InstrumentField::fxAmount), layerColours[(size_t) layer]);

    // Radens egen längd (kick och varje lager): Pattern = lika lång som hela mönstret.
    for (int row = 0; row <= totalLayers(); ++row)
    {
        auto box = std::make_unique<juce::ComboBox>();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (ParamID::rowBars (row))))
            box->addItemList (choice->choices, 1);
        box->setTooltip ((row == 0 ? juce::String ("KICK") : layerNames[row - 1]) + " LENGTH - How many bars this row lasts. Total = as long as the whole pattern "
                         "(which is at least as long as the longest row). A shorter row repeats (shown dimmed) while the longest row plays out, "
                         "or plays once if its loop button is off.");
        addAndMakeVisible (*box);
        lengthAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, ParamID::rowBars (row), *box));
        lengthBoxes.push_back (std::move (box));

        const auto layerOfRow = row == 0 ? Pattern::kickLayer : row - 1;
        const auto accent = row == 0 ? Theme::kick : layerColours[(size_t) (row - 1)];
        auto listen = std::make_unique<ListenButton>();
        listen->setAccent (accent);
        listen->setTooltip ((row == 0 ? juce::String ("KICK") : layerNames[row - 1]) + " LISTEN - Hold to hear just this row's pattern on the preview note "
                            "(the PREVIEWS note at the top), looping while you hold. Press again to start over; double-click to let it play until you click again.");
        listen->onStart = [this, layerOfRow] { processor.requestAudition (PluginBase::previewRequest (layerOfRow)); };
        listen->onStop = [this] { processor.requestAudition (PluginBase::previewStop); };
        addAndMakeVisible (*listen);
        listen->setVisible (! processor.isEffectVersion()); // förhandsspelning av en rad finns bara i instrumentet
        listenButtons.push_back (std::move (listen));

        auto loop = std::make_unique<LoopToggle> (accent);
        loop->setTooltip ((row == 0 ? juce::String ("KICK") : layerNames[row - 1]) + " LOOP - On: if this row is shorter than the pattern it repeats until the pattern is done. "
                          "Off: it plays once and is then silent.");
        addAndMakeVisible (*loop);
        loopAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamID::rowLoop (row), *loop));
        loopToggles.push_back (std::move (loop));

        auto switches = std::make_unique<RowSwitches>();
        const auto rowName = row == 0 ? juce::String ("KICK") : layerNames[row - 1];
        switches->mute.setTooltip (rowName + " MUTE - Switches this row off and on (the same as the power button on its tab). A muted row is shown dimmed.");
        switches->solo.setTooltip (rowName + " SOLO - Only the soloed rows play (click several to hear a few together). Click again to release. "
                                   "Solo is not saved with the project.");
        switches->mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd64a5a));
        switches->solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe6c23c));
        switches->solo.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff10121c));
        for (auto* button : { &switches->mute, &switches->solo })
        {
            button->setClickingTogglesState (false);
            button->setConnectedEdges (0);
            addAndMakeVisible (*button);
            button->setVisible (! processor.isEffectVersion()); // mute och solo finns bara i instrumentet
        }
        switches->mute.onClick = [this, row, layerOfRow]
        {
            const auto id = row == 0 ? juce::String (ParamID::kickOn) : ParamID::bass (row - 1, InstrumentField::on);
            if (auto* parameter = processor.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getValue() >= 0.5f ? 0.0f : 1.0f);
                parameter->endChangeGesture();
            }
            juce::ignoreUnused (layerOfRow);
            refreshSwitches();
            repaint();
        };
        switches->solo.onClick = [this, layerOfRow]
        {
            processor.soloMask ^= layerOfRow == Pattern::kickLayer ? PluginBase::kickSoloBit : 1 << layerOfRow;
            refreshSwitches();
            repaint();
        };
        rowSwitches.push_back (std::move (switches));
    }

    followButton.setClickingTogglesState (true);
    followButton.setToggleState (processor.followPlayhead.load(), juce::dontSendNotification);
    followButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
    followButton.setTooltip ("FOLLOW - On: the view flips to the bars that play, so you follow the song. Off: stay where you are.");
    followButton.onClick = [this] { processor.followPlayhead = followButton.getToggleState(); };
    addChildComponent (followButton);

    scrollBar.setAutoHide (false);
    scrollBar.setSingleStepSize (1.0);
    scrollBar.addListener (this);
    addChildComponent (scrollBar);

    zoomCorner.onStart = [this] { zoomStartHeight = effectiveRowHeight; };
    zoomCorner.onDragBy = [this] (int dy)
    {
        processor.sequencerRowHeight = juce::jlimit (28, 130, zoomStartHeight + dy);
        resized();
        repaint();
    };
    zoomCorner.onReset = [this] { processor.sequencerRowHeight = 0; resized(); repaint(); };
    addAndMakeVisible (zoomCorner);

    previousPageButton.setTooltip ("Previous four bars.");
    nextPageButton.setTooltip ("Next four bars.");
    previousPageButton.onClick = [this] { setPage (page - 1); };
    nextPageButton.onClick = [this] { setPage (page + 1); };
    addChildComponent (previousPageButton);
    addChildComponent (nextPageButton);

    startVisualTimer (Settings::Visual::other, 30);
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
        slider.getProperties().set ("ringMargin", 3.0);
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
    return juce::Rectangle<int> (knobPx, knobPx).withPosition (labelColumn.getX() + 1 + index * (knobPx + 2), compactRows ? row.getY() + (row.getHeight() - knobPx) / 2 : row.getY() + 1);
}

void StepSequencer::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    const auto next = juce::jlimit (0, std::max (0, 1 + numLayers() - visibleRows), (int) std::lround (newRangeStart));
    if (next == rowScroll)
        return;
    rowScroll = next;
    resized();
    repaint();
}

StepSequencer::ZoomCorner::ZoomCorner()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("ZOOM - Drag up or down to make the pattern rows lower or taller (the rows get compact when they are low, so you can see more of them). "
                "Double-click to go back to the automatic height.");
}

void StepSequencer::ZoomCorner::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff2a2e45));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (Theme::textDim);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Pil upp och pil ner.
    const auto c = bounds.getCentre();
    juce::Path arrows;
    arrows.addTriangle (c.x, c.y - 4.5f, c.x - 2.8f, c.y - 1.5f, c.x + 2.8f, c.y - 1.5f);
    arrows.addTriangle (c.x, c.y + 4.5f, c.x - 2.8f, c.y + 1.5f, c.x + 2.8f, c.y + 1.5f);
    g.setColour (Theme::text);
    g.fillPath (arrows);
}

void StepSequencer::ZoomCorner::mouseDown (const juce::MouseEvent&)
{
    if (onStart)
        onStart();
}

void StepSequencer::ZoomCorner::mouseDrag (const juce::MouseEvent& event)
{
    if (onDragBy)
        onDragBy (event.getDistanceFromDragStartY());
}

void StepSequencer::ZoomCorner::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onReset)
        onReset();
}

void StepSequencer::scrollRows (int delta)
{
    const auto rows = 1 + numLayers();
    const auto next = juce::jlimit (0, std::max (0, rows - visibleRows), rowScroll + delta);
    if (next == rowScroll)
        return;
    rowScroll = next;
    resized();
    repaint();
}

void StepSequencer::selectLayer (int layer)
{
    if (layer != Pattern::kickLayer)
        layer = juce::jlimit (0, numLayers() - 1, layer);

    if (layer == selectedLayer)
        return;
    selectedLayer = layer;
    revealSelected = true; // raden ska synas: resized scrollar till den om den ligger utanför
    resized();
    if (onLayerSelected)
        onLayerSelected (layer);
    repaint();
}

int StepSequencer::bars() const
{
    return Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
}

InstrumentGrid StepSequencer::grid (int layer) const
{
    const auto id = layer == Pattern::kickLayer ? juce::String (ParamID::rowGridKick) : ParamID::bass (layer, InstrumentField::grid);
    return (InstrumentGrid) juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (id)->load());
}

juce::String StepSequencer::nameOf (int layer) const
{
    if (layer == Pattern::kickLayer)
        return "KICK";
    return renameable() ? processor.getLayerName (layer) : layerNames[layer];
}

void StepSequencer::renameLayer (int layer)
{
    if (! renameable() || layer == Pattern::kickLayer)
        return;

    auto* window = new juce::AlertWindow ("Rename", "Name for this layer (it is shown in the pattern, the tabs and the piano roll):", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    renameDialog = window;
    window->addTextEditor ("name", processor.getLayerName (layer));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Default name", 2);
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<StepSequencer> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window, layer] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        safeThis->processor.setLayerName (layer, result == 2 ? juce::String() : window->getTextEditorContents ("name"));
        safeThis->repaint();
    }), true);
}

InstrumentGrid StepSequencer::gateGrid (int layer) const
{
    return (InstrumentGrid) juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateGrid))->load());
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

bool StepSequencer::rowSilent (int layer) const
{
    const auto solo = processor.soloMask.load();
    const auto isOn = processor.apvts.getRawParameterValue (layer == Pattern::kickLayer ? juce::String (ParamID::kickOn) : ParamID::bass (layer, InstrumentField::on))->load() >= 0.5f;
    const auto soloed = (solo & (layer == Pattern::kickLayer ? PluginBase::kickSoloBit : 1 << layer)) != 0;
    return ! isOn || (solo != 0 && ! soloed);
}

void StepSequencer::refreshSwitches()
{
    auto bits = 0;
    for (size_t row = 0; row < rowSwitches.size(); ++row)
    {
        const auto layerOfRow = row == 0 ? Pattern::kickLayer : (int) row - 1;
        if (layerOfRow != Pattern::kickLayer && layerOfRow >= processor.activeInstruments())
            continue;
        const auto muted = processor.apvts.getRawParameterValue (row == 0 ? juce::String (ParamID::kickOn) : ParamID::bass (layerOfRow, InstrumentField::on))->load() < 0.5f;
        const auto soloed = (processor.soloMask.load() & (layerOfRow == Pattern::kickLayer ? PluginBase::kickSoloBit : 1 << layerOfRow)) != 0;
        rowSwitches[row]->mute.setToggleState (muted, juce::dontSendNotification);
        rowSwitches[row]->solo.setToggleState (soloed, juce::dontSendNotification);
        bits |= (muted ? 1 : 0) << (row * 2);
        bits |= (soloed ? 1 : 0) << (row * 2 + 1);
    }
    bits |= processor.soloMask.load() != 0 ? 1 << 30 : 0; // något soloat: de andra raderna tonas ner
    lastSwitchBits = bits;
}

void StepSequencer::timerCallback()
{
    followVisualRate();
    // Rita om bara när något synligt ändrats: markörerna, mönstret, mönsterlängden eller rutnäten.
    auto changed = false;
    auto update = [&changed] (int& last, int now) { if (last != now) { last = now; changed = true; } };

    update (lastNamesVersion, processor.layerNamesVersion.load());
    if (const auto active = numLayers(); active != lastActive)
    {
        lastActive = active;
        if (selectedLayer != Pattern::kickLayer && selectedLayer >= active)
            selectedLayer = std::max (0, active - 1);
        resized(); // instrument har lagts till eller tagits bort: raderna ritas om
        changed = true;
    }
    const auto barsBefore = lastBars;
    update (lastBars, bars());
    if (lastBars != barsBefore)
    {
        page = juce::jlimit (0, numPages() - 1, page);
        resized(); // sidknapparna visas bara när mönstret är längre än en sida
    }
    update (lastVersion, processor.patternVersion.load());

    // Totalen är minst så lång som den längsta raden: visa det i TOTAL LENGTH.
    {
        const auto effective = lengths().total;
        auto choiceIndex = 0;
        while ((1 << choiceIndex) < effective && choiceIndex < 6)
            ++choiceIndex;
        if (auto* totalParameter = processor.apvts.getParameter (ParamID::patternBars);
            totalParameter != nullptr && (int) std::round (totalParameter->convertFrom0to1 (totalParameter->getValue())) != choiceIndex)
            Presets::setParameter (*totalParameter, totalParameter->convertTo0to1 ((float) choiceIndex));
    }

    // En rad som är en pianorulle styr sin längd i pianorullen: rutan här är då avstängd.
    for (size_t row = 0; row < lengthBoxes.size(); ++row)
    {
        const auto layerOfRow = row == 0 ? Pattern::kickLayer : (int) row - 1;
        const auto inRoll = processor.pattern.isCustom (layerOfRow);
        if (lengthBoxes[row]->isEnabled() == inRoll)
        {
            lengthBoxes[row]->setEnabled (! inRoll);
            lengthBoxes[row]->setTooltip (inRoll ? juce::String ("This row is a piano roll: set its length in the piano roll (LENGTH in its toolbar).")
                                                 : (row == 0 ? juce::String ("KICK") : layerNames[(int) row - 1]) + " LENGTH - How many bars this row lasts. Total = as long as the whole pattern "
                                                   "(which is at least as long as the longest row). A shorter row repeats (shown dimmed) while the longest row plays out, "
                                                   "or plays once if its loop button is off.");
        }
    }

    // M och S visar läget (som kan ha ändrats på instrumentets flik eller av automation).
    if (! processor.isEffectVersion())
    {
        const auto before = lastSwitchBits;
        refreshSwitches();
        if (lastSwitchBits != before)
            changed = true;
    }

    // Högtalarknapparna lyser när raden förhandsspelas.
    for (size_t row = 0; row < listenButtons.size(); ++row)
    {
        const auto layerOfRow = row == 0 ? Pattern::kickLayer : (int) row - 1;
        const auto previewing = processor.previewingLayer.load();
        listenButtons[row]->setActive (previewing == layerOfRow || previewing == PluginBase::previewAllRows);
    }

    // FOLLOW: sidan med takten som spelar visas (bara när mönstret är längre än en sida).
    followButton.setToggleState (processor.followPlayhead.load(), juce::dontSendNotification);
    if (processor.followPlayhead.load() && processor.transportStatus.patternActive.load() && bars() > barsPerPage)
    {
        const auto barInPattern = ((processor.transportStatus.bar.load() - 1) % bars() + bars()) % bars();
        setPage (barInPattern / barsPerPage);
    }

    for (int layer = 0; layer < numLayers(); ++layer)
    {
        update (lastGrids[(size_t) layer], (int) grid (layer));
        update (lastGateGrids[(size_t) layer], (int) gateGrid (layer));
    }
    {
        auto bits = 0;
        for (int layer = 0; layer < Pattern::numNoteLayers; ++layer)
            bits |= processor.pattern.isCustom (layer) ? 1 << layer : 0;
        if (bits != lastCustomBits && lastCustomBits != -1)
            resized(); // lanet tas bort eller läggs till när lagret får eller förlorar sin pianorulle
        update (lastCustomBits, bits);
    }

    // Markörerna: har inget annat ändrats ritas bara cellerna där de var och hamnar om.
    std::array<std::pair<int, int>, Pattern::numNoteLayers> movedMarkers {};
    auto numMoved = 0;
    auto visitMarker = [&] (int layer)
    {
        const auto now = markerStep (layer);
        auto& last = lastMarkers[(size_t) layer];
        if (now != last)
        {
            movedMarkers[(size_t) numMoved++] = { layer, last };
            last = now;
        }
    };
    visitMarker (Pattern::kickLayer);
    for (int layer = 0; layer < numLayers(); ++layer)
        visitMarker (layer);

    if (changed)
        repaint();
    else
        for (int i = 0; i < numMoved; ++i)
            repaintMarker (movedMarkers[(size_t) i].first, movedMarkers[(size_t) i].second, lastMarkers[(size_t) movedMarkers[(size_t) i].first]);
}

int StepSequencer::markerStep (int layer) const
{
    const auto isKick = layer == Pattern::kickLayer;
    const auto playTick = isKick ? processor.transportStatus.kickStep.load() : processor.transportStatus.instrumentSteps[(size_t) layer].load();
    if (mode == Mode::gate && ! isKick)
        return playTick < 0 ? -1 : playTick / Pattern::ticksPerStep (gateGrid (layer)); // samma som paint i GATE-läget
    return stepOfTick (layer, playTick);
}

void StepSequencer::repaintMarker (int layer, int oldStep, int newStep)
{
    const auto row = rowBounds (layer);
    if (row.isEmpty())
        return;
    const auto isKick = layer == Pattern::kickLayer;
    const auto gate = mode == Mode::gate && ! isKick;
    if (! gate && isCustom (layer))
    {
        repaint (row.expanded (2)); // pianorullens miniatyr: strecket kan ligga var som helst i raden
        return;
    }

    // Samma celler som paintToggleRow tänder: alla synliga kopior av radens steg (en kortare rad som loopar visas flera gånger).
    const auto lengthsNow = lengths();
    const auto numSteps = gate ? gateSteps (layer) : instrumentSteps (layer);
    const auto offset = pageOffset (numSteps);
    const auto ownSteps = std::max (1, gate ? gateOwnSteps (lengthsNow, layer) : instrumentOwnSteps (lengthsNow, layer));
    const auto loops = lengthsNow.loops[(size_t) rowOf (layer)];
    for (int step = 0; step < numSteps; ++step)
    {
        const auto absolute = offset + step;
        const auto beyond = absolute >= ownSteps;
        if (beyond && ! loops)
            continue;
        const auto source = beyond ? absolute % ownSteps : absolute;
        if (source == oldStep || source == newStep)
            repaint (stepBounds (row, step, numSteps).expanded (3.0f).getSmallestIntegerContainer());
    }
}

void StepSequencer::resized()
{
    auto area = getLocalBounds();

    // Radhöjden: automatisk (så många rader som får plats) eller vald med zoomhörnet. Låga rader blir kompakta.
    const auto rows = 1 + numLayers();
    constexpr int gap = 5, laneHeight = 76, normalMinHeight = 50;
    const auto manual = processor.sequencerRowHeight > 0;
    const auto freeHeight = area.getHeight() - headerHeight - 5 - (laneShown() ? laneHeight + 3 : 0);
    int rowHeight;
    if (manual)
        rowHeight = processor.sequencerRowHeight;
    else
        rowHeight = juce::jlimit (normalMinHeight, 56, (freeHeight - gap * rows) / std::max (1, rows));
    compactRows = rowHeight < normalMinHeight - 3;
    labelWidthNow = compactRows ? 246 : 162;
    knobPx = compactRows ? std::min (24, rowHeight - 4) : 28;
    effectiveRowHeight = rowHeight;
    const auto fits = std::max (1, freeHeight / (rowHeight + gap));
    visibleRows = std::min (rows, fits);
    rowScroll = juce::jlimit (0, rows - visibleRows, rowScroll);
    if (revealSelected && getHeight() > 100)
    {
        const auto selectedRow = rowOf (selectedLayer);
        if (selectedRow < rowScroll)
            rowScroll = selectedRow;
        else if (selectedRow >= rowScroll + visibleRows)
            rowScroll = selectedRow - visibleRows + 1;
        revealSelected = false;
    }

    // Rubrikremsan överst: VOL, FX över kolumnerna till vänster, och sidväljaren (bara för mönster längre än fyra takter) över stegen.
    auto header = area.removeFromTop (headerHeight);
    area.removeFromTop (5); // marginal under översikten
    labelColumn = area.removeFromLeft (labelWidthNow);
    headerLabels = header.removeFromLeft (labelWidthNow);
    const auto paged = bars() > barsPerPage;
    pageArea = header;
    previousPageButton.setVisible (paged);
    nextPageButton.setVisible (paged);
    followButton.setVisible (paged);
    auto strip = pageArea;
    if (paged)
    {
        previousPageButton.setBounds (strip.removeFromLeft (28));
        strip.removeFromLeft (4);
        nextPageButton.setBounds (strip.removeFromLeft (28));
        strip.removeFromLeft (6);
        strip.removeFromLeft (104); // texten "BARS 1-4 OF 32"
        followButton.setBounds (strip.removeFromRight (60));
        strip.removeFromRight (6);
    }
    overviewArea = strip.reduced (0, 1);

    // Rullningslist till höger när inte alla rader får plats, och zoomhörnet längst ner där den slutar.
    const auto scrolling = rows > visibleRows;
    auto barColumn = area.removeFromRight (14);
    area.removeFromRight (2);
    zoomCorner.setBounds (barColumn.removeFromBottom (14));
    scrollBar.setVisible (scrolling);
    if (scrolling)
    {
        scrollBar.setBounds (barColumn.withTrimmedBottom (2));
        scrollBar.setRangeLimits (0.0, (double) rows);
        scrollBar.setCurrentRange ((double) rowScroll, (double) visibleRows, juce::dontSendNotification);
    }

    // Raderna: lanet (velocity m.m.) ligger direkt under det valda lagrets rad, med en ram runt båda.
    stepsColumn = area;
    for (auto& rect : rowRects)
        rect = {};
    laneArea = {};
    selectionFrame = {};
    rowsTop = area.getY();
    const auto selectedRow = rowOf (selectedLayer);
    for (int row = rowScroll; row < rowScroll + visibleRows; ++row)
    {
        rowRects[(size_t) row] = area.removeFromTop (rowHeight);
        if (row == selectedRow)
        {
            auto frameBottom = rowRects[(size_t) row].getBottom();
            if (laneShown())
            {
                area.removeFromTop (3);
                laneArea = area.removeFromTop (laneHeight);
                frameBottom = laneArea.getBottom();
            }
            selectionFrame = juce::Rectangle<int>::leftTopRightBottom (std::max (1, labelColumn.getX() - 3), rowRects[(size_t) row].getY() - 3,
                                                                       stepsColumn.getRight() + 3, frameBottom + 3);
        }
        area.removeFromTop (gap);
    }
    rowsBottom = std::max (rowRects[(size_t) (rowScroll + visibleRows - 1)].getBottom(), laneArea.getBottom());

    // Varje rad: rattarna (volym och FX) längst till vänster med radens längd under, och till höger om dem namnet med
    // högtalaren (lyssna) och loop-knappen under. Låga rader lägger allt på en rad (radens längd döljs).
    for (size_t i = 0; i < rowKnobs.size(); ++i)
    {
        const auto shown = (int) i < rows && ! rowRects[i].isEmpty();
        rowKnobs[i]->level.setVisible (shown);
        rowKnobs[i]->fx.setVisible (shown);
        if (i < lengthBoxes.size())
            lengthBoxes[i]->setVisible (shown && ! compactRows);
        if (i < listenButtons.size())
        {
            listenButtons[i]->setVisible (shown && ! processor.isEffectVersion());
            loopToggles[i]->setVisible (shown);
        }
        if (i < rowSwitches.size())
        {
            rowSwitches[i]->mute.setVisible (shown && ! processor.isEffectVersion());
            rowSwitches[i]->solo.setVisible (shown && ! processor.isEffectVersion());
        }
        if (! shown)
            continue;

        const auto row = rowRects[i];
        rowKnobs[i]->level.setBounds (knobBounds (row, 0));
        rowKnobs[i]->fx.setBounds (knobBounds (row, 1));
        if (i < lengthBoxes.size())
            lengthBoxes[i]->setBounds (labelColumn.getX() + 1, row.getY() + 1 + knobPx + 1, 60, 18);
        if (i < listenButtons.size())
        {
            const auto buttonHeight = compactRows ? std::min (24, rowHeight - 4) : 24;
            const auto buttonsY = compactRows ? row.getY() + (rowHeight - buttonHeight) / 2 : row.getY() + 18;
            const auto buttonsX = labelColumn.getX() + (compactRows ? 2 * (knobPx + 2) + 4 : 66);
            listenButtons[i]->setBounds (buttonsX, buttonsY, 30, buttonHeight);
            loopToggles[i]->setBounds (buttonsX + 34, buttonsY, 28, buttonHeight);
        }
        if (i < rowSwitches.size())
        {
            // M och S: en tredje kolumn till höger (ovanpå varandra), eller efter loop-knappen i en låg rad.
            if (compactRows)
            {
                const auto height = std::min (24, rowHeight - 4);
                const auto x = labelColumn.getX() + 2 * (knobPx + 2) + 4 + 66;
                rowSwitches[i]->mute.setBounds (x, row.getY() + (rowHeight - height) / 2, 24, height);
                rowSwitches[i]->solo.setBounds (x + 26, row.getY() + (rowHeight - height) / 2, 24, height);
            }
            else
            {
                const auto height = std::max (14, std::min (22, (rowHeight - 4) / 2));
                const auto x = labelColumn.getX() + 132;
                rowSwitches[i]->mute.setBounds (x, row.getY() + 1, 26, height);
                rowSwitches[i]->solo.setBounds (x, row.getY() + 3 + height, 26, height);
            }
        }
    }
}

juce::Rectangle<int> StepSequencer::selectorBounds (juce::Rectangle<int> row) const
{
    // Väljaren: namnet med en radioknapp. Ovanför högtalaren och loop-knappen, eller i en låg rad efter dem.
    if (compactRows)
    {
        const auto x = labelColumn.getX() + 2 * (knobPx + 2) + 4 + 68 + 52;
        return juce::Rectangle<int>::leftTopRightBottom (x, row.getCentreY() - 9, labelColumn.getRight() - 2, row.getCentreY() + 9);
    }
    return labelColumn.withTrimmedLeft (66).withTrimmedRight (30).withY (row.getY()).withHeight (18);
}

juce::Rectangle<float> StepSequencer::stepBounds (juce::Rectangle<int> row, int step, int numSteps) const
{
    const auto width = (float) row.getWidth() / (float) numSteps;
    return { row.getX() + step * width + 1.0f, (float) row.getY(), std::max (1.0f, width - 2.0f), (float) row.getHeight() };
}

int StepSequencer::stepAt (int x, int layer, int numSteps, int ownSteps) const
{
    const auto fraction = (float) (x - stepsColumn.getX()) / (float) stepsColumn.getWidth();
    const auto absolute = pageOffset (numSteps) + juce::jlimit (0, numSteps - 1, (int) std::floor (fraction * (float) numSteps));
    if (absolute < ownSteps)
        return absolute;
    return lengths().loops[(size_t) rowOf (layer)] ? absolute % ownSteps : -1;
}

StepSequencer::Target StepSequencer::targetAt (juce::Point<int> point, int& layer) const
{
    layer = selectedLayer;
    const auto inLabels = point.x < stepsColumn.getX();

    // Rubrikremsan (översikten/sidväljaren) ovanför raderna: ett klick där ska aldrig träffa raden under (kickens).
    if (point.y < rowsTop)
        return Target::none;

    if (! laneArea.isEmpty() && point.y >= laneArea.getY() - 2 && point.y < laneArea.getBottom() + 2)
        return inLabels ? Target::none : Target::lane;

    for (int row = 0; row < 1 + numLayers(); ++row)
    {
        const auto& rect = rowRects[(size_t) row];
        if (! rect.isEmpty() && point.y < rect.getBottom() + 2)
        {
            layer = row == 0 ? Pattern::kickLayer : row - 1;
            return inLabels ? Target::label : Target::bass;
        }
    }
    return Target::none;
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

void StepSequencer::paintToggleRow (juce::Graphics& g, juce::Rectangle<int> row, int numSteps, int offset, int ownSteps, bool loops, juce::Colour colour,
                                    int playingStep, const std::function<bool (int)>& isOn, bool dimmedRow) const
{
    for (int step = 0; step < numSteps; ++step)
    {
        const auto bounds = stepBounds (row, step, numSteps).reduced (0.5f, 1.0f);
        if (! g.clipRegionIntersects (bounds.expanded (2.0f).getSmallestIntegerContainer()))
            continue; // utanför det som ritas om (t.ex. när bara markören flyttats)
        const auto absolute = offset + step;
        const auto beyond = absolute >= ownSteps;       // efter radens egen längd
        const auto silent = beyond && ! loops;          // raden spelar bara en gång
        const auto source = beyond ? absolute % ownSteps : absolute;
        const auto on = ! silent && isOn (source);
        const auto playing = ! silent && source == playingStep;
        const auto dimmed = dimmedRow || beyond;
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
            if (silent)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
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
    const auto numSteps = instrumentSteps (selectedLayer);
    const auto lengthsNow = lengths();
    const auto ownSteps = instrumentOwnSteps (lengthsNow, selectedLayer);
    const auto area = laneArea.toFloat();
    const auto colourOn = colourOf (selectedLayer);

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
        const auto absolute = pageOffset (numSteps) + step;
        const auto beyond = absolute >= ownSteps;         // efter radens egen längd: dämpad kopia, eller tom utan loop
        if (beyond && ! lengthsNow.loops[(size_t) rowOf (selectedLayer)])
            continue;
        const auto* note = noteAt (selectedLayer, beyond ? absolute % ownSteps : absolute);
        const auto on = note != nullptr;
        const auto colour = (on ? colourOn : Theme::textDim.withAlpha (0.25f)).withMultipliedAlpha (beyond ? 0.45f : 1.0f);
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

    const auto totalTicks = lengths().barsOf (rowOf (layer)) * Pattern::ticksPerBar; // radens egen längd
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
    const auto gridBottom = mode == Mode::notes ? laneArea.getBottom() : rowsBottom;
    paintGrid (g, juce::Rectangle<int>::leftTopRightBottom (stepsColumn.getX(), rowsTop, stepsColumn.getRight(), gridBottom));

    if (! selectionFrame.isEmpty())
    {
        g.setColour (colourOf (selectedLayer).withAlpha (0.07f));
        g.fillRoundedRectangle (selectionFrame.toFloat(), 6.0f);
    }

    const auto lengthsNow = lengths();
    auto paintLayerRow = [&] (int layer)
    {
        const auto row = rowBounds (layer);
        if (row.isEmpty())
            return;
        const auto colour = colourOf (layer);
        const auto isKick = layer == Pattern::kickLayer;
        const auto playTick = isKick ? processor.transportStatus.kickStep.load() : processor.transportStatus.instrumentSteps[(size_t) layer].load();
        const auto dimmedRow = isKick ? mode == Mode::gate : layer != selectedLayer; // kicken är en vanlig rad, men har ingen gate

        if (mode == Mode::gate && ! isKick)
        {
            // GATE: en egen av/på-rytm med eget rutnät. Raden dämpas när lagrets gate är avstängd.
            const auto gateTicks = Pattern::ticksPerStep (gateGrid (layer));
            const auto gateIsOn = processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateOn))->load() >= 0.5f;
            paintToggleRow (g, row, gateSteps (layer), pageOffset (gateSteps (layer)), gateOwnSteps (lengthsNow, layer), lengthsNow.loops[(size_t) rowOf (layer)], colour, playTick < 0 ? -1 : playTick / gateTicks,
                            [this, layer] (int step) { return processor.pattern.isGateStepOn (layer, step); },
                            layer != selectedLayer || ! gateIsOn);
        }
        else if (isCustom (layer))
            paintMiniature (g, row, layer, colour, playTick, dimmedRow);
        else
            paintToggleRow (g, row, instrumentSteps (layer), pageOffset (instrumentSteps (layer)), instrumentOwnSteps (lengthsNow, layer), lengthsNow.loops[(size_t) rowOf (layer)], colour, stepOfTick (layer, playTick),
                            [this, layer] (int step) { return noteTickAt (layer, step) >= 0; }, dimmedRow);
    };
    paintLayerRow (Pattern::kickLayer);
    for (int layer = 0; layer < numLayers(); ++layer)
        paintLayerRow (layer);
    if (! processor.isEffectVersion())
    {
        // En rad som inte hörs (avstängd, eller något annat är soloat) tonas ner.
        auto dimRow = [&] (int layer)
        {
            const auto row = rowBounds (layer);
            if (! row.isEmpty() && rowSilent (layer))
            {
                g.setColour (juce::Colour (0xff0d0f1a).withAlpha (0.6f));
                g.fillRoundedRectangle (juce::Rectangle<int>::leftTopRightBottom (stepsColumn.getX(), row.getY(), stepsColumn.getRight(), row.getBottom()).toFloat(), 3.0f);
            }
        };
        dimRow (Pattern::kickLayer);
        for (int layer = 0; layer < numLayers(); ++layer)
            dimRow (layer);
    }
    if (laneShown())
        paintLane (g);

    // Etiketter till vänster; det valda lagret markeras.
    // Rubriker över kolumnerna: vad de små rattarna och rutan är.
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    for (int index = 0; index < 2; ++index)
        g.drawText (index == 0 ? "VOL" : "FX", juce::Rectangle<int> (knobPx + 2, headerLabels.getHeight()).withCentre ({ labelColumn.getX() + 1 + knobPx / 2 + index * (knobPx + 2), headerLabels.getCentreY() }),
                    juce::Justification::centred);

    g.setFont (Theme::font (11.0f, true));
    for (int rowIndex = 0; rowIndex < 1 + numLayers(); ++rowIndex)
    {
        const auto layer = rowIndex == 0 ? Pattern::kickLayer : rowIndex - 1;
        const auto bounds = rowBounds (layer);
        if (bounds.isEmpty())
            continue;
        const auto row = selectorBounds (bounds);
        const auto colour = colourOf (layer);
        const auto selected = layer == selectedLayer && (renameable() || layer == Pattern::kickLayer);
        if (selected)
        {
            g.setColour (colour.withAlpha (0.18f));
            g.fillRoundedRectangle (row.toFloat().reduced (0.0f, 1.0f), 4.0f);
        }

        // Väljaren: en radioknapp och namnet. Den valda raden är tänd; raden visar sina inställningar (rutnät, lane, pianorulle).
        const auto selector = row.toFloat().reduced (0.0f, 1.0f);
        g.setColour (colour.withAlpha (selected ? 0.5f : 0.14f));
        g.drawRoundedRectangle (selector.reduced (0.5f), 4.0f, 1.0f);
        const auto dot = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ selector.getX() + 9.0f, selector.getCentreY() });
        g.setColour (selected || layer == Pattern::kickLayer ? colour : colour.withAlpha (0.6f));
        g.drawEllipse (dot, 1.4f);
        if (selected)
            g.fillEllipse (dot.reduced (2.2f));
        g.drawFittedText (nameOf (layer), row.withTrimmedLeft (18), juce::Justification::centredLeft, 1, 0.8f);
    }
    if (bars() > barsPerPage)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.0f, true));
        g.drawText ("BARS " + juce::String (page * barsPerPage + 1) + "-" + juce::String (page * barsPerPage + viewBars()) + " OF " + juce::String (bars()),
                    juce::Rectangle<int> (pageArea.getX() + 66, pageArea.getY(), 104, pageArea.getHeight()), juce::Justification::centredLeft);
    }

    // Översikten: en linje per rad, lika lång som radens längd jämfört med hela mönstret, och en ram runt de takter som visas.
    if (! overviewArea.isEmpty())
    {
        const auto overview = overviewArea.toFloat();
        g.setColour (juce::Colour (0xff0d0f1a));
        g.fillRoundedRectangle (overview, 3.0f);
        const auto rowsNow = lengths();
        const auto lineCount = 1 + numLayers();
        const auto lineHeight = (overview.getHeight() - 2.0f) / (float) lineCount;
        for (int i = 0; i < lineCount; ++i)
        {
            const auto layer = i == 0 ? Pattern::kickLayer : i - 1;
            const auto fraction = (float) rowsNow.barsOf (rowOf (layer)) / (float) std::max (1, rowsNow.total);
            g.setColour (colourOf (layer).withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (overview.getX() + 1.0f, overview.getY() + 1.0f + (float) i * lineHeight,
                                                (overview.getWidth() - 2.0f) * fraction, std::max (1.0f, lineHeight - 1.0f)));
        }
        const auto viewFraction = (float) viewBars() / (float) std::max (1, bars());
        const auto window = juce::Rectangle<float> (overview.getX() + 1.0f + (overview.getWidth() - 2.0f) * viewFraction * (float) page, overview.getY(),
                                                    (overview.getWidth() - 2.0f) * viewFraction, overview.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (window);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawRect (window, 1.0f);
    }
    if (! selectionFrame.isEmpty())
    {
        g.setColour (colourOf (selectedLayer).withAlpha (0.9f));
        g.drawRoundedRectangle (selectionFrame.toFloat().reduced (0.5f), 6.0f, 1.8f);
    }
    if (mode == Mode::gate || laneArea.isEmpty())
        return;

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    g.drawText (laneName(), labelColumn.withY (laneArea.getY()).withHeight (14), juce::Justification::centredLeft);
    if (renameable() || selectedLayer == Pattern::kickLayer)
    {
        g.setColour (colourOf (selectedLayer));
        g.drawText (nameOf (selectedLayer), labelColumn.withY (laneArea.getY() + 14).withHeight (14), juce::Justification::centredLeft);
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
        case Lane::gate:     return "NOTE LENGTH";
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
        case Target::bass:
        {
            if (mode == Mode::gate)
            {
                if (dragLayer == Pattern::kickLayer)
                    return; // kicken har ingen gate
                const auto gateIndex = stepAt (point.x, dragLayer, gateSteps (dragLayer), gateOwnSteps (lengths(), dragLayer));
                if (gateIndex < 0)
                    return;
                auto& gateStep = processor.pattern.gateStep (dragLayer, gateIndex);
                if (firstEvent)
                    paintOn = ! gateStep.load();
                gateStep = paintOn;
                break;
            }
            if (isCustom (dragLayer))
                return; // pianorulle: stegen redigeras inte här

            const auto step = stepAt (point.x, dragLayer, instrumentSteps (dragLayer), instrumentOwnSteps (lengths(), dragLayer));
            if (step < 0)
                return;
            const auto tps = ticksPerStep (dragLayer);
            if (firstEvent)
                paintOn = noteTickAt (dragLayer, step) < 0;

            if (paintOn)
            {
                if (noteTickAt (dragLayer, step) < 0)
                    processor.pattern.addNote (dragLayer, step * tps, juce::roundToInt (Pattern::defaultGate * (float) tps), 0, Pattern::defaultVelocity (dragLayer));
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
            auto* note = noteAt (selectedLayer, laneStepAt (point.x));
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

void StepSequencer::setPageFromOverview (int x)
{
    const auto fraction = juce::jlimit (0.0f, 0.999f, (float) (x - overviewArea.getX()) / (float) std::max (1, overviewArea.getWidth()));
    setPage ((int) (fraction * (float) bars() / (float) barsPerPage));
}

void StepSequencer::mouseDown (const juce::MouseEvent& event)
{
    // Översikten: klicka eller dra för att byta sida.
    if (overviewArea.contains (event.getPosition()) && bars() > barsPerPage)
    {
        overviewDragging = true;
        setPageFromOverview (event.x);
        return;
    }

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
        if (auto* note = noteAt (selectedLayer, laneStepAt (event.x)))
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
    if (overviewDragging)
    {
        setPageFromOverview (event.x);
        return;
    }
    editAt (event.getPosition(), false);
    if (dragTarget == Target::lane)
        hoverStep = laneStepAt (event.x);
}

void StepSequencer::mouseDoubleClick (const juce::MouseEvent& event)
{
    int layer = 0;
    if (targetAt (event.getPosition(), layer) == Target::label)
        renameLayer (layer);
}

void StepSequencer::mouseUp (const juce::MouseEvent&)
{
    if (overviewDragging)
    {
        overviewDragging = false;
        return;
    }
    if (edited)
        processor.patternChanged();

    dragTarget = Target::none;
}

void StepSequencer::mouseMove (const juce::MouseEvent& event)
{
    int layer = 0;
    const auto step = targetAt (event.getPosition(), layer) == Target::lane ? laneStepAt (event.x) : -1;
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
    // Skrollhjulet ändrar tonhöjden en halvton i taget i pitch-lanet, och scrollar raderna när det finns fler än som får plats.
    int layer = 0;
    const auto target = targetAt (event.getPosition(), layer);
    if (wheel.deltaY != 0.0f && target != Target::lane && 1 + numLayers() > visibleRows)
    {
        scrollRows (wheel.deltaY > 0.0f ? -1 : 1);
        return;
    }
    if (lane != Lane::pitch || target != Target::lane || wheel.deltaY == 0.0f)
        return;

    if (auto* note = noteAt (selectedLayer, laneStepAt (event.x)))
    {
        note->pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, note->pitch.load() + (wheel.deltaY > 0.0f ? 1 : -1));
        processor.patternChanged();
        repaint();
    }
}
