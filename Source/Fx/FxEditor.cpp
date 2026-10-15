#include "FxEditor.h"

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &kick, &gate, &lower, &mix, &info })
        addAndMakeVisible (component);

    lower.addTab ("PATTERN", Theme::pattern, pattern);
    lower.addTab ("EFFECTS", Theme::effects, effects);
    lower.addTab ("SIDECHAIN", Theme::mix, sidechain);
    lower.addTab ("LFO", Theme::lfo, lfo);
    lower.addTab ("MASTER", Theme::master, masterPage);

    setSize (1060, 766);
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
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (8);

    header.setBounds (area.removeFromTop (44));
    area.removeFromTop (4);

    auto top = area.removeFromTop (430);
    kick.setBounds (top.removeFromLeft (top.getWidth() / 2 - 5));
    top.removeFromLeft (10);
    gate.setBounds (top);
    area.removeFromTop (10);

    mix.setBounds (area.removeFromRight (190));
    area.removeFromRight (10);
    lower.setBounds (area);
}
