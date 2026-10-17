#include "SettingsPage.h"

namespace
{
    void styleLabel (juce::Label& label, float size, juce::Colour colour, bool bold = false)
    {
        label.setFont (Theme::font (size, bold));
        label.setColour (juce::Label::textColourId, colour);
        label.setMinimumHorizontalScale (0.7f);
    }
}

SettingsPage::SettingsPage (PluginBase& p) : processor (p)
{
    // ---- FÖNSTER
    styleLabel (scaleLabel, 12.0f, Theme::text, true);
    scaleLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (scaleLabel);
    scaleSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    scaleSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    scaleSlider.setRange (50.0, 160.0, 1.0);
    scaleSlider.setColour (juce::Slider::trackColourId, Theme::bass);
    scaleSlider.setColour (juce::Slider::thumbColourId, Theme::text);
    scaleSlider.setTooltip ("WINDOW SIZE - Makes the whole plugin window (and everything in it) bigger or smaller, from 50 % to 160 %. Use a small value on a small screen. "
                            "You can also drag the corner of the window. It applies to every Kickbase window and is remembered.");
    scaleSlider.onValueChange = [this] { scaleLabel.setText (juce::String (juce::roundToInt (scaleSlider.getValue())) + " %", juce::dontSendNotification); };
    scaleSlider.onDragEnd = [this] { processor.requestEditorScale ((float) scaleSlider.getValue() / 100.0f); };
    addAndMakeVisible (scaleSlider);

    const char* names[] { "SMALL", "MEDIUM", "LARGE", "HUGE" };
    const float scales[] { 0.65f, 0.9f, 1.15f, 1.4f };
    for (size_t i = 0; i < sizeButtons.size(); ++i)
    {
        sizeButtons[i].setButtonText (names[i]);
        sizeButtons[i].setTooltip ("Window size " + juce::String (juce::roundToInt (scales[i] * 100.0f)) + " %");
        sizeButtons[i].onClick = [this, scale = scales[i]]
        {
            processor.requestEditorScale (scale);
            scaleSlider.setValue (scale * 100.0, juce::dontSendNotification);
            scaleLabel.setText (juce::String (juce::roundToInt (scale * 100.0f)) + " %", juce::dontSendNotification);
        };
        addAndMakeVisible (sizeButtons[i]);
    }
    reduceAnimationsToggle.setClickingTogglesState (true);
    reduceAnimationsToggle.setToggleState (Settings::reduceAnimations(), juce::dontSendNotification);
    reduceAnimationsToggle.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    reduceAnimationsToggle.setTooltip ("REDUCE ANIMATIONS - Uses less CPU: the animated views (scopes, meters, playhead, knob rings) update half as often, the logo's glow is off "
                                       "and the live spectrum runs at the slow speed. The header and the spectrum follow at once; the other views when the window is opened again. "
                                       "Applies to every Kickbase window and is remembered.");
    reduceAnimationsToggle.onClick = [this] { Settings::setReduceAnimations (reduceAnimationsToggle.getToggleState()); };
    addAndMakeVisible (reduceAnimationsToggle);
    styleLabel (windowHint, 10.5f, Theme::textDim);
    windowHint.setText ("Drag the window corner for any size. Applies to all Kickbase windows.", juce::dontSendNotification);
    windowHint.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (windowHint);

    // ---- AI-LÄNKEN
    aiToggle.setClickingTogglesState (true);
    aiToggle.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    aiToggle.setTooltip ("AI LINK - Lets an AI assistant (through the Kickbase MCP bridge, for example in Claude Desktop) read and change this plugin. "
                         "Local only: nothing is sent over the internet by Kickbase. The same switch as the AI button in the top bar.");
    aiToggle.onClick = [this] { setBridge (aiToggle.getToggleState()); };
    addAndMakeVisible (aiToggle);
    styleLabel (aiStatus, 11.0f, Theme::textDim);
    addAndMakeVisible (aiStatus);

    styleLabel (keyLabel, 11.0f, Theme::text, true);
    keyLabel.setText ("ACCESS KEY (optional)", juce::dontSendNotification);
    addAndMakeVisible (keyLabel);
    keyEditor.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    keyEditor.setText (Settings::aiKey(), false);
    keyEditor.setTextToShowWhenEmpty ("no key: any local AI app may connect", Theme::textDim);
    keyEditor.setColour (juce::TextEditor::backgroundColourId, Theme::knobBodyDark);
    keyEditor.setColour (juce::TextEditor::textColourId, Theme::text);
    keyEditor.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    keyEditor.setTooltip ("ACCESS KEY - Your own password for the AI link. When one is set, Kickbase only answers an AI bridge that has the same key "
                          "(set it as KICKBASE_KEY for the bridge, or run install-mcp.ps1 -Key <key>). Leave it empty to allow any local AI app.");
    auto saveKey = [this] { Settings::set ("aiKey", keyEditor.getText().trim()); };
    keyEditor.onReturnKey = saveKey;
    keyEditor.onFocusLost = saveKey;
    addAndMakeVisible (keyEditor);

    showKeyButton.setClickingTogglesState (true);
    showKeyButton.setTooltip ("Show or hide the key");
    showKeyButton.onClick = [this] { keyEditor.setPasswordCharacter (showKeyButton.getToggleState() ? 0 : (juce::juce_wchar) 0x2022); keyEditor.repaint(); };
    addAndMakeVisible (showKeyButton);
    generateButton.setTooltip ("Makes a new random key");
    generateButton.onClick = [this]
    {
        keyEditor.setText (Settings::makeKey(), false);
        Settings::set ("aiKey", keyEditor.getText());
    };
    addAndMakeVisible (generateButton);
    copyButton.setTooltip ("Copies the key, so you can paste it into the AI app's setup");
    copyButton.onClick = [this] { juce::SystemClipboard::copyTextToClipboard (keyEditor.getText()); };
    addAndMakeVisible (copyButton);
    clearKeyButton.setTooltip ("Removes the key (any local AI app may connect again)");
    clearKeyButton.onClick = [this] { keyEditor.setText ({}, false); Settings::set ("aiKey", {}); };
    addAndMakeVisible (clearKeyButton);

    styleLabel (aiHint, 10.5f, Theme::textDim);
    aiHint.setJustificationType (juce::Justification::topLeft);
    aiHint.setText ("With a key, only an AI bridge that has the same key can connect: set KICKBASE_KEY for the bridge, or run install-mcp.ps1 -Key <key>. To chat with your own AI inside the synth, use the AI tab (top) and its KEY button.",
                    juce::dontSendNotification);
    addAndMakeVisible (aiHint);

    // ---- MAPPAR
    const std::array<std::pair<const char*, const char*>, 3> specs { { { "presetsFolder", "PRESETS" }, { "exportsFolder", "EXPORTS" }, { "wavetablesFolder", "WAVETABLES" } } };
    for (size_t i = 0; i < folders.size(); ++i)
    {
        auto& row = folders[i];
        row.key = specs[i].first;
        row.title = specs[i].second;
        styleLabel (row.name, 11.0f, Theme::text, true);
        row.name.setText (row.title, juce::dontSendNotification);
        styleLabel (row.path, 10.5f, Theme::textDim);
        row.path.setMinimumHorizontalScale (0.6f);
        row.change.setTooltip ("Choose another " + row.title.toLowerCase() + " folder. Files already in the old folder are not moved.");
        row.change.onClick = [this, &row] { chooseFolder (row); };
        row.reset.setTooltip ("Back to the standard folder (Documents\\Kickbase\\...)");
        row.reset.onClick = [this, &row] { Settings::setFolder (row.key, {}); refresh (true); };
        row.open.setTooltip ("Open the folder");
        row.open.onClick = [&row]
        {
            const auto folder = row.key == "presetsFolder" ? Settings::presetsFolder() : row.key == "exportsFolder" ? Settings::exportsFolder() : Settings::wavetablesFolder();
            folder.createDirectory();
            folder.startAsProcess();
        };
        for (auto* component : std::initializer_list<juce::Component*> { &row.name, &row.path, &row.change, &row.reset, &row.open })
            addAndMakeVisible (component);
    }
    styleLabel (folderHint, 10.5f, Theme::textDim);
    folderHint.setJustificationType (juce::Justification::topLeft);
    folderHint.setText ("Presets: your saved sounds and piano rolls. Exports: WAV and MIDI files. Wavetables: your own waves (and the AKWF pack).", juce::dontSendNotification);
    addAndMakeVisible (folderHint);

    // ---- VÅGFORMSPAKETET
    styleLabel (packStatus, 11.0f, Theme::text);
    packStatus.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (packStatus);
    styleLabel (packCount, 11.0f, Theme::textDim, true);
    addAndMakeVisible (packCount);
    styleLabel (packHint, 10.5f, Theme::textDim);
    packHint.setJustificationType (juce::Justification::topLeft);
    packHint.setText ("Over 4000 single-cycle waveforms (CC0). One download of about 20 MB from GitHub, only when you press the button.", juce::dontSendNotification);
    addAndMakeVisible (packHint);

    installButton.setTooltip ("INSTALL - Downloads the Adventure Kid Waveforms (AKWF) pack, over 4000 free single-cycle waves (CC0 / public domain), into your wavetables folder. "
                              "They then appear under LIBRARY in every oscillator's TABLE menu. This is the only thing Kickbase downloads, and only when you press this button.");
    installButton.onClick = [this] { pack.install(); updatePack(); };
    addAndMakeVisible (installButton);
    cancelButton.onClick = [this] { pack.cancel(); };
    addAndMakeVisible (cancelButton);
    packFolderButton.onClick = [this] { WavetablePack::packFolder().getParentDirectory().createDirectory(); WavetablePack::packFolder().getParentDirectory().startAsProcess(); };
    addAndMakeVisible (packFolderButton);
    progressBar.setPercentageDisplay (false);
    addChildComponent (progressBar);
    pack.onFinished = [safe = juce::Component::SafePointer<SettingsPage> (this)]
    {
        if (safe != nullptr)
        {
            safe->updatePack();
            safe->refresh (true);
        }
    };

    refresh (true);
    startTimerHz (10);
}

