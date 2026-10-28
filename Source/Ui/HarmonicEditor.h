#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// PH2: övertons- och faseditor för en oscillators wavetable. Stapel per överton (1 = grundtonen): styrka i dB, fas under, och överst
// den levande vågformen. Dra för att ändra (dras musen snabbt fylls staplarna emellan i), dubbelklick nollställer en stapel, dubbelklick
// i vågformen nollställer allt. Grundtonen tas bort genom att dra stapel 1 längst ner.
// Ändringen läggs på en fast utgångspunkt (tabellen när fönstret öppnades) och byggs om 100 ms efter sista ändringen; resultatet blir
// oscillatorns User-tabell. Vågformsvyn räknas direkt på en frame, så den följer musen utan fördröjning.
class HarmonicEditor : public juce::Component, private juce::Timer
{
public:
    HarmonicEditor (PluginBase& processor, int layer, int osc, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int shown = 64; // övertoner som går att ändra; zoomen visar 16, 32 eller 64 av dem

private:
    void timerCallback() override;
    void setColumn (int column, float value);
    void applyBrush (int column, float value);
    void setZoom (int count);
    void applyPreset (int id);
    float valueAt (juce::Point<float> position) const;
    void setFromMouse (juce::Point<float> position);
    void changed();
    void refreshBackdrop();
    void refreshPreview();
    std::vector<WavetableEdit::Edit> currentEdits() const;
    int shownFrame() const;
    juce::Rectangle<float> waveArea() const;
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
    std::array<float, shown> baseAmp {}, basePhase {}; // utgångspunktens amplitud och fas i vald frame (för presets)
    std::array<float, shown> backdrop {}; // utgångspunktens amplitud i vald frame, 0..1 av största
    std::array<float, Wavetable::frameSize> before {}, after {}; // vald frame: utgångspunkt och med ändringen

    enum Brush { draw = 1, line, smooth, random };
    juce::ComboBox frameBox, brushBox, zoomBox, presetBox;
    int visible = shown;     // antal visade övertoner (16 / 32 / 64)
    int seed = 1;            // RANDOM: nytt frö för varje drag, samma frö ger samma värden
    int anchorColumn = 0;    // LINE: där draget började
    float anchorValue = 0.0f;
    std::array<float, shown> gainStart {}, phaseStart {}; // LINE: banorna när draget började
    int lane = 0;           // 0 = styrka, 1 = fas (den som drogs igång)
    int lastColumn = -1;    // förra kolumnen under en dragning, för att fylla i hoppade staplar
    float lastValue = 0.0f; // och dess värde (dB eller grader, ej avrundat)
};
