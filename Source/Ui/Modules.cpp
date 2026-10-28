#include "InstrumentMenu.h"
#include "Modules.h"
#include "Help.h"
#include "HelpDialog.h"
#include "HistoryPanel.h"
#include "JogPanel.h"
#include "../Core/ProjectFile.h"

namespace
{
    constexpr int knobRowHeight = 74;
    constexpr int presetButtonWidth = 150;

    // Modulens presetknapp, uppe till höger i rubriken.
    juce::Rectangle<int> presetBounds (const juce::Component& module)
    {
        return { module.getWidth() - 10 - presetButtonWidth, 5, presetButtonWidth, 20 };
    }

    juce::RangedAudioParameter* param (PluginBase& processor, const juce::String& id)
    {
        auto* parameter = processor.apvts.getParameter (id);
        jassert (parameter != nullptr);
        return parameter;
    }

    void addKnobs (juce::Component& parent, std::vector<std::unique_ptr<KnobComponent>>& knobs, PluginBase& processor,
                   juce::Colour accent, std::initializer_list<std::pair<juce::String, const char*>> specs)
    {
        for (const auto& [id, name] : specs)
        {
            knobs.push_back (std::make_unique<KnobComponent> (processor.apvts, id, name, accent));
            parent.addAndMakeVisible (*knobs.back());
        }
    }

    template <typename T>
    std::vector<juce::Component*> pointers (std::vector<std::unique_ptr<T>>& items, size_t from, size_t to)
    {
        std::vector<juce::Component*> result;
        for (auto i = from; i < to && i < items.size(); ++i)
            result.push_back (items[i].get());
        return result;
    }
}

//==============================================================================
HeaderBar::HeaderBar (PluginBase& p, const juce::String& subtitleIn)
    : processor (p), subtitle (subtitleIn), logo (subtitleIn), presets (p, Presets::Section::full, Theme::pattern)
{
    addAndMakeVisible (presets);

    if (processor.getKeyboardState() != nullptr) // instrumentet
    {
        listenAll.setTooltip ("LISTEN - Hold to hear the whole synth (the kick, the instruments and the pattern) on the note chosen next to it, looping while you hold. "
                              "Press again to start over. Double-click to let it keep playing until you click again.");
        listenAll.setPlayIcon (true); // play/stopp-ikon i toppbaren; högtalaren finns kvar på raderna
        listenRoll.setPlayIcon (true);
        listenAll.onStart = [this] { processor.requestAudition (PluginBase::previewRequest (PluginBase::previewAllRows)); };
        listenAll.onStop = [this] { processor.requestAudition (PluginBase::previewStop); };
        addAndMakeVisible (listenAll);

        listenRoll.setTooltip ("LISTEN PIANO ROLL - Hold to hear just the open piano roll (from the start marker, or looping the red region) on the preview note. "
                               "Shift + Space does the same; Space plays the whole synth.");
        listenRoll.onStart = [this]
        {
            const auto row = processor.openRollLayer.load();
            if (row >= 0)
                processor.requestAudition (PluginBase::previewRequest (row));
        };
        listenRoll.onStop = [this] { processor.requestAudition (PluginBase::previewStop); };
        addChildComponent (listenRoll);

        for (int note = 24; note < 96; ++note) // C1 ... B6
            noteBox.addItem (juce::MidiMessage::getMidiNoteName (note, true, true, 5), note + 1);
        noteBox.setSelectedId (processor.previewNote.load() + 1, juce::dontSendNotification);
        noteBox.setTooltip ("PREVIEWS - The note all listening plays: this synth LISTEN button, the rows' speaker buttons, the piano rolls and the oscillators. "
                            "C5 by default.");
        noteBox.onChange = [this] { processor.previewNote = noteBox.getSelectedId() - 1; };
        addAndMakeVisible (noteBox);

        restartButton.setTooltip ("RESTART - Starts the counter and the pattern over at bar 1.1 (when the plugin runs on its own clock; with DAW SYNC the DAW decides).");
        restartButton.onClick = [this] { processor.requestRestart(); };
        addAndMakeVisible (restartButton);

        jogButton.setTooltip ("JOG - A turntable wheel for the plugin's own clock (scratch the music), a LOOP between two positions and a test tempo. "
                              "It never touches the DAW. Lit when LOOP is on.");
        jogButton.onClick = [this] { showJogPanel(); };
        addAndMakeVisible (jogButton);

        auto setUpLabel = [this] (juce::Label& label, float size)
        {
            label.setFont (Theme::font (size, true));
            label.setJustificationType (juce::Justification::centred);
            label.setColour (juce::Label::textColourId, Theme::text);
            label.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            label.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0b0d18));
            label.setColour (juce::Label::textWhenEditingColourId, Theme::text);
            label.setBorderSize ({ 0, 0, 0, 0 });
            addAndMakeVisible (label);
        };
        setUpLabel (bpmLabel, 13.0f);
        setUpLabel (barLabel, 14.0f);
        bpmLabel.onTextChange = [this]
        {
            const auto value = bpmLabel.getText().retainCharacters ("0123456789.").getDoubleValue();
            processor.freeRunBpm = value > 0.0 ? juce::jlimit (20.0, 300.0, value) : 0.0; // tom = tillbaka till värdens/standardtempot
            updateTransportLabels (processor.transportStatus.followingHost.load());
        };
        barLabel.onTextChange = [this]
        {
            if (const auto bar = barLabel.getText().retainCharacters ("0123456789").getIntValue(); bar >= 1)
                processor.seekBarRequest = bar - 1;
            updateTransportLabels (processor.transportStatus.followingHost.load());
        };
        scrubRelease.callback = [this]
        {
            if (scrubBar >= 0)
                processor.seekBarRequest = scrubBar;
            processor.transportHold = false;
            scrubBar = -1;
        };
        updateTransportLabels (false);
    }

    if (auto* gainParam = processor.apvts.getParameter (ParamID::gain))
    {
        masterKnob.setTooltip ("MASTER - Your listening volume: the same OUTPUT volume as the fader in the mixer (double-click for the default).");
        masterKnob.setColour (juce::Slider::rotarySliderFillColourId, Theme::master);
        masterKnob.setColour (juce::Slider::thumbColourId, Theme::text);
        masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamID::gain, masterKnob);
        masterKnob.setDoubleClickReturnValue (true, gainParam->convertFrom0to1 (gainParam->getDefaultValue()));
        addAndMakeVisible (masterKnob);
    }
    projectButton.onClick = [this] { showProjectMenu(); };
    addAndMakeVisible (projectButton);
    updateAiTooltip();
    startVisualTimer (Settings::Visual::topBar, 30);
    if (processor.getKeyboardState() != nullptr)
        rollWatch = std::make_unique<RollWatch> ([this] { syncRollButton(); });
}

void HeaderBar::updateAiTooltip()
{
    projectButton.setTooltip (processor.isBridgeEnabled()
        ? "MENU - New, open (and the 3 most recent), save (Ctrl+S) a project file (.kbproj), export, and the AI server. The AI SERVER (MCP) is ON (instance " + processor.getBridgeId() + "): an AI assistant using the Kickbase MCP bridge can read and change this plugin and hear/measure it offline."
        : "MENU - New, open (and the 3 most recent), save (Ctrl+S) a project file (.kbproj), export, and the AI server. The AI SERVER (MCP) is off. Switch it on to let an AI assistant (through the Kickbase MCP bridge, for example in Claude Desktop) read and change this plugin. Local only; nothing is sent over the internet by Kickbase.");
    repaint (projectButton.getBounds());
}

void HeaderBar::showProjectMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "New project");
    menu.addItem (2, "Open project...");
    const auto recent = recentProjects();
    if (! recent.isEmpty())
    {
        juce::PopupMenu recents;
        for (int i = 0; i < recent.size(); ++i)
            recents.addItem (10 + i, juce::File (recent[i]).getFileNameWithoutExtension());
        menu.addSubMenu ("Recent projects", recents);
    }
    menu.addSeparator();
    menu.addItem (3, "Save project   Ctrl+S");
    menu.addItem (4, "Save project as...");
    if (! processor.isEffectVersion())
    {
        menu.addSeparator();
        menu.addItem (5, "Export...");
    }
    menu.addSeparator();
    menu.addItem (6, "AI server (MCP)", true, processor.isBridgeEnabled());
    menu.addItem (7, "History...");
    menu.addItem (8, "Autosave every 3 min", true, Settings::get ("autosave") != "0");
    menu.addItem (9, "Open autosave...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&projectButton), [safe = juce::Component::SafePointer<HeaderBar> (this), recent] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        if (result == 1)      safe->newProject();
        else if (result == 2) safe->openProject();
        else if (result == 3) safe->saveProject (false);
        else if (result == 4) safe->saveProject (true);
        else if (result == 5) safe->showExportMenu();
        else if (result == 6)
        {
            safe->processor.setBridgeEnabled (! safe->processor.isBridgeEnabled());
            safe->updateAiTooltip();
        }
        else if (result == 7) safe->showHistory();
        else if (result == 8)
            Settings::set ("autosave", Settings::get ("autosave") == "0" ? juce::String() : juce::String ("0"));
        else if (result == 9)
            safe->openProject (Settings::rootFolder().getChildFile ("Autosave"));
        else if (result >= 10 && result - 10 < recent.size())
            safe->openProjectFile (juce::File (recent[result - 10]));
    });
}

// Icke-modal historikdialog; en enda instans (en andra gång lyfts den befintliga fram).
void HeaderBar::showHistory()
{
    static juce::Component::SafePointer<juce::DialogWindow> open;
    if (open != nullptr)
    {
        open->toFront (true);
        return;
    }
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new HistoryPanel (processor));
    o.dialogTitle = "History";
    o.dialogBackgroundColour = Theme::panel;
    o.componentToCentreAround = this;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = false;
    o.resizable = true;
    open = o.launchAsync();
}

void HeaderBar::showJogPanel()
{
    // CallOutBox med editorn som förälder, så att rattarna i panelen hittar processorn (KnobComponent söker uppåt efter editorn).
    auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
    if (editor == nullptr)
        return;
    juce::CallOutBox::launchAsynchronously (std::make_unique<JogPanel> (processor), editor->getLocalArea (this, jogButton.getBounds()), editor);
}

juce::StringArray HeaderBar::recentProjects() const
{
    juce::StringArray list;
    list.addLines (Settings::get ("recentProjects"));
    list.removeEmptyStrings();
    for (int i = list.size(); --i >= 0;)
        if (! juce::File (list[i]).existsAsFile())
            list.remove (i);
    return list;
}

void HeaderBar::rememberRecent (const juce::File& file)
{
    juce::StringArray list;
    list.add (file.getFullPathName());
    for (const auto& path : recentProjects())
        if (path != file.getFullPathName())
            list.add (path);
    while (list.size() > 3)
        list.remove (list.size() - 1);
    Settings::set ("recentProjects", list.joinIntoString ("\n"));
}

void HeaderBar::showNotice (const juce::String& text)
{
    notice = text;
    noticeUntil = juce::Time::getMillisecondCounterHiRes() * 0.001 + 2.5;
    repaint (nameBounds);
}

void HeaderBar::newProject()
{
    // Frågar om det nuvarande projektet ska sparas; Save sparar till projektfilen (eller frågar efter ett namn), sedan laddas Init-presetet.
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsYesNoCancel (juce::MessageBoxIconType::QuestionIcon, "New project",
                                                                                   "Save the current project before starting a new one?", "Save", "Discard", "Cancel", this),
                                  [safe = juce::Component::SafePointer<HeaderBar> (this)] (int choice) // 1 Save, 2 Discard, 0 Cancel
    {
        if (safe == nullptr || choice == 0)
            return;
        auto start = [safe]
        {
            if (safe == nullptr)
                return;
            safe->processor.loadFactoryPreset (Presets::Section::full, 0);
            safe->processor.currentProjectFile = juce::File();
            safe->repaint();
        };
        if (choice == 2)
        {
            start();
            return;
        }
        if (safe->processor.currentProjectFile != juce::File())
        {
            safe->saveProjectTo (safe->processor.currentProjectFile);
            start();
            return;
        }
        // Inget namn än: välj fil först, starta nytt när det är sparat (avbrutet val stannar kvar i projektet).
        safe->chooser = std::make_unique<juce::FileChooser> ("Save project", Settings::exportsFolder().getChildFile ("Kickbase.kbproj"), "*.kbproj");
        safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                    [safe, start] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File())
                return;
            safe->saveProjectTo (fc.getResult().withFileExtension ("kbproj"));
            start();
        });
    });
}

void HeaderBar::saveProject (bool askForFile)
{
    if (! askForFile && processor.currentProjectFile != juce::File())
    {
        saveProjectTo (processor.currentProjectFile);
        return;
    }
    const auto start = processor.currentProjectFile != juce::File() ? processor.currentProjectFile : Settings::exportsFolder().getChildFile ("Kickbase.kbproj");
    chooser = std::make_unique<juce::FileChooser> ("Save project", start, "*.kbproj");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<HeaderBar> (this)] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        safe->saveProjectTo (fc.getResult().withFileExtension ("kbproj"));
    });
}

void HeaderBar::saveProjectTo (const juce::File& file)
{
    juce::String error;
    if (! ProjectFile::save (processor, file, KICKBASE_VERSION, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save the project", error);
        return;
    }
    processor.currentProjectFile = file;
    rememberRecent (file);
    showNotice ("Saved " + file.getFileNameWithoutExtension());
}

void HeaderBar::openProject (const juce::File& startFolder)
{
    chooser = std::make_unique<juce::FileChooser> ("Open project", startFolder != juce::File() ? startFolder : Settings::exportsFolder(), "*.kbproj");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<HeaderBar> (this)] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        safe->openProjectFile (fc.getResult());
    });
}

void HeaderBar::openProjectFile (const juce::File& file)
{
    juce::String error;
    if (! ProjectFile::load (processor, processor.apvts.state.getType(), file, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not open the project", error);
        return;
    }
    processor.currentProjectFile = file;
    rememberRecent (file);
    showNotice ("Opened " + file.getFileNameWithoutExtension());
}

HeaderBar::~HeaderBar()
{
    processor.transportHold = false; // ett scrubbhopp som inte hunnit släppas får inte frysa klockan
    if (keyTarget != nullptr)
        keyTarget->removeKeyListener (this);
}

void HeaderBar::parentHierarchyChanged()
{
    // Ctrl+S: lyssnar på hela editorn (fönstret högst upp), så att det räcker att fönstret har fokus.
    auto* top = getTopLevelComponent();
    if (top == keyTarget)
        return;
    if (keyTarget != nullptr)
        keyTarget->removeKeyListener (this);
    keyTarget = top;
    if (keyTarget != nullptr)
        keyTarget->addKeyListener (this);
}

bool HeaderBar::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0))
    {
        saveProject (false);
        return true;
    }
    return false;
}

void HeaderBar::updateTransportLabels (bool following)
{
    if (processor.getKeyboardState() == nullptr)
        return;

    const auto& status = processor.transportStatus;
    if (! bpmLabel.isBeingEdited())
        bpmLabel.setText (juce::String (status.bpm.load(), 1) + " BPM", juce::dontSendNotification);
    if (! barLabel.isBeingEdited())
        barLabel.setText (scrubBar >= 0 ? juce::String (scrubBar + 1) + ".1" : juce::String (status.patternBar.load()) + "." + juce::String (status.beat.load()),
                          juce::dontSendNotification);

    if ((int) following == lastFollowing)
        return;
    lastFollowing = (int) following;

    // Med DAW-synk styr värden tempo och takt; en VST3 kan inte ändra dem.
    bpmLabel.setEditable (false, ! following, false);
    barLabel.setEditable (false, ! following, false);
    bpmLabel.setTooltip (following ? "BPM - styrs av DAW (the DAW sets the tempo; a plugin cannot change it)."
                                   : "BPM - Double-click to type the tempo used when the plugin runs on its own clock. Empty = the DAW's last tempo (or 140).");
    barLabel.setTooltip (following ? "BAR - styrs av DAW (the DAW sets the position)."
                                   : "BAR - Double-click to type a start bar, or scroll the mouse wheel here to scrub: the music pauses until you stop scrolling.");
}

void HeaderBar::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (processor.getKeyboardState() == nullptr || processor.transportStatus.followingHost.load()
        || ! barLabel.getBounds().contains (e.getPosition()) || wheel.deltaY == 0.0f)
        return;

    if (scrubBar < 0)
        scrubBar = std::max (0, processor.transportStatus.patternBar.load() - 1);
    scrubBar = std::max (0, scrubBar + (wheel.deltaY > 0.0f ? 1 : -1));
    processor.transportHold = true;
    barLabel.setText (juce::String (scrubBar + 1) + ".1", juce::dontSendNotification);
    scrubRelease.startTimer (450);
}

