#include "PianoRoll.h"
#include "../Core/RollFiles.h"
#include "LengthMenu.h"
#include "HelpDialog.h"

namespace
{
    constexpr float keyWidth = 46.0f, toolbarHeight = 46.0f, rulerHeight = 28.0f, edgeGrab = 6.0f, barSize = 11.0f, hintHeight = 16.0f;
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
            if (Pattern::ticksPerStep ((InstrumentGrid) g) == ticks)
                return Pattern::gridNames()[g];
        return juce::String (ticks) + " ticks";
    }
}

namespace
{
    // Fas P: ackorden i högerklicksmenyn (halvtoner från tonen).
    struct ChordChoice { const char* name; Pattern::Chord chord; };
    const std::array<ChordChoice, 11> chordChoices { {
        { "None", { 0, 0, 0 } },          { "Octave up (+12)", { 12, 0, 0 } },  { "Fifth (+7)", { 7, 0, 0 } },
        { "Power (+7 +12)", { 7, 12, 0 } }, { "Major (+4 +7)", { 4, 7, 0 } },  { "Minor (+3 +7)", { 3, 7, 0 } },
        { "Sus4 (+5 +7)", { 5, 7, 0 } },  { "Major 7 (+4 +7 +11)", { 4, 7, 11 } }, { "Minor 7 (+3 +7 +10)", { 3, 7, 10 } },
        { "Octave down (-12)", { -12, 0, 0 } }, { "Minor + low octave (-12 +3 +7)", { -12, 3, 7 } } } };
}

PianoRoll::PianoRoll (PluginBase& p, std::vector<juce::Colour> colours) : processor (p), layerColours (std::move (colours))
{
    setWantsKeyboardFocus (true);
    for (const auto c : juce::SystemStats::getEnvironmentVariable ("KB_LANES", {})) // snapshot-verktyget: en siffra per extra lane (1 = CUTOFF, 2 = PAN, 3 = FX SEND)
        if (c >= '1' && c < '1' + numLaneParams - 1 && lanes.size() < (size_t) maxLanes)
            lanes.push_back (Lane { (int) (c - '0'), 40.0f });
    setTooltip ("PIANO ROLL - Click an empty spot to add a note, drag to move it, right-click a note to delete it. "
                "Scroll = pitch, Ctrl + scroll = zoom, Shift + scroll = time. The strip at the bottom is velocity. "
                "Click HELP (bottom right) for tools, keys, chords and glide.");

    presetButton.setTooltip ("PRESET - Save this layer's piano roll as a preset, or load a saved one into this layer. Saved piano rolls can be loaded into any layer.");
    presetButton.onClick = [this] { showPresetMenu(); };
    midiButton.setTooltip ("MIDI - Export this piano roll as a MIDI file (to use in your DAW or another synth), or import a MIDI file into this layer. "
                           "Glide notes are exported as ordinary notes.");
    midiButton.onClick = [this] { showMidiMenu(); };

    followButton.setClickingTogglesState (true);
    followButton.setToggleState (processor.followPlayhead.load(), juce::dontSendNotification);
    followButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
    followButton.setTooltip ("FOLLOW - On: the view flips to the bars that play, so you follow the song. Off: stay where you are.");
    followButton.onClick = [this] { processor.followPlayhead = followButton.getToggleState(); };
    ghostButton.setTooltip ("GHOST NOTES - Choose which other rows show their notes faintly behind this one (kick, each instrument). Display only; "
                            "it does not change any sound. Default: all on.");
    ghostButton.onClick = [this] { showGhostMenu(); };

    for (auto* button : { &drawButton, &selectButton, &eraseButton, &copyButton, &pasteButton, &pasteOkButton, &pasteCancelButton })
    {
        button->setMouseClickGrabsKeyboardFocus (false); // Enter och Esc ska gå till pianorullen, inte till knappen
        button->setWantsKeyboardFocus (false);
    }
    drawButton.setTooltip ("PENCIL - Click in an empty spot to draw a note; click another pitch at the same tick to stack a note (up to four). "
                           "Drag a note to move it, or drag its right edge to change its length.");
    selectButton.setTooltip ("SELECT - Drag a box in an empty spot to select the notes it touches, like selecting icons on the desktop. "
                             "(Holding Shift does the same with any tool.) Then move, copy or delete them together.");
    eraseButton.setTooltip ("ERASER - Click a note, or drag across several, to remove them. Holding Alt turns any tool into the eraser while it is held.");
    drawButton.onClick = [this] { setTool (Tool::draw); };
    selectButton.onClick = [this] { setTool (Tool::select); };
    eraseButton.onClick = [this] { setTool (Tool::erase); };
    for (auto* button : { &drawButton, &selectButton, &eraseButton })
        button->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);

    copyButton.setTooltip ("COPY - Copies the selected notes (Ctrl + C). Then PASTE (Ctrl + V) to place them, by default right after the ones you copied.");
    copyButton.onClick = [this] { copySelection (false); grabKeyboardFocus(); };
    pasteButton.setTooltip ("PASTE - Shows the copied notes (in another colour with a red border) right after the ones you copied. Click or drag to choose "
                            "where they go, then OK (Enter) to place them or CANCEL (Esc).");
    pasteButton.onClick = [this] { startPaste(); grabKeyboardFocus(); };
    pasteOkButton.setButtonText (juce::String::fromUTF8 ("\xE2\x9C\x93  OK"));
    pasteOkButton.setTooltip ("OK - Places the pasted notes where they are shown (Enter).");
    pasteOkButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f7d4a));
    pasteOkButton.onClick = [this] { commitPaste(); grabKeyboardFocus(); };
    pasteCancelButton.setButtonText (juce::String::fromUTF8 ("\xE2\x9C\x95  CANCEL"));
    pasteCancelButton.setTooltip ("CANCEL - Throws the paste away (Esc).");
    pasteCancelButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8a2f35));
    pasteCancelButton.onClick = [this] { cancelPaste(); grabKeyboardFocus(); };
    listenButton.setTooltip ("LISTEN - Hold to hear just this piano roll on the preview note (the PREVIEWS note at the top, C5 by default), looping while you hold. "
                             "Press again to start over; double-click to let it play until you click again.");
    listenButton.onStart = [this] { syncPreviewWindow(); processor.requestAudition (PluginBase::previewRequest (baseLayer)); };
    listenButton.onStop = [this] { processor.requestAudition (PluginBase::previewStop); };
    midiInBox.addItem ("MIDI IN: ALL", 1);
    midiInBox.addItem ("MIDI IN: INSTRUMENT", 2);
    midiInBox.setSelectedId (2, juce::dontSendNotification);
    midiInBox.setTooltip ("MIDI IN - What the keyboard / MIDI plays while the piano roll is open. INSTRUMENT (default): the notes you play are heard on this row's sound, "
                          "live (the pattern does not play) and the keys light up on the piano. ALL: every row plays its pattern as usual. It goes back to ALL when you close the piano roll.");
    midiInBox.onChange = [this] { applyMidiIn(); grabKeyboardFocus(); };
    addAndMakeVisible (midiInBox);
    copyButton.setVisible (false);
    pasteButton.setVisible (false);
    pasteOkButton.setVisible (false);
    pasteCancelButton.setVisible (false);

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

    for (auto* c : std::initializer_list<juce::Component*> { &snapButton, &gridBox, &smallerButton, &biggerButton, &allButton, &deleteButton, &glideButton, &followButton, &ghostButton, &presetButton, &midiButton, &drawButton, &selectButton, &eraseButton,
                                                                 &copyButton, &pasteButton, &pasteOkButton, &pasteCancelButton, &listenButton, &popOutButton, &helpButton, &addLaneButton, &hBar, &vBar })
        addAndMakeVisible (c);
    addLaneButton.setTooltip ("+ LANE - Adds a lane under the notes (up to four in all). Each lane shows one value per note, like the VEL lane; choose the value (CUTOFF, PAN or FX SEND) by clicking the lane's name. "
                              "Click or drag in a lane to set that note's value (ALT + click removes it, so the note follows the knob again). "
                              "Drag the thin bar above a lane to make the lanes taller or lower.");
    addLaneButton.onClick = [this] { addLane(); };
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::pianoRoll, "the PIANO ROLL (tools, keys, chords, glide)");
    popOutButton.setTooltip ("POP OUT - Opens the piano roll in its own window that you can move and resize (for a second screen). Click DOCK, or close the window, to bring it back.");
    popOutButton.onClick = [this] { if (onPopOut) onPopOut(); };
    hBar.addListener (this);
    vBar.addListener (this);
    hBar.setAutoHide (false);
    vBar.setAutoHide (false);
    hBar.onZoom = [this] (double start, double size)
    {
        pixelsPerTick = (float) (gridArea.getWidth() / std::max (1.0, size));
        scrollTicks = (float) start;
        clampView();
        repaint();
    };
    vBar.onZoom = [this] (double start, double size)
    {
        rowH = juce::jlimit (7.0f, 60.0f, (float) (gridArea.getHeight() / std::max (1.0, size)));
        topPitch = (float) (highPitch() - start);
        clampView();
        repaint();
    };
    hBar.setSingleStepSize (24.0);
    vBar.setSingleStepSize (1.0);

    lengthButton.onClick = [this] { LengthMenu::show (processor, layer, lengthButton, [this] { selection.clear(); repaint(); }); };
    addAndMakeVisible (lengthButton);
    extendButton.setTooltip ("+ BAR - Adds one empty bar at the end of the row (the row stays that long until you put notes there or change LENGTH). "
                             "You can also click in the shaded bar after the end of the row: the row grows with the notes and shrinks again when you delete them (LENGTH: AUTO).");
    extendButton.onClick = [this] { extendRow(); grabKeyboardFocus(); };
    addAndMakeVisible (extendButton);
    voicesLabel.setJustificationType (juce::Justification::centred);
    voicesLabel.setFont (Theme::font (12.0f));
    voicesLabel.setColour (juce::Label::textColourId, Theme::textDim);
    voicesLabel.setTooltip ("VOICES - how many notes this instrument can play at once (chords and overlapping notes). Same setting as VOICES on the INSTR page.");
    voicesDownButton.setTooltip ("Fewer VOICES");
    voicesUpButton.setTooltip ("More VOICES");
    voicesDownButton.onClick = [this] { changeVoices (-1); grabKeyboardFocus(); };
    voicesUpButton.onClick = [this] { changeVoices (1); grabKeyboardFocus(); };
    for (auto* c : std::initializer_list<juce::Component*> { &voicesLabel, &voicesDownButton, &voicesUpButton })
        addAndMakeVisible (c);

    attachGridBox();
    updateToolButtons();
    updateToolbarVisibility();
    startVisualTimer (Settings::Visual::other, 30);
}

void PianoRoll::extendRow()
{
    if (bars() >= Pattern::maxBars)
        return;
    // En tom takt till: längden är minst så lång tills innehållet nått dit (då följer den innehållet igen).
    const auto before = processor.snapshotRow (layer);
    processor.pattern.setExtraBars (layer, bars() + 1);
    processor.patternChanged();
    processor.recordRowEdit (layer, before, "Add bar");
    updateScrollBars();
    repaint();
}

void PianoRoll::attachGridBox()
{
    gridAttachment.reset();
    gridAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, gridParameterId(), gridBox);
}

void PianoRoll::setLayer (int newLayer)
{
    // Ett rad-id: en vanlig NOTE-rad/kicken (0..10) eller skivraden Kind::slices (id från 11). Annat blir rad 0.
    auto instrument = 0;
    auto kind = RowRegistry::Kind::note;
    const auto isSliceRow = newLayer >= RowRegistry::firstSubRow && processor.pattern.rows.get (newLayer, instrument, kind)
                            && kind == RowRegistry::Kind::slices;
    if (! isSliceRow)
        newLayer = juce::jlimit (0, Pattern::numNoteLayers - 1, newLayer);
    if (newLayer != layer || isSliceRow != sliceMode)
    {
        const auto wasSliceMode = sliceMode;
        layer = newLayer;
        sliceMode = isSliceRow;
        baseLayer = isSliceRow ? instrument : newLayer;
        if (sliceMode || wasSliceMode)
            topPitch = sliceMode ? (float) (lowPitch() + visibleRows() - 1) : (float) visibleRows() * 0.5f; // skivor: Slice 1 längst ner; annars grundtonen i mitten
        askedVoicesNeed = 0;
        selection.clear();
        justCreated = {};
        hoverKey = {};
        dragKey = {};
        dragStartKey = {};
        attachGridBox();
        applyMidiIn();
        auditionStop (0);
        cursorTick = 0;
        loopStart = loopEnd = -1;
        if (isShowing())
        {
            processor.openRollLayer = baseLayer;
            syncPreviewWindow();
        }
    }
    repaint();
}

void PianoRoll::applyMidiIn()
{
    processor.midiInLayer = isShowing() && midiInBox.getSelectedId() == 2 ? baseLayer : -1;
}

void PianoRoll::visibilityChanged()
{
    applyMidiIn();
    processor.openRollLayer = isShowing() ? baseLayer : -1;
    if (isShowing())
        syncPreviewWindow();
    else
    {
        auditionStop (0);
        processor.postAuditionAllOff (baseLayer); // H2: ingen ton får hänga kvar när rullen stängs
        clearPreviewWindow();
    }
}

PianoRoll::~PianoRoll()
{
    auditionStop (0);
    processor.postAuditionAllOff (baseLayer);
    clearPreviewWindow();
    processor.openRollLayer = -1;
    processor.midiInLayer = -1;
}

void PianoRoll::syncPreviewWindow()
{
    processor.previewFromTick = cursorTick;
    processor.previewLoopStart = hasLoop() ? loopStart : -1;
    processor.previewLoopEnd = hasLoop() ? loopEnd : -1;
}

