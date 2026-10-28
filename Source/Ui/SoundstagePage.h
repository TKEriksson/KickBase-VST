#pragma once

#include "../Core/PluginBase.h"
#include "../Core/Mixer.h"
#include "Components.h"
#include "PresetButton.h"
#include "Scopes.h"

// Fliken SOUNDSTAGE: ljudbilden. Stereobredd och "mono under" (så att basen håller sig i mitten)
// och live-vyer av vilka frekvenser som spelas och hur stereobilden ser ut.
class SoundstagePage : public juce::Component
{
public:
    explicit SoundstagePage (PluginBase& processor);
    void resized() override;

private:
    PresetButton presets;
    KnobComponent width, monoBelow, enhance;
    // Vilka källor som går genom bredd/mono/enhance (flerval). Av = källan hoppar över stereoblocket.
    juce::Label stageLabel;
    juce::TextButton helpButton;
    std::array<juce::TextButton, Mixer::numSources> stageButtons;
    std::array<std::unique_ptr<juce::ButtonParameterAttachment>, Mixer::numSources> stageAttachments;
    SpectrumView spectrum;
    StereoView stereo;
};
