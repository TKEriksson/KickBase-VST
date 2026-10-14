#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "InstrumentProcessor.h"
#include "../Core/Knob.h"
#include "../Core/TransportDisplay.h"

class InstrumentEditor : public juce::AudioProcessorEditor
{
public:
    explicit InstrumentEditor (InstrumentProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    InstrumentProcessor& processor;

    TransportDisplay transportDisplay { processor.transportStatus, true };
    Knob gain { *this, processor.apvts, "gain", "Gain" };
    juce::MidiKeyboardComponent keyboard { processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentEditor)
};