void HeaderBar::paintOverChildren (juce::Graphics& g)
{
    // Statuspunkten på MENU-knappen: grön när AI-länken är på, grå när den är av.
    const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) projectButton.getRight() - 9.0f, (float) projectButton.getY() + 9.0f });
    g.setColour (processor.isBridgeEnabled() ? juce::Colour (0xff4cd964) : Theme::text.withAlpha (0.35f));
    g.fillEllipse (dot);

    // Projektnamnet (rubriken) under MENU/presetväljaren; en kort bekräftelse ("Saved X") ersätter det i ett par sekunder.
    const auto projectName = processor.currentProjectFile.getFileNameWithoutExtension();
    const auto shown = notice.isNotEmpty() ? notice : (projectName.isNotEmpty() ? projectName : juce::String ("Untitled project"));
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (notice.isNotEmpty() ? juce::Colour (0xff4cd964) : Theme::text.withAlpha(projectName.isNotEmpty() ? 0.75f : 0.4f));
    g.drawText (shown, nameBounds, juce::Justification::centredLeft, true);

    // Returpilen på omstartsknappen (ritad, så att den inte beror på att ett typsnitt har symbolen).
    if (! restartButton.isVisible())
        return;
    const auto centre = restartButton.getBounds().toFloat().getCentre();
    g.setColour (Theme::text);
    juce::Path arrow;
    arrow.startNewSubPath (centre.x + 5.0f, centre.y - 6.0f);
    arrow.lineTo (centre.x + 5.0f, centre.y + 2.0f);
    arrow.lineTo (centre.x - 4.0f, centre.y + 2.0f);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path head;
    head.addTriangle (centre.x - 8.0f, centre.y + 2.0f, centre.x - 3.0f, centre.y - 2.5f, centre.x - 3.0f, centre.y + 6.5f);
    g.fillPath (head);

    if (! jogButton.isVisible())
        return;

    // LP-ikonen på JOG-knappen: en skiva med hål och tonarm. Lyser när LOOP är på.
    const auto jogCentre = jogButton.getBounds().toFloat().getCentre();
    const auto jogColour = processor.globalLoop.load() ? Theme::pattern : Theme::text;
    g.setColour (jogColour);
    g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (jogCentre), 1.6f);
    g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre (jogCentre));
    g.drawLine (jogCentre.x + 6.0f, jogCentre.y - 8.0f, jogCentre.x + 1.0f, jogCentre.y - 2.0f, 1.4f);
}

void HeaderBar::resized()
{
    const auto hasPreviews = processor.getKeyboardState() != nullptr;
    const auto height = getHeight();
    const auto controlsHeight = 34, controlsY = (height - controlsHeight) / 2;
    // Till höger: mätare och lyssnarvolym längst ut, sedan tonart, sedan (instrumentet) en gemensam ruta för
    // PREVIEWS, LOOP, tempo/takt och JOG. Sätts först, så att vänstra delen kan krympa vid smal bredd.
    auto right = getLocalBounds().reduced (14, 0);
    const auto outSize = std::min (height - 4, 64);
    masterKnob.setBounds (right.removeFromRight (outSize).withSizeKeepingCentre (outSize, outSize)); // stor OUT-ratt (ä)
    right.removeFromRight (4);
    meterRect = right.removeFromRight (meterWidth).withSizeKeepingCentre (meterWidth, 28);
    right.removeFromRight (10);
    keyBlock = right.removeFromRight (84).withSizeKeepingCentre (84, 40);
    right.removeFromRight (8);
    const auto pillWidth = hasPreviews ? 244 : 214; // JOG-knappen (med LOOP i panelen) sitter i pillen
    const auto pillHeight = 34;
    transportGroup = {};
    previewGroup = {};
    if (hasPreviews)
    {
        const auto previewsWidth = rollShown ? 148 : 112;
        const auto groupHeight = 48;
        const auto groupWidth = previewsWidth + pillWidth + 18;
        transportGroup = right.removeFromRight (groupWidth).withSizeKeepingCentre (groupWidth, groupHeight);
        auto inner = transportGroup.reduced (6, 0);
        previewGroup = inner.removeFromLeft (previewsWidth).withTrimmedBottom (4).withTrimmedTop (3);
        inner.removeFromLeft (6);
        auto transport = inner;
        bpmPill = transport.removeFromBottom (pillHeight + 3).withTrimmedBottom (3);
    }
    else
        bpmPill = right.removeFromRight (pillWidth).withSizeKeepingCentre (pillWidth, pillHeight);
    restartButton.setBounds (bpmPill.getRight() - 32, bpmPill.getY() + 4, 26, bpmPill.getHeight() - 8);
    jogButton.setBounds (restartButton.getX() - 28, bpmPill.getY() + 4, 26, bpmPill.getHeight() - 8);
    {
        auto inner = bpmPill.reduced (bpmPill.getHeight() / 2 - 4, 0); // samma indelning som i paint
        bpmLabel.setBounds (inner.removeFromLeft (100).reduced (0, 4));
        barLabel.setBounds (inner.removeFromLeft (58).reduced (0, 4));
    }

    projectButton.setBounds (14 + logo.getWidth() + 22, controlsY, 84, controlsHeight); // MENU först, till vänster om presetväljaren
    const auto presetsX = projectButton.getRight() + 10;
    const auto leftLimit = (hasPreviews ? transportGroup.getX() : bpmPill.getX()) - 8;
    const auto presetWidth = std::max (70, std::min (hasPreviews ? 118 : 200, leftLimit - presetsX));
    presets.setBounds (presetsX, controlsY, presetWidth, controlsHeight);
    nameBounds = { projectButton.getX(), controlsY + controlsHeight + 1, presets.getRight() - projectButton.getX(), 14 };

    listenAll.setVisible (hasPreviews);
    noteBox.setVisible (hasPreviews);
    restartButton.setVisible (hasPreviews);
    jogButton.setVisible (hasPreviews);
    if (hasPreviews)
    {
        const auto y = previewGroup.getY() + 12, h = previewGroup.getHeight() - 14;
        listenAll.setBounds (previewGroup.getX() + 1, y, 30, h);
        listenRoll.setVisible (rollShown);
        if (rollShown)
            listenRoll.setBounds (listenAll.getRight() + 3, y, 30, h);
        noteBox.setBounds ((rollShown ? listenRoll.getRight() : listenAll.getRight()) + 5, y, 62, h);
    }
}

void HeaderBar::showExportMenu()
{
    juce::PopupMenu menu;
    // Delar: 0 hela mixen, 1 kicken, 2 alla instrument ihop, 3 tre filer, 4 + n instrument n ensamt (de som används).
    auto addPart = [this, &menu] (juce::PopupMenu& into, int part, const juce::String& text)
    {
        juce::PopupMenu lengths;
        for (const auto bars : { 4, 8, 16 })
            lengths.addItem (1 + part * 100 + bars, juce::String (bars) + " bars");
        into.addSubMenu (text, lengths);
    };
    addPart (menu, 0, "Full mix");
    addPart (menu, 1, "Kick");
    addPart (menu, 2, "All instruments (together)");
    juce::PopupMenu single;
    for (int layer = 0; layer < processor.activeInstruments(); ++layer)
        addPart (single, 4 + layer, "Instrument " + juce::String (layer + 1) + "  " + processor.getLayerName (layer));
    menu.addSubMenu ("One instrument", single);
    addPart (menu, 3, "Kick + instruments + full mix (3 files)");
    menu.addSeparator();
    menu.addItem (99999, "Without effects (dry: FX channel sends off)", true, exportWithoutEffects);

    juce::Component::SafePointer<HeaderBar> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&projectButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        if (result == 99999)
        {
            safeThis->exportWithoutEffects = ! safeThis->exportWithoutEffects;
            juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showExportMenu(); }); // menyn öppnas igen med rutan ikryssad
            return;
        }
        safeThis->chooseExportTarget ((result - 1) / 100, (result - 1) % 100);
    });
}

void HeaderBar::chooseExportTarget (int part, int bars)
{
    const auto singleLayer = part >= 4 ? part - 4 : -1;
    const juce::String partNames[] { "Full", "Kick", "Instruments", "", singleLayer >= 0 ? juce::File::createLegalFileName ("Instr " + juce::String (singleLayer + 1) + " " + processor.getLayerName (singleLayer)) : juce::String() };
    const auto namePart = singleLayer >= 0 ? partNames[4] : partNames[std::min (part, 2)];
    const auto dry = exportWithoutEffects;
    const auto baseName = "Kickbase " + juce::String (bars) + " bars "
                        + juce::String (juce::roundToInt (processor.transportStatus.bpm.load())) + " BPM" + (dry ? " dry" : "");
    auto folder = Settings::exportsFolder();
    folder.createDirectory();

    auto report = [] (bool ok, const juce::String& where)
    {
        if (! ok)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", "Could not write " + where);
    };

    juce::Component::SafePointer<HeaderBar> safeThis (this);
    if (part == 3)
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the three WAV files", folder);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis, bars, baseName, partNames, report, dry] (const juce::FileChooser& fc)
        {
            const auto directory = fc.getResult();
            if (safeThis == nullptr || directory == juce::File())
                return;
            for (int i = 0; i < 3; ++i)
            {
                const auto file = directory.getChildFile (baseName + " " + partNames[i] + ".wav");
                report (safeThis->processor.exportWav (file, (PluginBase::ExportPart) i, bars, -1, dry), file.getFullPathName());
            }
        });
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Export " + namePart + " as WAV",
                                                   folder.getChildFile (baseName + " " + namePart + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis, part, bars, report, dry, singleLayer] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safeThis == nullptr || file == juce::File())
            return;
        const auto wav = file.withFileExtension ("wav");
        const auto kind = singleLayer >= 0 ? PluginBase::ExportPart::instrument : (PluginBase::ExportPart) part;
        report (safeThis->processor.exportWav (wav, kind, bars, singleLayer, dry), wav.getFullPathName());
    });
}

void HeaderBar::syncRollButton()
{
    const auto rollOpen = processor.openRollLayer.load() >= 0;
    if (rollOpen == rollShown)
        return;
    rollShown = rollOpen;
    resized();
    if (auto* parent = getParentComponent())
        parent->resized(); // mätaren sitter efter PREVIEWS-rutan
    repaint();
}

void HeaderBar::timerCallback()
{
    followVisualRate();
    if (notice.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() * 0.001 > noticeUntil)
    {
        notice = {};
        repaint (nameBounds);
    }
    if (bridgeShown != processor.isBridgeEnabled()) // ändrad på inställningssidan eller via MCP
    {
        bridgeShown = processor.isBridgeEnabled();
        updateAiTooltip();
    }
    if (processor.getKeyboardState() != nullptr)
    {
        listenAll.setActive (processor.previewingLayer.load() == PluginBase::previewAllRows);
        syncRollButton();
        listenRoll.setActive (rollShown && processor.previewingLayer.load() == processor.openRollLayer.load());
        if (noteBox.getSelectedId() != processor.previewNote.load() + 1)
            noteBox.setSelectedId (processor.previewNote.load() + 1, juce::dontSendNotification);
    }

    // Toppvärdet av de senaste ~20 ms på masterns utgång, med mjukt fall och en håll-markör.
    processor.getMasterSection().outputTap.copyLatest (1024, meterLeft, meterRight);
    auto lo = 0.0f, ro = 0.0f;
    for (size_t i = 0; i < meterLeft.size(); ++i)
    {
        lo = std::max (lo, std::abs (meterLeft[i]));
        ro = std::max (ro, std::abs (meterRight[i]));
    }
    peakL = lo > peakL ? lo : peakL * 0.8f;
    peakR = ro > peakR ? ro : peakR * 0.8f;
    holdL = std::max (peakL, holdL * 0.985f);
    holdR = std::max (peakR, holdR * 0.985f);

    // Loggans glöd följer ljudet: en ny kick startar en våg åt höger, en ny basstöt en våg åt vänster.
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto triggers = processor.transportStatus.kickTriggers.load();
    if (lastKickTriggers >= 0 && triggers != lastKickTriggers)
        lastKickTime = now;
    lastKickTriggers = triggers;

    std::vector<float> instrumentLeft, instrumentRight;
    processor.instrumentAfterDuckTap.copyLatest (256, instrumentLeft, instrumentRight);
    auto instrumentPeak = 0.0f;
    for (const auto sample : instrumentLeft)
        instrumentPeak = std::max (instrumentPeak, std::abs (sample));
    if (instrumentPeak > 0.03f && instrumentPeak > lastInstrumentPeak * 1.35f + 0.01f && now - lastInstrumentTime > 0.06)
    {
        lastInstrumentTime = now;
        instrumentStrength = juce::jlimit (0.35, 1.0, (double) instrumentPeak * 3.0);
    }
    lastInstrumentPeak = instrumentPeak > lastInstrumentPeak ? instrumentPeak : lastInstrumentPeak * 0.85f;

    // Rita bara om det som rör sig: mätaren (30 gånger i sekunden), loggan så länge glöden går, och resten när tempo, takt, tonart eller
    // synk ändras. Att rita hela rubriken 30 gånger i sekunden kostade onödigt mycket.
    const auto reduced = Settings::reduceAnimations(); // (takten följer REDUCE ANIMATIONS och TOP BAR i OPTIMIZATIONS via followVisualRate)
    // Neonet lyser med ljudets nivå (utgångens topp): släckt när inget spelas, så att man ser ljuden.
    {
        const auto level = juce::jmax (peakL, peakR);
        const auto target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -80.0f) + 54.0f) / 30.0f);
        glow += (target - glow) * (target > glow ? 0.45f : 0.1f);
        if (glow < 0.01f)
            glow = 0.0f;
    }
    const auto glowMoved = std::abs (glow - glowShown) > 0.02f || (glow == 0.0f && glowShown != 0.0f);
    if (glowMoved)
        glowShown = glow;
    const auto logoActive = glowMoved || (! reduced && (now - lastKickTime < 1.9 || instrumentStrength * std::exp (-(now - lastInstrumentTime) * 2.2) > 0.01));
    if (logoActive || logoWasActive)
        repaint (0, 0, 14 + logo.getWidth() + 24, getHeight());
    logoWasActive = logoActive;
    repaint (meterBounds().expanded (2));

    const auto& status = processor.transportStatus;
    const auto hash = juce::roundToInt ((float) status.bpm.load() * 10.0f) * 31 + status.patternBar.load() * 7 + status.beat.load() * 3 + status.keyNote.load() * 11
                    + (status.followingHost.load() ? 1 : 0);
    if (hash != lastStatusHash)
    {
        lastStatusHash = hash;
        updateTransportLabels (status.followingHost.load());
        repaint();
    }
}

// Färgerna glider sakta mellan varandra (en ny färg var sjätte sekund).
static juce::Colour fadeColour (std::initializer_list<juce::uint32> palette, double offset, double now)
{
    const std::vector<juce::uint32> colours (palette);
    const auto position = std::fmod ((now + offset) / 6.0, (double) colours.size());
    const auto index = (int) std::floor (position);
    const auto mix = (float) (position - index);
    const auto smooth = mix * mix * (3.0f - 2.0f * mix);
    return juce::Colour (colours[(size_t) index]).interpolatedWith (juce::Colour (colours[(size_t) ((index + 1) % (int) colours.size())]), smooth);
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& status = processor.transportStatus;
    {
        // Loggan: glöden följer ljudet. En ny kick startar en våg åt höger, en ny stöt från instrumenten en våg åt vänster.
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto kickAge = now - lastKickTime, instrumentAge = now - lastInstrumentTime;
        // Halv fart mot förut (glöden går över loggan på 0,6 s och tonar ut på ~1,5 s), och starkare. Med minskade animationer står loggan still.
        const auto still = Settings::reduceAnimations();
        LogoPainter::Sweep kick { juce::jlimit (0.0f, 1.0f, (float) (kickAge / 0.60)), still ? 0.0f : (float) std::exp (-kickAge * 2.6), fadeColour ({ 0xffff5fa8, 0xffff9a3c, 0xffb98cff, 0xffffc857 }, 0.0, now) };
        LogoPainter::Sweep bass { 1.0f - juce::jlimit (0.0f, 1.0f, (float) (instrumentAge / 0.76)), still ? 0.0f : (float) (instrumentStrength * std::exp (-instrumentAge * 2.2)),
                                  fadeColour ({ 0xff3fd2ff, 0xff5fffb0, 0xff6b8cff, 0xff5ef2d6 }, 9.0, now) };
        logo.paint (g, { 14.0f, ((float) getHeight() - (float) LogoPainter::height) * 0.5f }, glowShown, kick, bass);
    }

    // Tonart med lampan under (DAW SYNC eller FREE RUN) längst till höger.
    {
        const auto keyNote = status.keyNote.load();
        auto block = keyBlock.toFloat();
        auto keyRow = block.removeFromTop (block.getHeight() * 0.5f);
        g.setFont (Theme::font (10.0f, true));
        g.setColour (Theme::textDim);
        g.drawText ("KEY", keyRow.removeFromLeft (26.0f), juce::Justification::centredLeft);
        g.setFont (Theme::font (16.0f, true));
        g.setColour (Theme::pattern);
        g.drawText (keyNote >= 0 ? Params::keyNames()[keyNote % 12] : juce::String ("-"), keyRow, juce::Justification::centredLeft);

        const auto following = status.followingHost.load();
        const auto lampColour = following ? Theme::mix : Theme::kick;
        const juce::Point<float> centre { block.getX() + 7.0f, block.getCentreY() };
        g.setColour (lampColour.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));
        g.setColour (lampColour.brighter (0.3f));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (centre));
        g.setFont (Theme::font (11.0f, true));
        g.setColour (Theme::textDim);
        g.drawText (following ? "DAW SYNC" : "FREE RUN", block.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    }

    // Gemensam ruta (instrumentet): PREVIEWS (högtalare, tonväljare), tempo/takt och JOG (med LOOP i panelen) hör ihop.
    if (! transportGroup.isEmpty())
    {
        const auto group = transportGroup.toFloat();
        // Glasdisplay (ä): mörkt glas med ljusreflex, tunn kant i mönsterfärgen och gemensam kantglöd (å).
        Theme::paintGlassDisplay (g, group, 9.0f);
        Theme::paintEdgeGlow (g, group, 9.0f, Theme::pattern, 0.8f); // svag färgad glöd i stället för röd ram
        g.setFont (Theme::font (8.5f, true));
        g.setColour (Theme::pattern);
        g.drawText ("PREVIEWS", previewGroup.toFloat().withHeight (12.0f), juce::Justification::centredLeft);
        const auto dividerX = (float) previewGroup.getRight() + 3.0f;
        g.setColour (Theme::pattern.withAlpha (0.35f));
        g.drawVerticalLine ((int) dividerX, group.getY() + 7.0f, group.getBottom() - 7.0f);
    }

    // Tempo och takt i samma kapselformade hårdvarupill; knappen längst till höger börjar om vid 1.1.
    {
        const auto pill = bpmPill.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121420), pill.getX(), pill.getY(),
                                                 juce::Colour (0xff1c1f30), pill.getX(), pill.getBottom(), false));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xff2c3148));
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);

        auto inner = pill.reduced (pill.getHeight() * 0.5f - 4.0f, 0.0f);
        g.setFont (Theme::font (13.0f, true));
        g.setColour (Theme::text);
        const auto labelsDrawn = bpmLabel.isVisible(); // instrumentet: etiketterna ritar tempot och takten (kan skrivas in)
        auto bpmArea = inner.removeFromLeft (100.0f);
        if (! labelsDrawn)
            g.drawText (juce::String (status.bpm.load(), 1) + " BPM", bpmArea, juce::Justification::centred);
        g.setColour (juce::Colour (0xff2c3148));
        g.fillRect (juce::Rectangle<float> (inner.getX(), pill.getY() + 7.0f, 1.0f, pill.getHeight() - 14.0f));
        g.setFont (Theme::font (14.0f, true));
        g.setColour (Theme::text);
        if (! labelsDrawn)
            g.drawText (juce::String (status.patternBar.load()) + "." + juce::String (status.beat.load()), // takten i mönstret (börjar om när mönstret är klart)
                        inner.removeFromLeft (58.0f), juce::Justification::centred);
    }

    // Utgångsmätare (L och R, -48 till 0 dB): visar att synten faktiskt ger ljud.
    {
        auto meter = meterBounds().toFloat();
        g.setFont (Theme::font (9.0f, true));
        g.setColour (Theme::textDim);
        g.drawText ("OUT", meter.removeFromLeft (26.0f), juce::Justification::centredLeft);
        auto bars = meter.reduced (0.0f, 1.0f);
        const auto barHeight = (bars.getHeight() - 3.0f) * 0.5f;
        auto drawBar = [&] (juce::Rectangle<float> bar, float peak, float hold)
        {
            auto fraction = [] (float amp) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (amp, -60.0f) + 48.0f) / 48.0f); };
            g.setColour (juce::Colour (0xff0b0d18));
            g.fillRoundedRectangle (bar, 2.0f);
            const auto filled = bar.withWidth (bar.getWidth() * fraction (peak));
            g.setGradientFill (juce::ColourGradient (Theme::mix, bar.getX(), 0.0f, Theme::master, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (filled, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillRect (bar.getX() + bar.getWidth() * fraction (hold) - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        };
        drawBar (bars.removeFromTop (barHeight), peakL, holdL);
        bars.removeFromTop (3.0f);
        drawBar (bars, peakR, holdR);
    }
}

//==============================================================================
KickMixControl::KickMixControl (juce::AudioProcessorValueTreeState& apvts, juce::Colour accent)
{
    label.setText ("SYNTH / SAMPLE", juce::dontSendNotification);
    label.setFont (Theme::font (11.0f));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    slider.parameter = apvts.getParameter (ParamID::kickMix);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.setColour (juce::Slider::thumbColourId, Theme::text);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setTooltip ("MIX - Balance between the synth kick and the loaded sample. Left = synth only, middle = both at full level, right = sample only. With no sample, left to middle is the same.");
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamID::kickMix, slider);
}

void KickMixControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromLeft (90));
    slider.setBounds (area);
}

//==============================================================================
KickModule::KickModule (PluginBase& p)
    : ModulePanel ("KICK", Theme::kick, &p.apvts, ParamID::kickOn), processor (p),
      keyMode (p.apvts, ParamID::kickKeyFollow, "KEY"),
      presets (p, Presets::Section::kick, Theme::kick),
      sampleView (p),
      library (p, Theme::kick),
      display (p)
{
    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    viewTabs.onChange = [this] (int tab) { processor.kickTab = tab; applyTab(); };
    setHeaderTabs (&viewTabs, 230);
    addAndMakeVisible (viewTabs);
    fxSend = std::make_unique<FxSendControl> (p.apvts, ParamID::kickFxChannel, ParamID::kickFxAmount, ParamID::kickFxSend, juce::StringArray (ParamID::kickFxSendToIds, numEffectChannels), "FX SEND", Theme::kick);
    addAndMakeVisible (*fxSend);
    setHeaderFxSend (fxSend.get());
    mixControl = std::make_unique<KickMixControl> (p.apvts, Theme::kick);
    addAndMakeVisible (*mixControl);
    setHeaderMix (mixControl.get(), 220);
    addAndMakeVisible (keyMode);
    addAndMakeVisible (display);

    auto knob = [&] (const char* id, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, id, name, Theme::kick));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };
    auto choice = [&] (const char* id, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, id, name));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };

    // I signalens ordning: stämning och form (envelopena ritas i displayen), kroppens övertoner,
    // klicket, drive och utgång.
    groups.addRow ({ { "TUNING", { knob (ParamID::kickTune, "TUNE"), &keyMode } },
                     { "ENVELOPES", { knob (ParamID::kickPitchAmount, "PITCH AMT"), knob (ParamID::kickLength, "LENGTH") } },
                     { "BODY", { knob (ParamID::kickPhase, "PHASE"), knob (ParamID::kickHarmonics, "HARMONICS"),
                                 knob (ParamID::kickHarmDecay, "H. DECAY"), knob (ParamID::kickHarmTilt, "H. TILT") } },
                     { "CLICK", { knob (ParamID::kickClick, "LEVEL"), knob (ParamID::kickClickLength, "LENGTH"),
                                  knob (ParamID::kickClickTone, "TONE") } },
                     { "DRIVE", { choice (ParamID::kickClipMode, "MODE"), knob (ParamID::kickDrive, "AMOUNT") } },
                     { "OUT", { knob (ParamID::kickHighCut, "HIGH CUT"), knob (ParamID::kickLevel, "LEVEL") } } });

    // SAMPLE-vyn: hur samplingen används (MODE, LEVEL, TUNE, OFFSET). Rattarna är desamma som förut, bara i en egen vy.
    sampleControls = { choice (ParamID::kickSampleMode, "MODE"), knob (ParamID::kickSampleLevel, "LEVEL"),
                       knob (ParamID::kickSampleTune, "TUNE"), knob (ParamID::kickSampleOffset, "OFFSET") };
    sampleGroups.addRow ({ { "SAMPLE", sampleControls,
                             "How a loaded sample is used: Click = only the first moments (the CLICK level in the SYNTH view), Layer = the whole sample on top of the synth kick, "
                             "Replace = the sample is the kick (the synth is silent). LEVEL and TUNE apply to Layer and Replace. OFFSET moves the sample against the synth kick: plus = later, minus = it starts a few ms into the sample, so the two do not cancel each other in phase." } });

    // Fler lager och former: kroppens form och FM, sub, brusig topp, bitkrossning, lågt skär (samplingen har en egen vy, SAMPLE).
    groups.addRow ({ { "SHAPE", { choice (ParamID::kickWave, "WAVE"), knob (ParamID::kickSub, "SUB"), knob (ParamID::kickFm, "FM"),
                                  knob (ParamID::kickFmRatio, "RATIO"), knob (ParamID::kickFmDecay, "FM DECAY") },
                     "The body's waveform, a sine sub one octave down, and FM (a second sine bends the body's phase at the start: punchier, more metallic)." },
                     { "TOP", { knob (ParamID::kickNoise, "NOISE"), knob (ParamID::kickNoiseDecay, "DECAY"), knob (ParamID::kickNoiseTone, "TONE") },
                     "A noisy top layer on top of the kick: snap and air, with its own decay and high-pass tone." },
                     { "CRUSH", { knob (ParamID::kickCrush, "BITS") } },
                     { "COMP", { knob (ParamID::kickComp, "AMOUNT") },
                     "Compressor after drive and crush: a fast attack and a release that follows the kick length, with automatic makeup gain. Tighter, denser kick; 0 = off." },
                     { "FILTER", { knob (ParamID::kickLowCut, "LOW CUT"), knob (ParamID::kickVelPitch, "VEL>PITCH") } } });

    // Extra klick-lager (nivå 0 = av): brusklick med eget filterläge och fördröjning, ovanpå CLICK-gruppen.
    groups.addRow ({ { "CLICK 2", { knob (ParamID::kickClick2Level, "LEVEL"), knob (ParamID::kickClick2Length, "LENGTH"), knob (ParamID::kickClick2Tone, "TONE"),
                                    knob (ParamID::kickClick2Delay, "DELAY"), choice (ParamID::kickClick2Mode, "MODE") },
                       "A second noise click layer. LEVEL 0 = off. DELAY starts it that many ms after the hit, LENGTH is how long it lasts, MODE picks the filter (Bandpass, Lowpass, Highpass) and TONE its frequency. Not heard in Replace mode or with the synth off." },
                     { "CLICK 3", { knob (ParamID::kickClick3Level, "LEVEL"), knob (ParamID::kickClick3Length, "LENGTH"), knob (ParamID::kickClick3Tone, "TONE"),
                                    knob (ParamID::kickClick3Delay, "DELAY"), choice (ParamID::kickClick3Mode, "MODE") },
                       "A third noise click layer, set up like CLICK 2. LEVEL 0 = off." } });

    // Sampleverktyg (av = som förut): klipp bort början, baklänges, utfasning. REVERSE är en växel i en cell som rattarna.
    {
        auto reverse = std::make_unique<juce::ToggleButton> ("REVERSE");
        reverse->setTooltip ("REVERSE - Plays the loaded sample backwards (after START has cut off its beginning). Off (default): as before.");
        reverse->getProperties().set ("labelled", true);
        reverse->setColour (juce::ToggleButton::tickColourId, Theme::kick);
        reverse->setColour (juce::ToggleButton::textColourId, Theme::textDim);
        sampleReverseAttachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (ParamID::kickSampleReverse), *reverse);
        controls.push_back (std::move (reverse));
        addAndMakeVisible (*controls.back());
        sampleControls.push_back (controls.back().get());
    }
    {
        auto* start = knob (ParamID::kickSampleStart, "START");
        auto* fade = knob (ParamID::kickSampleFade, "FADE");
        sampleControls.push_back (start);
        sampleControls.push_back (fade);
        // Reverse-växeln ligger sist i sampleControls; gruppen får ordningen START, FADE, REVERSE.
        sampleGroups.addRow ({ { "TOOLS", { start, fade, sampleControls[sampleControls.size() - 3] },
                                 "Edit the loaded sample: START cuts off its first ms, FADE fades out its last ms, REVERSE plays it backwards. All off = the sample as loaded." } });
    }

    synthToggle.setTooltip ("Kick SYNTH on/off. Off = only the loaded sample is heard (like Replace); with no sample the kick is silent. Normally leave on and use MIX.");
    sampleToggle.setTooltip ("Kick SAMPLE on/off. Off = the loaded sample is ignored (it stays loaded) and the synth kick plays alone. Normally leave on and use MIX.");
    synthToggleAttachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (ParamID::kickSynthOn), synthToggle);
    sampleToggleAttachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (ParamID::kickSampleOn), sampleToggle);
    // Växlarna och hinten visas inte längre (balansen är MIX i flikraden); parametrarna finns kvar för äldre presets.
    balanceHint.setText ("MIX (SYNTH view) balances synth and sample: left = synth only, middle = both, right = sample only.  MODE (SAMPLE view) decides how the sample is used.", juce::dontSendNotification);
    balanceHint.setFont (Theme::font (10.5f));
    balanceHint.setColour (juce::Label::textColourId, Theme::textDim);
    balanceHint.setTooltip ("MIX in the middle: the synth kick plus the sample at full level, as before. Click = only the sample's first moments, Layer = the whole sample on top, Replace = the sample alone.");
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::kick, "the kick (SYNTH and SAMPLE views)");
    addAndMakeVisible (helpButton);

    sampleLabel.setFont (Theme::font (11.0f));
    sampleLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (sampleLabel);

    loadButton.setTooltip ("Load a WAV/AIFF as the click (or drop a file on the kick module)");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a click sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadSample (fc.getResult());
                              });
    };
    addAndMakeVisible (loadButton);

    clearButton.setTooltip ("Use the synthesized click again");
    clearButton.onClick = [this] { processor.clearClickSample(); updateSampleLabel(); };
    addAndMakeVisible (clearButton);

    listenSampleButton.setTooltip ("HEAR SAMPLE - Plays just the loaded sample, whatever the MODE is (so you can check what you loaded).");
    listenSampleButton.onClick = [this] { processor.requestAudition (5); };
    addAndMakeVisible (listenSampleButton);

    addAndMakeVisible (sampleView);
    addAndMakeVisible (library);
    library.onLoaded = [this] { updateSampleLabel(); sampleView.refresh(); };
    sampleHelp.setFont (Theme::font (10.5f));
    sampleHelp.setColour (juce::Label::textColourId, Theme::textDim);
    sampleHelp.setJustificationType (juce::Justification::topLeft);
    sampleHelp.setText ("Click a kick in the library to load it. CLICK adds only the sample's first moments to your synth kick, LAYER plays the whole sample on top of it, "
                        "REPLACE makes the sample the kick. Hover MODE for the details.", juce::dontSendNotification);
    addAndMakeVisible (sampleHelp);

    viewTabs.select (processor.kickTab.load());
    updateSampleLabel();
    applyTab();
    startTimerHz (2); // projektet kan laddas om medan fönstret är öppet
}

void KickModule::applyTab()
{
    const auto sample = sampleTabShown();
    const auto envelope = envelopeTabShown();
    viewTabs.select (processor.kickTab.load());
    display.setVisible (envelope || ! sample); // i SYNTH visas den bara om det finns plats (se resized)
    keyMode.setVisible (! (sample || envelope));
    sampleLabel.setVisible (sample);
    sampleView.setVisible (sample);
    library.setVisible (sample);
    sampleHelp.setVisible (sample);
    loadButton.setVisible (sample);
    clearButton.setVisible (sample);
    listenSampleButton.setVisible (sample);
    for (auto& control : controls)
        control->setVisible (std::find (sampleControls.begin(), sampleControls.end(), control.get()) != sampleControls.end() ? sample : ! (sample || envelope));
    resized();
    repaint();
}

void KickModule::timerCallback()
{
    updateSampleLabel();
    sampleView.refresh();
    library.setLoadedName (processor.getClickSampleName());
}

void KickModule::updateSampleLabel()
{
    const auto name = processor.getClickSampleName();
    const auto text = name.isEmpty() ? juce::String ("SAMPLE: none  (drop a WAV here)") : "SAMPLE: " + name;
    if (sampleLabel.getText() != text)
        sampleLabel.setText (text, juce::dontSendNotification);
    clearButton.setEnabled (name.isNotEmpty());
    listenSampleButton.setEnabled (name.isNotEmpty());
}

void KickModule::loadSample (const juce::File& file)
{
    if (! processor.loadClickSample (file))
        sampleLabel.setText ("Could not read " + file.getFileName(), juce::dontSendNotification);
    else
    {
        updateSampleLabel();
        sampleView.refresh();
        library.setLoadedName (processor.getClickSampleName());
    }
}

bool KickModule::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac");
}

void KickModule::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    loadSample (juce::File (files[0]));
    repaint();
}

void KickModule::paintOverChildren (juce::Graphics& g)
{
    if (dragHover)
    {
        g.setColour (Theme::kick.withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (Theme::kick);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, 2.0f);
    }
}

void KickModule::resized()
{
    ModulePanel::resized();
    if (! isEmbedded())
    {
        presets.setBounds (presetBounds (*this));
        viewTabs.setBounds (presetBounds (*this).translated (-246, 0).withWidth (230));
    }
    auto area = getContentBounds();

    if (sampleTabShown())
    {
        // Nedersta raden: samplingens namn, HEAR SAMPLE, LOAD, X och hjälpknappen.
        auto sampleRow = area.removeFromBottom (24);
        helpButton.setBounds (sampleRow.removeFromRight (44));
        sampleRow.removeFromRight (10);
        clearButton.setBounds (sampleRow.removeFromRight (24));
        sampleRow.removeFromRight (4);
        loadButton.setBounds (sampleRow.removeFromRight (50));
        sampleRow.removeFromRight (10);
        listenSampleButton.setBounds (sampleRow.removeFromRight (96));
        sampleRow.removeFromRight (10);
        sampleLabel.setBounds (sampleRow);
        area.removeFromBottom (4);

        // SAMPLE: vågformen överst, under den biblioteket till vänster och hur samplingen används till höger.
        sampleView.setBounds (area.removeFromTop (juce::jlimit (90, 150, area.getHeight() / 3)));
        area.removeFromTop (6);
        auto right = area.removeFromRight (340);
        area.removeFromRight (8);
        library.setBounds (area);
        const auto sampleRows = sampleGroups.numRows();
        const auto rowHeight = juce::jlimit (60, 100, right.getHeight() / (sampleRows + 1) - ControlGroups::titleHeight - 3);
        sampleGroups.layout (right.removeFromTop (sampleRows * (ControlGroups::titleHeight + rowHeight + 3)), rowHeight);
        right.removeFromTop (8);
        sampleHelp.setBounds (right);
        return;
    }

    // Hjälpknappen ligger uppe till höger över kurvorna.
    const auto helpArea = area;
    helpButton.setBounds (helpArea.getRight() - 48, helpArea.getY() + 4, 44, 22);
    helpButton.toFront (false);

    if (envelopeTabShown())
    {
        display.setBounds (area); // ENVELOPE: kurvorna får hela ytan
        return;
    }

    // SYNTH: kontrollerna längst ner (48 px-rattar), envelopekurvorna ovanför om det finns plats.
    const auto rowHeight = 74;
    const auto groupsHeight = groups.numRows() * (ControlGroups::titleHeight + rowHeight + 3);
    groups.layout (area.removeFromBottom (groupsHeight), rowHeight);
    area.removeFromBottom (6);
    const auto showDisplay = area.getHeight() >= 100;
    display.setVisible (showDisplay);
    if (showDisplay)
        display.setBounds (area);
}

