#include "KickLibraryPanel.h"
#include "../Core/Settings.h"

//==============================================================================
KickTabs::KickTabs (juce::Colour accent)
{
    for (auto* button : { &synthButton, &sampleButton })
    {
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        addAndMakeVisible (*button);
    }
    synthButton.setTooltip ("SYNTH - The synthesized kick: pitch and volume envelopes, body, click, drive and the rest. Changing view never changes the sound.");
    sampleButton.setTooltip ("SAMPLE - Use a sample as the kick or on top of it: pick one from your library or load a file. Changing view never changes the sound.");
    synthButton.onClick = [this] { select (0); if (onChange) onChange (0); };
    sampleButton.onClick = [this] { select (1); if (onChange) onChange (1); };
    select (0);
}

void KickTabs::select (int tab)
{
    synthButton.setToggleState (tab == 0, juce::dontSendNotification);
    sampleButton.setToggleState (tab == 1, juce::dontSendNotification);
}

void KickTabs::resized()
{
    auto area = getLocalBounds().reduced (0, 1);
    const auto width = (area.getWidth() - 4) / 2;
    synthButton.setBounds (area.removeFromLeft (width));
    area.removeFromLeft (4);
    sampleButton.setBounds (area);
}

//==============================================================================
KickSampleView::KickSampleView (PluginBase& p) : processor (p)
{
    setMouseCursor (speakerCursor);
    setTooltip ("SAMPLE - The waveform of the loaded sample. Click it to hear just the sample, whatever MODE is.");
}

void KickSampleView::refresh()
{
    if (processor.clickForDisplay() != shown)
        repaint();
}

void KickSampleView::mouseDown (const juce::MouseEvent&)
{
    if (processor.clickForDisplay() != nullptr)
        processor.requestAudition (5); // bara samplingen (som HEAR SAMPLE)
}

void KickSampleView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto* sample = processor.clickForDisplay();
    shown = sample;
    if (sample == nullptr || sample->audio.getNumSamples() < 2)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawText ("No sample loaded. Click a kick in the library below, press LOAD, or drop a WAV here.", bounds, juce::Justification::centred);
        return;
    }

    const auto area = bounds.reduced (8.0f, 18.0f);
    const auto* data = sample->audio.getReadPointer (0);
    const auto length = sample->audio.getNumSamples();
    const auto width = juce::jmax (1, (int) area.getWidth());

    // Min och max per bildpunkt: övre kanten framåt, nedre kanten bakåt blir en fylld form.
    std::vector<juce::Point<float>> tops, bottoms;
    for (int x = 0; x < width; ++x)
    {
        const auto from = (int) ((juce::int64) x * length / width);
        const auto to = juce::jmax (from + 1, (int) ((juce::int64) (x + 1) * length / width));
        auto lowest = 0.0f, highest = 0.0f;
        for (int i = from; i < to && i < length; ++i)
        {
            lowest = juce::jmin (lowest, data[i]);
            highest = juce::jmax (highest, data[i]);
        }
        const auto px = area.getX() + (float) x;
        tops.emplace_back (px, area.getCentreY() - highest * area.getHeight() * 0.5f);
        bottoms.emplace_back (px, area.getCentreY() - lowest * area.getHeight() * 0.5f);
    }
    juce::Path upper, lower, shape;
    upper.startNewSubPath (tops.front());
    lower.startNewSubPath (bottoms.front());
    shape.startNewSubPath (tops.front());
    for (size_t i = 1; i < tops.size(); ++i)
    {
        upper.lineTo (tops[i]);
        lower.lineTo (bottoms[i]);
        shape.lineTo (tops[i]);
    }
    for (auto point = bottoms.rbegin(); point != bottoms.rend(); ++point)
        shape.lineTo (*point);
    shape.closeSubPath();
    g.setColour (Theme::kick.withAlpha (0.55f));
    g.fillPath (shape);
    g.setColour (Theme::kick);
    g.strokePath (upper, juce::PathStrokeType (1.0f));
    g.strokePath (lower, juce::PathStrokeType (1.0f));

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f));
    g.drawText (sample->name, bounds.reduced (8.0f, 3.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    g.drawText (juce::String (juce::roundToInt ((float) length / 44.1f)) + " ms", bounds.reduced (8.0f, 3.0f).removeFromTop (14.0f), juce::Justification::centredRight);
}

//==============================================================================
KickLibraryPanel::KickLibraryPanel (PluginBase& p, juce::Colour accentIn) : processor (p), accent (accentIn)
{
    setTooltip ("LIBRARY - Kicks from your library folder (SETTINGS > FOLDERS > LIBRARY). Click one to load it as the kick's sample; if MODE was CLICK it switches to REPLACE. "
                "Type to search by name, tag or pack.");

    searchBox.setTextToShowWhenEmpty ("Search kicks: name, tag or pack", Theme::textDim);
    searchBox.setFont (Theme::font (12.0f));
    searchBox.setSelectAllWhenFocused (true);
    searchBox.addListener (this);
    searchBox.setTooltip ("Search the kicks by name, tag, pack, author or licence. All the words you type must match.");
    addAndMakeVisible (searchBox);

    statusLabel.setFont (Theme::font (10.5f));
    statusLabel.setColour (juce::Label::textColourId, Theme::textDim);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (statusLabel);

    rescanButton.setTooltip ("RESCAN - Reads the library folder again (files you added or changed while Kickbase was open).");
    rescanButton.onClick = [this] { index->rescan (Settings::libraryFolder()); };
    addAndMakeVisible (rescanButton);

    list.setRowHeight (22);
    list.setColour (juce::ListBox::backgroundColourId, Theme::knobBodyDark.withAlpha (0.55f));
    list.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff24283c));
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    index->addChangeListener (this);
    snapshot = index->entries();
    hiddenPackages = index->problems().size();
    applyFilter();
    index->refreshIfChanged (Settings::libraryFolder()); // vid start: bakgrundstråden läser mappen
}

