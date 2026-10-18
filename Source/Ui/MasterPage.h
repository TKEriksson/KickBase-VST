#pragma once

#include "VisualTimer.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "MixerPage.h"
#include "PresetButton.h"
#include "Scopes.h"

// Lodrät mätare i dB.
class LevelMeter : public juce::Component, private VisualTimer
{
public:
    // `read` hämtar värdet i dB; `fromTop` = förstärkningsminskning (växer nedåt från 0).
    LevelMeter (std::function<float()> read, float minDb, float maxDb, bool fromTop, juce::Colour colour, juce::String label);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { followVisualRate(); repaint(); }

    std::function<float()> read;
    float minDb, maxDb;
    bool fromTop;
    juce::Colour colour;
    juce::String label;
};

// Fliken MASTER: mixern (gain, panorering och mätare per ljudkälla) och maximizern (drive, clip,
// tak, release och mätare).
class MasterPage : public juce::Component
{
public:
    explicit MasterPage (PluginBase& processor);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showView (bool mixer);

    PresetButton presets;
    juce::TextButton mixerTab { "MIXER" }, maximizerTab { "MAXIMIZER" };
    std::unique_ptr<MixerView> mixerView;
    std::unique_ptr<SourceToggles> maxSources; // vilka källor som går genom maximizern
    bool showingMixer = false;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    LevelMeter output, reduction;
    ScopeView scope, gainScope;
};
