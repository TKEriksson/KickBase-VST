#include "PresetButton.h"

namespace
{
    enum MenuIds
    {
        factoryBase = 1,
        userBase = 1000,
        saveId = 5000,
        openFolderId = 5001,
        nothingId = 5002
    };

    juce::String displayName (const PluginBase& processor, Presets::Section section)
    {
        if (section == Presets::Section::bass)
            return processor.isEffectVersion() ? "Gate" : "Instrument";
        if (section == Presets::Section::master)
            return "Maximizer";
        return Presets::sectionName (section);
    }

    const juce::String arrow = juce::String::fromUTF8 ("  \xe2\x96\xbe"); // "  " + liten pil nedåt
}

PresetButton::PresetButton (PluginBase& p, Presets::Section sectionIn, juce::Colour accent, int layerIn)
    : processor (p), section (sectionIn), layer (layerIn)
{
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    button.setTooltip (displayName (processor, section) + " presets");
    button.onClick = [this] { showMenu(); };
    addAndMakeVisible (button);

    updateText();
    startTimerHz (4); // projekt och presets kan laddas medan fönstret är öppet
}

void PresetButton::setLayer (int newLayer, juce::Colour accent)
{
    layer = newLayer;
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    updateText();
}

PresetButton::~PresetButton()
{
    if (saveDialog != nullptr)
    {
        saveDialog->setLookAndFeel (nullptr);
        saveDialog->exitModalState (0);
    }
}

void PresetButton::updateText()
{
    const auto name = processor.getPresetName (section, layer);
    const auto text = (name.isEmpty() || (emptyLabel != "Preset" && name == "Default") ? emptyLabel : name) + arrow;
    if (button.getButtonText() != text)
        button.setButtonText (text);
}

void PresetButton::showMenu()
{
    const auto effectVersion = processor.isEffectVersion();
    const auto current = processor.getPresetName (section, layer);
    const auto userPresets = processor.getUserPresets (section, layer);

    juce::PopupMenu menu;
    // Mönster: bara de man sparat själv. Ett sparat mönster innehåller alla rader (kick, instrument, pianorullar, längder, GATE och groove), så ingen
    // rad råkar bli kvar eller tömd. Färdiga rytmer för en rad finns i FILL.
    const auto patterns = section == Presets::Section::pattern;
    const auto channelPresets = section == Presets::Section::effects;
    if (! patterns && ! channelPresets)
    {
        menu.addSectionHeader ("FACTORY");
        for (int i = 0; i < Presets::factoryCount (section, effectVersion); ++i)
        {
            const auto name = Presets::factoryName (section, effectVersion, i);
            menu.addItem (factoryBase + i, name, true, name == current);
        }
    }

    if (patterns || channelPresets)
    {
        menu.addSectionHeader (patterns ? "PATTERN PRESETS - " + (layer == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (layer))
                                        : "FX PRESETS - " + Effects::channelLabel (layer));
        if (userPresets.isEmpty())
            menu.addItem (nothingId, "(none saved yet)", false);
    }
    if (! userPresets.isEmpty())
    {
        if (! patterns && ! channelPresets)
            menu.addSectionHeader ("USER");
        for (int i = 0; i < userPresets.size(); ++i)
        {
            const auto name = userPresets[i].getFileNameWithoutExtension();
            menu.addItem (userBase + i, name, true, name == current);
        }
    }

    menu.addSeparator();
    menu.addItem (saveId, "Save preset...");
    menu.addItem (openFolderId, "Open preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                        [this, userPresets] (int result)
    {
        if (result >= userBase && result < userBase + userPresets.size())
            loadFile (userPresets[result - userBase]);
        else if (result >= factoryBase && result < userBase)
            processor.loadFactoryPreset (section, result - factoryBase, layer);
        else if (result == saveId)
            askForNameAndSave();
        else if (result == openFolderId)
        {
            const auto folder = Presets::folder (section, processor.isEffectVersion());
            folder.createDirectory();
            folder.startAsProcess();
        }

        updateText();
    });
}

