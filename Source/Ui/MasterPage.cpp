#include "MasterPage.h"

LevelMeter::LevelMeter (std::function<float()> readIn, float minIn, float maxIn, bool fromTopIn, juce::Colour colourIn, juce::String labelIn)
    : read (std::move (readIn)), minDb (minIn), maxDb (maxIn), fromTop (fromTopIn), colour (colourIn), label (std::move (labelIn))
{
    startTimerHz (30);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    const auto labelArea = area.removeFromBottom (14);
    const auto bar = area.withSizeKeepingCentre (14, area.getHeight()).toFloat();

    g.setColour (Theme::knobBodyDark);
    g.fillRoundedRectangle (bar, 3.0f);

    const auto value = juce::jlimit (minDb, maxDb, read());
    const auto fraction = fromTop ? (maxDb - value) / (maxDb - minDb) : (value - minDb) / (maxDb - minDb);
    const auto height = bar.getHeight() * fraction;
    const auto filled = fromTop ? bar.withHeight (height) : bar.withTop (bar.getBottom() - height);

    g.setColour (colour);
    g.fillRoundedRectangle (filled, 3.0f);

    // Skala: var 6:e dB.
    g.setFont (Theme::font (8.0f));
    for (auto db = maxDb; db >= minDb; db -= 6.0f)
    {
        const auto f = fromTop ? (maxDb - db) / (maxDb - minDb) : (db - minDb) / (maxDb - minDb);
        const auto y = fromTop ? bar.getY() + bar.getHeight() * f : bar.getBottom() - bar.getHeight() * f;
        g.setColour (Theme::textDim.withAlpha (0.7f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (bar.getRight() + 2.0f, y - 5.0f, 24.0f, 10.0f), juce::Justification::left);
    }

    g.setFont (Theme::font (10.0f, true));
    g.setColour (Theme::textDim);
    g.drawText (label, labelArea, juce::Justification::centred);
}

//==============================================================================
MasterPage::MasterPage (PluginBase& p)
    : presets (p, Presets::Section::master, Theme::master),
      output ([&p] { return p.getMasterSection().outputPeakDb.load(); }, -36.0f, 0.0f, false, Theme::mix, "OUT"),
      reduction ([&p] { return p.getMasterSection().gainReductionDb.load(); }, -12.0f, 0.0f, true, Theme::master, "GR")
{
    addAndMakeVisible (presets);

    power.setColour (juce::ToggleButton::tickColourId, Theme::master);
    power.setTooltip ("Master on/off");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "masterOn", power);

    for (const auto& [id, name] : std::initializer_list<std::pair<const char*, const char*>> {
             { "masterDrive", "DRIVE" }, { "masterClip", "CLIP" }, { "masterCeiling", "CEILING" }, { "masterRelease", "RELEASE" } })
    {
        knobs.push_back (std::make_unique<KnobComponent> (p.apvts, id, name, Theme::master));
        addAndMakeVisible (*knobs.back());
    }

    addAndMakeVisible (output);
    addAndMakeVisible (reduction);
}

void MasterPage::paint (juce::Graphics& g)
{
    g.setFont (Theme::font (13.0f, true));
    g.setColour (Theme::master);
    g.drawText ("MAXIMIZER", getLocalBounds().withHeight (24).withTrimmedLeft (30), juce::Justification::centredLeft);

    g.setFont (Theme::font (10.0f));
    g.setColour (Theme::textDim);
    g.drawText ("Drive pushes level into the soft clipper and limiter; the limiter never lets peaks pass the ceiling.",
                getLocalBounds().removeFromBottom (14), juce::Justification::centredLeft);
}

void MasterPage::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom (16);

    auto header = area.removeFromTop (24);
    power.setBounds (header.removeFromLeft (26));
    presets.setBounds (header.removeFromRight (160));
    area.removeFromTop (6);

    auto meters = area.removeFromRight (110);
    output.setBounds (meters.removeFromLeft (55));
    reduction.setBounds (meters);

    auto row = area.withHeight (std::min (area.getHeight(), 100));
    const auto width = std::min (100, row.getWidth() / (int) knobs.size());
    for (auto& knob : knobs)
        knob->setBounds (row.removeFromLeft (width));
}
