#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// En ratt med etikett, kopplad till en parameter.
struct Knob
{
    Knob (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& name)
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        parent.addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        parent.addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
    }

    void setBounds (juce::Rectangle<int> area)
    {
        label.setBounds (area.removeFromTop (20));
        slider.setBounds (area);
    }

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
