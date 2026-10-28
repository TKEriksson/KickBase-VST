#include "SoundstagePage.h"
#include "HelpDialog.h"

SoundstagePage::SoundstagePage (PluginBase& p)
    : presets (p, Presets::Section::soundstage, Theme::master),
      width (p.apvts, "masterWidth", "WIDTH", Theme::master),
      monoBelow (p.apvts, "masterMonoBelow", "BASS MONO", Theme::master),
      enhance (p.apvts, "masterEnhance", "ENHANCE", Theme::master),
      spectrum (p),
      stereo (p)
{
    width.setDescription ("Stereo width of the sources chosen below: 0 % = mono, 100 % = as it is, 200 % = extra wide. "
                          "Only the side signal (left minus right) is scaled, so a mono sound is not affected; width comes from effects like chorus, echo and reverb.");
    monoBelow.setDescription ("BASS MONO: the sides (left minus right) below this frequency are removed, so the low end stays tight and centred "
                              "(and works on club systems). Only for the sources chosen below. 'Off' = leave the low end as it is.");
    enhance.setDescription ("Stereo enhancer: widens mono sounds (your kick and instruments are mono) by putting a delayed copy of the "
                            "highs in the sides. Mono-compatible. Watch the stereo view widen from a vertical line.");
    stageLabel.setText ("AFFECTS", juce::dontSendNotification);
    stageLabel.setFont (juce::FontOptions (11.0f));
    stageLabel.setColour (juce::Label::textColourId, Theme::master);
    stageLabel.setTooltip ("Which sources go through WIDTH, BASS MONO and ENHANCE. A source that is off keeps its own stereo image "
                           "(it still gets the master EQ, drive and limiter). All on = the whole mix, as before.");
    addAndMakeVisible (stageLabel);
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::soundstage, "SOUNDSTAGE (how AFFECTS, WIDTH, BASS MONO and ENHANCE work)");
    addAndMakeVisible (helpButton);
    for (int s = 0; s < Mixer::numLegacySources; ++s)
    {
        auto& button = stageButtons[(size_t) s];
        const auto colour = s == Mixer::kickSource ? Theme::kick : s < Mixer::firstFxSource ? Theme::layerColour (s - Mixer::firstBassSource) : Theme::master;
        button.setButtonText (s == Mixer::kickSource ? "KICK" : s < Mixer::firstFxSource ? "I" + juce::String (s) : Mixer::sourceName (s));
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, colour);
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        button.setTooltip (Mixer::sourceName (s) + ": on = goes through WIDTH, BASS MONO and ENHANCE. Off = keeps its own stereo image.");
        stageAttachments[(size_t) s] = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (Mixer::paramID (s, "Stage")), button);
        addAndMakeVisible (button);
    }
    for (auto* c : std::initializer_list<juce::Component*> { &presets, &width, &monoBelow, &enhance, &spectrum, &stereo })
        addAndMakeVisible (c);
}

void SoundstagePage::resized()
{
    auto area = getLocalBounds();

    auto left = area.removeFromLeft (250);
    area.removeFromLeft (10);
    presets.setBounds (left.removeFromTop (24));
    left.removeFromTop (6);
    const auto third = left.getWidth() / 3;
    auto knobs = left.removeFromTop (std::min (left.getHeight(), 100));
    width.setBounds (knobs.removeFromLeft (third));
    monoBelow.setBounds (knobs.removeFromLeft (third));
    enhance.setBounds (knobs);

    left.removeFromTop (6);
    auto labelRow = left.removeFromTop (16);
    helpButton.setBounds (labelRow.removeFromRight (50));
    stageLabel.setBounds (labelRow);
    const auto columns = 4, rowHeight = 22;
    const auto cell = left.getWidth() / columns;
    for (int s = 0; s < Mixer::numLegacySources; ++s)
        stageButtons[(size_t) s].setBounds (left.getX() + (s % columns) * cell, left.getY() + (s / columns) * (rowHeight + 2), cell - 2, rowHeight);

    stereo.setBounds (area.removeFromRight (std::min (220, area.getHeight() + 40)));
    area.removeFromRight (8);
    spectrum.setBounds (area);
}
