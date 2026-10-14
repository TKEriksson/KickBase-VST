#include "PluginEditor.h"

MyOwnFruitEditor::MyOwnFruitEditor (MyOwnFruitProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    waveformBox.addItemList (processor.apvts.getParameter (ParamID::waveform)->getAllValueStrings(), 1);
    addAndMakeVisible (waveformBox);
    waveformAttachment = std::make_unique<ComboAttachment> (processor.apvts, ParamID::waveform, waveformBox);

    addKnob (attack,    ParamID::attack,    "Attack");
    addKnob (decay,     ParamID::decay,     "Decay");
    addKnob (sustain,   ParamID::sustain,   "Sustain");
    addKnob (release,   ParamID::release,   "Release");
    addKnob (cutoff,    ParamID::cutoff,    "Cutoff");
    addKnob (resonance, ParamID::resonance, "Resonance");
    addKnob (gain,      ParamID::gain,      "Gain");

    addAndMakeVisible (keyboard);

    setSize (640, 340);
}

void MyOwnFruitEditor::addKnob (Knob& knob, const char* paramID, const juce::String& name)
{
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramID, knob.slider);
}

void MyOwnFruitEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f26));

    g.setColour (juce::Colours::orange);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("MyOwnFruit", 16, 10, 200, 32, juce::Justification::centredLeft);
}

void MyOwnFruitEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (8);

    auto header = area.removeFromTop (36);
    waveformBox.setBounds (header.removeFromRight (160).reduced (0, 4));

    area.removeFromTop (16);

    Knob* knobs[] = { &attack, &decay, &sustain, &release, &cutoff, &resonance, &gain };
    const auto knobWidth = area.getWidth() / (int) std::size (knobs);

    for (auto* knob : knobs)
    {
        auto column = area.removeFromLeft (knobWidth);
        knob->label.setBounds (column.removeFromTop (20));
        knob->slider.setBounds (column.reduced (4));
    }
}
