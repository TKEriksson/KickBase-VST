#include "InstrumentEditor.h"

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* component : std::initializer_list<juce::Component*> { &header, &synths, &tools, &info, &keyboard })
        addAndMakeVisible (component);

    synths.addTab ("PATTERN", Theme::pattern, pattern);
    synths.addTab ("KICK", Theme::kick, kick, kick.embedInTab());
    synths.addTab ("INSTRUMENT", Theme::bass, instruments, instruments.getHeader(), InstrumentHost::headerWidth);

    // På/av direkt på kickens flik (instrumenten har sin på/av i flikraden, bredvid väljaren).
    synths.addPower (1, processor.apvts, ParamID::kickOn);

    tools.addTab ("EFFECTS", Theme::effects, effects);
    tools.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tools.addTab ("LFO", Theme::lfo, lfo);
    tools.addTab ("MASTER", Theme::master, masterPage);
    tools.addTab ("SOUNDSTAGE", Theme::mix, soundstage);

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

    header.setBounds (area.removeFromTop (52));
    area.removeFromTop (4);

    keyboard.setBounds (area.removeFromBottom (50));
    area.removeFromBottom (6);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (6);

    synths.setBounds (area.removeFromTop (520));
    area.removeFromTop (6);
    tools.setBounds (area);
}
