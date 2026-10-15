#include "LfoPage.h"

LfoDisplay::LfoDisplay (PluginBase& p, int lfoIndex) : processor (p), index (lfoIndex)
{
    startTimerHz (30);
}

void LfoDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = bounds.reduced (6.0f, 8.0f);
    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const auto shape = (int) processor.apvts.getRawParameterValue (Modulation::paramID (index, "Shape"))->load();
    const auto on = processor.apvts.getRawParameterValue (Modulation::paramID (index, "On"))->load() >= 0.5f;
    const auto colour = on ? Theme::lfo : Theme::textDim;

    // En period av formen (random ritas som fyra slumpsteg).
    juce::Path path;
    constexpr int resolution = 120;
    for (int i = 0; i <= resolution; ++i)
    {
        const auto phase = (double) i / resolution;
        const auto cycle = shape == 5 ? (juce::int64) (phase * 4.0) : 0;
        const auto value = Modulation::shapeValue (shape, std::fmod (phase * (shape == 5 ? 4.0 : 1.0), 1.0), cycle);
        const juce::Point<float> point { area.getX() + (float) phase * area.getWidth(), area.getCentreY() - value * area.getHeight() * 0.5f };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (5.0f));
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.8f));

    // Var LFO:n är just nu.
    const auto phase = processor.modulator.phase[(size_t) index].load();
    const auto value = processor.modulator.value[(size_t) index].load();
    const juce::Point<float> dot { area.getX() + phase * area.getWidth(), area.getCentreY() - value * area.getHeight() * 0.5f };
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
}

//==============================================================================
LfoPanel::LfoPanel (PluginBase& p, int lfoIndex)
    : index (lfoIndex),
      shape (p.apvts, Modulation::paramID (lfoIndex, "Shape").toRawUTF8(), "SHAPE"),
      rate (p.apvts, Modulation::paramID (lfoIndex, "Rate").toRawUTF8(), "RATE"),
      display (p, lfoIndex)
{
    power.setColour (juce::ToggleButton::tickColourId, Theme::lfo);
    power.setTooltip ("LFO on/off");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, Modulation::paramID (lfoIndex, "On"), power);

    addAndMakeVisible (shape);
    addAndMakeVisible (rate);
    addAndMakeVisible (display);

    for (int r = 0; r < Modulation::routesPerLfo; ++r)
    {
        const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + r));
        destinations.push_back (std::make_unique<ChoiceComponent> (
            p.apvts, Modulation::paramID (lfoIndex, ("Dest" + letter).toRawUTF8()).toRawUTF8(), "TARGET " + letter));
        amounts.push_back (std::make_unique<KnobComponent> (
            p.apvts, Modulation::paramID (lfoIndex, ("Amount" + letter).toRawUTF8()).toRawUTF8(), "AMOUNT", Theme::lfo));
        addAndMakeVisible (*destinations.back());
        addAndMakeVisible (*amounts.back());
    }
}

void LfoPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::knobBodyDark.withAlpha (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (Theme::lfo);
    g.setFont (Theme::font (13.0f, true));
    g.drawText ("LFO " + juce::String (index + 1), getLocalBounds().withHeight (24).withTrimmedLeft (32), juce::Justification::centredLeft);
}

void LfoPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto header = area.removeFromTop (20);
    power.setBounds (header.removeFromLeft (24));
    area.removeFromTop (4);

    auto left = area.removeFromLeft (92);
    shape.setBounds (left.removeFromTop (40));
    left.removeFromTop (4);
    rate.setBounds (left.removeFromTop (40));
    area.removeFromLeft (6);

    auto right = area.removeFromRight (170);
    area.removeFromRight (6);
    display.setBounds (area);

    const auto rowHeight = right.getHeight() / Modulation::routesPerLfo;
    for (int r = 0; r < Modulation::routesPerLfo; ++r)
    {
        auto row = right.removeFromTop (rowHeight);
        amounts[(size_t) r]->setBounds (row.removeFromRight (62));
        destinations[(size_t) r]->setBounds (row.withSizeKeepingCentre (row.getWidth() - 4, 40));
    }
}

//==============================================================================
LfoPage::LfoPage (PluginBase& p)
    : presets (p, Presets::Section::lfo, Theme::lfo), first (p, 0), second (p, 1)
{
    addAndMakeVisible (presets);
    addAndMakeVisible (first);
    addAndMakeVisible (second);
}

void LfoPage::resized()
{
    auto area = getLocalBounds();
    presets.setBounds (area.removeFromTop (22).removeFromRight (160));
    area.removeFromTop (4);

    const auto half = (area.getWidth() - 10) / 2;
    first.setBounds (area.removeFromLeft (half));
    area.removeFromLeft (10);
    second.setBounds (area);
}