void PianoRoll::clearPreviewWindow()
{
    processor.previewFromTick = 0;
    processor.previewLoopStart = processor.previewLoopEnd = -1;
}

void PianoRoll::playThisRow()
{
    syncPreviewWindow();
    processor.requestAudition (processor.previewingLayer.load() == baseLayer ? PluginBase::previewStop : PluginBase::previewRequest (baseLayer));
}

void PianoRoll::clearLoop()
{
    if (! hasLoop())
        return;
    loopStart = loopEnd = -1;
    syncPreviewWindow();
    repaint();
}

void PianoRoll::auditionStart (int pitch, float velocity)
{
    // Skivor: skivans index är "MIDI-tonen" (unik per skiva, så att noteOff hittar rätt) och skickas även som skiva.
    const auto midi = sliceMode ? juce::jlimit (0, 127, RowRegistry::sliceOfPitch (pitch)) : juce::jlimit (0, 127, rootNote() + pitch);
    if (auditionPitch == midi)
        return;
    if (auditionPitch >= 0)
        processor.postNoteAudition (baseLayer, auditionPitch, 64, false);
    auditionPitch = midi;
    auditionStartedAt = juce::Time::getMillisecondCounterHiRes();
    processor.postNoteAudition (baseLayer, midi, (int) (velocity * 127.0f), true, sliceMode ? midi : -1);
}

// Tonen klingar så länge musen hålls, och minst minimumTicks (tonens längd).
void PianoRoll::auditionStop (int minimumTicks)
{
    if (auditionPitch < 0)
        return;
    const auto note = auditionPitch;
    auditionPitch = -1;
    const auto noteMs = (double) minimumTicks / (double) Pattern::ticksPerQuarter * 60000.0 / std::max (20.0, processor.transportStatus.bpm.load());
    const auto leftMs = juce::jlimit (0.0, 4000.0, noteMs - (juce::Time::getMillisecondCounterHiRes() - auditionStartedAt));
    if (leftMs < 20.0)
    {
        processor.postNoteAudition (baseLayer, note, 64, false);
        return;
    }
    juce::Component::SafePointer<PianoRoll> safeThis (this);
    const auto row = baseLayer;
    juce::Timer::callAfterDelay ((int) leftMs, [safeThis, note, row]
    {
        if (safeThis != nullptr && safeThis->auditionPitch != note)
            safeThis->processor.postNoteAudition (row, note, 64, false);
    });
}

int PianoRoll::sliceCount() const
{
    const auto* sample = processor.instrumentSampleSlot (baseLayer).active();
    return sample != nullptr ? (int) sample->slices.size() : 0;
}

int PianoRoll::bars() const
{
    return processor.pattern.lengthBars (layer); // radens egen längd (AUTO: innehållet, eller ett valt antal)
}

juce::String PianoRoll::gridParameterId() const
{
    return baseLayer == Pattern::kickLayer ? juce::String (ParamID::rowGridKick) : ParamID::bass (baseLayer, InstrumentField::grid);
}

int PianoRoll::playTickNow() const
{
    return baseLayer == Pattern::kickLayer ? processor.transportStatus.kickStep.load() : processor.transportStatus.instrumentSteps[(size_t) baseLayer].load();
}

juce::String PianoRoll::layerName() const
{
    return baseLayer == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (baseLayer);
}

InstrumentGrid PianoRoll::grid() const
{
    return (InstrumentGrid) juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (gridParameterId())->load());
}

int PianoRoll::snapTicks() const { return snapEnabled ? Pattern::ticksPerStep (grid()) : 1; }

int PianoRoll::snapped (float tick) const
{
    const auto step = snapTicks();
    return juce::jlimit (0, editTicks() - 1, (int) std::floor (tick / (float) step + 0.0001f) * step);
}

int PianoRoll::rootNote() const
{
    return processor.rowRootNote (baseLayer);
}

int PianoRoll::rootPitchClass() const { return rootNote() % 12; }

float PianoRoll::ticksToX (float tick) const { return gridArea.getX() + (tick - scrollTicks) * pixelsPerTick; }
float PianoRoll::xToTicks (float x) const { return scrollTicks + (x - gridArea.getX()) / pixelsPerTick; }
float PianoRoll::pitchToY (int pitch) const { return gridArea.getY() + (topPitch - (float) pitch) * rowHeight(); }
// Raden för tonhöjden p har sin överkant vid topPitch - p (se pitchToY), alltså ligger y i raden p när topPitch - p <= d < topPitch - p + 1: p = ceil (topPitch - d).
// (floor gav raden under pekaren: krysset låg en rad ovanför tonen man siktade på.)
int PianoRoll::yToPitch (float y) const { return (int) std::ceil (topPitch - (y - gridArea.getY()) / rowHeight()); }

void PianoRoll::fitToWidth()
{
    // Högst fyra takter i bild åt gången; längre mönster skrollas (eller zoomas ut med Ctrl + skroll).
    pixelsPerTick = gridArea.getWidth() / (float) std::max (1, std::min (totalTicks(), 4 * Pattern::ticksPerBar));
    scrollTicks = 0.0f;
}

void PianoRoll::clampView()
{
    const auto fit = gridArea.getWidth() / (float) std::max (1, viewTicks());
    pixelsPerTick = juce::jlimit (fit, std::max (fit, maxPixelsPerTick), pixelsPerTick);
    const auto visibleTicks = gridArea.getWidth() / pixelsPerTick;
    scrollTicks = juce::jlimit (0.0f, std::max (0.0f, (float) viewTicks() - visibleTicks), scrollTicks);

    // Raderna: hela tonhöjdsområdet ska gå att nå, men inte mer.
    const auto rows = (float) visibleRows();
    topPitch = juce::jlimit ((float) lowPitch() + rows - 1.0f, (float) highPitch(), topPitch);
    if (rows > (float) (highPitch() - lowPitch() + 1))
        topPitch = (float) highPitch();

    updateScrollBars();
}

void PianoRoll::updateScrollBars()
{
    hBar.setRangeLimits (0.0, (double) std::max (1, viewTicks()), juce::dontSendNotification);
    hBar.setCurrentRange (scrollTicks, (double) (gridArea.getWidth() / std::max (0.01f, pixelsPerTick)), juce::dontSendNotification);

    vBar.setRangeLimits (0.0, (double) (highPitch() - lowPitch() + 1), juce::dontSendNotification);
    vBar.setCurrentRange ((double) (highPitch() - topPitch), (double) visibleRows(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &hBar)
    {
        scrollTicks = (float) newRangeStart;
        processor.followPlayhead = false; // en hand på sidoscrollbaren: FOLLOW stängs av (knappen visar det i nästa timertick)
    }
    else
        topPitch = (float) highPitch() - (float) newRangeStart;
    clampView();
    repaint();
}

void PianoRoll::showGhostMenu()
{
    const auto mask = processor.ghostMask.load();
    juce::PopupMenu menu;
    menu.addItem (1, "ALL ON");
    menu.addItem (2, "ALL OFF");
    menu.addSeparator();
    std::vector<int> rows { Pattern::kickLayer };
    for (int i = 0; i < processor.activeInstruments(); ++i)
        rows.push_back (i);
    for (auto row : rows)
    {
        juce::PopupMenu::Item item (row == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (row));
        item.itemID = 100 + row;
        item.isTicked = (mask & (1u << row)) != 0;
        item.colour = layerColours[(size_t) juce::jlimit (0, (int) layerColours.size() - 1, row)];
        item.isEnabled = row != baseLayer; // den egna raden är ingen spökrad
        menu.addItem (item);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&ghostButton), [this, safe = juce::Component::SafePointer<PianoRoll> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        auto m = processor.ghostMask.load();
        if (result == 1)       m = 0xffffffffu;
        else if (result == 2)  m = 0u;
        else                   m ^= 1u << (result - 100);
        processor.ghostMask = m;
        repaint();
        showGhostMenu(); // flervalet: menyn står kvar tills man klickar utanför
    });
}

int PianoRoll::soundingRoot() const
{
    if (baseLayer == Pattern::kickLayer)
        return rootNote();
    return rootNote() + 12 * (int) processor.apvts.getRawParameterValue (ParamID::bass (baseLayer, InstrumentField::octave))->load();
}

void PianoRoll::afterLoad (InstrumentGrid newGrid)
{
    if (auto* gridParameter = processor.apvts.getParameter (gridParameterId()))
        Presets::setParameter (*gridParameter, gridParameter->convertTo0to1 ((float) newGrid));
    selection.clear();
    processor.patternChanged();
    repaint();
}

void PianoRoll::showPresetMenu()
{
    const auto files = RollFiles::list();
    juce::PopupMenu menu;
    if (! files.isEmpty())
    {
        menu.addSectionHeader ("SAVED PIANO ROLLS");
        for (int i = 0; i < files.size(); ++i)
            menu.addItem (100 + i, files[i].getFileNameWithoutExtension());
        menu.addSeparator();
    }
    menu.addItem (1, "Save this piano roll as a preset...");
    menu.addItem (2, "Open the preset folder");

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [safeThis, files] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        if (result == 1)
            safeThis->askForNameAndSave();
        else if (result == 2)
        {
            RollFiles::folder().createDirectory();
            RollFiles::folder().startAsProcess();
        }
        else if (juce::isPositiveAndBelow (result - 100, files.size()))
        {
            auto newGrid = InstrumentGrid::sixteenth;
            PluginBase::EditScope undo (safeThis->processor, "Load piano roll");
            if (RollFiles::load (safeThis->processor.pattern, safeThis->layer, files[result - 100], newGrid))
                safeThis->afterLoad (newGrid);
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Piano roll", "Could not read that piano roll file.");
        }
    });
}

void PianoRoll::askForNameAndSave()
{
    auto* window = new juce::AlertWindow ("Save piano roll", "Name:", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    saveDialog = window;
    window->addTextEditor ("name", "My piano roll");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;
        const auto name = window->getTextEditorContents ("name").trim();
        const auto fileName = juce::File::createLegalFileName (name);
        if (fileName.isEmpty() || ! RollFiles::save (safeThis->processor.pattern, safeThis->layer, safeThis->grid(),
                                                      RollFiles::folder().getChildFile (fileName + ".roll")))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save", "Check that the name is valid: " + name);
    }), true);
}

void PianoRoll::showMidiMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Export as a MIDI file...");
    menu.addItem (2, "Import a MIDI file into this layer...");

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&midiButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        const auto folder = Settings::exportsFolder();
        folder.createDirectory();

        if (result == 1)
        {
            safeThis->chooser = std::make_unique<juce::FileChooser> ("Export the piano roll as MIDI", folder.getChildFile ("Kickbase Instr " + juce::String (safeThis->layer + 1) + ".mid"), "*.mid");
            safeThis->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                            [safeThis] (const juce::FileChooser& fc)
                                            {
                                                if (safeThis == nullptr || fc.getResult() == juce::File())
                                                    return;
                                                const auto file = fc.getResult().withFileExtension ("mid");
                                                if (! RollFiles::exportMidi (safeThis->processor.pattern, safeThis->layer, safeThis->bars(), safeThis->processor.transportStatus.bpm.load(),
                                                                             safeThis->soundingRoot(), file))
                                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", "Could not write " + file.getFullPathName());
                                                else if (const auto slides = RollFiles::countSlideNotes (safeThis->processor.pattern, safeThis->layer); slides > 0)
                                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MIDI exported",
                                                                                            juce::String (slides) + " glide note(s) were written as plain notes, because MIDI has no glide notes.");
                                            });
        }
        else
        {
            safeThis->chooser = std::make_unique<juce::FileChooser> ("Import a MIDI file", folder, "*.mid;*.midi");
            safeThis->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                            [safeThis] (const juce::FileChooser& fc)
                                            {
                                                if (safeThis == nullptr || ! fc.getResult().existsAsFile())
                                                    return;
                                                auto skippedNotes = 0;
                                                PluginBase::EditScope undo (safeThis->processor, "Import MIDI");
                                                if (RollFiles::importMidi (safeThis->processor.pattern, safeThis->layer, fc.getResult(), safeThis->soundingRoot(), &skippedNotes))
                                                {
                                                    safeThis->afterLoad (safeThis->grid());
                                                    if (skippedNotes > 0)
                                                        safeThis->flash (juce::String (skippedNotes) + " MIDI notes were outside the pattern or exceeded the stack limit");
                                                }
                                                else
                                                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", "No notes found in that MIDI file.");
                                            });
        }
    });
}

PianoRoll::Tool PianoRoll::effectiveTool() const
{
    if (altHeld && drag != Drag::box)
        return Tool::erase; // Alt hålls: sudd, vilket verktyg som än är valt
    return (shiftHeld || drag == Drag::box) ? Tool::select : tool;
}

void PianoRoll::setTool (Tool newTool)
{
    tool = newTool;
    updateToolButtons();
    updateCursor (getMouseXYRelative().toFloat(), hoverEdge);
    grabKeyboardFocus();
}

void PianoRoll::updateToolButtons()
{
    const auto effective = effectiveTool();
    drawButton.setToggleState (effective == Tool::draw, juce::dontSendNotification);
    selectButton.setToggleState (effective == Tool::select, juce::dontSendNotification);
    eraseButton.setToggleState (effective == Tool::erase, juce::dontSendNotification);
}

void PianoRoll::updateToolbarVisibility()
{
    const auto hasClipboard = ! processor.noteClipboard.notes.empty();
    copyButton.setVisible (! pasting && ! selection.empty());   // bara när något är markerat
    pasteButton.setVisible (! pasting && hasClipboard);
    pasteOkButton.setVisible (pasting);
    pasteCancelButton.setVisible (pasting);
}

