#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "PresetButton.h"

// Lodrät mätare i dB.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    // `read` hämtar värdet i dB; `fromTop` = förstärkningsminskning (växer nedåt från 0).
    LevelMeter (std::function<float()> read, float minDb, float maxDb, bool fromTop, juce::Colour colour, juce::String label);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    std::function<float()> read;
    float minDb, maxDb;
    bool fromTop;
    juce::Colour colour;
    juce::String label;
};

// Fliken MASTER: drive, clip, tak, release och mätare.
class MasterPage : public juce::Component
{
public:
    explicit MasterPage (PluginBase& processor);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PresetButton presets;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    LevelMeter output, reduction;
};
