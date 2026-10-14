#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class MyOwnFruitEditor : public juce::AudioProcessorEditor
{
public:
    explicit MyOwnFruitEditor (MyOwnFruitProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob& knob, const char* paramID, const juce::String& name);

    MyOwnFruitProcessor& processor;

    juce::ComboBox waveformBox;
    std::unique_ptr<ComboAttachment> waveformAttachment;

    Knob attack, decay, sustain, release, cutoff, resonance, gain;

    juce::MidiKeyboardComponent keyboard { processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyOwnFruitEditor)
};