//==============================================================================
InstrumentModule::InstrumentModule (PluginBase& p, int layerIn)
    : ModulePanel ("INSTR " + juce::String (layerIn + 1), Theme::layerColour (layerIn), &p.apvts, ParamID::bass (layerIn, InstrumentField::on)),
      processor (p),
      layer (layerIn),
      refs (p.apvts),
      sources (&p.selectedInstrumentSource[(size_t) layerIn]),
      presets (p, Presets::Section::bass, Theme::layerColour (layerIn), layerIn)
{
    using F = InstrumentField;
    auto id = [this] (F f) { return ParamID::bass (layer, f); };
    const auto layerAccent = Theme::layerColour (layer);

    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    juce::StringArray sendToIDs;
    for (int ch = 0; ch < numEffectChannels; ++ch)
        sendToIDs.add (id (ParamID::fxSendToField (ch)));
    fxSend = std::make_unique<FxSendControl> (p.apvts, id (F::fxChannel), id (F::fxAmount), id (F::fxSend), sendToIDs, "FX SEND", layerAccent);
    addAndMakeVisible (*fxSend);
    setHeaderFxSend (fxSend.get());
    envelope.onBeforePaint = [this] { current = refs.readInstrument (layer, 36).withTempo (processor.transportStatus.bpm.load(), false); };

    envelope.addCurve ({ [this] (double t) { return InstrumentSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f; },
                         layerAccent, true, "FILTER" });
    envelope.addCurve ({ [this] (double t) { return ampValueAt (t); }, Theme::text.withAlpha (0.8f), false, "AMP" });
    envelope.addCurve ({ [this] (double t) { return pitchValueAt (t); }, juce::Colour (0xffe6c23c).withAlpha (0.9f), false, "PITCH" }); // tonhöjdsenvelopen (storleken, uppåt eller nedåt)
    envelope.addHandle ({ [this] {
                              const auto t = current.pitchEnvDecayMs * 0.001 / 3.0;
                              return juce::Point<float> ((float) t, pitchValueAt (t)); },
                          param (p, id (F::pitchEnvDecay)), nullptr, false, juce::Colour (0xffe6c23c) });

    envelope.addHandle ({ [this] { return juce::Point<float> (std::max (0.0015f, current.envAttackMs * 0.001f), current.envAmount / 8.0f); },
                          param (p, id (F::envAttack)), param (p, id (F::envAmount)), false, layerAccent });
    envelope.addHandle ({ [this] {
                              const auto t = current.envAttackMs * 0.001 + current.envDecayMs * 0.001 / 3.0;
                              return juce::Point<float> ((float) t, InstrumentSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f); },
                          param (p, id (F::envDecay)), nullptr, false, layerAccent });

    envelope.addHandle ({ [this] { return juce::Point<float> (current.attackMs * 0.001f, 1.0f); },
                          param (p, id (F::attack)), nullptr, false, Theme::text });
    envelope.addHandle ({ [this] {
                              const auto t = (current.attackMs + current.decayMs * 0.5f) * 0.001;
                              return juce::Point<float> ((float) t, ampValueAt (t)); },
                          param (p, id (F::decay)), param (p, id (F::sustain)), false, Theme::text });
    envelope.setNoteOff ([this] { return noteOffNow(); });
    envelope.addHandle ({ [this] { return juce::Point<float> ((float) noteOffNow().seconds + current.releaseMs * 0.001f, 0.0f); },
                          param (p, id (F::release)), nullptr, false, Theme::text });
    instrPage.addAndMakeVisible (envelope);
    envelopeListen.setAccent (layerAccent);
    envelopeListen.setTooltip ("LISTEN - Hold to hear this instrument on the preview note (the PREVIEWS note at the top) for as long as you hold; it starts over if the sound "
                               "ends. Double-click to keep it playing until you click again. Use it while you drag the handles.");
    envelopeListen.onStart = [this] { hold.start (PluginBase::heldLayerAudition (layer)); };
    envelopeListen.onStop = [this] { hold.stop(); };
    instrPage.addAndMakeVisible (envelopeListen);
    HelpDialog::setupButton (instrHelpButton, *this, HelpDialog::Topic::instr, "the INSTR tab (envelopes, pitch, sub, noise, drive, phase)");
    instrPage.addAndMakeVisible (instrHelpButton);
    instrPage.addAndMakeVisible (subScope);
    noiseFilterButton.setTooltip ("NOISE THROUGH FILTER - On: the noise goes through Filter 1 and Filter 2 (set on the FILTER tab). Off: it bypasses both filters.");
    noiseFilterButton.getProperties().set ("labelled", true);
    noiseFilterButton.setColour (juce::ToggleButton::tickColourId, layerAccent);
    noiseFilterButton.setColour (juce::ToggleButton::textColourId, Theme::textDim);
    instrPage.addAndMakeVisible (noiseFilterButton);
    noiseFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::bass (layer, F::filterNoise), noiseFilterButton);

    // Flikarna: INSTR | OSC 1-3, SAMPLE och FM med en på/av-knapp på fliken | FILTER, MIX och TIMING (avdelare efter INSTR och FM).
    sources.setSubTabs();
    sources.addTab ("INSTR", layerAccent, instrPage);
    sources.addSeparator();
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
        oscs[(size_t) osc] = std::make_unique<OscPanel> (p, layer, osc, layerAccent);
    samplePage = std::make_unique<SamplePage> (p, layer, layerAccent);
    addAndMakeVisible (sources);
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
    {
        sources.addTab ("OSC " + juce::String (osc + 1), layerAccent, *oscs[(size_t) osc]);
        sources.addPower (1 + osc, p.apvts, ParamID::bass (layer, ParamID::oscField (osc, F::aOn)));
    }
    sources.addTab ("SAMPLE", layerAccent, *samplePage);
    sources.addPower (1 + ParamID::oscsPerLayer, p.apvts, ParamID::bass (layer, F::sampleOn));
    fmPanel = std::make_unique<FmPanel> (p, layer, layerAccent);
    sources.addTab ("FM", layerAccent, *fmPanel);
    sources.addPower (2 + ParamID::oscsPerLayer, p.apvts, ParamID::bass (layer, F::fmOn),
                      Help::forParameter (ParamID::bass (layer, F::fmOn))); // FM efter SAMPLE: fliknummer 2 + oscsPerLayer
    sources.addSeparator();
    filterPage = std::make_unique<FilterPage> (p, layer, layerAccent);
    sources.addTab ("FILTER", layerAccent, *filterPage);
    mixPanel = std::make_unique<InstrMixPanel> (p, layer, layerAccent);
    sources.addTab ("MIX", layerAccent, *mixPanel);
    timingPanel = std::make_unique<InstrTimingPanel> (p, layer, layerAccent);
    sources.addTab ("TIMING", layerAccent, *timingPanel);
    instrTimeline = std::make_unique<TimelineChart> (p, layer, layerAccent, false); // skrivskyddad; klick hoppar till TIMING-fliken (nr 8)
    instrTimeline->onSelect = [this] (int) { sources.setCurrentTab (8); };
    instrPage.addAndMakeVisible (*instrTimeline);

    // B1: förhandsknapp i flikraden. OSC 1-3 och SAMPLE spelar bara sin källa, FILTER/FM/MIX hela instrumentet (INSTR har sin egen knapp i sidan).
    for (int tab = 1; tab <= 8; ++tab)
    {
        const auto source = tab - 1; // fliknummer 1-3 = OSC 1-3, 4 = SAMPLE (källa 3)
        const auto solo = tab <= 1 + ParamID::oscsPerLayer;
        const auto whole = tab >= 5;
        if (! solo && ! whole)
            continue;
        sources.setPreview (tab,
                            [this, source, whole] { hold.start (whole ? PluginBase::heldLayerAudition (layer) : PluginBase::heldSoloAudition (layer, source)); },
                            [this] { hold.stop(); },
                            whole ? "LISTEN - Plays the whole instrument on the preview note. Hold to play, double-click to latch."
                                  : "LISTEN - Plays only this source on the preview note. Hold to play, double-click to latch.");
    }

    auto knob = [&] (const juce::String& paramID, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, paramID, name, layerAccent));
        instrPage.addAndMakeVisible (*controls.back());
        return controls.back().get();
    };
    auto choice = [&] (const juce::String& paramID, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, paramID, name));
        instrPage.addAndMakeVisible (*controls.back());
        return controls.back().get();
    };

    auto* warpEnvKnob = dynamic_cast<KnobComponent*> (knob (id (F::warpEnv), "AMOUNT"));
    // Under källorna, i signalens ordning: tonhöjd, sub/brus, filtrets envelope (själva filtren: FILTER-fliken) -> volym och utgång.
    groups.addRow ({ { "PITCH", { knob (id (F::octave), "OCTAVE"), knob (id (F::tune), "TUNE"),
                                  knob (id (F::pitchEnvAmount), "ENV AMT"), knob (id (F::pitchEnvDecay), "ENV DEC") } },
                     { "SUB", { knob (id (F::sub), "LEVEL"), choice (id (F::subWave), "WAVE"), choice (id (F::subOctave), "OCTAVE") } },
                     { "NOISE", { knob (id (F::noise), "LEVEL"), knob (id (F::noiseTone), "TONE"), choice (id (F::noiseType), "TYPE") } },
                     { "FILTER ENVELOPE", { knob (id (F::envAmount), "AMOUNT"), knob (id (F::envAttack), "ATTACK"), knob (id (F::envDecay), "DECAY"),
                                            knob (id (F::velToEnv), "VEL") } } });
    groups.addRow ({ { "VOLUME ENVELOPE", { knob (id (F::attack), "ATTACK"), knob (id (F::decay), "DECAY"),
                                            knob (id (F::sustain), "SUSTAIN"), knob (id (F::release), "RELEASE") } },
                     { "WARP ENV", { warpEnvKnob, choice (id (F::warpEnvSource), "SOURCE"), knob (id (F::warpEnvDecay), "DIRECT DEC") } },
                     { "DRIVE", { knob (id (F::drive), "AMOUNT"), choice (id (F::driveMode), "MODE") } },
                     { "LAYER OUT", { knob (id (F::level), "LEVEL"), knob (id (F::duck), "DUCK"), knob (id (F::gate), "LENGTH"),
                                      knob (id (F::voices), "VOICES") } } }); // (notlängden: GATE är hackrytmen; GLIDE har ingen ratt: parametern används inte)
    // Fas PH: polaritet, startfas och nudge (tidsförskjutning ±2 ms) för att få kick och bas att summera rätt.
    groups.addRow ({ { "PHASE", { choice (id (F::polarity), "POLARITY"), knob (id (F::phaseDeg), "START"), knob (id (F::nudgeMs), "NUDGE") } } });
    // Fas P: varning när VOICES x unison (summan av de påslagna oscillatorernas stämmor) blir stort, eftersom varje röst kostar lika mycket CPU.
    if (auto* voicesKnob = dynamic_cast<KnobComponent*> (controls.back().get()))
        voicesKnob->setWarning ([this]() -> juce::String
        {
            auto value = [this] (F field) { return (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, field))->load(); };
            const auto stacks = (value (F::aOn) != 0 ? value (F::aUnison) : 0) + (value (F::bOn) != 0 ? value (F::bUnison) : 0)
                              + (value (F::cOn) != 0 ? value (F::cUnison) : 0);
            const auto voices = value (F::voices);
            if (voices < 2 || voices * stacks <= 24)
                return {};
            return juce::String (voices) + " voices x " + juce::String (stacks) + " unison voices = " + juce::String (voices * stacks)
                 + " oscillator voices: heavy on the CPU. Lower VOICES or UNISON if the DAW stutters.";
        });
    // Fas S: WARP ENV gör inget när OSC 1:s WARP-läge är Off.
    warpEnvKnob->setWarning ([this]() -> juce::String
    {
        auto value = [this] (F field) { return processor.apvts.getRawParameterValue (ParamID::bass (layer, field))->load(); };
        if (value (F::warpEnv) <= 0.0f || value (F::aWarp) >= 0.5f)
            return {};
        return "WARP ENV needs a WARP mode on OSC 1 (it is Off now), otherwise it does nothing.";
    });
    // ENV 2, ENV 3, VELOCITY och RANDOM (modulationskällorna) finns i ENV-fliken längst ner (fas L: raden här var en dubblett).
}

bool InstrumentModule::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac");
}

void InstrumentModule::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    samplePage->loadSample (juce::File (files[0]));
    sources.setCurrentTab (1 + ParamID::oscsPerLayer); // visa SAMPLE-fliken
    repaint();
}

void InstrumentModule::paintOverChildren (juce::Graphics& g)
{
    if (dragHover)
    {
        g.setColour (getAccent().withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (getAccent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, 2.0f);
    }
}

//==============================================================================
SampleView::SampleView (PluginBase& p, int layerIn, juce::Colour accentIn) : processor (p), layer (layerIn), accent (accentIn)
{
    setMouseCursor (speakerCursor);
    setTooltip ("SAMPLE - The waveform of this layer's sample. Click and hold it to hear just the sample for as long as you hold (it loops if the sample ends). The white line marks where each note starts (START); "
                "the moving marker shows where the sample is playing.");
    startTimerHz (30);
}

void SampleView::timerCallback()
{
    const auto* sample = processor.getActiveInstrumentSample (layer);
    const auto start = processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::sampleStart))->load();
    const auto position = processor.transportStatus.instrumentSamplePosition[(size_t) layer].load();
    if (position >= 0.0f && (marker < 0.0f || position < marker))
        flashWallTime = juce::Time::getMillisecondCounterHiRes(); // en ny ton (positionen började om)
    const auto flashing = juce::Time::getMillisecondCounterHiRes() - flashWallTime < 500.0;
    const auto slicer = sample != nullptr && sample->slices.size() > 1; // en sampling med flera skivor har "Slice N"-rader
    if (sample != shown || slicer != shownSlicer || start != lastStart || position != marker || flashing)
    {
        shown = sample;
        shownSlicer = slicer;
        lastStart = start;
        marker = position;
        repaint();
    }
}

void SampleView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto* sample = processor.getActiveInstrumentSample (layer);
    if (sample == nullptr || sample->audio.getNumSamples() < 2)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawText ("Drop a WAV here, or press LOAD", bounds, juce::Justification::centred);
        return;
    }

    const auto area = bounds.reduced (6.0f, 8.0f);
    const auto* data = sample->audio.getReadPointer (0);
    const auto length = sample->audio.getNumSamples();
    const auto width = std::max (1, (int) area.getWidth());
    const auto viewLow = scroll, viewSpan = 1.0f / zoom; // zoomen: vilken del (0..1) av samplingen som syns

    juce::Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (area.toNearestInt());

    juce::Path path;
    for (int x = 0; x < width; ++x)
    {
        const auto from = juce::jlimit (0, length - 1, (int) ((viewLow + viewSpan * (float) x / (float) width) * (float) length));
        const auto to = std::max (from + 1, (int) ((viewLow + viewSpan * (float) (x + 1) / (float) width) * (float) length));
        auto lowest = 0.0f, highest = 0.0f;
        for (int i = from; i < to && i < length; ++i)
        {
            lowest = std::min (lowest, data[i]);
            highest = std::max (highest, data[i]);
        }
        const auto px = area.getX() + (float) x;
        path.addRectangle (px, area.getCentreY() - highest * area.getHeight() * 0.5f, 1.0f, std::max (1.0f, (highest - lowest) * area.getHeight() * 0.5f));
    }
    g.setColour (accent.withAlpha (0.85f));
    g.fillPath (path);

    if (shownSlicer && showSlices)
    {
        // E2: varje skiva är en egen färgad ruta med rubrik "Slice N" (raden i mönstret).
        const auto count = (int) sample->slices.size();
        for (int i = 0; i < count; ++i)
        {
            const auto fromX = normToX ((float) sample->slices[(size_t) i] / (float) length);
            const auto toSample = i + 1 < count ? sample->slices[(size_t) i + 1] : length;
            const auto toX = normToX ((float) toSample / (float) length);
            const auto box = juce::Rectangle<float> (fromX, area.getY(), std::max (1.0f, toX - fromX), area.getHeight());
            const auto colour = juce::Colour::fromHSV (std::fmod ((float) i * 0.13f, 1.0f), 0.55f, 0.95f, 1.0f);
            const auto hasRow = true; // alla skivor är tangenter i slicer-rullen
            g.setColour (colour.withAlpha (i == heldSlice ? 0.45f : hasRow ? 0.2f : 0.06f));
            g.fillRect (box);
            g.setColour (colour.withAlpha (hasRow ? 0.9f : 0.35f));
            g.drawRect (box, 1.5f);
            if (box.getWidth() > 34.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (hasRow ? 0.92f : 0.4f));
                g.setFont (Theme::font (10.0f));
                g.drawText ("Slice " + juce::String (i + 1), box.withHeight (14.0f).reduced (3.0f, 0.0f), juce::Justification::centredLeft);
            }
        }
    }
    else if (shownSlicer)
    {
        // Skivgränserna; START gäller inte när samplingen har flera skivor.
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        for (size_t i = 0; i < sample->slices.size(); ++i)
        {
            const auto x = normToX ((float) sample->slices[i] / (float) length);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
            g.setFont (Theme::font (9.0f));
            g.drawText (juce::String ((int) i + 1), juce::Rectangle<float> (x + 2.0f, area.getY(), 24.0f, 12.0f), juce::Justification::topLeft);
        }
    }
    else
    {
        const auto startX = normToX (juce::jlimit (0.0f, 1.0f, lastStart));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawVerticalLine ((int) startX, area.getY(), area.getBottom());
    }

    // Spelmarkör: det som spelats lyses upp, ett lysande streck där samplingen är nu, och en blixt när en ny ton börjar.
    if (marker >= 0.0f)
    {
        const auto markerX = normToX (marker);
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (area.withRight (markerX).toNearestInt());
            g.setColour (accent.brighter (0.5f).withAlpha (0.9f));
            g.fillPath (path);
        }
        g.setColour (accent.withAlpha (0.3f));
        g.fillRect (juce::Rectangle<float> (markerX - 3.0f, area.getY(), 6.0f, area.getHeight()));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (markerX - 0.75f, area.getY(), 1.5f, area.getHeight()));
    }
    const auto sinceFlash = juce::Time::getMillisecondCounterHiRes() - flashWallTime;
    if (sinceFlash < 500.0)
    {
        g.setColour (accent.withAlpha (0.16f * (float) std::exp (-sinceFlash / 140.0)));
        g.fillRoundedRectangle (bounds, 6.0f);
    }

    // Glöd när musen är över rutan.
    if (hover)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 4.0f);
        g.setColour (accent.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 1.5f);
    }
}

