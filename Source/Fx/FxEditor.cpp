#include "FxEditor.h"

namespace
{
    constexpr int baseWidth = 1060, baseHeight = 830;
    constexpr float defaultScale = 0.95f;
}

FxEditor::FxEditor (FxProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (root);
    for (auto* component : std::initializer_list<juce::Component*> { &header, &synths, &tools, &info })
        root.addAndMakeVisible (component);

    synths.addTab ("PATTERN", Theme::pattern, pattern);
    synths.addTab ("KICK", Theme::kick, kick, kick.embedInTab());
    synths.addTab ("GATE", Theme::gate, gate, gate.embedInTab());

    synths.addPower (1, processor.apvts, ParamID::kickOn);
    synths.addPower (2, processor.apvts, ParamID::bass (0, BassField::on));

    tools.addTab ("EFFECTS", Theme::effects, effects);
    tools.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tools.addTab ("LFO", Theme::lfo, lfo);
    tools.addTab ("MATRIX", Theme::lfo, matrix);
    tools.addTab ("MASTER", Theme::master, masterPage);
    tools.addTab ("SOUNDSTAGE", Theme::mix, soundstage);

    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 16 / 10, baseHeight * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    const auto scale = processor.editorScale > 0.0f ? processor.editorScale : defaultScale;
    setSize (juce::roundToInt ((float) baseWidth * scale), juce::roundToInt ((float) baseHeight * scale));
    sized = true;
}

FxEditor::~FxEditor()
{
    setLookAndFeel (nullptr);
}

void FxEditor::resized()
{
    const auto scale = root.fitTo (*this, baseWidth, baseHeight);
    if (sized)
        processor.editorScale = scale;

    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (10);
    header.setBounds (area.removeFromTop (52));
    area.removeFromTop (4);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (6);
    synths.setBounds (area.removeFromTop (520));
    area.removeFromTop (6);
    tools.setBounds (area);
}