void PianoRoll::updateCursor (juce::Point<float> position, bool onNoteEdge)
{
    if (! gridArea.contains (position))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }
    if (pasting)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }
    switch (effectiveTool())
    {
        case Tool::draw:   setMouseCursor (onNoteEdge ? juce::MouseCursor (juce::MouseCursor::LeftRightResizeCursor) : pencilCursor); break;
        case Tool::erase:  setMouseCursor (eraserCursor); break;
        case Tool::select: setMouseCursor (onNoteEdge ? juce::MouseCursor::LeftRightResizeCursor
                                                      : hoverKey.valid() ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor); break;
    }
}

void PianoRoll::flash (const juce::String& text)
{
    flashText = text;
    flashUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
    repaint();
}

void PianoRoll::eraseBetween (juce::Point<float> from, juce::Point<float> to)
{
    // Genom hela sträckan, så att inget hoppas över när musen rör sig snabbt.
    const auto steps = std::max (1, (int) (from.getDistanceFrom (to) / 3.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const auto point = from + (to - from) * ((float) i / (float) steps);
        if (! gridArea.contains (point))
            continue;
        bool edge = false;
        if (const auto key = noteAtPoint (point, edge); key.valid())
        {
            removeKeys ({ key });
            edited = true;
        }
    }
    repaint();
}

void PianoRoll::copySelection (bool cut)
{
    if (selection.empty())
        return;

    const auto notes = readNotes();
    auto first = std::numeric_limits<int>::max(), last = 0;
    for (const auto& key : selection)
        if (const auto found = notes.find (key); found != notes.end())
        {
            first = std::min (first, key.tick);
            last = std::max (last, key.tick + found->second.length);
        }
    if (first == std::numeric_limits<int>::max())
        return;

    auto& clip = processor.noteClipboard;
    clip.notes.clear();
    for (const auto& key : selection)
        if (const auto found = notes.find (key); found != notes.end())
            clip.notes.push_back ({ key.tick - first, found->second.length, found->second.pitch, found->second.velocity, found->second.slide, found->second.chord, key.index, found->second.lock });

    // Nästa inklistring börjar där de kopierade slutar (avrundat uppåt till ett steg i rutnätet).
    const auto step = std::max (1, snapTicks());
    clip.span = ((last - first + step - 1) / step) * step;
    clip.nextStart = first + clip.span;

    const auto count = (int) clip.notes.size();
    if (cut)
        deleteSelected();
    flash ((cut ? "Cut " : "Copied ") + juce::String (count) + (count == 1 ? " note" : " notes"));
}

void PianoRoll::startPaste()
{
    auto& clip = processor.noteClipboard;
    if (clip.notes.empty() || pasting)
        return;

    pasting = true;
    selection.clear();
    pasteShift = 0;
    pasteStart = clip.nextStart >= 0 ? clip.nextStart : snapped (xToTicks (gridArea.getX()));
    pasteStart = clampSafe (pasteStart, 0, std::max (0, editTicks() - 1));

    // Visa var inklistringen börjar.
    const auto x = ticksToX ((float) pasteStart);
    if (x < gridArea.getX() || x > gridArea.getRight() - 40.0f)
    {
        scrollTicks = (float) pasteStart - 24.0f;
        clampView();
    }
    updateToolbarVisibility();
    updateCursor (getMouseXYRelative().toFloat(), false);
    repaint();
}

void PianoRoll::movePasteTo (juce::Point<float> position)
{
    pasteStart = clampSafe (snapped (xToTicks (position.x) - pasteGrabTicks), 0, std::max (0, editTicks() - 1));

    // Dra uppåt eller nedåt flyttar också tonhöjden, men inte så att något hamnar utanför.
    auto lowest = highPitch(), highest = lowPitch();
    for (const auto& n : processor.noteClipboard.notes)
    {
        lowest = std::min (lowest, n.pitch);
        highest = std::max (highest, n.pitch);
    }
    pasteShift = clampSafe (pasteGrabShift + (yToPitch (position.y) - pasteGrabRow), lowPitch() - lowest, highPitch() - highest);
    repaint();
}

juce::Rectangle<float> PianoRoll::ghostBounds() const
{
    juce::Rectangle<float> bounds;
    auto first = true;
    for (const auto& n : processor.noteClipboard.notes)
    {
        const auto tick = pasteStart + n.dt;
        const auto rect = juce::Rectangle<float> (ticksToX ((float) tick), pitchToY (juce::jlimit (lowPitch(), highPitch(), n.pitch + pasteShift)),
                                                  std::max (3.0f, ticksToX ((float) (tick + n.length)) - ticksToX ((float) tick)), rowHeight());
        bounds = first ? rect : bounds.getUnion (rect);
        first = false;
    }
    return bounds;
}

void PianoRoll::commitPaste()
{
    if (! pasting)
        return;

    auto& clip = processor.noteClipboard;
    KeySet placed;
    auto skipped = 0;
    // Huvudtonerna först (index 0), så att de staplade har något att läggas på.
    auto order = clip.notes;
    std::stable_sort (order.begin(), order.end(), [] (const auto& a, const auto& b) { return (a.index != 0) < (b.index != 0); });
    for (const auto& n : order)
    {
        const auto tick = pasteStart + n.dt;
        if (tick < 0 || tick >= editTicks() || ! processor.pattern.acceptsTick (layer, tick))
        {
            ++skipped;
            continue;
        }
        const auto length = std::min (n.length, Pattern::maxTicks - tick);
        const auto pitch = juce::jlimit (lowPitch(), highPitch(), n.pitch + pasteShift);
        // Inklistring skriver aldrig över: upptagen tick => ny stapel (samma tonhöjd som en ton där hoppas över).
        const auto occupied = processor.pattern.stackSize (layer, tick) > 0;
        auto samePitch = false;
        for (int i = 0; occupied && i < Pattern::maxStack; ++i)
            if (NoteData probe; liveNote ({ tick, i }, probe) && probe.pitch == pitch)
                samePitch = true;
        if (samePitch)
        {
            ++skipped;
            continue;
        }
        if (! occupied)
        {
            processor.pattern.addNote (layer, tick, length, pitch, n.velocity, n.slide, n.chord);
            for (int p = 0; p < Pattern::numLocks; ++p)
                if (n.lock[(size_t) p] >= 0.0f)
                    processor.pattern.setLock (layer, tick, p, n.lock[(size_t) p]);
            placed.insert ({ tick, 0 });
        }
        else
        {
            Pattern::StackNote stacked;
            stacked.length = length;
            stacked.pitch = pitch;
            stacked.velocity = n.velocity;
            stacked.slide = n.slide;
            const auto slot = processor.pattern.addExtra (layer, tick, stacked);
            if (slot < 0)
            {
                ++skipped;
                continue;
            }
            placed.insert ({ tick, slot });
        }
    }

    pasting = false;
    selection = placed;
    clip.nextStart = pasteStart + clip.span; // nästa inklistring hamnar efter den här
    commit();
    updateToolbarVisibility();
    updateCursor (getMouseXYRelative().toFloat(), false);
    flash ("Pasted " + juce::String ((int) placed.size()) + (placed.size() == 1 ? " note" : " notes")
           + (skipped > 0 ? " (" + juce::String (skipped) + " skipped: outside the pattern, same pitch or full stack)" : juce::String()));
}

void PianoRoll::cancelPaste()
{
    if (! pasting)
        return;
    pasting = false;
    updateToolbarVisibility();
    updateCursor (getMouseXYRelative().toFloat(), false);
    flash ("Paste cancelled");
}

void PianoRoll::commit()
{
    processor.pattern.setCustom (layer, true); // lagret visas nu som pianorulle i stället för stegknappar
    processor.patternChanged();
    processor.recordRowEdit (layer, undoBase, "Edit notes");
    undoBase = processor.snapshotRow (layer);
    checkVoicesNeeded();
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
    rowH = juce::jlimit (7.0f, 60.0f, rowH + delta);
    topPitch = centrePitch + (float) visibleRows() * 0.5f;
    clampView();
    repaint();
}

void PianoRoll::resized()
{
    auto area = getLocalBounds().toFloat();
    toolbarArea = area.removeFromTop (toolbarHeight);
    hintArea = area.removeFromBottom (hintHeight);
    // Lanerna: håll minst 120 px kvar till notgallret.
    const auto gaps = laneGap * (float) (lanes.size() - 1);
    const auto budget = std::max (minLaneHeight * (float) lanes.size(), area.getHeight() - rulerHeight - 120.0f - 4.0f - barSize - gaps);
    auto heights = 0.0f;
    for (auto& lane : lanes)
    {
        lane.height = juce::jlimit (minLaneHeight, 200.0f, lane.height);
        heights += lane.height;
    }
    for (auto it = lanes.rbegin(); it != lanes.rend() && heights > budget; ++it) // för lite plats: de nedersta lanerna krymper först
    {
        const auto cut = std::min (heights - budget, it->height - minLaneHeight);
        it->height -= cut;
        heights -= cut;
    }
    const auto lanesTotal = heights + gaps;
    auto laneY = area.getBottom() - lanesTotal;
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        laneArea[i] = juce::Rectangle<float> (keyWidth, laneY, std::max (0.0f, area.getWidth() - keyWidth - barSize), lanes[i].height);
        laneY += lanes[i].height + laneGap;
    }
    const auto reserved = lanesTotal + 4.0f + barSize;
    keyArea = area.withWidth (keyWidth).withTrimmedTop (rulerHeight).withTrimmedBottom (reserved);
    rulerArea = area.withTrimmedLeft (keyWidth).withTrimmedRight (barSize).withHeight (rulerHeight);
    gridArea = area.withTrimmedLeft (keyWidth).withTrimmedRight (barSize).withTrimmedTop (rulerHeight).withTrimmedBottom (reserved);
    addLaneButton.setVisible (lanes.size() < (size_t) maxLanes);
    addLaneButton.setBounds (juce::Rectangle<float> (0.0f, gridArea.getBottom() + 1.0f, keyWidth, barSize).toNearestInt());
    vBar.setBounds (juce::Rectangle<float> (gridArea.getRight(), gridArea.getY(), barSize, gridArea.getHeight()).toNearestInt());
    hBar.setBounds (juce::Rectangle<float> (gridArea.getX(), gridArea.getBottom() + 1.0f, gridArea.getWidth(), barSize).toNearestInt());

    // Verktygsraden i två rader. Överst: verktygen (penna, markera, sudd), markera alla, ta bort, kopiera/klistra in och LISTEN.
    // Under: SNAP, rutnät, radhöjd, GLIDE, FOLLOW och längst till höger MIDI och PRESET.
    auto toolRows = toolbarArea.toNearestInt();
    auto top = toolRows.removeFromTop (toolRows.getHeight() / 2).reduced (2, 1);
    auto bottom = toolRows.reduced (2, 1);

    drawButton.setBounds (top.removeFromLeft (62));
    top.removeFromLeft (2);
    selectButton.setBounds (top.removeFromLeft (62));
    top.removeFromLeft (2);
    eraseButton.setBounds (top.removeFromLeft (62));
    top.removeFromLeft (10);
    allButton.setBounds (top.removeFromLeft (78));
    top.removeFromLeft (2);
    deleteButton.setBounds (top.removeFromLeft (38));
    top.removeFromLeft (10);
    const auto slotA = top.removeFromLeft (66);
    top.removeFromLeft (2);
    const auto slotB = top.removeFromLeft (88);
    copyButton.setBounds (slotA);
    pasteOkButton.setBounds (slotA);
    pasteButton.setBounds (slotB);
    pasteCancelButton.setBounds (slotB);
    top.removeFromLeft (10);
    listenButton.setBounds (top.removeFromLeft (36));
    midiInBox.setBounds (top.removeFromRight (170));
    infoArea =juce::Rectangle<float> ((float) listenButton.getRight() + 10.0f, (float) top.getY() - 1.0f,
                                       std::max (0.0f, (float) (top.getRight() - listenButton.getRight()) - 10.0f), 22.0f);

    presetButton.setBounds (bottom.removeFromRight (62));
    bottom.removeFromRight (4);
    midiButton.setBounds (bottom.removeFromRight (48));
    bottom.removeFromRight (4);
    helpButton.setBounds (bottom.removeFromRight (50));
    bottom.removeFromRight (4);
    popOutButton.setBounds (bottom.removeFromRight (64));
    snapButton.setBounds (bottom.removeFromLeft (50));
    bottom.removeFromLeft (4);
    gridBox.setBounds (bottom.removeFromLeft (76));
    bottom.removeFromLeft (10);
    smallerButton.setBounds (bottom.removeFromLeft (24));
    bottom.removeFromLeft (2);
    biggerButton.setBounds (bottom.removeFromLeft (24));
    bottom.removeFromLeft (10);
    glideButton.setBounds (bottom.removeFromLeft (56));
    bottom.removeFromLeft (4);
    followButton.setBounds (bottom.removeFromLeft (64));
    bottom.removeFromLeft (4);
    ghostButton.setBounds (bottom.removeFromLeft (62));
    bottom.removeFromLeft (12);
    lengthButton.setBounds (bottom.removeFromLeft (110));
    bottom.removeFromLeft (4);
    extendButton.setBounds (bottom.removeFromLeft (52));
    bottom.removeFromLeft (12);
    const auto showVoices = baseLayer != Pattern::kickLayer;
    voicesDownButton.setVisible (showVoices);
    voicesUpButton.setVisible (showVoices);
    voicesLabel.setVisible (showVoices);
    voicesDownButton.setBounds (bottom.removeFromLeft (24));
    voicesLabel.setBounds (bottom.removeFromLeft (76));
    voicesUpButton.setBounds (bottom.removeFromLeft (24));

    const auto first = pixelsPerTick == 1.0f && scrollTicks == 0.0f;
    if (first)
    {
        fitToWidth();
        topPitch = (float) visibleRows() * 0.5f; // grundtonen i mitten
        if (processor.snapshotPaste)
        {
            // Skärmbildsverktyget: markera de första tonerna, kopiera och börja klistra in.
            const auto notes = readNotes();
            for (const auto& [key, data] : notes)
                if (selection.size() < 4)
                    selection.insert (key);
            copySelection (false);
            startPaste();
        }
    }
    clampView();
}

