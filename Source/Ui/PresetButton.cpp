#include "PresetButton.h"

namespace
{
    enum MenuIds
    {
        factoryBase = 1,
        userBase = 1000,
        saveId = 5000,
        openFolderId = 5001
    };

    const juce::String arrow = juce::String::fromUTF8 ("  \xe2\x96\xbe"); // "  " + liten pil nedåt
}

PresetButton::PresetButton (PluginBase& p, Presets::Section sectionIn, juce::Colour accent, int layerIn)
    : processor (p), section (sectionIn), layer (layerIn)
{
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    button.setTooltip (Presets::sectionName (section) + " presets");
    button.onClick = [this] { showMenu(); };
    addAndMakeVisible (button);

    updateText();
    startTimerHz (4); // projekt och presets kan laddas medan fönstret är öppet
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
    const auto text = (name.isEmpty() ? juce::String ("Preset") : name) + arrow;
    if (button.getButtonText() != text)
        button.setButtonText (text);
}

void PresetButton::showMenu()
{
    const auto effectVersion = processor.isEffectVersion();
    const auto current = processor.getPresetName (section, layer);
    const auto userPresets = processor.getUserPresets (section);

    juce::PopupMenu menu;
    menu.addSectionHeader ("FACTORY");
    for (int i = 0; i < Presets::factoryCount (section, effectVersion); ++i)
    {
        const auto name = Presets::factoryName (section, effectVersion, i);
        menu.addItem (factoryBase + i, name, true, name == current);
    }

    if (! userPresets.isEmpty())
    {
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
            processor.loadUserPreset (section, userPresets[result - userBase], layer);
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
    auto* window = new juce::AlertWindow ("Save " + Presets::sectionName (section) + " preset",
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
