#include "InstrumentEditor.h"

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &kick, &bass, &lower, &mix, &info, &keyboard })
        addAndMakeVisible (component);

    lower.addTab ("PATTERN", Theme::pattern, pattern);
    lower.addTab ("EFFECTS", Theme::effects, effects);
    lower.addTab ("SIDECHAIN", Theme::mix, sidechain);
    lower.addTab ("LFO", Theme::lfo, lfo);
    lower.addTab ("MASTER", Theme::master, masterPage);

    keyboard.setOctaveForMiddleC (5); // samma tonnamn som FL Studio (mittens C = C5)
    keyboard.setKeyWidth (14.0f);
    keyboard.setLowestVisibleKey (24);

    setSize (1060, 830);
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

    keyboard.setBounds (area.removeFromBottom (54));
    area.removeFromBottom (6);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (8);

    auto top = area.removeFromTop (430);
    kick.setBounds (top.removeFromLeft (top.getWidth() / 2 - 5));
    top.removeFromLeft (10);
    bass.setBounds (top);
    area.removeFromTop (10);

    mix.setBounds (area.removeFromRight (190));
    area.removeFromRight (10);
    lower.setBounds (area);
}
