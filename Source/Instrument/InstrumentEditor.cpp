#include "InstrumentEditor.h"

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &synths, &tools, &info, &keyboard })
        addAndMakeVisible (component);

    synths.addTab ("KICK", Theme::kick, kick);
    synths.addTab ("BASS 1", Theme::layerColour (0), bass1);
    synths.addTab ("BASS 2", Theme::layerColour (1), bass2);
    synths.addTab ("BASS 3", Theme::layerColour (2), bass3);

    tools.addTab ("PATTERN", Theme::pattern, pattern);
    tools.addTab ("EFFECTS", Theme::effects, effects);
    tools.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tools.addTab ("LFO", Theme::lfo, lfo);
    tools.addTab ("MASTER", Theme::master, masterPage);

    keyboard.setOctaveForMiddleC (5); // samma tonnamn som FL Studio (mittens C = C5)
    keyboard.setKeyWidth (14.0f);
    keyboard.setLowestVisibleKey (24);

    setSize (1060, 900);
}

InstrumentEditor::~InstrumentEditor()
{
    setLookAndFeel (nullptr);
}

void InstrumentEditor::paint (juce::Graphics& g)
{
    Theme::paintBackground (g, getLocalBounds());
}

void InstrumentEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    header.setBounds (area.removeFromTop (44));
    area.removeFromTop (4);

    keyboard.setBounds (area.removeFromBottom (50));
    area.removeFromBottom (6);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (6);

    synths.setBounds (area.removeFromTop (490));
    area.removeFromTop (6);
    tools.setBounds (area);
}