SettingsPage::~SettingsPage()
{
    pack.onFinished = nullptr;
    pack.cancel();
}

void SettingsPage::setBridge (bool on)
{
    processor.setBridgeEnabled (on);
    aiToggle.setToggleState (processor.isBridgeEnabled(), juce::dontSendNotification);
    refresh (false);
}

void SettingsPage::chooseFolder (FolderRow& row)
{
    const auto current = row.key == "presetsFolder" ? Settings::presetsFolder() : row.key == "exportsFolder" ? Settings::exportsFolder() : Settings::wavetablesFolder();
    current.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Choose the " + row.title.toLowerCase() + " folder", current);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this, key = row.key] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() != juce::File() && fc.getResult().isDirectory())
                                  Settings::setFolder (key, fc.getResult());
                              refresh (true);
                          });
}

void SettingsPage::updatePack()
{
    const auto busy = pack.isBusy();
    installButton.setEnabled (! busy);
    cancelButton.setEnabled (busy);
    progressBar.setVisible (busy);
    packProgress = (double) pack.getProgress();
    const auto status = pack.getStatus();
    packStatus.setText (status.isNotEmpty() ? status : juce::String (WavetablePack::installedCount() > 0 ? "The pack is installed." : "The pack is not installed."),
                        juce::dontSendNotification);
}