void PresetButton::loadFile (const juce::File& file)
{
    juce::Component::SafePointer<PresetButton> safeThis (this);

    if (section == Presets::Section::bass || section == Presets::Section::kick)
    {
        // Ett instrument (eller en kick) kan ha en länkad effekt: den läggs på första lediga FX-kanal.
        const auto link = PluginBase::readPresetLink (file);
        askFxChannel (link.kind == "effect" ? link.name : juce::String(), [safeThis, file] (int fxChannel)
        {
            if (safeThis == nullptr)
                return;
            safeThis->processor.loadInstrumentPreset (safeThis->section, safeThis->layer, file, fxChannel);
            safeThis->updateText();
        });
        return;
    }

    if (section == Presets::Section::pattern)
    {
        loadPatternFile (file);
        return;
    }

    if (section != Presets::Section::effects || ! processor.fxChannelHasEffects (layer))
    {
        processor.loadUserPreset (section, file, layer);
        return;
    }

    // Ett FX-preset ersätter kanalens effekter, EQ och kompressor: fråga först om något finns att förlora.
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Load FX preset")
                                      .withMessage ("Loading \"" + file.getFileNameWithoutExtension() + "\" replaces the effects, EQ and compressor of " + Effects::channelLabel (layer)
                                                    + ". The other channels are not touched. Continue?")
                                      .withButton ("Replace").withButton ("Cancel"),
                                  [safeThis, file] (int choice)
    {
        if (safeThis == nullptr || choice != 1)
            return;
        safeThis->processor.loadUserPreset (safeThis->section, file, safeThis->layer);
        safeThis->updateText();
    });
}

void PresetButton::loadPatternFile (const juce::File& file)
{
    juce::Component::SafePointer<PresetButton> safeThis (this);
    const auto kickRow = layer == Pattern::kickLayer;
    const auto rowName = kickRow ? juce::String ("KICK") : processor.getLayerName (layer);
    const auto presetName = file.getFileNameWithoutExtension();
    const auto link = PluginBase::readPresetLink (file);
    const auto instrumentFile = link.valid() ? processor.findUserPreset (kickRow ? Presets::Section::kick : Presets::Section::bass, link.name) : juce::File();

    if (instrumentFile.existsAsFile())
    {
        // Presetet hör ihop med ett instrument: fråga om det ska ersätta det som finns, så att inget instrument försvinner av misstag.
        const auto what = kickRow ? juce::String ("kick") : juce::String ("instrument");
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Load pattern preset")
                                          .withMessage ("\"" + presetName + "\" comes with the " + what + " preset \"" + link.name + "\". Replace the " + what + " of " + rowName
                                                        + " with it? The notes, piano roll and GATE of " + rowName + " are replaced either way. The other rows are not touched.")
                                          .withButton ("Replace the " + what).withButton ("Keep my " + what).withButton ("Cancel"),
                                      [safeThis, file, instrumentFile] (int choice)
        {
            if (safeThis == nullptr || choice == 0)
                return;
            if (choice == 2)
            {
                safeThis->processor.loadPatternPreset (safeThis->layer, file, false);
                safeThis->updateText();
                return;
            }
            const auto effect = PluginBase::readPresetLink (instrumentFile);
            safeThis->askFxChannel (effect.kind == "effect" ? effect.name : juce::String(), [safeThis, file] (int fxChannel)
            {
                if (safeThis == nullptr)
                    return;
                safeThis->processor.loadPatternPreset (safeThis->layer, file, true, fxChannel);
                safeThis->updateText();
            });
        });
        return;
    }

    auto hasNotes = false;
    processor.pattern.forEachNote (layer, [&hasNotes] (int, const Pattern::Note&) { hasNotes = true; });
    const auto missing = link.valid(); // länkat instrument finns inte längre
    if (! hasNotes && ! missing)
    {
        processor.loadUserPreset (section, file, layer);
        return;
    }

    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Load pattern preset")
                                      .withMessage ("Loading \"" + presetName + "\" replaces the notes, piano roll and GATE of " + rowName + ". The other rows are not touched."
                                                    + (missing ? juce::String (" The " + juce::String (kickRow ? "kick" : "instrument") + " preset \"" + link.name + "\" it was saved with no longer exists, so only the pattern is loaded.")
                                                               : juce::String()) + " Continue?")
                                      .withButton ("Replace").withButton ("Cancel"),
                                  [safeThis, file] (int choice)
    {
        if (safeThis == nullptr || choice != 1)
            return;
        safeThis->processor.loadPatternPreset (safeThis->layer, file, false);
        safeThis->updateText();
    });
}