KickLibraryPanel::~KickLibraryPanel()
{
    index->removeChangeListener (this);
    searchBox.removeListener (this);
    list.setModel (nullptr);
}

void KickLibraryPanel::visibilityChanged()
{
    if (isShowing())
        index->refreshIfChanged (Settings::libraryFolder()); // (inte i en timer) bara när vyn visas
}

void KickLibraryPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    snapshot = index->entries();
    hiddenPackages = index->problems().size();
    applyFilter();
}

void KickLibraryPanel::setLoadedName (const juce::String& name)
{
    if (name != loadedName)
    {
        loadedName = name;
        list.repaint();
    }
}

void KickLibraryPanel::applyFilter()
{
    shown = snapshot != nullptr ? LibraryIndex::filter (*snapshot, "Kicks", searchBox.getText()) : std::vector<int>();
    list.updateContent();
    list.repaint();
    updateStatus();
}

void KickLibraryPanel::updateStatus()
{
    auto total = 0;
    if (snapshot != nullptr)
        for (const auto& entry : *snapshot)
            total += entry.category == "Kicks" ? 1 : 0;
    juce::String text;
    if (message.isNotEmpty())
        text = message;
    else if (total == 0)
        text = hiddenPackages > 0 ? juce::String (hiddenPackages) + " package problem(s): see SETTINGS" : juce::String();
    else
        text = (shown.size() == (size_t) total ? juce::String (total) + " kicks" : juce::String ((int) shown.size()) + " of " + juce::String (total) + " kicks")
               + (hiddenPackages > 0 ? "  -  " + juce::String (hiddenPackages) + " package problem(s): see SETTINGS" : juce::String());
    statusLabel.setText (text, juce::dontSendNotification);
    repaint();
}

void KickLibraryPanel::loadRow (int row)
{
    if (snapshot == nullptr || row < 0 || row >= (int) shown.size())
        return;
    const auto& entry = (*snapshot)[(size_t) shown[(size_t) row]];
    if (! processor.loadClickSample (entry.file))
    {
        message = "Could not read " + entry.file.getFileName();
        updateStatus();
        return;
    }
    message = {};
    // MODE: ett tydligt val av användaren. Stod den på CLICK blir det REPLACE (en gest, så att DAW:en ser en ändring); annars rörs den inte.
    if (auto* mode = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (ParamID::kickSampleMode)))
        if (mode->getIndex() == 0)
        {
            mode->beginChangeGesture();
            mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));
            mode->endChangeGesture();
        }
    loadedName = entry.file.getFileNameWithoutExtension();
    list.repaint();
    updateStatus();
    if (onLoaded)
        onLoaded();
}

void KickLibraryPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (snapshot == nullptr || row < 0 || row >= (int) shown.size())
        return;
    const auto& entry = (*snapshot)[(size_t) shown[(size_t) row]];
    if (selected)
    {
        g.setColour (accent.withAlpha (0.22f));
        g.fillRect (0, 0, width, height);
    }
    const auto isLoaded = loadedName.isNotEmpty() && entry.file.getFileNameWithoutExtension() == loadedName;
    if (isLoaded)
    {
        g.setColour (accent);
        g.fillEllipse (6.0f, (float) height * 0.5f - 3.0f, 6.0f, 6.0f);
    }
    auto area = juce::Rectangle<int> (0, 0, width, height).withTrimmedLeft (18).withTrimmedRight (8);
    juce::Rectangle<int> right = area.removeFromRight (juce::jmin (140, area.getWidth() / 3));
    g.setFont (Theme::font (12.0f, isLoaded));
    g.setColour (isLoaded ? Theme::text : Theme::text.withAlpha (0.9f));
    g.drawText (entry.name, area.removeFromLeft (area.getWidth() * 11 / 20), juce::Justification::centredLeft, true);
    g.setFont (Theme::font (10.5f));
    g.setColour (Theme::textDim);
    g.drawText (entry.tags.joinIntoString (" "), area, juce::Justification::centredLeft, true);
    if (entry.personal)
    {
        g.setColour (juce::Colour (0xffe6c23c));
        g.drawText ("PERSONAL", right, juce::Justification::centredRight, false);
    }
    else
        g.drawText (entry.package, right, juce::Justification::centredRight, true);
}

void KickLibraryPanel::paintOverChildren (juce::Graphics& g)
{
    if (! shown.empty() || snapshot == nullptr)
        return;
    // Tom lista: säg varför (ingen sökträff, eller inget bibliotek än).
    auto area = list.getBounds().reduced (12);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (12.0f));
    const auto total = snapshot->size();
    g.drawFittedText (total == 0 ? "The library has no kicks yet. Put kick packs in the LIBRARY folder (SETTINGS > FOLDERS > OPEN), then press RESCAN."
                                 : "No kick matches the search.",
                      area, juce::Justification::centred, 3);
}

void KickLibraryPanel::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (24);
    rescanButton.setBounds (top.removeFromRight (64));
    top.removeFromRight (6);
    searchBox.setBounds (top);
    auto status = area.removeFromBottom (16);
    statusLabel.setBounds (status);
    area.removeFromTop (4);
    list.setBounds (area);
}