void SettingsPage::refresh (bool includeFiles)
{
    if (! scaleSlider.isMouseButtonDown())
    {
        const auto scale = processor.editorScale > 0.0f ? processor.editorScale : (Settings::uiScale() > 0.0f ? Settings::uiScale() : 0.9f);
        if (std::abs (scaleSlider.getValue() - scale * 100.0) > 0.6)
            scaleSlider.setValue (scale * 100.0, juce::dontSendNotification);
        scaleLabel.setText (juce::String (juce::roundToInt (scaleSlider.getValue())) + " %", juce::dontSendNotification);
    }

    aiToggle.setToggleState (processor.isBridgeEnabled(), juce::dontSendNotification);
    aiToggle.setButtonText (processor.isBridgeEnabled() ? "AI LINK: ON" : "AI LINK: OFF");
    aiStatus.setText (processor.isBridgeEnabled() ? "Instance " + processor.getBridgeId() + ", " + juce::String (processor.getBridgeConnections()) + " connected"
                                                      + (Settings::aiKey().isNotEmpty() ? " (key on)" : "")
                                                  : "Off",
                      juce::dontSendNotification);

    if (includeFiles)
    {
        for (auto& row : folders)
        {
            const auto folder = row.key == "presetsFolder" ? Settings::presetsFolder() : row.key == "exportsFolder" ? Settings::exportsFolder() : Settings::wavetablesFolder();
            const auto isDefault = Settings::get (row.key).isEmpty();
            row.path.setText (folder.getFullPathName() + (isDefault ? "  (standard)" : ""), juce::dontSendNotification);
            row.path.setTooltip (folder.getFullPathName());
            row.reset.setEnabled (! isDefault);
        }
        const auto count = WavetablePack::installedCount();
        packCount.setText (count > 0 ? juce::String (count) + " waveforms installed" : "Not installed", juce::dontSendNotification);
    }
    updatePack();
}

