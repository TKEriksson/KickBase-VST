#pragma once

#include "../Core/InstrumentSynth.h"
#include "../Core/PluginBase.h"
#include "Components.h"

// MIX-fliken i källpanelen: en strip per källa (OSC 1-3, SAMPLE, NOISE, SUB, FM) med nivå och pan (NOISE också bredd; SUB är alltid mono i mitten),
// och till vänster den sammanlagda vågformen av det instrumentet spelar (alla källor genom filter och envelope, några cykler efter anslaget).
class InstrMixPanel : public juce::Component, private juce::Timer
{
public:
    InstrMixPanel (PluginBase& processor, int layer, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Strip
    {
        juce::String name;
        std::unique_ptr<KnobComponent> level, pan, width; // pan och width kan saknas
        juce::Rectangle<int> frame;
    };

    void timerCallback() override;
    void addStrip (const char* name, InstrumentField level, const InstrumentField* pan, const InstrumentField* width, int colourIndex);
    void renderWave();

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    ParamRefs refs;
    std::vector<std::unique_ptr<Strip>> strips;
    std::vector<float> peaks;      // min/max-par per kolumn
    float waveGain = 1.0f;
    juce::Rectangle<int> waveArea;
    juce::String lastKey;
    InstrumentSynth synth;
};
