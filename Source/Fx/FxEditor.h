#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FxProcessor.h"
#include "../Core/Knob.h"
#include "../Core/TransportDisplay.h"

class FxEditor : public juce::AudioProcessorEditor
{
public:
    explicit FxEditor (FxProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FxProcessor& processor;

    TransportDisplay transportDisplay { processor.transportStatus, false };
    Knob gateLength { *this, processor.apvts, "gateLength", "Gate Length" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxEditor)
};