void PianoRoll::timerCallback()
{
    followVisualRate();
    pruneSelection(); // ångra/gör om från historiken eller menyn kan ha tagit bort valda toner
    if (baseLayer != Pattern::kickLayer)
    {
        const auto text = "VOICES " + juce::String (voices());
        if (voicesLabel.getText() != text)
            voicesLabel.setText (text, juce::dontSendNotification);
    }
    if (const auto colourVersion = processor.layerStructureVersion.load(); colourVersion != lastColourVersion)
    {
        lastColourVersion = colourVersion;
        for (size_t i = 0; i + 1 < layerColours.size(); ++i) // sist kommer kicken
            layerColours[i] = Theme::layerColour ((int) i);
        repaint();
    }
    if (! isShowing())
        return;

    auto changed = false;
    auto update = [&changed] (int& last, int now) { if (last != now) { last = now; changed = true; } };

    // Skift hålls ner: MARKERA-knappen trycks in och muspekaren blir ett kryss. Knapparna följer läget och urklippet.
    const auto shiftNow = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
    const auto altNow = juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown();
    if (shiftNow != shiftHeld || altNow != altHeld)
    {
        shiftHeld = shiftNow;
        altHeld = altNow;
        updateCursor (getMouseXYRelative().toFloat(), hoverEdge);
    }
    updateToolButtons();
    updateToolbarVisibility();
    listenButton.setActive (processor.previewingLayer.load() == baseLayer);
    if (flashText.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() > flashUntil)
    {
        flashText = {};
        changed = true;
    }

    update (lastVersion, processor.patternVersion.load());
    const std::pair<uint64_t, uint64_t> live { processor.liveNotesLow.load(), processor.liveNotesHigh.load() };
    if (live != liveNow)
    {
        liveNow = live; // tangenterna som spelas just nu tänds
        changed = true;
    }
    const auto playTick = playTickNow();
    update (lastPlayTick, playTick < 0 ? -1 : playTick / 6); // var 6:e tick räcker (tonernas lysning följer med)

    // FOLLOW: när uppspelningen lämnar bilden bläddrar vyn till de takter som spelar.
    followButton.setToggleState (processor.followPlayhead.load(), juce::dontSendNotification);
    if (processor.followPlayhead.load() && playTick >= 0 && pixelsPerTick > 0.0f)
    {
        const auto visibleTicks = gridArea.getWidth() / pixelsPerTick;
        if ((float) playTick < scrollTicks || (float) playTick >= scrollTicks + visibleTicks)
        {
            scrollTicks = std::floor ((float) playTick / visibleTicks) * visibleTicks;
            changed = true;
        }
    }
    if (bars() != lastBars)
    {
        // Kortare: visa hela mönstret igen. Längre (+ BAR, klick efter slutet): vyn står kvar.
        if (lastBars != 0 && bars() < lastBars)
            fitToWidth();
        lastBars = bars();
        changed = true;
    }
    update (lastGrid, (int) grid());
    const auto lengthText = LengthMenu::buttonText (processor.pattern, layer);
    if (lengthButton.getButtonText() != lengthText)
    {
        lengthButton.setButtonText ("LENGTH " + lengthText);
        lengthButton.setTooltip (LengthMenu::tooltip (layerName()));
        changed = true;
    }
    if (changed)
    {
        clampView();
        pruneSelection();
        repaint();
    }
}

template <typename F>
void PianoRoll::forEachLive (int firstTick, int lastTick, F&& visit) const
{
    for (int tick = std::max (0, firstTick); tick <= std::min (lastTick, Pattern::maxTicks - 1); ++tick)
        for (int index = 0; index < Pattern::maxStack; ++index)
            if (NoteData data; liveNote ({ tick, index }, data))
                visit (NoteKey { tick, index }, data);
}

bool PianoRoll::liveNote (NoteKey key, NoteData& out) const
{
    if (! juce::isPositiveAndBelow (key.tick, Pattern::maxTicks) || ! juce::isPositiveAndBelow (key.index, Pattern::maxStack))
        return false;
    const auto& n = processor.pattern.note (layer, key.tick);
    if (! n.on.load())
        return false;
    if (key.index == 0)
    {
        out = { n.length.load(), n.pitch.load(), n.velocity.load(), n.slide.load(), n.getChord() };
        for (int p = 0; p < Pattern::numLocks; ++p)
            out.lock[(size_t) p] = n.lock[(size_t) p].load();
        return true;
    }
    const auto& e = n.extra[(size_t) key.index - 1];
    if (! e.on.load())
        return false;
    out = { e.length.load(), e.pitch.load(), e.velocity.load(), e.slide.load(), {} };
    return true;
}

void PianoRoll::setLiveVelocity (NoteKey key, float velocity)
{
    auto& n = processor.pattern.edit (layer, key.tick);
    (key.index == 0 ? n.velocity : n.extra[(size_t) key.index - 1].velocity) = velocity;
}

void PianoRoll::setLiveSlide (NoteKey key, bool slide)
{
    auto& n = processor.pattern.edit (layer, key.tick);
    (key.index == 0 ? n.slide : n.extra[(size_t) key.index - 1].slide) = slide;
}

PianoRoll::NoteMap PianoRoll::readNotes() const
{
    NoteMap notes;
    forEachLive (0, Pattern::maxTicks - 1, [&notes] (NoteKey key, const NoteData& data) { notes[key] = data; });
    return notes;
}

void PianoRoll::normalise (NoteMap& notes, std::map<NoteKey, NoteKey>* remap)
{
    for (auto it = notes.begin(); it != notes.end();)
    {
        const auto tick = it->first.tick;
        const auto head = notes.find ({ tick, 0 });
        if (head != notes.end())
        {
            it = notes.upper_bound ({ tick, Pattern::maxStack });
            continue;
        }

        const auto firstStacked = notes.lower_bound ({ tick, 1 });
        if (firstStacked != notes.end() && firstStacked->first.tick == tick)
        {
            const auto from = firstStacked->first;
            auto promoted = firstStacked->second;
            promoted.chord = {};
            notes.erase (firstStacked);
            notes[{ tick, 0 }] = promoted;
            if (remap != nullptr)
                (*remap)[from] = { tick, 0 };
        }
        it = notes.upper_bound ({ tick, Pattern::maxStack });
    }
}

PianoRoll::Relocated PianoRoll::relocate (const NoteMap& source, const KeySet& moving, int deltaTicks, int deltaPitch, int lowest, int highest)
{
    Relocated result;
    if (deltaTicks == 0 && deltaPitch == 0)
    {
        result.target = source;
        for (const auto& key : moving)
            if (source.find (key) != source.end())
                result.where[key] = key;
        return result;
    }

    for (const auto& [key, data] : source)
        if (moving.count (key) == 0)
            result.target[key] = data;

    for (const auto& key : moving)
    {
        const auto found = source.find (key);
        if (found == source.end())
            continue;
        auto data = found->second;
        data.pitch = juce::jlimit (lowest, highest, data.pitch + deltaPitch);
        const auto tick = key.tick + deltaTicks;

        // Samma tonhöjd på samma tick är samma ton (pennan vägrar också): flytten avbryts (full), ingen ton skrivs över.
        NoteKey place;
        auto duplicate = false;
        for (int i = 0; i < Pattern::maxStack; ++i)
            if (const auto other = result.target.find ({ tick, i }); other != result.target.end() && other->second.pitch == data.pitch)
                duplicate = true;
        if (! duplicate)
        {
            // Helst samma stackindex, annars den lägsta lediga platsen. Fullt: hela flytten avbryts (full).
            if (result.target.count ({ tick, key.index }) == 0)
                place = { tick, key.index };
            else
                for (int i = 0; i < Pattern::maxStack && ! place.valid(); ++i)
                    if (result.target.count ({ tick, i }) == 0)
                        place = { tick, i };
        }
        if (! place.valid())
        {
            result.target = source;
            result.where.clear();
            for (const auto& movingKey : moving)
                if (source.find (movingKey) != source.end())
                    result.where[movingKey] = movingKey;
            result.full = true;
            return result;
        }
        if (place.index != 0)
            data.chord = {};
        result.target[place] = data;
        result.where[key] = place;
    }

    std::map<NoteKey, NoteKey> promoted;
    normalise (result.target, &promoted);
    for (auto& entry : result.where)
        if (const auto p = promoted.find (entry.second); p != promoted.end())
            entry.second = p->second;
    return result;
}

void PianoRoll::applyNotes (const NoteMap& target)
{
    auto& pattern = processor.pattern;
    auto normalized = target;
    normalise (normalized);

    // Toner som inte finns i målet stängs av sist (så att ljudtråden aldrig ser ett tomt mönster mitt i ett byte).
    std::vector<NoteKey> gone;
    forEachLive (0, Pattern::maxTicks - 1, [&] (NoteKey key, const NoteData&)
    {
        if (normalized.find (key) == normalized.end())
            gone.push_back (key);
    });

    for (const auto& [key, d] : normalized)  // huvudtonen (index 0) kommer före de staplade
    {
        auto& n = pattern.edit (layer, key.tick);
        if (key.index == 0)
        {
            if (n.length.load() != d.length) n.length = d.length;
            if (n.pitch.load() != d.pitch) n.pitch = d.pitch;
            if (n.velocity.load() != d.velocity) n.velocity = d.velocity;
            if (n.slide.load() != d.slide) n.slide = d.slide;
            if (n.getChord() != d.chord) n.setChord (d.chord);
            for (int p = 0; p < Pattern::numLocks; ++p)
                if (n.lock[(size_t) p].load() != d.lock[(size_t) p]) n.lock[(size_t) p] = d.lock[(size_t) p];
            if (! n.on.load())
                n.on = true;
        }
        else if (key.index < Pattern::maxStack)
        {
            auto& e = n.extra[(size_t) key.index - 1];
            if (e.length.load() != d.length) e.length = d.length;
            if (e.pitch.load() != d.pitch) e.pitch = d.pitch;
            if (e.velocity.load() != d.velocity) e.velocity = d.velocity;
            if (e.slide.load() != d.slide) e.slide = d.slide;
            if (! e.on.load())
                e.on = true;
        }
    }
    for (const auto& key : gone)
        if (auto* n = pattern.existing (layer, key.tick))
        {
            if (key.index == 0)
            {
                if (normalized.find ({ key.tick, 0 }) != normalized.end())
                    continue;
                n->on = false;
                n->clearExtras();
                n->clearLocks();
            }
            else if (key.index < Pattern::maxStack)
                n->extra[(size_t) key.index - 1].on = false;
        }
}

void PianoRoll::removeKeys (const KeySet& keys)
{
    auto notes = readNotes();
    for (const auto& key : keys)
    {
        notes.erase (key);
        selection.erase (key);
    }
    std::map<NoteKey, NoteKey> remap;
    normalise (notes, &remap);
    KeySet retained;
    for (const auto& key : selection)
        if (const auto moved = remap.find (key); moved != remap.end())
            retained.insert (moved->second);
        else if (notes.find (key) != notes.end())
            retained.insert (key);
    selection = std::move (retained);
    applyNotes (notes);
}

void PianoRoll::pruneSelection()
{
    NoteData probe;
    for (auto it = selection.begin(); it != selection.end();)
        it = (it->tick >= editTicks() || ! liveNote (*it, probe)) ? selection.erase (it) : std::next (it);
}

void PianoRoll::selectAll()
{
    selection.clear();
    for (const auto& [key, data] : readNotes())
        if (key.tick < editTicks()) // bara det som hör till mönstret
            selection.insert (key);
    repaint();
}

void PianoRoll::deleteSelected()
{
    if (selection.empty())
        return;
    removeKeys (KeySet (selection));
    selection.clear();
    commit();
    repaint();
}

void PianoRoll::moveSelected (int deltaTicks, int deltaPitch)
{
    if (selection.empty())
        return;

    // Flytta alla markerade så långt det går utan att något hamnar utanför.
    const auto notes = readNotes();
    auto minTick = editTicks(), maxTick = 0, minPitch = highPitch(), maxPitch = lowPitch();
    for (const auto& key : selection)
        if (const auto found = notes.find (key); found != notes.end())
        {
            minTick = std::min (minTick, key.tick);
            maxTick = std::max (maxTick, key.tick);
            minPitch = std::min (minPitch, found->second.pitch);
            maxPitch = std::max (maxPitch, found->second.pitch);
        }
    deltaTicks = clampSafe (deltaTicks, -minTick, editTicks() - 1 - maxTick);
    deltaPitch = clampSafe (deltaPitch, lowPitch() - minPitch, highPitch() - maxPitch);

    const auto moved = relocate (notes, selection, deltaTicks, deltaPitch, lowPitch(), highPitch());
    if (moved.full)
    {
        flash ("Blocked: that tick is full, or already has a note with that pitch");
        return;
    }
    applyNotes (moved.target);
    selection.clear();
    for (const auto& entry : moved.where)
        selection.insert (entry.second);
    commit();
    repaint();
}

