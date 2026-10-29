#include "MasterPage.h"

#include "HelpDialog.h"

LevelMeter::LevelMeter (std::function<float()> readIn, float minIn, float maxIn, bool fromTopIn, juce::Colour colourIn, juce::String labelIn)
    : read (std::move (readIn)), minDb (minIn), maxDb (maxIn), fromTop (fromTopIn), colour (colourIn), label (std::move (labelIn))
{
    startVisualTimer (Settings::Visual::maximizer, 30);
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
      sidechainPage (p),
      output ([&p] { return p.getMasterSection().outputPeakDb.load(); }, -36.0f, 0.0f, false, Theme::mix, "OUT"),
      reduction ([&p] { return p.getMasterSection().gainReductionDb.load(); }, -12.0f, 0.0f, true, Theme::master, "GR"),
      scope (p, { { &p.getMasterSection().inputTap, 2, juce::Colours::white.withAlpha (0.6f), false, "INTO LIMITER" },
                  { &p.getMasterSection().outputTap, 2, Theme::master, true, "OUTPUT" } }),
      gainScope (p, { { &p.getMasterSection().gainTap, 0, Theme::master, true, "GAIN" } }, ScopeView::Mode::gainReduction)
{
    scope.setTooltip ("MAXIMIZER SCOPE - Live: the signal going into the limiter (white outline) and what comes out (red). "
                      "Where the white sticks out above the red lines (the ceiling), the limiter is pushing it down.");
    gainScope.setTooltip ("GAIN REDUCTION - How much the limiter turns the signal down, over time.");
    scope.setVisual (Settings::Visual::maximizer);
    gainScope.setVisual (Settings::Visual::maximizer);
    scope.guideLevel = [&p]
    {
        const auto* ceiling = p.apvts.getRawParameterValue ("masterCeiling");
        return juce::Decibels::decibelsToGain (ceiling->load());
    };
    addAndMakeVisible (scope);
    addAndMakeVisible (gainScope);
    addAndMakeVisible (presets);
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::master, "the MASTER tab (mixer and maximizer)");
    addAndMakeVisible (helpButton);

    power.setColour (juce::ToggleButton::tickColourId, Theme::master);
    power.setTooltip ("Master on/off");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "masterOn", power);

    for (const auto& [id, name] : std::initializer_list<std::pair<const char*, const char*>> {
             { "masterDrive", "DRIVE" }, { "masterClip", "CLIP" }, { "masterCeiling", "CEILING" }, { "masterRelease", "RELEASE" },
             { ParamID::gain, "OUTPUT" } })
    {
        knobs.push_back (std::make_unique<KnobComponent> (p.apvts, id, name, Theme::master));
        addAndMakeVisible (*knobs.back());
    }

    addAndMakeVisible (output);
    addAndMakeVisible (reduction);

    // Tre vyer: MIXER (bara i instrumentet), SIDECHAIN och MAXIMIZER.
    addChildComponent (sidechainPage);
    sidechainTab.setTooltip ("SIDECHAIN - The kick ducks the other sounds: depth, curve, length and which sources are ducked.");
    maximizerTab.setTooltip ("MAXIMIZER - Drive, clip and limiter on the whole mix."
                             + juce::String (p.getMixer() != nullptr ? " Choose which sources go through it at the bottom." : ""));
    sidechainTab.onClick = [this] { showView (View::sidechain); };
    maximizerTab.onClick = [this] { showView (View::maximizer); };
    for (auto* tab : { &mixerTab, &sidechainTab, &maximizerTab })
        tab->setColour (juce::TextButton::buttonOnColourId, Theme::master.withAlpha (0.55f));
    addAndMakeVisible (sidechainTab);
    addAndMakeVisible (maximizerTab);

    if (p.getMixer() != nullptr)
    {
        mixerView = std::make_unique<MixerView> (p);
        addChildComponent (*mixerView);
        maxSources = std::make_unique<SourceToggles> (p, "Max", "THROUGH THE MAXIMIZER");
        addChildComponent (*maxSources);

        mixerTab.setTooltip ("MIXER - Gain, pan and level meter for every sound source (kick, instruments and FX channels), "
                             "plus SC (sidechain on/off) and MAX (through the maximizer or not) per source.");
        mixerTab.onClick = [this] { showView (View::mixer); };
        addAndMakeVisible (mixerTab);
        showView (View::mixer);
    }
    else
    {
        showView (View::maximizer);
    }
}

void MasterPage::showView (View newView)
{
    view = newView;
    const auto mixer = view == View::mixer, maximizer = view == View::maximizer;
    mixerTab.setToggleState (mixer, juce::dontSendNotification);
    sidechainTab.setToggleState (view == View::sidechain, juce::dontSendNotification);
    maximizerTab.setToggleState (maximizer, juce::dontSendNotification);
    if (mixerView != nullptr)
        mixerView->setVisible (mixer);
    sidechainPage.setVisible (view == View::sidechain);
    presets.setVisible (maximizer); // mixern och sidechainen har egna preset (sidechainen i sin egen sida)
    helpButton.setVisible (! (view == View::sidechain)); // sidechainsidan har sin egen hjälpknapp
    if (maxSources != nullptr)
        maxSources->setVisible (maximizer);
    for (auto& knob : knobs)
        knob->setVisible (maximizer);
    for (auto* c : std::initializer_list<juce::Component*> { &output, &reduction, &scope, &gainScope })
        c->setVisible (maximizer);
    resized();
    repaint();
}

void MasterPage::paint (juce::Graphics& g)
{
    // Ingen brödsmula i MASTER-flikraden (Tobias 29 okt); flikknapparna visar var man är.
    if (view != View::maximizer)
        return;

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
    helpButton.setBounds (header.removeFromRight (44));
    header.removeFromRight (6);
    presets.setBounds (header.removeFromRight (160));
    if (mixerView != nullptr)
    {
        header.removeFromLeft (6);
        mixerTab.setBounds (header.removeFromLeft (90));
    }
    header.removeFromLeft (6);
    sidechainTab.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (6);
    maximizerTab.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (10);
    crumbArea = header.withTrimmedRight (6);
    area.removeFromTop (6);

    if (mixerView != nullptr && view == View::mixer)
    {
        mixerView->setBounds (area.withHeight (area.getHeight() + 16));
        return;
    }
    if (view == View::sidechain)
    {
        sidechainPage.setBounds (area.withHeight (area.getHeight() + 16));
        return;
    }
    if (maxSources != nullptr)
    {
        maxSources->setBounds (area.removeFromBottom (24));
        area.removeFromBottom (2);
    }

    auto meters = area.removeFromRight (110);
    output.setBounds (meters.removeFromLeft (55));
    reduction.setBounds (meters);
    area.removeFromRight (10);

    // Rattarna i två rader till vänster, vågformerna till höger om dem.
    auto knobArea = area.removeFromLeft (310);
    area.removeFromLeft (10);
    const auto rowHeight = knobArea.getHeight() / 2;
    auto top = knobArea.removeFromTop (rowHeight);
    const auto cell = top.getWidth() / 3;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& row = i < 3 ? top : knobArea;
        knobs[i]->setBounds (row.removeFromLeft (cell));
    }

    scope.setBounds (area.removeFromTop (area.getHeight() * 7 / 10));
    area.removeFromTop (4);
    gainScope.setBounds (area);
}
