#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "PresetButton.h"

// Visar en LFO:s form och var den är just nu.
class LfoDisplay : public juce::Component, private juce::Timer
{
public:
    LfoDisplay (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    PluginBase& processor;
    const int index;
};

// En LFO: på/av, form, hastighet och två mål med mängd.
class LfoPanel : public juce::Component
{
public:
    LfoPanel (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const int index;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    ChoiceComponent shape, rate;
    LfoDisplay display;
    std::vector<std::unique_ptr<ChoiceComponent>> destinations;
    std::vector<std::unique_ptr<KnobComponent>> amounts;
};

// Fliken LFO.
class LfoPage : public juce::Component
{
public:
    explicit LfoPage (PluginBase& processor);
    void resized() override;

private:
    PresetButton presets;
    LfoPanel first, second;
};