juce::Rectangle<float> PianoRoll::noteBounds (NoteKey key) const
{
    jassert (juce::isPositiveAndBelow (key.tick, Pattern::maxTicks));
    NoteData n;
    liveNote (key, n);
    const auto x0 = ticksToX ((float) key.tick);
    const auto x1 = ticksToX ((float) (key.tick + n.length));
    return { x0, pitchToY (n.pitch), std::max (3.0f, x1 - x0), rowHeight() };
}

PianoRoll::NoteKey PianoRoll::noteAtPoint (juce::Point<float> p, bool& onRightEdge) const
{
    onRightEdge = false;
    // Bakifrån, så att en ton som ligger överst (senare start, högre stackindex) vinner.
    const auto first = std::max (0, (int) xToTicks (gridArea.getX()) - Pattern::ticksPerBar * 2);
    const auto last = std::min (editTicks() - 1, (int) xToTicks (gridArea.getRight()) + 1);
    NoteData probe;
    for (int tick = last; tick >= first; --tick)
        for (int index = Pattern::maxStack - 1; index >= 0; --index)
        {
            if (! liveNote ({ tick, index }, probe))
                continue;
            auto bounds = noteBounds ({ tick, index });
            bounds.setWidth (std::max (bounds.getWidth(), edgeGrab * 1.5f)); // en kort ton får ett grepp som räcker utanför dess högerkant
            if (bounds.contains (p))
            {
                onRightEdge = p.x > bounds.getRight() - edgeGrab;
                return { tick, index };
            }
        }
    return {};
}

float PianoRoll::velocityX (NoteKey key) const
{
    return ticksToX ((float) key.tick) + 1.5f + 4.0f * (float) key.index; // staplade toner får egna staplar bredvid huvudtonens
}

PianoRoll::NoteKey PianoRoll::noteForVelocity (float x) const
{
    NoteKey best;
    auto bestDistance = 7.0f;
    forEachLive (0, editTicks() - 1, [&] (NoteKey key, const NoteData&)
    {
        const auto distance = std::abs (velocityX (key) - x);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = key;
        }
    });
    return best;
}

juce::String PianoRoll::describe (NoteKey key) const
{
    NoteData n;
    if (! liveNote (key, n))
        return {};
    const auto pitch = n.pitch;
    const auto beat = key.tick / Pattern::ticksPerQuarter;
    juce::String text = juce::MidiMessage::getMidiNoteName (rootNote() + pitch, true, true, 4)
                      + "  (" + (pitch > 0 ? "+" : "") + juce::String (pitch) + ")  "
                      + "bar " + juce::String (beat / 4 + 1) + "." + juce::String (beat % 4 + 1) + "  "
                      + "length " + gridNameFor (n.length) + "  "
                      + "vel " + juce::String (juce::roundToInt (n.velocity * 100.0f)) + " %";
    if (n.slide)
        text << "  GLIDE";
    if (key.index == 0)
        for (int p = 0; p < Pattern::numLocks; ++p)
            if (n.lock[(size_t) p] >= 0.0f)
                text << "  " << laneName (p + 1) << " " << juce::roundToInt (n.lock[(size_t) p] * 100.0f) << " %";
    if (key.index > 0)
        text << "  STACK " << key.index + 1 << "/" << processor.pattern.stackSize (layer, key.tick);
    if (key.index == 0 && std::any_of (n.chord.begin(), n.chord.end(), [] (auto v) { return v != 0; }))
    {
        text << "  CHORD";
        for (const auto interval : n.chord)
            if (interval != 0)
                text << " " << (interval > 0 ? "+" : "") << interval;
        if (voices() < 2)
            text << " (chord needs VOICES 2 or more)";
    }
    return text;
}

int PianoRoll::voices() const
{
    if (baseLayer == Pattern::kickLayer)
        return 1;
    return (int) processor.apvts.getRawParameterValue (ParamID::bass (baseLayer, InstrumentField::voices))->load();
}

void PianoRoll::setVoices (int count)
{
    if (auto* param = processor.apvts.getParameter (ParamID::bass (baseLayer, InstrumentField::voices)))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) count));
        param->endChangeGesture();
    }
    voicesLabel.setText ("VOICES " + juce::String (voices()), juce::dontSendNotification);
}

void PianoRoll::changeVoices (int delta)
{
    if (baseLayer == Pattern::kickLayer)
        return;
    const auto target = juce::jlimit (1, 8, voices() + delta); // 8 = InstrumentVoices::maxVoices
    if (target == voices())
        return;
    const auto needed = processor.pattern.maxSimultaneous (layer);
    if (target >= needed)
    {
        askedVoicesNeed = 0;
        setVoices (target);
        return;
    }
    // Färre röster än mönstret behöver: den högsta tonen stjäls hela tiden (3 ms utton), så fråga först.
    juce::Component::SafePointer<PianoRoll> safeThis (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                      juce::MessageBoxIconType::WarningIcon, "Lower VOICES",
                                      "This pattern plays up to " + juce::String (needed) + " notes at once, but VOICES " + juce::String (target)
                                          + " only gives " + juce::String (target) + ". The top note is then stolen all the time (a 3 ms fade), so notes will cut off.",
                                      "Continue", "Cancel"),
                                  [safeThis, target] (int choice)
    {
        if (safeThis != nullptr && choice == 1)
            safeThis->setVoices (target);
    });
}

void PianoRoll::checkVoicesNeeded()
{
    if (baseLayer == Pattern::kickLayer)
        return;
    const auto needed = processor.pattern.maxSimultaneous (layer);
    if (needed <= voices())
    {
        askedVoicesNeed = 0;
        return;
    }
    if (needed <= askedVoicesNeed || needed > 8) // redan frågat (och fått nej); 8 = InstrumentVoices::maxVoices
        return;
    askedVoicesNeed = needed;
    juce::Component::SafePointer<PianoRoll> safeThis (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsYesNo (
                                      juce::MessageBoxIconType::QuestionIcon, "More VOICES?",
                                      "Increase VOICES to " + juce::String (needed) + "? The pattern plays up to " + juce::String (needed)
                                          + " notes at once. With No the notes stay, but the top note is stolen while VOICES is too low."),
                                  [safeThis, needed] (int choice)
    {
        if (safeThis != nullptr && choice == 1)
            safeThis->setVoices (needed);
    });
}

void PianoRoll::setChordOnSelection (const Pattern::Chord& chord)
{
    if (selection.empty() || baseLayer == Pattern::kickLayer)
        return;
    auto target = readNotes();
    for (const auto& key : selection)
        if (key.index == 0) // bara huvudtonen bär ackord
            if (const auto found = target.find (key); found != target.end())
                found->second.chord = chord;
    applyNotes (target);
    commit();
    repaint();
}

