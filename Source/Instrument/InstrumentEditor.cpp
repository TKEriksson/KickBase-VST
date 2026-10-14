#include "InstrumentEditor.h"

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (transportDisplay);
    keyboard.setOctaveForMiddleC (5); // samma tonnamn som FL Studio och displayen (mittens C = C5)
    addAndMakeVisible (keyboard);

    setSize (640, 340);
}

void InstrumentEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f26));

    g.setColour (juce::Colours::orange);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("MyOwnFruit", 16, 10, 300, 32, juce::Justification::centredLeft);
}

void InstrumentEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (44);

    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (12);

    transportDisplay.setBounds (area.removeFromLeft (260));
    area.removeFromLeft (16);
    gain.setBounds (area.removeFromLeft (100));
}
