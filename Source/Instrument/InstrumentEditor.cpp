#include "InstrumentEditor.h"

namespace
{
    // Gränssnittet ritas i den här storleken (logiska punkter) och skalas sedan till fönstret. Standardskalan är lite under 1.
    constexpr int baseWidth = 1180, baseHeight = 1010;
    constexpr float defaultScale = 0.9f;
}

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (root);
    for (auto* component : std::initializer_list<juce::Component*> { &header, &synths, &tools, &info, &keyboard })
        root.addAndMakeVisible (component);

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

    // Fönstret går att dra större och mindre; hela gränssnittet skalas med.
    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 16 / 10, baseHeight * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    const auto scale = processor.editorScale > 0.0f ? processor.editorScale : defaultScale;
    setSize (juce::roundToInt ((float) baseWidth * scale), juce::roundToInt ((float) baseHeight * scale));
    sized = true;
}

InstrumentEditor::~InstrumentEditor()
{
    setLookAndFeel (nullptr);
}

void InstrumentEditor::resized()
{
    const auto scale = root.fitTo (*this, baseWidth, baseHeight);
    if (sized)
        processor.editorScale = scale;

    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (10);

    header.setBounds (area.removeFromTop (52));
    area.removeFromTop (4);

    keyboard.setBounds (area.removeFromBottom (50));
    area.removeFromBottom (6);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (6);

    synths.setBounds (area.removeFromTop (580));
    area.removeFromTop (6);
    tools.setBounds (area);
}
