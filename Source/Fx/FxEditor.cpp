#include "FxEditor.h"

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (transportDisplay);

    setSize (440, 200);
}

void FxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f26));

    g.setColour (juce::Colours::orange);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("MyOwnFruit FX", 16, 10, 300, 32, juce::Justification::centredLeft);
}

void FxEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (44);

    transportDisplay.setBounds (area.removeFromLeft (260));
    area.removeFromLeft (16);
    gateLength.setBounds (area.removeFromLeft (100));
}
