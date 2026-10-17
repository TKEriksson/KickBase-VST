#include "SoundstagePage.h"

SoundstagePage::SoundstagePage (PluginBase& p)
    : presets (p, Presets::Section::soundstage, Theme::master),
      width (p.apvts, "masterWidth", "WIDTH", Theme::master),
      monoBelow (p.apvts, "masterMonoBelow", "MONO BELOW", Theme::master),
      enhance (p.apvts, "masterEnhance", "ENHANCE", Theme::master),
      spectrum (p),
      stereo (p)
{
    width.setDescription ("Stereo width of the whole mix: 0 % = mono, 100 % = as it is, 200 % = extra wide. "
                          "The kick and instruments are mono; width comes from effects like chorus, echo and reverb.");
    monoBelow.setDescription ("Everything below this frequency is made mono, so the low end stays tight and centred "
                              "(and works on club systems). 'Off' = leave the low end as it is.");
    enhance.setDescription ("Stereo enhancer: widens mono sounds (your kick and instruments are mono) by putting a delayed copy of the "
                            "highs in the sides. Mono-compatible. Watch the stereo view widen from a vertical line.");
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

    stereo.setBounds (area.removeFromRight (std::min (220, area.getHeight() + 40)));
    area.removeFromRight (8);
    spectrum.setBounds (area);
}
