#pragma once

#include "VisualTimer.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "MixerPage.h"
#include "PresetButton.h"
#include "Scopes.h"
#include "SidechainPage.h"

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

// Fliken MASTER: mixern (gain, panorering och mätare per ljudkälla), sidechainen (ducking, kurva) och
// maximizern (drive, clip, tak, release och mätare). Mixern finns bara i instrumentet.
class MasterPage : public juce::Component
{
public:
    explicit MasterPage (PluginBase& processor);
    void paint (juce::Graphics&) override;
    void resized() override;
    void showSidechain() { showView (View::sidechain); }

private:
    enum class View { mixer, sidechain, maximizer };
    void showView (View);

    PresetButton presets;
    SidechainPage sidechainPage;
    juce::TextButton helpButton;
    juce::TextButton mixerTab { "MIXER" }, sidechainTab { "SIDECHAIN" }, maximizerTab { "MAXIMIZER" };
    juce::Rectangle<int> crumbArea; // glasdisplayen MASTER > vy i flikraden
    std::unique_ptr<MixerView> mixerView;
    std::unique_ptr<SourceToggles> maxSources; // vilka källor som går genom maximizern
    View view = View::maximizer;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    LevelMeter output, reduction;
    ScopeView scope, gainScope;
};
