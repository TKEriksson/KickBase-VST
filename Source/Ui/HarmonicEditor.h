#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// PH2: övertons- och faseditor för en oscillators wavetable. Stapel per överton (1 = grundtonen): styrka i dB överst, fas under.
// Dra för att ändra, dubbelklick nollställer en stapel. Ändringen läggs på en fast utgångspunkt (tabellen när fönstret öppnades)
// och byggs om 100 ms efter sista ändringen; resultatet blir oscillatorns User-tabell.
class HarmonicEditor : public juce::Component, private juce::Timer
{
public:
    HarmonicEditor (PluginBase& processor, int layer, int osc, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr int shown = 64; // visade övertoner

private:
    void timerCallback() override;
    void setFromMouse (juce::Point<float> position);
    void changed();
    void refreshBackdrop();
    juce::Rectangle<float> ampLane() const;
    juce::Rectangle<float> phaseLane() const;
    int columnAt (float x) const;

    PluginBase& processor;
    const int layer, osc;
    juce::Colour accent;

    std::vector<float> baseline;
    int steps = 16;
    juce::String tableName;
    bool available = false;

    std::array<float, shown> gainDb {};   // 0 = oförändrad, -120 = borttagen
    std::array<float, shown> phaseDeg {}; // läggs till fasen
    std::array<float, shown> backdrop {}; // utgångspunktens amplitud i vald frame, 0..1 av största

    juce::ComboBox frameBox;
    juce::TextButton zeroFundamental { "No fundamental" }, reset { "Reset" };
    int lane = 0; // 0 = styrka, 1 = fas (den som drogs igång)
};
