#include "FxEditor.h"

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &tabs, &info })
        addAndMakeVisible (component);

    tabs.addTab ("PATTERN", Theme::pattern, pattern);
    tabs.addTab ("KICK", Theme::kick, kick);
    tabs.addTab ("GATE", Theme::gate, gate);
    tabs.addTab ("EFFECTS", Theme::effects, effects);
    tabs.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tabs.addTab ("LFO", Theme::lfo, lfo);
    tabs.addTab ("MASTER", Theme::master, masterPage);

    setSize (1060, 700);
}

FxEditor::~FxEditor()
{
    setLookAndFeel (nullptr);
}

void FxEditor::paint (juce::Graphics& g)
{
    Theme::paintBackground (g, getLocalBounds());
}

void FxEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    header.setBounds (area.removeFromTop (44));
    area.removeFromTop (4);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (6);
    tabs.setBounds (area);
}
