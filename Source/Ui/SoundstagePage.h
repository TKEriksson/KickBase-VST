#pragma once

#include "../Core/PluginBase.h"
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
    KnobComponent width, monoBelow;
    SpectrumView spectrum;
    StereoView stereo;
};
