#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"

// Bilden av en wavetable, som i Serum: alla vågformer staplade bakåt i en svag 3D-vy, och den
// som spelas nu (positionen, med LFO och warp) ritad framför i lagrets färg.
class WavetableView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    WavetableView (PluginBase& processor, int layer, int osc, juce::Colour accent);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float raw (BassField field) const;

    PluginBase& processor;
    const int layer, osc;
    juce::Colour accent;
    int positionIndex = -1;
    float lastPosition = -1.0f, lastWarp = -1.0f;
    int lastTable = -1, lastWarpMode = -1;
};

// En oscillator (OSC A eller OSC B) i ett baslager: tabell, warp, bilden och rattarna.
class OscPanel : public juce::Component
{
public:
    OscPanel (PluginBase& processor, int layer, int osc, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const int osc;
    juce::Colour accent;
    ChoiceComponent table, warp;
    WavetableView view;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
};
