#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// Knapp som visar delens nuvarande preset och öppnar en meny med fabrikspresets, egna
// presets, "Save preset..." och "Open preset folder".
class PresetButton : public juce::Component, private juce::Timer
{
public:
    PresetButton (PluginBase& processor, Presets::Section section, juce::Colour accent);
    ~PresetButton() override;

    void resized() override { button.setBounds (getLocalBounds()); }

private:
    void timerCallback() override { updateText(); }
    void updateText();
    void showMenu();
    void askForNameAndSave();

    PluginBase& processor;
    const Presets::Section section;
    juce::TextButton button;

    // Spara-dialogen lånar editorns LookAndFeel och måste stängas om editorn stängs.
    juce::Component::SafePointer<juce::AlertWindow> saveDialog;
};