void SampleView::mouseDown (const juce::MouseEvent& e)
{
    // Manuellt läge: Skift-klick lägger en skivmarkör, Alt-klick tar bort den närmaste.
    if (shownSlicer && (e.mods.isShiftDown() || e.mods.isAltDown()) && processor.instrumentSampleSlot (layer).sliceMode() == SampleSlicing::manual)
    {
        const auto position = xToNorm ((float) e.x);
        auto markers = processor.instrumentSampleSlot (layer).manualMarkers();
        if (e.mods.isShiftDown())
            markers.push_back (position);
        else if (! markers.empty())
            markers.erase (std::min_element (markers.begin(), markers.end(), [position] (float a, float b) { return std::abs (a - position) < std::abs (b - position); }));
        processor.setInstrumentSliceMarkers (layer, std::move (markers));
        return;
    }
    if (manualEditing())
    {
        // Högerklick tar bort gränsen under musen; vänsterklick på en gräns börjar dra den (ingen provlyssning).
        if (const auto nearest = nearestMarker ((float) e.x); nearest >= 0)
        {
            if (e.mods.isPopupMenu())
            {
                auto markers = processor.instrumentSampleSlot (layer).manualMarkers();
                markers.erase (markers.begin() + nearest);
                processor.setInstrumentSliceMarkers (layer, std::move (markers));
                repaint();
            }
            else
                draggedMarker = nearest;
            return;
        }
        if (e.mods.isPopupMenu())
            return;
    }
    if (const auto* sample = processor.getActiveInstrumentSample (layer); sample != nullptr && shownSlicer && showSlices)
    {
        // Ett klick på en skiva spelar bara den, så länge musen hålls.
        const auto index = sliceAt ((float) e.x, *sample);
        if (index >= 0)
        {
            heldSlice = index;
            heldNote = processor.rowRootNote (layer);
            processor.postNoteAudition (layer, heldNote, 100, true, index);
            repaint();
        }
        return;
    }
    if (processor.getActiveInstrumentSample (layer) != nullptr)
        hold.start (PluginBase::heldSoloAudition (layer, 3)); // bara samplingen, så länge musen hålls (loopar om den tar slut)
}

void SampleView::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedMarker < 0)
        return;
    auto markers = processor.instrumentSampleSlot (layer).manualMarkers();
    if (draggedMarker >= (int) markers.size())
        return;
    // Gränsen får inte passera sina grannar, så att den dragna behåller sitt index.
    const auto low = draggedMarker > 0 ? markers[(size_t) draggedMarker - 1] + 0.0005f : 0.0005f;
    const auto high = draggedMarker + 1 < (int) markers.size() ? markers[(size_t) draggedMarker + 1] - 0.0005f : 0.9995f;
    markers[(size_t) draggedMarker] = juce::jlimit (low, std::max (low, high), xToNorm ((float) e.x));
    processor.setInstrumentSliceMarkers (layer, std::move (markers));
    repaint();
}

void SampleView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Dubbelklick på en ledig plats lägger en ny gräns (ett enkelklick provlyssnar skivan).
    if (! manualEditing() || nearestMarker ((float) e.x) >= 0)
        return;
    auto markers = processor.instrumentSampleSlot (layer).manualMarkers();
    markers.push_back (xToNorm ((float) e.x));
    processor.setInstrumentSliceMarkers (layer, std::move (markers));
    repaint();
}

void SampleView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (processor.getActiveInstrumentSample (layer) == nullptr)
        return;
    // Hjulet zoomar mot musen; med Skift scrollar det i sidled.
    if (e.mods.isShiftDown())
        scroll = juce::jlimit (0.0f, 1.0f - 1.0f / zoom, scroll - wheel.deltaY * 0.5f / zoom);
    else
    {
        const auto under = xToNorm ((float) e.x);
        const auto fraction = juce::jlimit (0.0f, 1.0f, (float) (e.x - 6) / (float) std::max (1, getWidth() - 12));
        zoom = juce::jlimit (1.0f, 64.0f, zoom * std::exp (wheel.deltaY * 1.5f));
        scroll = juce::jlimit (0.0f, 1.0f - 1.0f / zoom, under - fraction / zoom);
    }
    repaint();
}

float SampleView::normToX (float norm) const
{
    return 6.0f + (norm - scroll) * zoom * (float) std::max (1, getWidth() - 12);
}

float SampleView::xToNorm (float x) const
{
    return juce::jlimit (0.0f, 1.0f, scroll + juce::jlimit (0.0f, 1.0f, (x - 6.0f) / (float) std::max (1, getWidth() - 12)) / zoom);
}

bool SampleView::manualEditing() const
{
    return shownSlicer && showSlices && processor.instrumentSampleSlot (layer).sliceMode() == SampleSlicing::manual;
}

int SampleView::nearestMarker (float x) const
{
    const auto markers = processor.instrumentSampleSlot (layer).manualMarkers();
    auto best = -1;
    auto bestDistance = 6.0f; // pixlar
    for (int i = 0; i < (int) markers.size(); ++i)
        if (const auto distance = std::abs (normToX (markers[(size_t) i]) - x); distance <= bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    return best;
}

void SampleView::mouseUp (const juce::MouseEvent&)
{
    draggedMarker = -1;
    hold.stop();
    if (heldNote >= 0)
        processor.postNoteAudition (layer, heldNote, 0, false);
    heldNote = -1;
    heldSlice = -1;
    repaint();
}

int SampleView::sliceAt (float x, const ClickSample& sample) const
{
    const auto length = sample.audio.getNumSamples();
    const auto position = std::min (0.9999f, xToNorm (x)) * (float) length;
    auto found = -1;
    for (size_t i = 0; i < sample.slices.size(); ++i)
        if ((float) sample.slices[i] <= position)
            found = (int) i;
    return found;
}

SamplePage::SamplePage (PluginBase& p, int layerIn, juce::Colour accent)
    : processor (p), layer (layerIn), view (p, layerIn, accent)
{
    using F = InstrumentField;
    auto id = [layerIn] (F f) { return ParamID::bass (layerIn, f); };
    auto knob = [&] (F f, const char* name)
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, id (f), name, accent));
        addAndMakeVisible (*controls.back());
    };
    auto choice = [&] (F f, const char* name)
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, id (f), name));
        addAndMakeVisible (*controls.back());
    };
    knob (F::sampleLevel, "LEVEL");
    knob (F::sampleStart, "START");
    knob (F::sampleRoot, "ROOT");
    choice (F::sampleLoop, "LOOP");
    choice (F::sampleKey, "KEY");

    // Skivor: varje skiva har en egen rad "Slice N" i mönstret (en not där spelar skivan i originalhastighet, utan loop).
    // Sample-editor: REVERSE per ton, AMP- och PITCH-kurvor (alla av som standard = som förut).
    auto editKnob = [&] (std::vector<std::unique_ptr<juce::Component>>& list, F f, const char* name)
    {
        list.push_back (std::make_unique<KnobComponent> (p.apvts, id (f), name, accent));
        addAndMakeVisible (*list.back());
    };
    auto editToggle = [&] (juce::ToggleButton& toggle, F f, const char* tip,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment)
    {
        toggle.setTooltip (tip);
        toggle.getProperties().set ("labelled", true);
        toggle.setColour (juce::ToggleButton::tickColourId, accent);
        toggle.setColour (juce::ToggleButton::textColourId, Theme::textDim);
        addAndMakeVisible (toggle);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id (f), toggle);
    };
    editToggle (sampleReverseToggle, F::sampleReverse, "REVERSE - Every note plays the sample backwards, from its end (START counts from the end). Off (default): as before.", sampleReverseAttachment);
    editToggle (ampToggle, F::sampleAmpOn, "AMP - Fades the sample out over TIME with the SHAPE curve, so a long sample can be shortened. Off (default): as before.", ampAttachment);
    editToggle (pitchToggle, F::samplePitchOn, "PITCH - The sample starts SEMIS higher or lower and glides to its own pitch over TIME. Off (default): as before.", pitchAttachment);
    editKnob (ampControls, F::sampleAmpMs, "TIME");
    editKnob (ampControls, F::sampleAmpShape, "SHAPE");
    editKnob (pitchControls, F::samplePitchSemis, "SEMIS");
    editKnob (pitchControls, F::samplePitchMs, "TIME");
    editKnob (pitchControls, F::samplePitchShape, "SHAPE");

    reverseToggle.setButtonText ("SLICES REVERSE");
    reverseToggle.setTooltip ("SLICES REVERSE - Plays every slice backwards.");
    reverseToggle.getProperties().set ("labelled", true);
    reverseToggle.setColour (juce::ToggleButton::tickColourId, accent);
    reverseToggle.setColour (juce::ToggleButton::textColourId, Theme::textDim);
    addAndMakeVisible (reverseToggle);
    sliceCapNotice.setFont (Theme::font (10.0f));
    sliceCapNotice.setColour (juce::Label::textColourId, juce::Colour (0xffe0a040));
    addChildComponent (sliceCapNotice);
    reverseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id (F::slicerReverse), reverseToggle);
    filterSendToggle.setTooltip ("SEND THROUGH FILTER - On: the sample goes through Filter 1 and Filter 2 (set on the FILTER tab). "
                                 "Off (default): it bypasses both filters, so a sample keeps its full sound.");
    filterSendToggle.setButtonText ("FILTER");
    filterSendToggle.getProperties().set ("labelled", true);
    filterSendToggle.setColour (juce::ToggleButton::tickColourId, accent);
    filterSendToggle.setColour (juce::ToggleButton::textColourId, Theme::textDim);
    addAndMakeVisible (filterSendToggle);
    filterSendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id (F::filterSample), filterSendToggle);
    sliceModeBox.addItemList ({ "Grid", "Auto", "Manual" }, 1);
    sliceModeBox.setTooltip ("Where the slices are: Grid = equal parts (the slider sets how many, max 128), Auto = cut at the hits (the slider sets how sensitive), "
                             "Manual = your own markers (press CREATE to start from the Grid/Auto result; Shift-click adds a marker, Alt-click removes the nearest).");
    sliceModeBox.onChange = [this] { applySlicing(); };
    addAndMakeVisible (sliceModeBox);
    createButton.setTooltip ("CREATE - Turns the slices from Grid or Auto into your own markers (Manual mode), so you can then drag, add and remove them: "
                             "drag a boundary to move it, double-click to add one, right-click a boundary to remove it (zoom with the mouse wheel).");
    createButton.onClick = [this]
    {
        const auto* sample = processor.getActiveInstrumentSample (layer);
        if (sample == nullptr || sample->slices.size() < 2)
            return;
        std::vector<float> markers;
        for (size_t i = 1; i < sample->slices.size(); ++i)
            markers.push_back ((float) sample->slices[i] / (float) sample->audio.getNumSamples());
        processor.setInstrumentSliceMarkers (layer, std::move (markers));
        processor.setInstrumentSlicing (layer, SampleSlicing::manual, (float) sensitivityAmount.getValue(), juce::roundToInt (gridAmount.getValue()));
        syncSlicerControls();
        showSlicesButton.setToggleState (true, juce::dontSendNotification);
        view.setShowSlices (true);
    };
    addAndMakeVisible (createButton);
    gridAmount.setRange (1.0, (double) SampleSlicing::maxSlices, 1.0);
    gridAmount.setTooltip ("How many equal slices (max 128).");
    sensitivityAmount.setRange (0.0, 1.0, 0.01);
    sensitivityAmount.setTooltip ("How sensitive the hit detection is: higher = more slices.");
    for (auto* slider : { &gridAmount, &sensitivityAmount })
    {
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
        slider->onValueChange = [this] { applySlicing(); };
        addChildComponent (*slider);
    }
    syncSlicerControls();

    // E2: SHOW väljer vad vyn visar: hela samplingen, eller varje skiva som en egen ruta (klick spelar bara den skivan).
    showLabel.setFont (Theme::font (10.0f));
    showLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (showLabel);
    showSampleButton.setTooltip ("SHOW SAMPLE - Shows the whole waveform. Click and hold it to hear the sample.");
    showSlicesButton.setTooltip ("SHOW SLICES - Shows every slice as its own coloured box with its name (its row in the pattern). Click a slice to hear only that slice.");
    for (auto* button : { &showSampleButton, &showSlicesButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (4711 + layerIn);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        addAndMakeVisible (*button);
    }
    showSampleButton.setToggleState (true, juce::dontSendNotification);
    showSampleButton.onClick = [this] { view.setShowSlices (false); };
    showSlicesButton.onClick = [this] { view.setShowSlices (true); };
    gridAmount.setTextValueSuffix (" slices");
    sensitivityAmount.setTextValueSuffix ("");

    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::sample, "the SAMPLE source (loading, root note, loop, slices)");
    addAndMakeVisible (helpButton);
    addAndMakeVisible (view);
    label.setFont (Theme::font (11.0f));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);

    loadButton.setTooltip ("Load a WAV/AIFF/FLAC as this layer's sample (or drop a file on the page). The sample is turned on for you. "
                           "Turn the oscillators off (the power buttons on the OSC tabs) to hear only the sample.");
    loadButton.onClick = [this]
    {
        if (isMulti())
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose the zone samples (name them like Piano_C3 to set the root note)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      if (fc.getResults().isEmpty())
                                          return;
                                      const auto added = processor.addInstrumentMultiSamples (layer, fc.getResults());
                                      if (added == 0)
                                      {
                                          label.setText ("Could not read the files (max " + juce::String (MultiSampling::maxZones) + " zones, "
                                                         + juce::String ((int) MultiSampling::maxSecondsPerZone) + " s each)", juce::dontSendNotification);
                                          return;
                                      }
                                      if (auto* on = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::sampleOn)))
                                          Presets::setParameter (*on, 1.0f);
                                      if (auto* level = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::sampleLevel)); level != nullptr && level->getValue() <= 0.0f)
                                          Presets::setParameter (*level, 1.0f);
                                      updateLabel();
                                  });
            return;
        }
        chooser = std::make_unique<juce::FileChooser> ("Choose a sample for this layer", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadSample (fc.getResult());
                              });
    };
    addAndMakeVisible (loadButton);
    sourceBox.addItemList ({ "Single", "Multi" }, 1);
    sourceBox.setTooltip ("SINGLE = one sample (with slices). MULTI = several samples (zones) on different keys, e.g. a sampled piano: LOAD takes many files at once "
                          "and spreads them over the keyboard by the root note in each file name (Piano_C3, bass_048). No slices in Multi.");
    addAndMakeVisible (sourceBox);
    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, id (F::sampleSource), sourceBox);
    clearButton.setTooltip ("Remove the sample (in Multi: all zones) from this layer");
    clearButton.onClick = [this]
    {
        if (isMulti())
            processor.clearInstrumentMultiSamples (layer);
        else
            processor.clearInstrumentSample (layer);
        updateLabel();
    };
    addAndMakeVisible (clearButton);
    hearButton.setTooltip ("HEAR - Plays just this layer's sample (you can also click the waveform).");
    hearButton.onClick = [this] { processor.requestAudition (PluginBase::soloAudition (layer, 3)); };
    addAndMakeVisible (hearButton);

    updateLabel();
    startTimerHz (2); // projektet kan laddas om medan fönstret är öppet
}

void SamplePage::timerCallback()
{
    updateLabel();
    syncSlicerControls();
}

void SamplePage::applySlicing()
{
    if (syncing)
        return;
    auto& slot = processor.instrumentSampleSlot (layer);
    const auto mode = juce::jmax (0, sliceModeBox.getSelectedItemIndex());
    processor.setInstrumentSlicing (layer, mode, (float) sensitivityAmount.getValue(), juce::roundToInt (gridAmount.getValue()));
    syncSlicerControls();
}

