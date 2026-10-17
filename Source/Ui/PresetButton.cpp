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
        {
            const auto file = userPresets[result - userBase];
            if (section != Presets::Section::pattern && section != Presets::Section::effects)
            {
                processor.loadUserPreset (section, file, layer);
            }
            else
            {
                // Ett mönster-preset ersätter den valda radens toner (pianorulle, GATE och inställningar), ett FX-preset kanalens effekter: fråga först om något finns att förlora.
                auto hasNotes = false;
                if (section == Presets::Section::pattern)
                {
                    for (int tick = 0; tick < Pattern::maxTicks && ! hasNotes; ++tick)
                        hasNotes = processor.pattern.note (layer, tick).on.load();
                }
                else
                {
                    for (int slot = 0; slot < slotsPerChannel && ! hasNotes; ++slot)
                        hasNotes = Effects::slotType (processor.apvts, layer, slot) != EffectType::off;
                }
                if (! hasNotes)
                {
                    processor.loadUserPreset (section, file, layer);
                }
                else
                {
                    juce::Component::SafePointer<PresetButton> safeThis (this);
                    const auto rowName = layer == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (layer);
                    const auto message = section == Presets::Section::pattern
                        ? "Loading \"" + file.getFileNameWithoutExtension() + "\" replaces the notes, piano roll and GATE of " + rowName + ". The other rows are not touched. Continue?"
                        : "Loading \"" + file.getFileNameWithoutExtension() + "\" replaces the effects, EQ and compressor of " + Effects::channelLabel (layer) + ". The other channels are not touched. Continue?";
                    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                      .withTitle (section == Presets::Section::pattern ? "Load pattern preset" : "Load FX preset")
                                                      .withMessage (message)
                                                      .withButton ("Replace").withButton ("Cancel"),
                                                  [safeThis, file] (int choice)
                    {
                        if (safeThis == nullptr || choice != 1)
                            return;
                        safeThis->processor.loadUserPreset (safeThis->section, file, safeThis->layer);
                        safeThis->updateText();
                    });
                }
            }
        }
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

void PresetButton::askForNameAndSave()
{
    auto* window = new juce::AlertWindow ("Save " + displayName (processor, section) + " preset",
                                          "Name:", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    saveDialog = window;

    const auto current = processor.getPresetName (section, layer);
    window->addTextEditor ("name", current.isEmpty() ? juce::String ("My preset") : current);
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetButton> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;

        const auto name = window->getTextEditorContents ("name");
        if (! safeThis->processor.saveUserPreset (safeThis->section, name, safeThis->layer))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save preset",
                                                    "Check that the name is valid: " + name);
        safeThis->updateText();
    }), true);
}
