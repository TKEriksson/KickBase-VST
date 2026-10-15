#include "InstrumentEditor.h"

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &tabs, &info, &keyboard })
        addAndMakeVisible (component);

    tabs.addTab ("PATTERN", Theme::pattern, pattern);
    tabs.addTab ("KICK", Theme::kick, kick);
    tabs.addTab ("BASS 1", Theme::layerColour (0), bass1);
    tabs.addTab ("BASS 2", Theme::layerColour (1), bass2);
    tabs.addTab ("BASS 3", Theme::layerColour (2), bass3);
    tabs.addTab ("EFFECTS", Theme::effects, effects);
    tabs.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tabs.addTab ("LFO", Theme::lfo, lfo);
    tabs.addTab ("MASTER", Theme::master, masterPage);

    keyboard.setOctaveForMiddleC (5); // samma tonnamn som FL Studio (mittens C = C5)
    keyboard.setKeyWidth (14.0f);
    keyboard.setLowestVisibleKey (24);

    setSize (1060, 760);
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

    tabs.setBounds (area);
}