void PianoRoll::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto colour = layerColours[(size_t) juce::jlimit (0, (int) layerColours.size() - 1, baseLayer)];

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
            if (pitch < lowPitch() || pitch > highPitch())
                continue;
            const auto y = pitchToY (pitch);
            const auto pc = ((rootPc + pitch) % 12 + 12) % 12;
            g.setColour ((sliceMode ? pitch % 2 != 0 : isBlackKey (pc)) ? juce::Colours::black.withAlpha (0.28f) : juce::Colours::white.withAlpha (0.03f));
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
        for (int tick = firstTick; tick <= viewTicks() && ticksToX ((float) tick) <= gridArea.getRight(); tick += step)
        {
            const auto x = ticksToX ((float) tick);
            const auto isBar = tick % Pattern::ticksPerBar == 0, isBeat = tick % Pattern::ticksPerQuarter == 0;
            if (! isBar && ! isBeat && (float) step * pixelsPerTick < 4.0f)
                continue;
            g.setColour (isBar ? Theme::textDim.withAlpha (0.55f) : isBeat ? Theme::textDim.withAlpha (0.28f) : Theme::grid.withAlpha (snapEnabled ? 0.6f : 0.3f));
            g.drawVerticalLine ((int) x, gridArea.getY(), gridArea.getBottom());
        }

        // Takten efter radens slut: skuggad, ett klick där (penna) förlänger raden.
        if (totalTicks() < Pattern::maxTicks)
        {
            const auto x = std::max (gridArea.getX(), ticksToX ((float) totalTicks()));
            if (x < gridArea.getRight())
            {
                const auto ghost = juce::Rectangle<float>::leftTopRightBottom (x, gridArea.getY(), gridArea.getRight(), gridArea.getBottom());
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRect (ghost);
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (10.5f, true));
                g.drawText ("+ CLICK TO EXTEND THE ROW", ghost.withHeight (20.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);
            }
        }

        // D6: spökton – de andra radernas toner, svagt och utan att gå att röra, med MIDI-tonen som gemensam höjdaxel.
        {
            const auto gFirst = std::max (0, (int) xToTicks (gridArea.getX()) - Pattern::ticksPerBar * 2);
            const auto gLast = std::min (editTicks() - 1, (int) xToTicks (gridArea.getRight()) + 1);
            for (int other = 0; other < Pattern::numNoteLayers; ++other)
            {
                if (sliceMode || other == baseLayer || (processor.ghostMask.load() & (1u << other)) == 0)
                    continue;
                const auto shift = processor.rowRootNote (other) - rootNote();
                const auto otherColour = layerColours[(size_t) juce::jlimit (0, (int) layerColours.size() - 1, other)];
                for (int tick = gFirst; tick <= gLast; ++tick)
                {
                    const auto stacked = processor.pattern.stackSize (other, tick);
                    for (int index = 0; index < stacked; ++index)
                    {
                        Pattern::StackNote sn;
                        if (! processor.pattern.getStack (other, tick, index, sn))
                            continue;
                        const auto x0 = ticksToX ((float) tick), x1 = ticksToX ((float) (tick + sn.length));
                        const juce::Rectangle<float> gb { x0, pitchToY (sn.pitch + shift), std::max (3.0f, x1 - x0), rowHeight() };
                        g.setColour (otherColour.withAlpha (0.16f));
                        g.fillRoundedRectangle (gb.reduced (0.0f, 0.5f), 2.5f);
                        g.setColour (otherColour.withAlpha (0.32f));
                        g.drawRoundedRectangle (gb.reduced (0.0f, 0.5f), 2.5f, 1.0f);
                    }
                }
            }
        }

        // Tonerna.
        const auto chordAlpha = voices() >= 2 ? 1.0f : 0.35f;
        const auto playHead = playTickNow();
        const auto first = std::max (0, (int) xToTicks (gridArea.getX()) - Pattern::ticksPerBar * 2);
        const auto last = std::min (editTicks() - 1, (int) xToTicks (gridArea.getRight()) + 1);
        NoteData note;
        for (int tick = first; tick <= last; ++tick)
            for (int index = 0; index < Pattern::maxStack; ++index)
        {
            const NoteKey key { tick, index };
            if (! liveNote (key, note))
                continue;

            const auto b = noteBounds (key).reduced (0.0f, 0.5f);
            const auto selected = selection.count (key) > 0;
            const auto active = key == hoverKey || key == dragKey;
            auto c = colour.withMultipliedBrightness (0.55f + 0.45f * note.velocity);
            if (note.slide)
            {
                c = Theme::pattern.withMultipliedBrightness (0.7f + 0.3f * note.velocity);

                // Rampen: från tonhöjden som klingar till glide-tonens tonhöjd, under glide-tonens längd.
                const auto fromY = pitchToY (pitchBefore (tick)) + rowHeight() * 0.5f;
                g.setColour (Theme::pattern.withAlpha (0.9f));
                g.drawLine (b.getX(), fromY, b.getRight(), b.getCentreY(), 2.0f);
            }
            // B3: tonen som klingar just nu (uppspelningsmarkören ligger inom den) lyser upp.
            const auto playing = playHead >= 0 && playHead >= key.tick && playHead < key.tick + note.length;
            if (playing)
                c = c.brighter (0.6f).interpolatedWith (juce::Colours::white, 0.35f);
            if (selected)
                c = c.brighter (0.45f).interpolatedWith (juce::Colours::white, 0.25f);
            if (key.index == 0)
                for (const auto interval : note.chord)
                    if (const auto p = note.pitch + interval; interval != 0 && p >= lowPitch() && p <= highPitch())
                    {
                        const auto cb = b.withY (pitchToY (p) + 0.5f);
                        g.setColour (c.withAlpha (0.22f * chordAlpha));
                        g.fillRoundedRectangle (cb, 2.5f);
                        g.setColour (c.brighter (0.5f).withAlpha (0.8f * chordAlpha));
                        g.drawRoundedRectangle (cb, 2.5f, 1.0f);
                    }
            g.setGradientFill (juce::ColourGradient (c.brighter (0.35f), b.getX(), b.getY(), c.darker (0.2f), b.getX(), b.getBottom(), false));
            g.fillRoundedRectangle (b, 2.5f);
            g.setColour (selected ? juce::Colour (0xffffe27a) : active ? juce::Colours::white : c.brighter (0.5f));
            g.drawRoundedRectangle (b, 2.5f, selected || active ? 1.6f : 1.0f);

            if (b.getWidth() > 24.0f && b.getHeight() > 9.0f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.setFont (Theme::font (std::min (9.0f, b.getHeight() - 2.0f), true));
                g.drawText (juce::MidiMessage::getMidiNoteName (rootNote() + note.pitch, true, true, 4),
                            b.reduced (3.0f, 0.0f), juce::Justification::centredLeft);
            }

        }

        // Glide-plus: plusset vid glide-tonen.
        if (plusKey.valid() && drag == Drag::none)
        {
            const auto p = plusRect().reduced (1.0f);
            g.setColour (Theme::pattern);
            g.fillEllipse (p);
            g.setColour (juce::Colours::black.withAlpha (0.85f));
            g.drawLine (p.getCentreX() - 3.5f, p.getCentreY(), p.getCentreX() + 3.5f, p.getCentreY(), 1.6f);
            g.drawLine (p.getCentreX(), p.getCentreY() - 3.5f, p.getCentreX(), p.getCentreY() + 3.5f, 1.6f);
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

        // Glidestrecket medan det ritas.
        if (drag == Drag::stroke)
        {
            g.setColour (Theme::pattern.withAlpha (0.9f));
            g.drawLine (juce::Line<float> (strokeStart, strokeEnd), 2.0f);
        }

        // Inklistring: de kopierade tonerna som ett spöke (annan färg, röd kant) där de hamnar om man bekräftar.
        if (pasting)
        {
            const auto& clip = processor.noteClipboard;
            const juce::Colour ghostFill (0xffffa347), ghostBorder (0xffff3b3b);
            for (const auto& n : clip.notes)
            {
                const auto tick = pasteStart + n.dt;
                if (tick >= editTicks())
                    continue;
                const auto x0 = ticksToX ((float) tick), x1 = ticksToX ((float) (tick + n.length));
                const auto b = juce::Rectangle<float> (x0, pitchToY (juce::jlimit (lowPitch(), highPitch(), n.pitch + pasteShift)),
                                                       std::max (3.0f, x1 - x0), rowHeight()).reduced (0.0f, 0.5f);
                g.setColour (ghostFill.withAlpha (0.8f));
                g.fillRoundedRectangle (b, 2.5f);
                g.setColour (ghostBorder);
                g.drawRoundedRectangle (b, 2.5f, 1.8f);
            }
            const auto frame = ghostBounds().expanded (3.0f);
            g.setColour (ghostBorder.withAlpha (0.55f));
            g.drawRect (frame, 1.0f);

            const auto banner = juce::Rectangle<float> (gridArea.getX(), gridArea.getY(), gridArea.getWidth(), 16.0f);
            g.setColour (ghostBorder.withAlpha (0.85f));
            g.fillRect (banner);
            g.setColour (juce::Colours::white);
            g.setFont (Theme::font (10.5f, true));
            g.drawText ("PASTING " + juce::String ((int) clip.notes.size()) + " NOTES  -  click or drag to choose where  -  OK / Enter places them,  CANCEL / Esc cancels",
                        banner.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
        }

        // Pennan: en förhandsvisning av tonen som skulle läggas där man klickar (rutan under spetsen), och ett litet hårkors precis där
        // musen pekar, så att man ser exakt vart klicket hamnar.
        if (hoverPointValid && ! pasting && effectiveTool() == Tool::draw && gridArea.contains (hoverPoint))
        {
            if (drag == Drag::none && ! hoverKey.valid())
            {
                const auto start = snapped (xToTicks (hoverPoint.x));
                const auto pitch = juce::jlimit (lowPitch(), highPitch(), yToPitch (hoverPoint.y));
                const auto length = ! glideButton.getToggleState() && lastLength > 0 ? lastLength : Pattern::ticksPerStep (grid());
                const auto x0 = ticksToX ((float) start), x1 = ticksToX ((float) std::min (start + length, editTicks()));
                const auto preview = juce::Rectangle<float> (x0, pitchToY (pitch), std::max (3.0f, x1 - x0), rowHeight()).reduced (0.0f, 0.5f);
                g.setColour (colour.withAlpha (0.16f));
                g.fillRoundedRectangle (preview, 2.5f);
                g.setColour (colour.brighter (0.4f).withAlpha (0.8f));
                g.drawRoundedRectangle (preview, 2.5f, 1.0f);
            }

            for (const auto& pass : { std::pair<float, juce::Colour> { 3.0f, juce::Colours::black.withAlpha (0.8f) }, { 1.2f, juce::Colours::white } })
            {
                g.setColour (pass.second);
                g.drawLine (hoverPoint.x - 5.0f, hoverPoint.y, hoverPoint.x + 5.0f, hoverPoint.y, pass.first);
                g.drawLine (hoverPoint.x, hoverPoint.y - 5.0f, hoverPoint.x, hoverPoint.y + 5.0f, pass.first);
            }
        }

        // Loopen (rött) och startmarkören.
        const auto loopRed = juce::Colour (0xffff4d5e);
        if (hasLoop())
        {
            const auto x1 = ticksToX ((float) loopStart), x2 = ticksToX ((float) loopEnd);
            g.setColour (loopRed.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (x1, gridArea.getY(), x2 - x1, gridArea.getHeight()));
            g.setColour (loopRed.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (x1 - 0.5f, gridArea.getY(), 1.5f, gridArea.getHeight()));
            g.fillRect (juce::Rectangle<float> (x2 - 1.0f, gridArea.getY(), 1.5f, gridArea.getHeight()));
        }
        else if (cursorTick > 0)
        {
            g.setColour (juce::Colour (0xff7ad7ff).withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (ticksToX ((float) cursorTick) - 0.5f, gridArea.getY(), 1.5f, gridArea.getHeight()));
        }

        // Uppspelningsmarkören.
        const auto playTick = playTickNow();
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
            if (pitch < lowPitch() || pitch > highPitch())
                continue;
            const auto y = pitchToY (pitch);
            const auto black = sliceMode ? pitch % 2 != 0 : isBlackKey (((rootPc + pitch) % 12 + 12) % 12); // skivor: varannan tangent mörk
            const auto key = juce::Rectangle<float> (keyArea.getX(), y, keyArea.getWidth() - 2.0f, rowHeight()).reduced (0.0f, 0.5f);
            g.setColour (black ? juce::Colour (0xff23263a) : juce::Colour (0xffc4c8de));
            g.fillRoundedRectangle (key, 2.0f);
            if (sliceMode)
            {
                g.setColour (black ? Theme::textDim : juce::Colour (0xff23263a));
                g.drawText ("Slice " + juce::String (RowRegistry::sliceOfPitch (pitch) + 1), key.reduced (7.0f, 0.0f), juce::Justification::centredLeft);
                continue;
            }
            // Tangenten som spelas live (MIDI IN: INSTRUMENT) lyser i radens färg.
            const auto midiNote = rootNote() + pitch;
            const auto liveBits = midiNote < 64 ? liveNow.first : liveNow.second;
            const auto pressed = midiNote >= 0 && midiNote < 128 && ((liveBits >> (midiNote & 63)) & 1) != 0;
            if (pressed)
            {
                g.setColour (colour.withAlpha (0.9f));
                g.fillRoundedRectangle (key, 2.0f);
            }
            if (pitch == 0)
            {
                g.setColour (colour);
                g.fillRoundedRectangle (key.withWidth (4.0f), 2.0f);
            }
            g.setColour (pressed ? juce::Colour (0xff0b0d18) : black ? Theme::textDim : juce::Colour (0xff23263a));
            g.drawText (juce::MidiMessage::getMidiNoteName (rootNote() + pitch, true, true, 4), key.reduced (7.0f, 0.0f), juce::Justification::centredLeft);
            g.setColour (pressed ? juce::Colour (0xff0b0d18) : black ? Theme::textDim.withAlpha (0.7f) : juce::Colour (0xff5b607c));
            g.drawText ((pitch > 0 ? "+" : "") + juce::String (pitch), key.reduced (3.0f, 0.0f), juce::Justification::centredRight);
        }
    }

    // Linjalen: takt.slag. Klick = startmarkör, drag eller dubbelklick = loop (rött).
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (rulerArea.toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (rulerArea);
        const auto loopRed = juce::Colour (0xffff4d5e);
        if (hasLoop())
        {
            const auto x1 = ticksToX ((float) loopStart), x2 = ticksToX ((float) loopEnd);
            g.setColour (loopRed.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> (x1, rulerArea.getY(), x2 - x1, rulerArea.getHeight()));
        }
        for (int beat = 0; beat < bars() * 4 + 4 && beat * Pattern::ticksPerQuarter < viewTicks(); ++beat)
        {
            const auto x = ticksToX ((float) (beat * Pattern::ticksPerQuarter));
            if (x < rulerArea.getX() - 20.0f || x > rulerArea.getRight())
                continue;
            const auto barLine = beat % 4 == 0;
            g.setColour (barLine ? Theme::text : Theme::textDim.withAlpha (0.7f));
            g.fillRect (juce::Rectangle<float> (x, rulerArea.getBottom() - (barLine ? rulerArea.getHeight() : 7.0f), 1.0f, barLine ? rulerArea.getHeight() : 7.0f));
            g.setFont (Theme::font (barLine ? 13.0f : 9.5f, barLine));
            g.drawText (barLine ? juce::String (beat / 4 + 1) : juce::String (beat / 4 + 1) + "." + juce::String (beat % 4 + 1),
                        juce::Rectangle<float> (x + 4.0f, rulerArea.getY(), 40.0f, rulerArea.getHeight() - (barLine ? 0.0f : 3.0f)), barLine ? juce::Justification::centredLeft : juce::Justification::bottomLeft);
        }
        if (! hasLoop() && cursorTick > 0)
        {
            const auto x = ticksToX ((float) cursorTick);
            g.setColour (juce::Colour (0xff7ad7ff));
            juce::Path marker;
            marker.addTriangle (x - 5.0f, rulerArea.getY(), x + 5.0f, rulerArea.getY(), x, rulerArea.getY() + 8.0f);
            g.fillPath (marker);
        }
        const auto playTick = playTickNow();
        if (playTick >= 0)
        {
            g.setColour (juce::Colour (0xffffe27a));
            g.fillRect (juce::Rectangle<float> (ticksToX ((float) playTick) - 0.5f, rulerArea.getY(), 1.5f, rulerArea.getHeight()));
        }
    }

    // Verktygsraden: pianorullens längd, vad musen är över, och antal markerade.
    g.setFont (Theme::font (9.5f, true));
    g.setColour (Theme::textDim);

    g.setFont (Theme::font (10.0f, true));
    g.setColour (colour);
    g.drawText (layerName(), infoArea.withWidth (86.0f), juce::Justification::centredLeft);
    g.setColour (Theme::text);
    const auto info = flashText.isNotEmpty() ? flashText
                    : hoverKey.valid() ? describe (hoverKey)
                    : selection.empty() ? juce::String()
                                        : juce::String ((int) selection.size()) + " selected";
    g.drawText (info, infoArea.withTrimmedLeft (88.0f), juce::Justification::centredRight);

    // Tipsrad längst ner: genvägarna.
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, false));
    g.drawText ("SPACE: play / stop   |   SHIFT + SPACE: play only this piano roll   |   RULER: click = start here, drag / double-click = loop (red), Esc = clear   |   CTRL + C / X / V: copy, cut, paste   |   DEL: delete   |   ARROWS: move notes",
                hintArea.withTrimmedLeft (6.0f), juce::Justification::centredLeft);

    // Lanerna under notgallret: delarna (tunna streck), huvudet till vänster och lanens innehåll.
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        const auto area = laneArea[i];
        g.setColour (Theme::textDim.withAlpha (splitterLane == (int) i && drag == Drag::splitter ? 0.9f : 0.35f));
        g.fillRect (juce::Rectangle<float> (area.getX(), area.getY() - laneGap + 1.0f, area.getWidth(), 2.0f));
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRect (area);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (9.0f, true));
        auto header = laneHeader ((int) i);
        if (i == 0)
        {
            g.drawText ("VEL", header.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
            continue;
        }
        g.drawText (juce::String (laneName (lanes[i].param)) + " v", header.withTrimmedLeft (4.0f).withTrimmedRight (12.0f), juce::Justification::centredLeft);
        g.drawText ("x", header.removeFromRight (12.0f), juce::Justification::centred);
        paintLockLane ((int) i, g);
    }
    {
        juce::Graphics::ScopedSaveState state (g);
        const auto velocityArea = laneArea[0];
        g.reduceClipRegion (velocityArea.toNearestInt());
        NoteData note;
        for (int tick = 0; tick < editTicks(); ++tick)
        {
            for (int index = 0; index < Pattern::maxStack; ++index)
            {
                const NoteKey key { tick, index };
                if (! liveNote (key, note))
                    continue;
                const auto x = velocityX (key);
                if (x < velocityArea.getX() - 4.0f || x > velocityArea.getRight())
                    continue;
                const auto height = note.velocity * (velocityArea.getHeight() - 6.0f);
                const auto lit = selection.count (key) > 0 || (key == dragKey && drag == Drag::velocity);
                g.setColour (lit ? juce::Colour (0xffffe27a) : colour.withAlpha (0.85f));
                g.fillRect (juce::Rectangle<float> (x, velocityArea.getBottom() - 3.0f - height, 3.0f, height));
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x + 1.5f, velocityArea.getBottom() - 3.0f - height }));
            }
        }
    }

    // Ram runt hela pianorullen när den är aktiv: då gäller alla genvägar.
    if (hasKeyboardFocus (true))
    {
        g.setColour (colour.withAlpha (0.9f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 2.0f);
    }
}

void PianoRoll::setHover (NoteKey key)
{
    const auto changed = key != hoverKey;
    hoverKey = key;
    if (changed || effectiveTool() == Tool::draw)
        repaint(); // med pennan följer hårkorset och förhandsvisningen musen
}