void SettingsPage::timerCallback()
{
    refresh (++tick % 20 == 0);
}

void SettingsPage::paint (juce::Graphics& g)
{
    auto frame = [&g] (juce::Rectangle<int> area, const juce::String& title)
    {
        const auto bounds = area.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xdd121420), bounds.getX(), bounds.getY(), juce::Colour (0xdd181b28), bounds.getX(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour (0xff24283c));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Theme::bass.withAlpha (0.92f));
        g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.08f));
        g.drawText (title, area.withHeight (16).withTrimmedLeft (8), juce::Justification::centredLeft);
    };
    frame (windowBox, "WINDOW SIZE");
    frame (aiBox, "AI LINK");
    frame (folderBox, "FOLDERS");
    frame (packBox, "WAVETABLE PACK");
}

void SettingsPage::resized()
{
    auto area = getLocalBounds();
    windowBox = area.removeFromLeft (230);
    area.removeFromLeft (10);
    aiBox = area.removeFromLeft (330);
    area.removeFromLeft (10);
    folderBox = area.removeFromLeft (360);
    area.removeFromLeft (10);
    packBox = area;

    // WINDOW SIZE
    {
        auto box = windowBox.reduced (10, 0).withTrimmedTop (20);
        auto top = box.removeFromTop (26);
        scaleLabel.setBounds (top.removeFromRight (50));
        scaleSlider.setBounds (top);
        box.removeFromTop (6);
        auto buttons = box.removeFromTop (26);
        const auto width = buttons.getWidth() / (int) sizeButtons.size();
        for (auto& button : sizeButtons)
            button.setBounds (buttons.removeFromLeft (width).reduced (2, 0));
        box.removeFromTop (8);
        reduceAnimationsToggle.setBounds (box.removeFromTop (24));
        box.removeFromTop (6);
        windowHint.setBounds (box);
    }

    // AI LINK
    {
        auto box = aiBox.reduced (10, 0).withTrimmedTop (20);
        auto top = box.removeFromTop (26);
        aiToggle.setBounds (top.removeFromLeft (118));
        top.removeFromLeft (8);
        aiStatus.setBounds (top);
        box.removeFromTop (6);
        keyLabel.setBounds (box.removeFromTop (16));
        auto keyRow = box.removeFromTop (26);
        showKeyButton.setBounds (keyRow.removeFromRight (52));
        keyRow.removeFromRight (4);
        keyEditor.setBounds (keyRow);
        box.removeFromTop (4);
        auto keyButtons = box.removeFromTop (24);
        generateButton.setBounds (keyButtons.removeFromLeft (92));
        keyButtons.removeFromLeft (4);
        copyButton.setBounds (keyButtons.removeFromLeft (62));
        keyButtons.removeFromLeft (4);
        clearKeyButton.setBounds (keyButtons.removeFromLeft (62));
        box.removeFromTop (6);
        aiHint.setBounds (box);
    }

    // FOLDERS
    {
        auto box = folderBox.reduced (10, 0).withTrimmedTop (20);
        for (auto& row : folders)
        {
            auto line = box.removeFromTop (24);
            row.name.setBounds (line.removeFromLeft (110));
            row.open.setBounds (line.removeFromRight (42));
            line.removeFromRight (3);
            row.reset.setBounds (line.removeFromRight (48));
            line.removeFromRight (3);
            row.change.setBounds (line.removeFromRight (62));
            row.path.setBounds (box.removeFromTop (16));
            box.removeFromTop (6);
        }
        box.removeFromTop (4);
        folderHint.setBounds (box);
    }

    // WAVETABLE PACK
    {
        auto box = packBox.reduced (10, 0).withTrimmedTop (20);
        packCount.setBounds (box.removeFromTop (18));
        packStatus.setBounds (box.removeFromTop (32));
        progressBar.setBounds (box.removeFromTop (14));
        box.removeFromTop (6);
        auto buttons = box.removeFromTop (26);
        cancelButton.setBounds (buttons.removeFromRight (62));
        buttons.removeFromRight (4);
        installButton.setBounds (buttons);
        box.removeFromTop (4);
        packFolderButton.setBounds (box.removeFromTop (24));
        box.removeFromTop (4);
        packHint.setBounds (box);
    }
}
