#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "Cursors.h"

// Bilden av en wavetable, som i Serum: alla vågformer staplade bakåt i en svag 3D-vy, och den
// som spelas nu (positionen, med LFO och warp) ritad framför i lagrets färg.
class WavetableView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    WavetableView (PluginBase& processor, int layer, int osc, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    void timerCallback() override;
    float raw (BassField field) const;

    bool hover = false;
    juce::MouseCursor speakerCursor = Cursors::speaker();

    PluginBase& processor;
    const int layer, osc;
    juce::Colour accent;
    int positionIndex = -1;
    float lastPosition = -1.0f, lastWarp = -1.0f;
    int lastTable = -1, lastWarpMode = -1;
};

// En oscillator (OSC 1, 2 eller 3) i ett baslager: tabell, warp, bilden och rattarna.
class OscPanel : public juce::Component, private juce::Timer
{
public:
    OscPanel (PluginBase& processor, int layer, int osc, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseTable (int index);
    void showTableMenu();
    void loadFromLibrary (const juce::File& fileOrFolder);
    void chooseWavFile();
    juce::String currentTableName() const;
    std::vector<juce::File> menuFiles; // biblioteksval i menyn (id 1000 +)

    PluginBase& processor;
    const int layer, osc;
    juce::Colour accent;
    ChoiceComponent warp;
    juce::Label tableLabel;
    juce::TextButton tableButton;
    std::array<juce::TextButton, 4> shapeButtons; // SIN, TRI, SAW, SQR: de klassiska vågformerna
    juce::TextButton wavButton { "WAV" };
    std::unique_ptr<juce::FileChooser> chooser;
    int lastTable = -1;
    WavetableView view;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
};