void PianoRoll::mouseMove (const juce::MouseEvent& event)
{
    hoverPoint = event.position;
    hoverPointValid = true;
    bool edge = false;
    const auto key = gridArea.contains (event.position) && ! pasting ? noteAtPoint (event.position, edge) : NoteKey {};
    hoverEdge = edge;
    shiftHeld = event.mods.isShiftDown();
    altHeld = event.mods.isAltDown();
    setHover (key);
    updatePlus (event.position, key);
    updateCursor (event.position, edge);
    if (splitterAt (event.position) >= 0)
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

const char* PianoRoll::laneName (int param)
{
    static const char* names[] = { "VEL", "CUT", "PAN", "FX" };
    return names[juce::jlimit (0, numLaneParams - 1, param)];
}

const char* PianoRoll::laneTooltip (int param)
{
    static const char* tips[] = { "Velocity", "Filter cutoff", "Pan", "Effect send" };
    return tips[juce::jlimit (0, numLaneParams - 1, param)];
}

int PianoRoll::splitterAt (juce::Point<float> p) const
{
    for (int i = 0; i < (int) lanes.size(); ++i)
    {
        const auto& area = laneArea[(size_t) i];
        if (juce::Rectangle<float> (0.0f, area.getY() - laneGap - 1.0f, area.getRight(), laneGap + 2.0f).contains (p))
            return i;
    }
    return -1;
}

void PianoRoll::paintLockLane (int index, juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState state (g);
    const auto area = laneArea[(size_t) index];
    g.reduceClipRegion (area.toNearestInt());
    const auto param = lanes[(size_t) index].param - 1;
    const auto colour = layerColours[(size_t) juce::jlimit (0, (int) layerColours.size() - 1, baseLayer)];
    NoteData note;
    for (int tick = 0; tick < editTicks(); ++tick)
    {
        const NoteKey key { tick, 0 };
        if (! liveNote (key, note))
            continue;
        const auto x = velocityX (key);
        if (x < area.getX() - 4.0f || x > area.getRight())
            continue;
        const auto value = note.lock[(size_t) param];
        const auto lit = selection.count (key) > 0 || (key == dragKey && drag == Drag::lane);
        if (value < 0.0f)
        {
            g.setColour (Theme::textDim.withAlpha (0.4f));
            g.fillRect (juce::Rectangle<float> (x, area.getBottom() - 4.0f, 3.0f, 1.5f)); // inget lås: noten följer ratten
            continue;
        }
        const auto height = value * (area.getHeight() - 6.0f);
        g.setColour (lit ? juce::Colour (0xffffe27a) : colour.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (x, area.getBottom() - 3.0f - height, 3.0f, height));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ x + 1.5f, area.getBottom() - 3.0f - height }));
    }
}

void PianoRoll::showLaneMenu (int index)
{
    juce::PopupMenu menu;
    for (int param = 1; param < numLaneParams; ++param)
        menu.addItem (param, laneTooltip (param), true, lanes[(size_t) index].param == param);
    menu.addSeparator();
    menu.addItem (100, "Remove this lane");
    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (laneHeader (index).toNearestInt())), [safeThis, index] (int result)
    {
        if (safeThis == nullptr || result == 0 || index >= (int) safeThis->lanes.size())
            return;
        if (result == 100)
            safeThis->lanes.erase (safeThis->lanes.begin() + index);
        else
            safeThis->lanes[(size_t) index].param = result;
        safeThis->resized();
        safeThis->clampView();
        safeThis->updateScrollBars();
        safeThis->repaint();
    });
}

void PianoRoll::addLane()
{
    if (lanes.size() >= (size_t) maxLanes)
        return;
    Lane lane;
    lane.param = 1;
    lanes.push_back (lane);
    resized();
    clampView();
    updateScrollBars();
    repaint();
}

juce::Rectangle<float> PianoRoll::plusRect() const
{
    if (! plusKey.valid())
        return {};
    const auto b = noteBounds (plusKey);
    const auto y = plusBelow ? b.getBottom() + 8.0f : b.getY() - 8.0f;
    return juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ b.getRight(), y });
}

void PianoRoll::updatePlus (juce::Point<float> position, NoteKey hovered)
{
    const auto before = plusKey;
    NoteData note;
    if (hovered.valid() && liveNote (hovered, note) && note.slide)
    {
        plusKey = hovered;
        plusBelow = position.y >= noteBounds (hovered).getCentreY(); // plusset ligger på musens sida av tonen
    }
    else if (plusKey.valid() && (! liveNote (plusKey, note) || ! (plusRect().expanded (4.0f).contains (position) || noteBounds (plusKey).expanded (16.0f).contains (position))))
        plusKey = {};
    if (plusKey != before)
        repaint();
}

void PianoRoll::mouseExit (const juce::MouseEvent&)
{
    hoverPointValid = false;
    if (drag != Drag::stroke)
        plusKey = {};
    setHover ({});
    repaint();
}

void PianoRoll::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus(); // för Ctrl + A, Delete och piltangenterna
    undoBase = processor.snapshotRow (layer);
    edited = false;
    drag = Drag::none;
    dragKey = {};

    // Mittenknappen (hjulklick) panorerar vyn i sidled och höjdled i stället för att rita en ton.
    if (event.mods.isMiddleButtonDown())
    {
        drag = Drag::pan;
        panStart = event.position;
        panScrollTicks = scrollTicks;
        panTopPitch = topPitch;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }

    // Inklistring: ett klick eller drag väljer var tonerna hamnar (spöket följer musen).
    if (pasting)
    {
        if (! gridArea.contains (event.position))
            return;
        if (event.mods.isPopupMenu())
        {
            cancelPaste();
            return;
        }
        pasteGrabTicks = ghostBounds().contains (event.position) ? xToTicks (event.position.x) - (float) pasteStart : 0.0f;
        pasteGrabRow = yToPitch (event.position.y);
        pasteGrabShift = pasteShift;
        drag = Drag::paste;
        movePasteTo (event.position);
        return;
    }

    if (const auto split = splitterAt (event.position); split >= 0)
    {
        drag = Drag::splitter;
        splitterLane = split;
        splitterStartY = event.position.y;
        splitterStartHeight = lanes[(size_t) split].height;
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        return;
    }
    for (int i = 1; i < (int) lanes.size(); ++i)
    {
        if (laneHeader (i).contains (event.position))
        {
            if (laneHeader (i).removeFromRight (12.0f).contains (event.position))
            {
                lanes.erase (lanes.begin() + i);
                resized();
                clampView();
                updateScrollBars();
                repaint();
            }
            else
                showLaneMenu (i);
            return;
        }
        if (laneArea[(size_t) i].contains (event.position))
        {
            const auto key = noteForVelocity (event.position.x);
            if (key.valid() && key.index == 0)
            {
                dragLane = i;
                dragKey = key;
                if (event.mods.isAltDown())
                {
                    processor.pattern.clearLock (layer, key.tick, lanes[(size_t) i].param - 1);
                    edited = true;
                    flash (juce::String (laneName (lanes[(size_t) i].param)) + " lock removed (the note follows the knob again)");
                    commit();
                    dragKey = {};
                    repaint();
                    return;
                }
                drag = Drag::lane;
                mouseDrag (event);
            }
            else if (key.valid())
                flash ("Stacked notes follow their main note's " + juce::String (laneName (lanes[(size_t) i].param)) + " lock; edit the first note of the stack.");
            return;
        }
    }
    if (laneArea[0].contains (event.position))
    {
        const auto key = noteForVelocity (event.position.x);
        if (key.valid())
        {
            drag = Drag::velocity;
            dragKey = key;
            mouseDrag (event);
        }
        return;
    }
    if (rulerArea.contains (event.position))
    {
        if (event.mods.isPopupMenu())
        {
            clearLoop();
            return;
        }
        const auto tick = juce::jlimit (0, std::max (0, totalTicks() - 1), snapped (xToTicks (event.position.x)));
        if (event.getNumberOfClicks() >= 2)
        {
            // Dubbelklick: hela takten blir loopen.
            loopStart = tick / Pattern::ticksPerBar * Pattern::ticksPerBar;
            loopEnd = std::min (loopStart + Pattern::ticksPerBar, totalTicks());
            syncPreviewWindow();
            repaint();
            return;
        }
        drag = Drag::ruler;
        rulerAnchor = tick;
        rulerDragged = false;
        return;
    }
    if (keyArea.contains (event.position) && ! event.mods.isPopupMenu())
    {
        drag = Drag::keys;
        auditionStart (yToPitch (event.position.y), Pattern::defaultVelocity (layer));
        return;
    }
    // Glide-plus: ett klick på plusset börjar ett glidestreck från glide-tonens mitt (höjd) och slut (tid).
    if (! event.mods.isPopupMenu() && plusKey.valid() && plusRect().expanded (2.0f).contains (event.position))
    {
        const auto b = noteBounds (plusKey);
        drag = Drag::stroke;
        strokeFromPlus = true;
        strokeStart = { b.getRight(), b.getCentreY() };
        strokeEnd = event.position;
        return;
    }
    if (! gridArea.contains (event.position))
        return;

    bool edge = false;
    const auto key = noteAtPoint (event.position, edge);

    if (event.mods.isPopupMenu())
    {
        if (key.valid())
        {
            // Högerklick på en markerad ton tar bort hela markeringen, annars bara tonen.
            if (selection.count (key) == 0)
                selection = { key };
            deleteSelected();
        }
        else if (! strokeMenuNow)
        {
            // Högerdrag ritar ett glidestreck; släpps musen utan drag visas menyn (mouseUp).
            drag = Drag::stroke;
            strokeStart = strokeEnd = event.position;
            return;
        }
        else
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Select all", true);
            menu.addItem (2, "Clear selection", ! selection.empty());
            menu.addItem (3, "Delete selected", ! selection.empty());
            if (baseLayer != Pattern::kickLayer && ! sliceMode) // skivor har inga ackord
            {
                // Samma ackord på alla markerade bockas för.
                const auto hasChordTargets = std::any_of (selection.begin(), selection.end(), [] (const auto& selectedKey) { return selectedKey.index == 0; });
                std::optional<Pattern::Chord> common;
                bool same = true;
                for (const auto& selectedKey : selection)
                {
                    if (selectedKey.index != 0)
                        continue;
                    const auto chord = processor.pattern.note (layer, selectedKey.tick).getChord();
                    if (common && *common != chord) same = false;
                    common = chord;
                }
                juce::PopupMenu chords;
                for (int i = 0; i < (int) chordChoices.size(); ++i)
                    chords.addItem (10 + i, chordChoices[(size_t) i].name, true, same && common && *common == chordChoices[(size_t) i].chord);
                menu.addSubMenu (voices() >= 2 ? "Chord (selected notes)" : "Chord (selected notes, needs VOICES 2 or more)", chords, hasChordTargets);
            }
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
                else if (result >= 10 && result < 10 + (int) chordChoices.size()) safeThis->setChordOnSelection (chordChoices[(size_t) (result - 10)].chord);
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
                        const auto before = safeThis->processor.snapshotRow (safeThis->layer);
                        safeThis->processor.pattern.clearNotes (safeThis->layer); // nollställer pianorullsmarkeringen
                        safeThis->processor.patternChanged();
                        safeThis->processor.recordRowEdit (safeThis->layer, before, "Delete the piano roll");
                        if (safeThis->onDeleted)
                            safeThis->onDeleted();
                    });
                }
            });
        }
        return;
    }

    altHeld = event.mods.isAltDown();
    const auto additive = event.mods.isShiftDown() || event.mods.isCtrlDown() || event.mods.isCommandDown(); // Skift eller Ctrl lägger till i markeringen
    const auto currentTool = event.mods.isAltDown() ? Tool::erase : additive ? Tool::select : effectiveTool();

    // Sudd: ta bort tonen under musen, och alla man drar över.
    if (currentTool == Tool::erase)
    {
        drag = Drag::erase;
        lastErasePoint = event.position;
        eraseBetween (event.position, event.position);
        return;
    }

    if (key.valid())
    {
        if (key != justCreated)
            justCreated = {};

        // Skift eller Ctrl lägger till eller tar bort tonen i markeringen utan att flytta något.
        if (additive)
        {
            if (selection.count (key) > 0)
                selection.erase (key);
            else
                selection.insert (key);
            repaint();
            return;
        }

        // En omarkerad ton blir ensam markerad; en markerad ton behåller hela markeringen (så att den flyttas med).
        if (selection.count (key) == 0)
            selection = { key };

        drag = edge ? Drag::resize : Drag::move;
        dragKey = key;
        dragStartKey = key;
        NoteData grabbed;
        liveNote (key, grabbed);
        dragStartPitch = grabbed.pitch;
        grabOffsetTicks = xToTicks (event.position.x) - (float) key.tick;
        dragSnapshot = readNotes();
        dragSelection = selection;
        if (! edge)
        {
            NoteData grabbedNote;
            if (liveNote (key, grabbedNote))
                auditionStart (grabbedNote.pitch, grabbedNote.velocity);
        }
        repaint();
        return;
    }

    // MARKERA-verktyget, skift eller Ctrl: dra en ruta i tomt (skift/Ctrl lägger till i markeringen, annars ersätter rutan den).
    if (currentTool == Tool::select)
    {
        drag = Drag::box;
        boxStart = boxEnd = event.position;
        boxBase = additive ? selection : KeySet();
        selection = boxBase;
        repaint();
        return;
    }

    // (Ett klick i takten efter radens slut lägger tonen där; raden växer med innehållet när ändringen sparas: Pattern::refreshLengths.)

    // Tom plats: en ny ton på steget under musen, i raden under musen.
    const auto start = snapped (xToTicks (event.position.x));
    const auto pitch = juce::jlimit (lowPitch(), highPitch(), yToPitch (event.position.y));
    if (! processor.pattern.acceptsTick (layer, start))
        return; // låst rad: klick efter slutet gör ingenting
    const auto glide = glideButton.getToggleState();
    const auto length = ! glide && lastLength > 0 ? std::max (lastLength, snapTicks()) : Pattern::ticksPerStep (grid());
    NoteKey created { start, 0 };
    if (processor.pattern.note (layer, start).on.load())
    {
        if (processor.pattern.note (layer, start).pitch.load() == pitch)
        {
            flash ("A note at this tick already has that pitch");
            return;
        }
        for (int index = 1; index < Pattern::maxStack; ++index)
        {
            Pattern::StackNote existing;
            if (processor.pattern.getStack (layer, start, index, existing) && existing.pitch == pitch)
            {
                flash ("A note at this tick already has that pitch");
                return;
            }
        }
        Pattern::StackNote stacked { length, pitch, Pattern::defaultVelocity (layer), glide };
        const auto slot = processor.pattern.addExtra (layer, start, stacked);
        if (slot < 0)
        {
            flash ("This tick already has the maximum number of notes");
            return;
        }
        created.index = slot;
    }
    else
        processor.pattern.addNote (layer, start, length, pitch, Pattern::defaultVelocity (layer), glide);
    selection = { created };
    auditionStart (pitch, Pattern::defaultVelocity (layer));
    justCreated = created;
    drag = Drag::resize; // klick = en kort ton; dras musen åt höger medan knappen hålls följer längden draget
    dragKey = created;
    dragStartKey = created;
    dragStartPitch = pitch;
    grabOffsetTicks = 0.0f;
    dragSnapshot = readNotes();
    dragSelection = selection;
    edited = true;
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& event)
{
    hoverPoint = event.position;
    hoverPointValid = true;
    if (drag == Drag::none)
        return;

    if (drag == Drag::splitter)
    {
        lanes[(size_t) splitterLane].height = splitterStartHeight - (event.position.y - splitterStartY);
        resized();
        clampView();
        updateScrollBars();
        repaint();
        return;
    }

    if (drag == Drag::pan)
    {
        scrollTicks = panScrollTicks - (event.position.x - panStart.x) / std::max (0.05f, pixelsPerTick);
        topPitch = panTopPitch + (event.position.y - panStart.y) / std::max (1.0f, rowHeight());
        clampView();
        repaint();
        return;
    }

    if (drag == Drag::stroke)
    {
        strokeEnd = event.position;
        if (strokeFromPlus)
        {
            // Snappa till en ton som börjar efter strecket startar: målet blir dess början och mitt.
            bool edge = false;
            const auto target = gridArea.contains (event.position) ? noteAtPoint (event.position, edge) : NoteKey {};
            if (target.valid() && target != plusKey)
                if (const auto tb = noteBounds (target); tb.getX() > strokeStart.x)
                    strokeEnd = { tb.getX(), tb.getCentreY() };
        }
        repaint();
        return;
    }

    if (drag == Drag::ruler)
    {
        if (! rulerDragged && std::abs (event.getDistanceFromDragStartX()) < 4)
            return;
        rulerDragged = true;
        const auto tick = juce::jlimit (0, std::max (0, totalTicks() - 1), snapped (xToTicks (event.position.x)));
        loopStart = std::min (rulerAnchor, tick);
        loopEnd = std::min (totalTicks(), std::max (rulerAnchor, tick) + snapTicks());
        syncPreviewWindow();
        repaint();
        return;
    }
    if (drag == Drag::keys)
    {
        if (keyArea.getVerticalRange().contains (event.position.y))
            auditionStart (yToPitch (event.position.y), Pattern::defaultVelocity (layer));
        return;
    }
    if (drag == Drag::paste)
    {
        movePasteTo (event.position);
        return;
    }
    if (drag == Drag::erase)
    {
        eraseBetween (lastErasePoint, event.position);
        lastErasePoint = event.position;
        return;
    }

    if (drag == Drag::box)
    {
        boxEnd = event.position;
        const auto rect = juce::Rectangle<float> (boxStart, boxEnd);
        selection = boxBase;
        forEachLive (0, editTicks() - 1, [&] (NoteKey key, const NoteData&)
        {
            if (noteBounds (key).intersects (rect))
                selection.insert (key);
        });
        repaint();
        return;
    }

    if (! dragKey.valid())
        return;

    if (drag == Drag::velocity)
    {
        setLiveVelocity (dragKey, juce::jlimit (0.02f, 1.0f, 1.0f - (event.position.y - laneArea[0].getY() - 3.0f) / (laneArea[0].getHeight() - 6.0f)));
        edited = true;
        repaint();
        return;
    }

    if (drag == Drag::lane)
    {
        const auto& area = laneArea[(size_t) dragLane];
        const auto value = juce::jlimit (0.0f, 1.0f, 1.0f - (event.position.y - area.getY() - 3.0f) / (area.getHeight() - 6.0f));
        processor.pattern.setLock (layer, dragKey.tick, lanes[(size_t) dragLane].param - 1, value);
        edited = true;
        repaint();
        return;
    }

    if (drag == Drag::resize)
    {
        // Alla markerade ändras lika mycket som den grepna.
        const auto step = snapTicks();
        const auto end = snapEnabled ? (int) std::round (xToTicks (event.position.x) / (float) step) * step : (int) std::round (xToTicks (event.position.x));
        const auto& grabbed = dragSnapshot[dragStartKey];
        const auto delta = end - (dragStartKey.tick + grabbed.length);

        auto target = dragSnapshot;
        auto shortest = Pattern::maxTicks;
        for (const auto& key : dragSelection)
            shortest = std::min (shortest, dragSnapshot[key].length);
        const auto limitedDelta = std::max (delta, step - shortest); // ingen ton kortare än ett steg
        for (const auto& key : dragSelection)
            target[key].length = juce::jlimit (1, Pattern::maxTicks - key.tick, dragSnapshot[key].length + limitedDelta);
        applyNotes (target);
        lastLength = target[dragStartKey].length;
        edited = true;
        repaint();
        return;
    }

    // Flytta: ny starttid (snap) och ny rad för den grepna tonen; resten av markeringen följer med.
    const auto newTick = snapped (xToTicks (event.position.x) - grabOffsetTicks);
    const auto newPitch = juce::jlimit (lowPitch(), highPitch(), yToPitch (event.position.y));
    auto deltaTicks = newTick - dragStartKey.tick, deltaPitch = newPitch - dragStartPitch;

    auto minTick = editTicks(), maxTick = 0, minPitch = highPitch(), maxPitch = lowPitch();
    for (const auto& key : dragSelection)
    {
        minTick = std::min (minTick, key.tick);
        maxTick = std::max (maxTick, key.tick);
        minPitch = std::min (minPitch, dragSnapshot[key].pitch);
        maxPitch = std::max (maxPitch, dragSnapshot[key].pitch);
    }
    deltaTicks = clampSafe (deltaTicks, -minTick, editTicks() - 1 - maxTick);
    deltaPitch = clampSafe (deltaPitch, lowPitch() - minPitch, highPitch() - maxPitch);

    const auto moved = relocate (dragSnapshot, dragSelection, deltaTicks, deltaPitch, lowPitch(), highPitch());
    if (moved.full)
    {
        flash ("Blocked: that tick is full, or already has a note with that pitch");
        return;
    }
    applyNotes (moved.target);
    auditionStart (dragStartPitch + deltaPitch, 0.8f); // hör tonen man flyttar till
    selection.clear();
    for (const auto& entry : moved.where)
        selection.insert (entry.second);
    if (const auto grabbed = moved.where.find (dragStartKey); grabbed != moved.where.end())
    {
        dragKey = grabbed->second;
        if (const auto placed = moved.target.find (dragKey); placed != moved.target.end())
            lastLength = placed->second.length;
        hoverKey = dragKey;
    }
    edited = true;
    repaint();
}