void SamplePage::syncSlicerControls()
{
    const auto& slot = processor.instrumentSampleSlot (layer);
    const auto mode = slot.sliceMode();
    const juce::ScopedValueSetter<bool> guard (syncing, true);
    sliceModeBox.setSelectedItemIndex (mode, juce::dontSendNotification);
    gridAmount.setValue (slot.sliceGridCount(), juce::dontSendNotification);
    sensitivityAmount.setValue (slot.sliceSensitivity(), juce::dontSendNotification);
    gridAmount.setVisible (mode == SampleSlicing::grid);
    sensitivityAmount.setVisible (mode == SampleSlicing::automatic);
    createButton.setVisible (mode != SampleSlicing::manual);
    const auto* sample = processor.getActiveInstrumentSample (layer);
    juce::ignoreUnused (sample); // inget tak längre: alla skivor spelas från slicer-rullen
    sliceCapNotice.setVisible (false);
}

bool SamplePage::isMulti() const
{
    if (auto* value = processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::sampleSource)))
        return (int) value->load() == 1;
    return false;
}

void SamplePage::updateLabel()
{
    juce::String name;
    if (isMulti())
    {
        const auto& set = processor.instrumentMultiSample (layer);
        const auto count = set.zoneCount();
        if (count > 0)
            name = juce::String (count) + (count == 1 ? " zone: " : " zones: ") + set.zoneName (0) + (count > 1 ? ", ..." : "");
    }
    else
        name = processor.getInstrumentSampleName (layer);
    const auto text = name.isEmpty() ? juce::String (isMulti() ? "No zones loaded" : "No sample loaded") : name;
    if (label.getText() != text)
        label.setText (text, juce::dontSendNotification);
    clearButton.setEnabled (name.isNotEmpty());
    hearButton.setEnabled (name.isNotEmpty() && ! isMulti());
}

void SamplePage::loadSample (const juce::File& file)
{
    if (! processor.loadInstrumentSample (layer, file))
    {
        label.setText ("Could not read " + file.getFileName(), juce::dontSendNotification);
        return;
    }

    // En sampling som inte hörs är förvirrande: slå på den och höj nivån om den står på noll.
    if (auto* on = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::sampleOn)))
        Presets::setParameter (*on, 1.0f);
    if (auto* level = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::sampleLevel)); level != nullptr && level->getValue() <= 0.0f)
        Presets::setParameter (*level, 1.0f);
    updateLabel();
}

void SamplePage::paint (juce::Graphics& g)
{
    for (const auto& box : editBoxes)
    {
        if (box.isEmpty())
            continue;
        g.setColour (Theme::panelAlt);
        g.fillRoundedRectangle (box.toFloat(), 5.0f);
        g.setColour (Theme::panelBorder);
        g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 5.0f, 1.0f);
    }
}

void SamplePage::resized()
{
    auto area = getLocalBounds().reduced (4);
    auto top = area.removeFromTop (24);
    helpButton.setBounds (top.removeFromRight (44).withTrimmedTop (2).withTrimmedBottom (2));
    top.removeFromRight (6);
    filterSendToggle.setBounds (top.removeFromRight (84));
    top.removeFromRight (8);
    clearButton.setBounds (top.removeFromRight (24));
    top.removeFromRight (4);
    loadButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (4);
    hearButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (4);
    sourceBox.setBounds (top.removeFromRight (74));
    top.removeFromRight (8);
    label.setBounds (top);
    area.removeFromTop (6);

    auto right = area.removeFromRight (juce::jlimit (240, 420, area.getWidth() * 2 / 5));
    area.removeFromRight (8);
    // Sample-editorn under vågformen: tre rutor PITCH, AMP och REVERSE. Varje ruta har sin växel överst och
    // rattarna (~48 px) under, så de syns som en grupp.
    {
        auto band = area.removeFromBottom (juce::jlimit (96, 118, area.getHeight() / 3));
        area.removeFromBottom (4);
        const auto total = 3 + 2 + 1;
        const auto unit = band.getWidth() / total;
        const int widths[3] { unit * 3, unit * 2, band.getWidth() - unit * 5 };
        juce::ToggleButton* toggles[3] { &pitchToggle, &ampToggle, &sampleReverseToggle };
        std::vector<std::unique_ptr<juce::Component>>* knobSets[3] { &pitchControls, &ampControls, nullptr };
        for (int i = 0; i < 3; ++i)
        {
            auto box = band.removeFromLeft (widths[i]);
            band.removeFromLeft (i < 2 ? 4 : 0);
            editBoxes[(size_t) i] = box;
            auto inner = box.reduced (4, 3);
            toggles[i]->setBounds (inner.removeFromTop (22));
            if (knobSets[i] != nullptr)
            {
                const auto cell = inner.getWidth() / std::max (1, (int) knobSets[i]->size());
                for (auto& knob : *knobSets[i])
                    knob->setBounds (inner.removeFromLeft (cell));
            }
        }
    }
    view.setBounds (area);

    auto knobsRow = right.removeFromTop (right.getHeight() * 2 / 5);
    const auto cell = knobsRow.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        controls[(size_t) i]->setBounds (knobsRow.removeFromLeft (cell));
    right.removeFromTop (6);
    auto choices = right.removeFromTop (44);
    controls[3]->setBounds (choices.removeFromLeft (choices.getWidth() / 2).reduced (6, 0));
    controls[4]->setBounds (choices.reduced (6, 0));

    right.removeFromTop (4);
    auto toggles = right.removeFromTop (22);
    reverseToggle.setBounds (toggles);
    right.removeFromTop (3);
    auto modeRow = right.removeFromTop (22);
    sliceModeBox.setBounds (modeRow.removeFromLeft (80));
    modeRow.removeFromLeft (4);
    createButton.setBounds (modeRow.removeFromRight (64));
    modeRow.removeFromRight (4);
    gridAmount.setBounds (modeRow);
    sensitivityAmount.setBounds (modeRow);
    right.removeFromTop (3);
    auto showRow = right.removeFromTop (22);
    showLabel.setBounds (showRow.removeFromLeft (44));
    showSampleButton.setBounds (showRow.removeFromLeft (70));
    showRow.removeFromLeft (4);
    showSlicesButton.setBounds (showRow.removeFromLeft (70));
    right.removeFromTop (3);
    sliceCapNotice.setBounds (right.removeFromTop (28));
}

//==============================================================================
float InstrumentModule::pitchValueAt (double seconds) const
{
    // Hur långt tonhöjden är från tonen (0..1 av 48 halvtoner), faller exponentiellt: -60 dB efter ENV DEC.
    const auto decay = std::max (0.001, current.pitchEnvDecayMs * 0.001);
    return (float) (std::abs (current.pitchEnvSemis) / 48.0f * std::exp (std::log (0.001) * seconds / decay));
}

EnvelopeView::NoteOff InstrumentModule::noteOffNow() const
{
    const auto real = processor.typicalNoteSeconds (layer);
    return { real > 0.0 ? real : 0.18, real > 0.0 };
}

float InstrumentModule::ampValueAt (double seconds) const
{
    // Envelopen för en typisk ton i instrumentets mönster (finns ingen, ett exempel på 180 ms).
    const auto gateSeconds = noteOffNow().seconds;
    const auto attack = std::max (0.00005, current.attackMs * 0.001);
    const auto decay = std::max (0.0001, current.decayMs * 0.001);
    const auto release = std::max (0.0001, current.releaseMs * 0.001);
    const auto ln = std::log (0.001);

    auto held = [&] (double t)
    {
        if (t < attack)
            return t / attack;
        return current.sustain + (1.0 - current.sustain) * std::exp (ln * (t - attack) / decay);
    };

    if (seconds < gateSeconds)
        return (float) held (seconds);

    return (float) (held (gateSeconds) * std::exp (ln * (seconds - gateSeconds) / release));
}

void InstrumentModule::resized()
{
    ModulePanel::resized();
    if (! isEmbedded())
        presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    // Flikarna fyller panelen: INSTR, OSC 1-3, SAMPLE, FILTER.
    sources.setBounds (area);
}

void InstrumentModule::layoutInstrPage()
{
    // Envelopegrafen till vänster, rattraderna till höger (två rader, högst 100 px rattar; resten är frigjord yta).
    auto area = instrPage.getLocalBounds().reduced (0, 4);
    auto leftColumn = area.removeFromLeft (juce::jlimit (220, 340, area.getWidth() / 4));
    instrTimeline->setBounds (leftColumn.removeFromBottom (leftColumn.getHeight() / 3).reduced (0, 2));
    envelope.setBounds (leftColumn.reduced (0, 2));
    envelopeListen.setBounds (envelope.getRight() - 40, envelope.getY() + 4, 36, 24); // högtalaren i grafens övre högra hörn
    // HELP nere till vänster: tidsetiketterna ligger överst (EnvelopeView.cpp:95) och kurvnamnen nere till höger.
    instrHelpButton.setBounds (envelope.getX() + 10, envelope.getBottom() - 12 - 18, 40, 18);
    area.removeFromLeft (6);
    const auto rowHeight = juce::jlimit (40, 100, area.getHeight() / groups.numRows() - ControlGroups::titleHeight - 3);
    groups.layout (area, rowHeight);

    // Mini-oscilloskopen sitter i SUB-gruppens rubrikrad, till höger om rubriken.
    const auto sub = groups.frameOf ("SUB");
    if (! sub.isEmpty())
        subScope.setBounds (sub.getRight() - 6 - 54, sub.getY() + 1, 54, ControlGroups::titleHeight);

    // NOISE: skicka-genom-filter-knappen i rubrikraden, till höger om rubriken.
    const auto noise = groups.frameOf ("NOISE");
    if (! noise.isEmpty())
        noiseFilterButton.setBounds (noise.getRight() - 6 - 62, noise.getY() - 1, 62, ControlGroups::titleHeight + 3);
}

//==============================================================================
SubScope::SubScope (PluginBase& p, int layerIn) : processor (p), layer (layerIn)
{
    setInterceptsMouseClicks (true, false); // tooltip
    setTooltip ("SUB - Shows the sub oscillator: its wave (WAVE) and size (LEVEL). It glows and moves while this instrument is sounding, so you can see the sub is running even when "
                "every oscillator is off. It is dim and still when the instrument is silent.");
    startTimerHz (30);
}

void SubScope::timerCallback()
{
    using F = InstrumentField;
    const auto newLevel = processor.apvts.getRawParameterValue (ParamID::bass (layer, F::sub))->load();
    const auto newWave = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, F::subWave))->load();
    const auto nowSounding = newLevel > 0.001f && processor.transportStatus.instrumentSounding[(size_t) layer].load();

    const auto target = nowSounding ? 1.0f : 0.0f;
    const auto newGlow = glow + (target - glow) * 0.3f;
    const auto changed = newLevel != level || newWave != wave || std::abs (newGlow - glow) > 0.002f || nowSounding;

    level = newLevel;
    wave = newWave;
    sounding = nowSounding;
    glow = std::abs (target - newGlow) < 0.01f ? target : newGlow;
    if (sounding)
        phase = std::fmod (phase + 0.035f, 1.0f); // vågen rullar åt vänster
    if (changed)
        repaint();
}

void SubScope::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = Theme::layerColour (layer);

    g.setColour (juce::Colours::black.withAlpha (0.35f + 0.1f * glow));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (accent.withAlpha (0.25f + 0.55f * glow));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    const auto inner = bounds.reduced (3.0f, 2.0f);
    const auto centreY = inner.getCentreY();
    const auto halfHeight = inner.getHeight() * 0.5f;
    const auto size = juce::jlimit (0.0f, 1.0f, level / 0.6f); // LEVEL är 0..1; det mesta av räckvidden används långt under 1
    const auto amplitude = level > 0.001f ? halfHeight * (0.25f + 0.75f * size) : 0.0f;

    juce::Path path;
    const auto points = (int) inner.getWidth();
    for (int i = 0; i <= points; ++i)
    {
        const auto x = (float) i / (float) points;
        const auto p = std::fmod (x * 2.0f + phase, 1.0f); // två perioder över rutan
        float shape;
        switch (wave)
        {
            case 1:  shape = 1.0f - 4.0f * std::abs (p - 0.5f); break;
            case 2:  shape = std::tanh (4.0f * std::sin (juce::MathConstants<float>::twoPi * p)); break;
            case 3:  shape = 2.0f * p - 1.0f; break;
            default: shape = std::sin (juce::MathConstants<float>::twoPi * p); break;
        }
        const auto y = centreY - shape * amplitude;
        if (i == 0)
            path.startNewSubPath (inner.getX() + x * inner.getWidth(), y);
        else
            path.lineTo (inner.getX() + x * inner.getWidth(), y);
    }

    if (glow > 0.02f)
    {
        g.setColour (accent.withAlpha (0.3f * glow));
        g.strokePath (path, juce::PathStrokeType (3.5f));
    }
    g.setColour (juce::Colour (0xffffffff).interpolatedWith (accent, 0.35f).withAlpha (0.35f + 0.65f * glow));
    g.strokePath (path, juce::PathStrokeType (1.2f));
}

//==============================================================================
void InstrumentHost::Header::resized()
{
    auto area = getLocalBounds();
    if (power != nullptr)
    {
        power->setBounds (area.removeFromLeft (28));
        area.removeFromLeft (16);
    }
    selector.setBounds (area.removeFromLeft (150));
    area.removeFromLeft (8);
    addButton.setBounds (area.removeFromLeft (28));
    area.removeFromLeft (4);
    removeButton.setBounds (area.removeFromLeft (28));
    area.removeFromLeft (18);
    if (extras != nullptr)
        extras->setBounds (area);
}

InstrumentHost::InstrumentHost (PluginBase& p) : processor (p)
{
    header.selector.setTooltip ("INSTRUMENT - Choose which instrument you are editing (up to 10). Right-click or use the menu to rename it. "
                                "Every instrument has its own pattern, oscillators, filter, envelopes, effect send and sidechain depth.");
    header.selector.onClick = [this] { showSelector(); };
    header.addButton.setTooltip ("ADD INSTRUMENT - Adds another instrument (max 10). It starts with default settings and an empty pattern.");
    header.addButton.onClick = [this] { addInstrument(); };
    header.removeButton.setTooltip ("REMOVE INSTRUMENT - Removes the LAST instrument and deletes everything in it from memory (pattern, sound, sample, "
                                    "wavetables, modulation). You are asked first if it has anything in it. To silence another one, turn it off with its power button.");
    header.removeButton.onClick = [this] { removeInstrument(); };
    for (auto* component : std::initializer_list<juce::Component*> { &header.selector, &header.addButton, &header.removeButton })
        header.addAndMakeVisible (component);

    header.selector.addMouseListener (this, false); // högerklick: instrumentmenyn (InstrumentMenu.h)
    processor.beforeTrackChange = [this] { resetPanels(); };
    lastStructureVersion = processor.layerStructureVersion.load();
    current = juce::jlimit (0, p.activeInstruments() - 1, p.selectedInstrument);
    select (current);
    refresh();
    startTimerHz (4);
}

void InstrumentHost::resetPanels()
{
    header.extras = nullptr;
    header.power = nullptr;
    for (size_t i = 0; i < extras.size(); ++i)
    {
        if (extras[i] != nullptr)
        {
            header.removeChildComponent (extras[i]);
            extras[i] = nullptr;
        }
        if (powers[i] != nullptr)
        {
            header.removeChildComponent (powers[i]);
            powers[i] = nullptr;
        }
    }
    for (auto& module : modules)
        module.reset();
}

void InstrumentHost::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && event.eventComponent == &header.selector)
        InstrumentMenu::show (processor, current, header.selector);
}

InstrumentHost::~InstrumentHost()
{
    processor.beforeTrackChange = nullptr;
    header.selector.removeMouseListener (this);
    for (auto* e : extras)
        if (e != nullptr)
            header.removeChildComponent (e);
    for (auto* p : powers)
        if (p != nullptr)
            header.removeChildComponent (p);
}

InstrumentModule& InstrumentHost::moduleFor (int layer)
{
    auto& slot = modules[(size_t) layer];
    if (slot == nullptr)
    {
        slot = std::make_unique<InstrumentModule> (processor, layer);
        addChildComponent (*slot);
        extras[(size_t) layer] = slot->embedInTab (false); // på/av ligger längst till vänster i Header, inte i sidans kontroller
        header.addChildComponent (*extras[(size_t) layer]);
        powers[(size_t) layer] = slot->getPower();
        if (powers[(size_t) layer] != nullptr)
            header.addChildComponent (*powers[(size_t) layer]);
        slot->setBounds (getLocalBounds());
    }
    return *slot;
}

void InstrumentHost::resized()
{
    for (auto& module : modules)
        if (module != nullptr)
            module->setBounds (getLocalBounds());
}

void InstrumentHost::select (int layer)
{
    layer = juce::jlimit (0, processor.activeInstruments() - 1, layer);
    current = layer;
    processor.selectedInstrument = layer;
    moduleFor (layer);

    for (int i = 0; i < ParamID::numInstrumentLayers; ++i)
    {
        if (modules[(size_t) i] != nullptr)
            modules[(size_t) i]->setVisible (i == layer);
        if (extras[(size_t) i] != nullptr)
            extras[(size_t) i]->setVisible (i == layer);
        if (powers[(size_t) i] != nullptr)
            powers[(size_t) i]->setVisible (i == layer);
    }
    header.extras = extras[(size_t) layer];
    header.power = powers[(size_t) layer];
    header.resized();
    lastNamesVersion = -1;
    refresh();
}

