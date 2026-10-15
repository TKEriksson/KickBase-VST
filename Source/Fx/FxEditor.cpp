#include "FxEditor.h"

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &synths, &tools, &info })
        addAndMakeVisible (component);

    synths.addTab ("PATTERN", Theme::pattern, pattern);
    synths.addTab ("KICK", Theme::kick, kick, kick.embedInTab());
    synths.addTab ("GATE", Theme::gate, gate, gate.embedInTab());

    tools.addTab ("EFFECTS", Theme::effects, effects);
    tools.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tools.addTab ("LFO", Theme::lfo, lfo);
    tools.addTab ("MASTER", Theme::master, masterPage);

    setSize (1060, 830);
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
    synths.setBounds (area.removeFromTop (520));
    area.removeFromTop (6);
    tools.setBounds (area);
}