void PianoRoll::addStroke (const juce::Point<float>& from, const juce::Point<float>& to)
{
    // Strecket läses vänster till höger: början är glidens start, slutet dess mål. Glidens längd är streckets längd.
    const auto& a = from.x <= to.x ? from : to;
    const auto& b = from.x <= to.x ? to : from;
    const auto start = juce::jlimit (0, std::max (0, totalTicks() - 1), snapped (xToTicks (a.x)));
    auto end = std::max (start + snapTicks(), snapped (xToTicks (b.x)));
    auto pitch = juce::jlimit (lowPitch(), highPitch(), yToPitch (b.y));

    // Strecket fastnar på en ton som börjar vid slutet: glidemålet blir tonens tonhöjd.
    if (const auto& target = processor.pattern.note (layer, end); end < totalTicks() && target.on.load())
        pitch = target.pitch.load();

    if (! processor.pattern.acceptsTick (layer, start))
        return;

    // Glide-tonen läggs på starten (före en ton om strecket börjar före den); ligger en ton redan där blir den en staplad glide-ton.
    const auto length = end - start;
    NoteKey created { start, 0 };
    if (processor.pattern.note (layer, start).on.load())
    {
        Pattern::StackNote stacked { length, pitch, Pattern::defaultVelocity (layer), true };
        const auto slot = processor.pattern.addExtra (layer, start, stacked);
        if (slot < 0)
        {
            flash ("This tick already has the maximum number of notes");
            return;
        }
        created.index = slot;
    }
    else
        processor.pattern.addNote (layer, start, length, pitch, Pattern::defaultVelocity (layer), true);
    selection = { created };
    edited = true;
}

void PianoRoll::mouseUp (const juce::MouseEvent& event)
{
    if (drag == Drag::stroke)
    {
        drag = Drag::none;
        const auto fromPlus = strokeFromPlus;
        strokeFromPlus = false;
        if (fromPlus && strokeStart.getDistanceFrom (strokeEnd) < 6.0f)
        {
            repaint(); // ett klick utan drag på plusset gör inget
            return;
        }
        if (strokeStart.getDistanceFrom (strokeEnd) < 6.0f)
        {
            strokeMenuNow = true;
            mouseDown (event);
            strokeMenuNow = false;
            return;
        }
        addStroke (strokeStart, strokeEnd);
        if (edited)
            commit();
        repaint();
        return;
    }

    if (drag == Drag::pan || drag == Drag::splitter)
    {
        drag = Drag::none;
        splitterLane = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
        return;
    }

    if (drag == Drag::ruler && ! rulerDragged)
    {
        // Klick på linjalen: startmarkören flyttas dit (spelas den här raden börjar den om därifrån) och en markerad loop släpps.
        cursorTick = rulerAnchor;
        loopStart = loopEnd = -1;
        syncPreviewWindow();
        if (processor.previewingLayer.load() == baseLayer)
            processor.requestAudition (PluginBase::previewRequest (baseLayer));
    }
    auditionStop (drag == Drag::keys || lastLength <= 0 ? Pattern::ticksPerQuarter / 2 : lastLength);
    if (edited)
        commit();
    drag = Drag::none;
    dragKey = {};
    dragStartKey = {};
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& event)
{
    bool edge = false;
    if (! gridArea.contains (event.position) || event.mods.isPopupMenu() || event.mods.isMiddleButtonDown() || event.mods.isShiftDown() || event.mods.isCtrlDown())
        return;
    const auto key = noteAtPoint (event.position, edge);
    if (! key.valid())
        return;

    // Ett dubbelklick på en plats där första klicket just skapade tonen ska inte sätta slide på den.
    if (key == justCreated)
    {
        justCreated = {};
        return;
    }

    // Slide på/av för alla markerade (eller bara tonen om den inte är markerad).
    auto targets = selection.count (key) > 0 ? selection : KeySet { key };
    NoteData clicked;
    liveNote (key, clicked);
    const auto turnOn = ! clicked.slide;
    for (const auto& target : targets)
        setLiveSlide (target, turnOn);
    commit();
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto ctrl = mods.isCtrlDown() || mods.isCommandDown();

    // Mellanslag: förhandsspela hela synthen (igen = stopp). Skift + mellanslag: bara den här pianorullen.
    if (key.getKeyCode() == juce::KeyPress::spaceKey && ! ctrl)
    {
        if (mods.isShiftDown())
            playThisRow();
        else
            processor.requestAudition (processor.previewingLayer.load() == PluginBase::previewAllRows ? PluginBase::previewStop : PluginBase::previewRequest (PluginBase::previewAllRows));
        return true;
    }

    undoBase = processor.snapshotRow (layer); // före en eventuell ändring (Delete, piltangenter, klipp ut, inklistring)

    // Inklistring pågår: Enter bekräftar, Esc avbryter. Skrollning och zoom fungerar som vanligt.
    if (pasting)
    {
        if (key == juce::KeyPress::returnKey)
            commitPaste();
        else if (key == juce::KeyPress::escapeKey)
            cancelPaste();
        return true;
    }
    if (ctrl && (key.getKeyCode() == 'C' || key.getKeyCode() == 'c'))
    {
        copySelection (false);
        return true;
    }
    if (ctrl && (key.getKeyCode() == 'X' || key.getKeyCode() == 'x'))
    {
        copySelection (true);
        return true;
    }
    if (ctrl && (key.getKeyCode() == 'V' || key.getKeyCode() == 'v'))
    {
        startPaste();
        return true;
    }
    if ((mods.isCtrlDown() || mods.isCommandDown()) && (key.getKeyCode() == 'A' || key.getKeyCode() == 'a'))
    {
        selectAll();
        return true;
    }
    if (ctrl && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z' || key.getKeyCode() == 'Y' || key.getKeyCode() == 'y'))
    {
        // Ctrl+Z ångrar; Ctrl+Skift+Z och Ctrl+Y gör om. Det gäller hela plugin:et (parametrar och alla rader), inte bara den här rullen.
        const auto redo = key.getKeyCode() == 'Y' || key.getKeyCode() == 'y' || mods.isShiftDown();
        selection.clear();
        if (redo)
            processor.undoManager.redo();
        else
            processor.undoManager.undo();
        edited = false;
        repaint();
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelected();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (selection.empty())
            clearLoop();
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