void InstrumentHost::refresh()
{
    const auto count = processor.activeInstruments();
    if (current >= count)
    {
        select (count - 1);
        return;
    }
    lastCount = count;
    lastNamesVersion = processor.layerNamesVersion.load();
    header.selector.setButtonText (processor.getLayerName (current) + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
    header.selector.setColour (juce::TextButton::textColourOffId, Theme::layerColour (current));
    header.addButton.setEnabled (count < ParamID::numInstrumentLayers);
    header.removeButton.setEnabled (count > 1);
}

void InstrumentHost::timerCallback()
{
    // En färg ändrades eller ett instrument klistrades in: temat uppdateras och panelerna byggs om med de nya färgerna.
    if (processor.layerStructureVersion.load() != lastStructureVersion)
    {
        lastStructureVersion = processor.layerStructureVersion.load();
        InstrumentMenu::applyColours (processor);
        resetPanels();
        select (juce::jlimit (0, processor.activeInstruments() - 1, processor.selectedInstrument));
        return;
    }

    // ENV-fliken (och andra) kan byta instrument: följ med.
    if (processor.selectedInstrument != current && juce::isPositiveAndBelow (processor.selectedInstrument, processor.activeInstruments()))
        select (processor.selectedInstrument);

    if (processor.activeInstruments() != lastCount || processor.layerNamesVersion.load() != lastNamesVersion)
        refresh();

    // Brödsmulan i huvudflikraden ritas av TabbedPanel: be den rita om när instrument, namn eller underflik ändras.
    if (const auto crumbNow = crumb(); crumbNow != lastCrumb)
    {
        lastCrumb = crumbNow;
        if (auto* tabs = findParentComponentOfClass<TabbedPanel>())
            tabs->repaint();
    }
}

void InstrumentHost::showSelector()
{
    juce::PopupMenu menu;
    const auto count = processor.activeInstruments();
    for (int i = 0; i < count; ++i)
    {
        const auto on = processor.apvts.getRawParameterValue (ParamID::bass (i, InstrumentField::on))->load() >= 0.5f;
        menu.addItem (i + 1, juce::String (i + 1) + "   " + processor.getLayerName (i) + (on ? "" : "   (off)"), true, i == current);
    }
    menu.addSeparator();
    menu.addItem (100, "Rename " + processor.getLayerName (current) + " ...");
    menu.addItem (101, "Add instrument", count < ParamID::numInstrumentLayers);
    menu.addItem (102, "Remove last instrument", count > 1);

    juce::Component::SafePointer<InstrumentHost> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header.selector).withMinimumWidth (header.selector.getWidth()),
                        [safeThis] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;
                            if (result == 100)
                                safeThis->rename (safeThis->current);
                            else if (result == 101)
                                safeThis->addInstrument();
                            else if (result == 102)
                                safeThis->removeInstrument();
                            else
                                safeThis->select (result - 1);
                        });
}

void InstrumentHost::rename (int layer)
{
    auto* window = new juce::AlertWindow ("Rename", "Name for this instrument (it is shown in the pattern, the mixer and the piano roll):", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    renameDialog = window;
    window->addTextEditor ("name", processor.getLayerName (layer));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Default name", 2);
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<InstrumentHost> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window, layer] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        safeThis->processor.setLayerName (layer, result == 2 ? juce::String() : window->getTextEditorContents ("name"));
        safeThis->processor.markStateChanged();
    }), true);
}

void InstrumentHost::addInstrument()
{
    const auto count = processor.activeInstruments();
    if (count >= ParamID::numInstrumentLayers)
        return;
    processor.setInstrumentCount (count + 1);
    select (count);
}

void InstrumentHost::removeInstrument()
{
    const auto count = processor.activeInstruments();
    if (count <= 1 || confirmOpen)
        return;

    const auto layer = count - 1;
    juce::Component::SafePointer<InstrumentHost> safeThis (this);
    auto remove = [safeThis, layer, count]
    {
        if (safeThis == nullptr)
            return;
        auto& host = *safeThis;
        if (host.current == layer)
            host.select (layer - 1);

        // Panelen rivs först, så att inget i den lyssnar på parametrarna som nollställs.
        if (host.extras[(size_t) layer] != nullptr)
        {
            host.header.removeChildComponent (host.extras[(size_t) layer]);
            host.extras[(size_t) layer] = nullptr;
        }
        host.modules[(size_t) layer].reset();
        host.processor.setInstrumentCount (count - 1);
        host.refresh();
    };

    juce::String what;
    if (! processor.instrumentHasContent (layer, &what))
    {
        remove();
        return;
    }

    confirmOpen = true;
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                      juce::MessageBoxIconType::WarningIcon, "Remove instrument",
                                      "Remove " + processor.getLayerName (layer) + "?\n\nIt has " + what + ".\n\nEverything in it is deleted (pattern, sound, sample, "
                                      "wavetables, modulation) and it can't be undone. Presets you have saved are not touched.",
                                      "Remove", "Cancel"),
                                  [safeThis, remove] (int choice)
    {
        if (safeThis == nullptr)
            return;
        safeThis->confirmOpen = false;
        if (choice == 1)
            remove();
    });
}

//==============================================================================
GateModule::GateModule (PluginBase& p)
    : ModulePanel ("GATE", Theme::gate, &p.apvts, ParamID::bass (0, InstrumentField::on)),
      refs (p.apvts),
      key (p.apvts, ParamID::fxKey, "KEY"),
      presets (p, Presets::Section::bass, Theme::gate)
{
    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    envelope.onBeforePaint = [this]
    {
        attackMs = refs.get (ParamID::gateAttack);
        releaseMs = refs.get (ParamID::gateRelease);
    };

    // En öppning på 100 ms med attack och release.
    envelope.addCurve ({ [this] (double t)
                         {
                             constexpr double open = 0.1;
                             const auto attack = std::max (0.0001, attackMs * 0.001);
                             const auto release = std::max (0.0001, releaseMs * 0.001);
                             const auto opened = std::min (1.0, t / attack);
                             if (t < open)
                                 return (float) opened;
                             return (float) std::max (0.0, std::min (1.0, open / attack) - (t - open) / release);
                         },
                         Theme::gate, true, "GATE" });

    envelope.addHandle ({ [this] { return juce::Point<float> (attackMs * 0.001f, 1.0f); },
                          param (p, ParamID::gateAttack), nullptr, false, Theme::gate });
    envelope.addHandle ({ [this] { return juce::Point<float> (0.1f + releaseMs * 0.001f, 0.0f); },
                          param (p, ParamID::gateRelease), nullptr, false, Theme::gate });
    addAndMakeVisible (envelope);

    addAndMakeVisible (key);
    addKnobs (*this, knobs, p, Theme::gate, {
        { ParamID::gateAttack, "ATTACK" }, { ParamID::gateRelease, "RELEASE" } });
    knobs.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (0, InstrumentField::level), "LEVEL", Theme::gate));
    addAndMakeVisible (*knobs.back());

    help.setText ("Put Kickbase FX on the mixer track of your instrument or bass synth. The synth is gated by the "
                  "gate steps and ducked under the kick. When the DAW is stopped, audio passes through untouched.",
                  juce::dontSendNotification);
    help.setFont (Theme::font (11.0f));
    help.setColour (juce::Label::textColourId, Theme::textDim);
    help.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (help);

    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::gate, "the GATE tab");
    addAndMakeVisible (helpButton);
}

void GateModule::resized()
{
    ModulePanel::resized();
    if (! isEmbedded())
        presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    auto helpRow = area.removeFromBottom (48);
    helpButton.setBounds (helpRow.removeFromRight (44).removeFromTop (24));
    help.setBounds (helpRow);
    auto row = area.removeFromBottom (knobRowHeight);
    area.removeFromBottom (6);
    envelope.setBounds (area);

    const auto cell = row.getWidth() / 5;
    key.setBounds (row.removeFromLeft (cell).reduced (4, 14));
    auto items = pointers (knobs, 0, 3);
    layoutRow (items, row.withTrimmedRight (cell));
}

//==============================================================================
namespace
{
    juce::StringArray instrumentNames()
    {
        juce::StringArray names;
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            names.add ("INSTR " + juce::String (layer + 1));
        return names;
    }

    // Instrumentens färger, och (för pianorullen) kickens efter dem.
    std::vector<juce::Colour> instrumentColours (bool withKick = false)
    {
        std::vector<juce::Colour> colours;
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            colours.push_back (Theme::layerColour (layer));
        if (withKick)
            colours.push_back (Theme::kick);
        return colours;
    }
}

PatternModule::PatternModule (PluginBase& p, bool isInstrument)
    : processor (p), instrument (isInstrument),
      presets (p, Presets::Section::pattern, Theme::pattern),
      sequencer (p, isInstrument ? instrumentNames() : juce::StringArray { "GATE" },
                 isInstrument ? instrumentColours() : std::vector<juce::Colour> { Theme::gate }),
      pianoRoll (p, instrumentColours (true))
{
    presets.setEmptyLabel ("Pattern preset");
    presets.setTooltip ("PATTERN PRESET - Saves or loads the selected row (kick or instrument): its notes and piano roll, GATE, grid, note length, swing, humanize, delay, length and loop. "
                        "The other rows are not touched. The whole sound is not part of it: that is the instrument's own preset.");
    addAndMakeVisible (presets);
    if (instrument)
        grooveBar = std::make_unique<PatternGrooveBar> (p.apvts); // visas i flikraden (InstrumentEditor)

    // Lanerna för tonerna: velocity, tonlängd och tonhöjd (glide ritas i pianorullen), valda i en rullgardinslista.
    laneOrder = { StepSequencer::Lane::velocity, StepSequencer::Lane::gate };
    laneBox.addItem ("Velocity", 1);
    laneBox.addItem ("Note length", 2);
    if (instrument)
    {
        laneOrder.push_back (StepSequencer::Lane::pitch);
        laneBox.addItem ("Pitch", 3);
    }
    laneLabel.setText ("EDIT IN LANE", juce::dontSendNotification);
    laneLabel.setFont (Theme::font (11.0f, true));
    laneLabel.setColour (juce::Label::textColourId, Theme::textDim);
    laneBox.setTooltip ("EDIT IN LANE - What the strip under the selected row shows and edits for each note: its VELOCITY (volume), its NOTE LENGTH or its PITCH. "
                        "Not available for a row that is edited in the piano roll (set those there).");
    laneBox.onChange = [this] { selectLane (laneOrder[(size_t) juce::jlimit (0, (int) laneOrder.size() - 1, laneBox.getSelectedId() - 1)]); };
    addAndMakeVisible (laneLabel);
    addAndMakeVisible (laneBox);

    const auto numLayers = instrument ? ParamID::numInstrumentLayers : 1;
    for (int layer = 0; layer < numLayers; ++layer)
    {
        const auto colour = instrument ? Theme::layerColour (layer) : Theme::gate;
        grids.push_back (std::make_unique<ChoiceComponent> (p.apvts, ParamID::bass (layer, InstrumentField::grid), "GRID"));
        lengths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, InstrumentField::gate), "ALL LENGTHS", colour));
        lengths.back()->setDescription ("NOTE LENGTH SCALE - Scales the length of every note in this instrument at once (100 % = as long as written). "
                                        "Lower = shorter, punchier notes. Set a single note's length with EDIT IN LANE > Note length or in the piano roll.");
        addChildComponent (*grids.back());
        addChildComponent (*lengths.back());
        if (instrument)
        {
            delays.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, InstrumentField::delay), "DELAY", colour));
            delays.back()->setDescription ("DELAY - Pushes this instrument's notes later by 0-50 ms, for example to let the kick come first. It can only move notes later, never earlier.");
            addChildComponent (*delays.back());
            swings.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, InstrumentField::swing), "SWING", colour));
            swings.back()->setDescription ("SWING - This instrument's own swing: its off-beat sixteenths come later. It is added to the SWING of the whole pattern (in the PATTERN tab row), up to 100 %.");
            addChildComponent (*swings.back());
            humanizes.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, InstrumentField::humanize), "HUMAN", colour));
            humanizes.back()->setDescription ("HUMANIZE - This instrument's own random timing: every note comes 0-20 ms late at random. It is added to the HUMANIZE of the whole pattern (in the PATTERN tab row).");
            addChildComponent (*humanizes.back());
        }

        if (instrument)
        {
            gateGrids.push_back (std::make_unique<ChoiceComponent> (p.apvts, ParamID::bass (layer, InstrumentField::gateGrid), "GATE GRID"));
            gateDepths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, InstrumentField::gateDepth), "DEPTH", colour));
            gateSmooths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, InstrumentField::gateSmooth), "SMOOTH", colour));
            gateDepths.back()->setDescription ("How much the gate cuts the sound in its closed steps: 0 % = nothing, 100 % = fully silent.");
            gateSmooths.back()->setDescription ("How soft the gate's edges are. Short = hard chopped, longer = smoother.");

            auto onButton = std::make_unique<juce::TextButton> ("GATE ON");
            onButton->setClickingTogglesState (true);
            onButton->setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.6f));
            onButton->setTooltip ("GATE ON - Turns this layer's gate on. The gate is a separate on/off rhythm (its own grid) that chops the layer's sound; "
                                  "it does not change the notes. Click the steps to the right to open and close it.");
            gateOnAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                p.apvts, ParamID::bass (layer, InstrumentField::gateOn), *onButton));
            gateOnButtons.push_back (std::move (onButton));
            addChildComponent (*gateGrids.back());
            addChildComponent (*gateDepths.back());
            addChildComponent (*gateSmooths.back());
            addChildComponent (*gateOnButtons.back());

        }
    }

    if (instrument)
    {
        kickGrid = std::make_unique<ChoiceComponent> (p.apvts, ParamID::rowGridKick, "GRID");
        addChildComponent (*kickGrid);
    }

    // PATTERN | GATE: vad rutnätet redigerar. Bara i instrumentet (FX-versionens mönster är redan gaten).
    for (auto* button : { &patternTab, &gateTab })
    {
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        addChildComponent (*button);
    }
    patternTab.setTooltip ("PATTERN - The notes. The grid is a simple piano roll (click steps to place notes). PIANO ROLL is the advanced view of the same notes.");
    gateTab.setTooltip ("GATE - A separate on/off rhythm that chops the layer's sound, with its own grid. It does not change the notes: "
                        "notes can sound outside the gate. Turn it on with GATE ON.");
    patternTab.onClick = [this] { showMode (StepSequencer::Mode::notes); };
    gateTab.onClick = [this] { showMode (StepSequencer::Mode::gate); };
    patternTab.setVisible (instrument);
    gateTab.setVisible (instrument);

    fillButton.setTooltip ("FILL - Fill the selected layer with a rhythm (rolling, galloping, triplets, offbeat) or clear it. "
                           "This replaces the layer's notes (and the piano roll).");
    fillButton.onClick = [this] { showFillMenu(); };
    addAndMakeVisible (fillButton);
    gateFillButton.setTooltip ("FILL - Fill the selected layer's gate steps with a rhythm (all open, alternating, offbeats ...).");
    gateFillButton.onClick = [this] { showGateFillMenu(); };
    addChildComponent (gateFillButton);

    sequencer.onLayerSelected = [this] (int layer) { showLayer (layer); };
    sequencer.onOpenPianoRoll = [this] (int layer) { if (layer < RowRegistry::firstSubRow) sequencer.selectLayer (layer); showLayer (layer); showPianoRoll (true); };
    pianoRoll.onDeleted = [this] { showPianoRoll (false); };
    pianoRoll.onPopOut = [this] { togglePopOut(); };
    addAndMakeVisible (sequencer);
    addChildComponent (pianoRoll);

    // Pianorullen ändrar tonhöjd, så den finns bara i instrumentet.
    pianoButton.setClickingTogglesState (false);
    pianoButton.setTooltip ("PIANO ROLL - Green with a +: this row has no piano roll yet, click to add one. Blue: the row has a piano roll, click to open it "
                            "(click again to go back to the steps). The piano roll is the advanced view of the pattern, like FL Studio's: notes anywhere in time "
                            "(1/4 down to 1/64 and triplets), pitch up to 96 semitones either way, glide notes and velocity. Once you edit there, the row "
                            "shows a small piano roll instead of step buttons.");
    pianoButton.onClick = [this] { showPianoRoll (! processor.pianoRollOpen); };
    addChildComponent (pianoButton);

    selectLane (StepSequencer::Lane::velocity);
    mode = instrument && processor.selectedPatternMode == 1 ? StepSequencer::Mode::gate : StepSequencer::Mode::notes;
    sequencer.setMode (mode);
    patternTab.setToggleState (mode == StepSequencer::Mode::notes, juce::dontSendNotification);
    gateTab.setToggleState (mode == StepSequencer::Mode::gate, juce::dontSendNotification);
    {
        const auto layer = processor.selectedPatternLayer;
        const auto valid = layer == Pattern::kickLayer || juce::isPositiveAndBelow (layer, instrument ? Pattern::numLayers : 1);
        sequencer.selectLayer (valid ? layer : 0);
        showLayer (valid ? layer : 0);
    }
    updateVisibility();
    startTimerHz (8);
}

juce::String PatternModule::rowName (int row) const
{
    const auto layer = row == 0 ? Pattern::kickLayer : row - 1;
    return layer == Pattern::kickLayer ? juce::String ("KICK") : instrument ? processor.getLayerName (layer) : juce::String ("GATE");
}

