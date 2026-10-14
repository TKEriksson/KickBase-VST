#include "FxEditor.h"

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &kick, &gate, &pattern, &mix })
        addAndMakeVisible (component);

    setSize (1060, 676);
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

    auto top = area.removeFromTop (370);
    kick.setBounds (top.removeFromLeft (top.getWidth() / 2 - 5));
    top.removeFromLeft (10);
    gate.setBounds (top);
    area.removeFromTop (10);

    mix.setBounds (area.removeFromRight (190));
    area.removeFromRight (10);
    pattern.setBounds (area);
}