void PresetButton::askFxChannel (const juce::String& fxName, std::function<void (int)> done)
{
    if (fxName.isEmpty())
    {
        done (-1);
        return;
    }
    if (! processor.findUserPreset (Presets::Section::effects, fxName).existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Linked effect missing",
                                                "This preset was saved with the FX preset \"" + fxName + "\", which no longer exists. It is loaded without the effect.",
                                                "OK", nullptr, juce::ModalCallbackFunction::create ([done] (int) { done (-1); }));
        return;
    }

    const auto freeChannel = processor.firstFreeFxChannel();
    if (freeChannel >= 0)
    {
        done (freeChannel);
        return;
    }

    // Alla FX-kanaler används: välj vilken som ersätts, ladda utan effekten, eller avbryt (stäng menyn).
    juce::PopupMenu menu;
    menu.addSectionHeader ("No free FX channel for \"" + fxName + "\"");
    for (int channel = 0; channel < numEffectChannels; ++channel)
    {
        const auto name = processor.getPresetName (Presets::Section::effects, channel);
        menu.addItem (1 + channel, "Replace " + Effects::channelLabel (channel) + (name.isEmpty() ? juce::String() : " (" + name + ")"));
    }
    menu.addSeparator();
    menu.addItem (100, "Load without the effect");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button), [done] (int result)
    {
        if (result >= 1 && result <= numEffectChannels)
            done (result - 1);
        else if (result == 100)
            done (-1);
    });
}

void PresetButton::askForNameAndSave()
{
    auto* window = new juce::AlertWindow ("Save " + displayName (processor, section) + " preset",
                                          "Name:", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    saveDialog = window;

    const auto current = processor.getPresetName (section, layer);
    window->addTextEditor ("name", current.isEmpty() ? juce::String ("My preset") : current);

    // Länkar: ett instrument-/kick-preset kan ta med sin effekt (sparas som FX-preset), ett mönster-preset sitt instrument (sparas som instrument-preset).
    const auto instrumentPreset = section == Presets::Section::bass || section == Presets::Section::kick;
    const auto patternRow = section == Presets::Section::pattern;
    const auto kickRow = layer == Pattern::kickLayer;
    const auto sourceSection = patternRow ? (kickRow ? Presets::Section::kick : Presets::Section::bass) : section;
    const auto sourceLayer = patternRow && kickRow ? 0 : layer;
    const auto effectChannel = instrumentPreset || patternRow ? processor.sendChannelOf (sourceSection, sourceLayer) : -1;
    const auto effectLinkable = effectChannel >= 0 && processor.fxChannelHasEffects (effectChannel);

    auto baseName = current.isEmpty() || current == "Default" ? juce::String ("My preset") : current;
    if (patternRow)
    {
        const auto instrumentName = processor.getPresetName (sourceSection, sourceLayer);
        baseName = instrumentName.isEmpty() || instrumentName == "Default" ? (kickRow ? juce::String ("My kick") : processor.getLayerName (layer)) : instrumentName;
        window->addComboBox ("link", { "Pattern only", kickRow ? "Also include the kick (saved as a kick preset)" : "Also include the instrument (saved as an instrument preset)" }, "Include:");
        window->addTextEditor ("instrument", baseName, kickRow ? "Kick preset name:" : "Instrument preset name:");
    }
    else if (effectLinkable)
    {
        window->addComboBox ("link", { "Preset only", "Also save its effect (" + Effects::channelLabel (effectChannel) + ") as an FX preset" }, "Include:");
    }
    if (effectLinkable)
    {
        const auto fxName = processor.getPresetName (Presets::Section::effects, effectChannel);
        window->addTextEditor ("fx", fxName.isNotEmpty() ? fxName : baseName + " FX", "FX preset name:");
    }
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetButton> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;

        const auto name = window->getTextEditorContents ("name");
        const auto* link = window->getComboBoxComponent ("link");
        const auto withLink = link != nullptr && link->getSelectedItemIndex() == 1;
        const auto fxName = withLink && window->getTextEditor ("fx") != nullptr ? window->getTextEditorContents ("fx") : juce::String();
        auto saved = false;
        if (safeThis->section == Presets::Section::bass || safeThis->section == Presets::Section::kick)
            saved = safeThis->processor.saveInstrumentPreset (safeThis->section, safeThis->layer, name, fxName);
        else if (safeThis->section == Presets::Section::pattern)
            saved = safeThis->processor.savePatternPreset (safeThis->layer, name, withLink ? window->getTextEditorContents ("instrument") : juce::String(), fxName);
        else
            saved = safeThis->processor.saveUserPreset (safeThis->section, name, safeThis->layer);
        if (! saved)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save preset",
                                                    "Check that the name is valid: " + name);
        safeThis->updateText();
    }), true);
}