void PatternModule::timerCallback()
{
    // Instrument som lagts till eller tagits bort: raderna följer, och ett borttaget instrument kan inte vara valt.
    if (instrument && processor.activeInstruments() != lastActive)
    {
        lastActive = processor.activeInstruments();
        if (shownLayer != Pattern::kickLayer && shownLayer >= lastActive)
        {
            sequencer.selectLayer (0);
            showLayer (0);
        }
        resized();
        repaint();
    }

    // (Kickens toner spelar hela sitt ljud, så deras längd gör inget.)
    const auto custom = processor.pattern.isCustom (shownLayer);
    if (custom == lastLaneEditable)
        updateVisibility();
    laneBox.setItemEnabled (2, shownLayer != Pattern::kickLayer);
    if (shownLayer == Pattern::kickLayer && sequencer.getLane() == StepSequencer::Lane::gate)
        selectLane (StepSequencer::Lane::velocity);

    if (instrument && lastNamesVersion != processor.layerNamesVersion.load())
    {
        lastNamesVersion = processor.layerNamesVersion.load();
        repaint(); // rubriken visar det valda lagrets namn
    }
    updatePianoButton();
}

void PatternModule::showMode (StepSequencer::Mode newMode)
{
    mode = newMode;
    processor.selectedPatternMode = mode == StepSequencer::Mode::gate ? 1 : 0;
    patternTab.setToggleState (mode == StepSequencer::Mode::notes, juce::dontSendNotification);
    gateTab.setToggleState (mode == StepSequencer::Mode::gate, juce::dontSendNotification);
    sequencer.setMode (mode);
    updateVisibility();
}

void PatternModule::updatePianoButton()
{
    pianoButton.setState (! processor.pattern.isCustom (shownLayer), processor.pianoRollOpen);
}

void PatternModule::togglePopOut()
{
    if (popWindow != nullptr)
    {
        addChildComponent (pianoRoll); // flyttar ut den ur fönstret
        popWindow.reset();
        pianoRoll.setPoppedOut (false);
        resized();
        updateVisibility();
        return;
    }
    pianoRoll.setSize (940, 540);
    popWindow = std::make_unique<PopWindow>();
    popWindow->setUsingNativeTitleBar (true);
    popWindow->setContentNonOwned (&pianoRoll, true);
    popWindow->setResizable (true, true);
    popWindow->setResizeLimits (640, 320, 3000, 2000);
    popWindow->onClose = [safe = juce::Component::SafePointer<PatternModule> (this)]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr && safe->popWindow != nullptr) safe->togglePopOut(); });
    };
    popWindow->centreWithSize (popWindow->getWidth(), popWindow->getHeight());
    pianoRoll.setVisible (true);
    popWindow->setVisible (true);
    pianoRoll.setPoppedOut (true);
    updateVisibility();
}

void PatternModule::showPianoRoll (bool open)
{
    if (! open && popWindow != nullptr)
        togglePopOut();
    processor.pianoRollOpen = open;
    updatePianoButton();
    if (open && mode == StepSequencer::Mode::gate)
        showMode (StepSequencer::Mode::notes);
    updateVisibility();
}

void PatternModule::updateVisibility()
{
    const auto gating = instrument && mode == StepSequencer::Mode::gate;
    const auto rollOpen = ! gating && instrument && processor.pianoRollOpen;

    if (popWindow == nullptr)
        pianoRoll.setVisible (rollOpen);
    sequencer.setVisible (! (rollOpen && popWindow == nullptr)); // utbruten rulle: stegen syns kvar i huvudfönstret
    pianoButton.setVisible (instrument && ! gating);
    fillButton.setVisible (! gating);
    gateFillButton.setVisible (gating);
    // EDIT IN LANE syns inte alls när raden har pianorulle (där sätts velocity, längd och tonhöjd i pianorullen).
    const auto laneEditable = ! processor.pattern.isCustom (shownLayer);
    laneLabel.setVisible (! gating && ! rollOpen && laneEditable);
    laneBox.setVisible (! gating && ! rollOpen && laneEditable);
    lastLaneEditable = laneEditable;

    for (size_t i = 0; i < grids.size(); ++i)
    {
        const auto shown = (int) i == shownLayer;
        grids[i]->setVisible (shown && ! gating);
        if (i < delays.size())
        {
            delays[i]->setVisible (shown && ! gating);
            swings[i]->setVisible (shown && ! gating);
            humanizes[i]->setVisible (shown && ! gating);
        }
        if (i < gateGrids.size())
        {
            gateGrids[i]->setVisible (shown && gating);
            gateDepths[i]->setVisible (shown && gating);
            gateSmooths[i]->setVisible (shown && gating);
            gateOnButtons[i]->setVisible (shown && gating);
        }
    }
    if (kickGrid != nullptr)
        kickGrid->setVisible (shownLayer == Pattern::kickLayer && ! gating);
    updateLengthKnobs();
    updatePianoButton();
    resized();
    repaint();
}

// ALL LENGTHS (instrumentets noters längd, alla på en gång) hör ihop med tonlängden: knappen visas bara när EDIT IN LANE står på Note length.
void PatternModule::updateLengthKnobs()
{
    const auto gating = instrument && mode == StepSequencer::Mode::gate;
    const auto wanted = ! gating && sequencer.getLane() == StepSequencer::Lane::gate && shownLayer != Pattern::kickLayer;
    for (size_t i = 0; i < lengths.size(); ++i)
        lengths[i]->setVisible (wanted && (int) i == shownLayer);
}

void PatternModule::selectLane (StepSequencer::Lane lane)
{
    sequencer.setLane (lane);
    for (size_t i = 0; i < laneOrder.size(); ++i)
        if (laneOrder[i] == lane)
            laneBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
    updateLengthKnobs();
    resized();
    repaint();
}

void PatternModule::showLayer (int layer)
{
    shownLayer = layer;
    const auto baseOfRow = layer >= RowRegistry::firstSubRow ? juce::jlimit (0, Pattern::numNoteLayers - 1, processor.pattern.rows.instrumentOf (layer)) : layer;
    presets.setLayer (baseOfRow, baseOfRow == Pattern::kickLayer ? Theme::kick : instrument ? Theme::layerColour (baseOfRow) : Theme::gate);
    processor.selectedPatternLayer = layer;
    pianoRoll.setLayer (layer);
    updateVisibility();
}

void PatternModule::showKickFillMenu()
{
    // Kickens mallar, på rutnätet 1/16: en takt som upprepas över hela mönstret.
    const juce::StringArray names { "Four on the floor", "Every other beat", "Half-time (beat 3)", "Double (every 8th)", "Syncopated (x..x..x.)", "Clear" };
    const char* bars[] { "x...x...x...x...", "x.......x.......", "........x.......", "x.x.x.x.x.x.x.x.", "x..x..x.x..x..x.", "................" };

    juce::PopupMenu menu;
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    juce::Component::SafePointer<PatternModule> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fillButton), [safeThis, bars] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        auto& p = safeThis->processor;
        PluginBase::EditScope undo (p, "Kick pattern");
        juce::String steps;
        steps << bars[result - 1]; // en takt: kickraden är AUTO och följer innehållet
        p.pattern.setKickSteps (steps, Pattern::ticksPerStep (InstrumentGrid::sixteenth));
        if (auto* gridParam = p.apvts.getParameter (ParamID::rowGridKick))
            Presets::setParameter (*gridParam, gridParam->convertTo0to1 ((float) InstrumentGrid::sixteenth));
        p.patternChanged();
    });
}

void PatternModule::showFillMenu()
{
    if (shownLayer == Pattern::kickLayer)
    {
        showKickFillMenu();
        return;
    }

    juce::PopupMenu menu;
    const auto names = Pattern::templateNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    juce::Component::SafePointer<PatternModule> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fillButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        auto& p = safeThis->processor;
        const auto layer = safeThis->shownLayer;
        PluginBase::EditScope undo (p, "Pattern template: " + p.getLayerName (layer));
        const auto grid = p.pattern.applyLayerTemplate (layer, (PatternTemplate) (result - 1));
        if (auto* gridParam = p.apvts.getParameter (ParamID::bass (layer, InstrumentField::grid)))
            Presets::setParameter (*gridParam, gridParam->convertTo0to1 ((float) grid));
        p.patternChanged();
    });
}

void PatternModule::showGateFillMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "All open");
    menu.addItem (2, "Alternate (open, closed, open ...)");
    menu.addItem (3, "Offbeats (closed, open, closed ...)");
    menu.addItem (4, "Chop (every 4th step closed)");
    menu.addItem (5, "All closed");

    juce::Component::SafePointer<PatternModule> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gateFillButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        auto& p = safeThis->processor;
        const auto layer = safeThis->shownLayer;
        PluginBase::EditScope undo (p, "Gate fill: " + p.getLayerName (layer));
        const auto gridIndex = juce::jlimit (0, Pattern::numGrids - 1, (int) p.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateGrid))->load());
        const auto stepsPerBar = Pattern::instrumentStepsPerBar ((InstrumentGrid) gridIndex);

        for (int i = 0; i < Pattern::maxGateSteps; ++i)
        {
            const auto step = i % stepsPerBar;
            const auto open = result == 1 || (result == 2 && step % 2 == 0) || (result == 3 && step % 2 == 1) || (result == 4 && step % 4 != 3);
            p.pattern.setGateStep (layer, i, open);
        }
        if (auto* on = p.apvts.getParameter (ParamID::bass (layer, InstrumentField::gateOn)))
            Presets::setParameter (*on, 1.0f); // en fylld gate är till för att höras
        p.patternChanged();
    });
}

void PatternModule::paint (juce::Graphics& g)
{
    if (! blockFrame.isEmpty() && instrument)
    {
        const auto colour = shownLayer == Pattern::kickLayer ? Theme::kick : Theme::layerColour (shownLayer);
        g.setColour (colour.withAlpha (0.07f));
        g.fillRoundedRectangle (blockFrame.toFloat(), 6.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.drawRoundedRectangle (blockFrame.toFloat().reduced (0.9f), 6.0f, 1.8f);
    }

    // Rubrik för det valda lagrets inställningar.
    if (! headingBounds.isEmpty())
    {
        g.setFont (Theme::font (11.0f, true));
        g.setColour (! instrument ? Theme::gate : shownLayer == Pattern::kickLayer ? Theme::kick : Theme::layerColour (shownLayer));
        g.drawText (! instrument ? juce::String ("GATE") : shownLayer == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (shownLayer),
                    headingBounds, juce::Justification::centredLeft);
    }
}

PatternGrooveBar::PatternGrooveBar (juce::AudioProcessorValueTreeState& apvts)
{
    auto setUpLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Theme::font (11.0f, true));
        label.setColour (juce::Label::textColourId, Theme::textDim);
        label.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (label);
    };
    setUpLabel (noteEndLabel, "NOTE END");
    setUpLabel (swingLabel, "SWING");
    setUpLabel (humanizeLabel, "HUMANIZE");

    noteEnd.parameter = apvts.getParameter (ParamID::noteEnd);
    if (noteEnd.parameter != nullptr)
        noteEnd.addItemList (noteEnd.parameter->getAllValueStrings(), 1);
    noteEnd.setTooltip ("NOTE END - " + Help::forParameter (ParamID::noteEnd));
    addAndMakeVisible (noteEnd);
    noteEndAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamID::noteEnd, noteEnd);

    auto setUpSlider = [this, &apvts] (FxSendControl::AmountSlider& slider, const char* paramID, const juce::String& tooltip)
    {
        slider.parameter = apvts.getParameter (paramID);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::trackColourId, Theme::pattern);
        slider.setColour (juce::Slider::thumbColourId, Theme::text);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setTooltip (tooltip);
        addAndMakeVisible (slider);
    };
    setUpSlider (swing, ParamID::patSwing, "SWING - For the whole pattern: pushes the off-beat sixteenths of ALL instruments later (0 % = straight, 100 % = triplet feel). Each instrument can add its own swing under GRID. The kick stays on the grid.");
    setUpSlider (humanize, ParamID::patHumanize, "HUMANIZE - For the whole pattern: every note of ALL instruments comes a random 0-20 ms late, so it feels played rather than programmed. Each instrument can add its own under GRID. The kick stays on the grid.");
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamID::patSwing, swing);
    humanizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamID::patHumanize, humanize);
}

void PatternGrooveBar::resized()
{
    auto area = getLocalBounds();
    noteEndLabel.setBounds (area.removeFromLeft (64));
    area.removeFromLeft (4);
    noteEnd.setBounds (area.removeFromLeft (100));
    area.removeFromLeft (10);
    swingLabel.setBounds (area.removeFromLeft (44));
    area.removeFromLeft (4);
    swing.setBounds (area.removeFromLeft (76));
    area.removeFromLeft (10);
    humanizeLabel.setBounds (area.removeFromLeft (64));
    area.removeFromLeft (4);
    humanize.setBounds (area);
}

void PatternModule::resized()
{
    auto area = getLocalBounds();

    // En smal kolumn till vänster: mönstrets längd/slut/presets, PATTERN | GATE (ovanpå varandra) och det valda lagrets inställningar för
    // läget. Vilket lager som redigeras väljs med knapparna vid namnen i raderna.
    auto column = area.removeFromLeft (134);
    area.removeFromLeft (10);

    // Kortet runt det valda instrumentets inställningar: luft mellan ramen och innehållet.
    const auto blockTop = column.getY();
    column = column.reduced (8, 0);
    column.removeFromTop (8);

    headingBounds = column.removeFromTop (16);
    if (instrument)
    {
        patternTab.setBounds (column.removeFromTop (24));
        column.removeFromTop (3);
        gateTab.setBounds (column.removeFromTop (24));
        column.removeFromTop (6);
    }

    // PATTERN: notrutnätet, lanerna (ovanpå varandra), tonlängd, FILL och PIANO ROLL.
    auto notesColumn = column;
    for (auto& grid : grids)
        grid->setBounds (notesColumn.getX(), notesColumn.getY(), notesColumn.getWidth(), 40);
    if (kickGrid != nullptr)
        kickGrid->setBounds (notesColumn.getX(), notesColumn.getY(), notesColumn.getWidth(), 40);
    notesColumn.removeFromTop (44);

    // Instrumentets egen groove: SWING, HUMAN och DELAY (kickens rad ligger alltid på rutnätet).
    if (shownLayer >= 0 && shownLayer < (int) delays.size())
    {
        // (x) En ratt per rad med riktigt rattshuvud (knobRowHeight = 74 px med etiketter), inte tre i en rad.
        const auto swingBounds = notesColumn.removeFromTop (knobRowHeight);
        const auto humanizeBounds = notesColumn.removeFromTop (knobRowHeight);
        const auto delayBounds = notesColumn.removeFromTop (knobRowHeight);
        for (size_t i = 0; i < delays.size(); ++i)
        {
            swings[i]->setBounds (swingBounds);
            humanizes[i]->setBounds (humanizeBounds);
            delays[i]->setBounds (delayBounds);
        }
        notesColumn.removeFromTop (2);
    }
    if (lastLaneEditable)
    {
        laneLabel.setBounds (notesColumn.removeFromTop (14));
        laneBox.setBounds (notesColumn.removeFromTop (24));
        notesColumn.removeFromTop (6);
    }
    // ALL LENGTHS-ratten tar plats bara när den syns (annars hamnar FILL långt ner).
    const auto knobShown = shownLayer >= 0 && shownLayer < (int) lengths.size() && lengths[(size_t) shownLayer]->isVisible();
    auto knobRow = notesColumn.removeFromTop (knobShown ? 66 : 0);
    for (auto& lengthKnob : lengths)
        lengthKnob->setBounds (knobRow.withSizeKeepingCentre (knobRow.getWidth() * 3 / 4, knobRow.getHeight()));
    notesColumn.removeFromTop (knobShown ? 3 : 0);
    const auto gatingNow = instrument && mode == StepSequencer::Mode::gate;
    if (! gatingNow)
    {
        presets.setBounds (notesColumn.removeFromTop (24));
        notesColumn.removeFromTop (4);
    }
    fillButton.setBounds (notesColumn.removeFromTop (24));
    notesColumn.removeFromTop (4);
    pianoButton.setBounds (notesColumn.removeFromTop (30));

    // GATE: på/av, gatens rutnät, djup och mjukhet, FILL.
    auto gateColumn = column;
    for (auto& button : gateOnButtons)
        button->setBounds (gateColumn.getX(), gateColumn.getY(), gateColumn.getWidth(), 26);
    gateColumn.removeFromTop (30);
    for (auto& grid : gateGrids)
        grid->setBounds (gateColumn.getX(), gateColumn.getY(), gateColumn.getWidth(), 40);
    gateColumn.removeFromTop (44);
    auto gateKnobs = gateColumn.removeFromTop (72);
    const auto half = gateKnobs.getWidth() / 2;
    for (auto& depth : gateDepths)
        depth->setBounds (gateKnobs.withWidth (half));
    for (auto& smooth : gateSmooths)
        smooth->setBounds (gateKnobs.withTrimmedLeft (half));
    gateColumn.removeFromTop (4);
    if (gatingNow)
    {
        presets.setBounds (gateColumn.removeFromTop (24));
        gateColumn.removeFromTop (4);
    }
    gateFillButton.setBounds (gateColumn.removeFromTop (24));

    // Ramen runt allt som hör till det valda instrumentet (i dess färg, som ramen runt dess rad i mönstret).
    auto bottom = headingBounds.getBottom();
    for (auto* component : std::initializer_list<juce::Component*> { &presets, &pianoButton, &fillButton, &gateFillButton })
        if (component->isVisible())
            bottom = std::max (bottom, component->getBottom());
    for (auto& depth : gateDepths)
        if (depth->isVisible())
            bottom = std::max (bottom, depth->getBottom());
    blockFrame = juce::Rectangle<int>::leftTopRightBottom (0, blockTop, 134, bottom + 8);

    sequencer.setBounds (area);
    if (popWindow == nullptr)
        pianoRoll.setBounds (area);
}
