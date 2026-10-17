#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// Knapp som visar delens nuvarande preset och öppnar en meny med fabrikspresets, egna
// presets, "Save preset..." och "Open preset folder".
class PresetButton : public juce::Component, private juce::Timer
{
public:
    // `layer`: vilket baslager presetet gäller (bara Bass).
    PresetButton (PluginBase& processor, Presets::Section section, juce::Colour accent, int layer = 0);
    ~PresetButton() override;

    void resized() override { button.setBounds (getLocalBounds()); }

    // För mönster-presets: knappen gäller den valda raden. `emptyLabel` är texten när inget preset är laddat (t.ex. "Pattern preset").
    void setLayer (int newLayer, juce::Colour accent);
    void setEmptyLabel (const juce::String& text) { emptyLabel = text; updateText(); }
    void setTooltip (const juce::String& text) { button.setTooltip (text); }

private:
    void timerCallback() override { updateText(); }
    void updateText();
    void showMenu();
    void askForNameAndSave();

    PluginBase& processor;
    const Presets::Section section;
    int layer;
    juce::String emptyLabel { "Preset" };
    juce::TextButton button;

    // Spara-dialogen lånar editorns LookAndFeel och måste stängas om editorn stängs.
    juce::Component::SafePointer<juce::AlertWindow> saveDialog;
};
