#include "MixerPage.h"
#include "Help.h"
#include "InstrumentMenu.h"

namespace
{
    constexpr float meterMinDb = -60.0f, meterMaxDb = 6.0f;

    float fractionForDb (float db)
    {
        return juce::jlimit (0.0f, 1.0f, (db - meterMinDb) / (meterMaxDb - meterMinDb));
    }

    void paintMeterBars (juce::Graphics& g, juce::Rectangle<int> area, const float* levels, const float* holds, int count, juce::Colour colour)
    {
        const auto barWidth = (float) area.getWidth() / (float) count;
        for (int c = 0; c < count; ++c)
        {
            const auto bar = juce::Rectangle<float> (area.getX() + c * barWidth + 0.5f, (float) area.getY(), barWidth - 1.0f, (float) area.getHeight());
            g.setColour (Theme::knobBodyDark);
            g.fillRect (bar);

            // Grön, gul över -12 dB och röd över -3 dB.
            const auto level = fractionForDb (levels[c]);
            const auto filled = bar.withTop (bar.getBottom() - bar.getHeight() * level);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5a5a), 0.0f, bar.getY(),
                                                     colour, 0.0f, bar.getY() + bar.getHeight() * (1.0f - fractionForDb (-9.0f)), false));
            g.fillRect (filled);

            g.setColour (juce::Colours::white.withAlpha (0.85f));
            const auto y = bar.getBottom() - bar.getHeight() * fractionForDb (holds[c]);
            g.fillRect (juce::Rectangle<float> (bar.getX(), y, bar.getWidth(), 1.5f));
        }

        // Streck vid 0 dB.
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawHorizontalLine ((int) (area.getBottom() - area.getHeight() * fractionForDb (0.0f)), (float) area.getX(), (float) area.getRight());
    }

    // Peak med fall (dB), och en hållen topp som sjunker långsamt.
    void ballistics (float& level, float& hold, float newPeakDb)
    {
        level = newPeakDb > level ? newPeakDb : std::max (newPeakDb, level - 2.0f);
        hold = newPeakDb > hold ? newPeakDb : hold - 0.35f;
        hold = std::max (hold, level);
    }

    void styleFader (juce::Slider& fader, juce::Colour colour)
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        fader.setColour (juce::Slider::trackColourId, colour);
        fader.setColour (juce::Slider::thumbColourId, Theme::text);
        fader.setMouseDragSensitivity (200);
    }

    void styleToggle (juce::TextButton& button, juce::Colour colour, const juce::String& tooltip)
    {
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.65f));
        button.setTooltip (tooltip);
    }
}

namespace
{
    // Namnet på en källa: instrumenten har sina egna namn.
    juce::String labelOf (const PluginBase& processor, int source)
    {
        if (source >= Mixer::firstBassSource && source < Mixer::firstFxSource)
            return processor.getLayerName (source - Mixer::firstBassSource);
        return Mixer::sourceName (source);
    }

    // Används källan just nu? (Instrument som inte finns har ingen remsa, och en effektkanal som inget skickas till och som saknar effekt döljs.)
    bool inUse (const PluginBase& processor, int source, int fxMask)
    {
        if (source >= Mixer::firstFxSource)
            return (fxMask & (1 << (source - Mixer::firstFxSource))) != 0;
        return source < Mixer::firstBassSource || source - Mixer::firstBassSource < processor.activeInstruments();
    }
}

juce::Colour mixerSourceColour (int source)
{
    if (source == Mixer::kickSource)
        return Theme::kick;
    if (source < Mixer::firstFxSource)
        return Theme::layerColour (source - Mixer::firstBassSource);
    return Theme::effects;
}

//==============================================================================
MixerStrip::MixerStrip (PluginBase& p, int sourceIn)
    : processor (p), mixer (*p.getMixer()), source (sourceIn), colour (mixerSourceColour (sourceIn))
{
    const auto name = Mixer::sourceName (source);

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    pan.setColour (juce::Slider::rotarySliderFillColourId, colour);
    pan.setTooltip (name + " PAN - " + Help::forParameter (Mixer::paramID (source, "Pan")));
    pan.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (pan);
    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, Mixer::paramID (source, "Pan"), pan);
    pan.setDoubleClickReturnValue (true, 0.0);

    styleFader (fader, colour);
    fader.setTooltip (name + " GAIN - " + Help::forParameter (Mixer::paramID (source, "Gain")));
    fader.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (fader);
    faderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, Mixer::paramID (source, "Gain"), fader);
    fader.setDoubleClickReturnValue (true, 0.0);

    if (Mixer::hasDuck (source))
    {
        styleToggle (duckButton, colour, name + " SC - " + Help::forParameter (Mixer::paramID (source, "Duck")));
        addAndMakeVisible (duckButton);
        duckAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Mixer::paramID (source, "Duck"), duckButton);
    }
    styleToggle (maxButton, colour, name + " MAX - " + Help::forParameter (Mixer::paramID (source, "Max")));
    addAndMakeVisible (maxButton);
    maxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Mixer::paramID (source, "Max"), maxButton);

    if (source >= Mixer::firstFxSource)
    {
        const auto muteID = ParamID::fxMuteIds[source - Mixer::firstFxSource];
        styleToggle (muteButton, colour, name + " MUTE - " + Help::forParameter (muteID));
        addAndMakeVisible (muteButton);
        muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, muteID, muteButton);
    }
}

void MixerStrip::updateMeter()
{
    for (int c = 0; c < 2; ++c)
        ballistics (meterDb[(size_t) c], holdDb[(size_t) c], mixer.takePeakDb (source, c));
    repaint (meterArea);
}

void MixerStrip::resized()
{
    auto area = getLocalBounds().reduced (2, 2);
    area.removeFromTop (13); // namnet
    pan.setBounds (area.removeFromTop (30).withSizeKeepingCentre (28, 28));

    auto buttons = area.removeFromBottom (16);
    area.removeFromBottom (2);
    if (source >= Mixer::firstFxSource)
    {
        muteButton.setBounds (area.removeFromBottom (16).reduced (1, 0));
        area.removeFromBottom (2);
    }
    if (Mixer::hasDuck (source))
    {
        duckButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
        maxButton.setBounds (buttons.reduced (1, 0));
    }
    else
        maxButton.setBounds (buttons.reduced (1, 0));

    area.removeFromBottom (13); // dB-texten
    fader.setBounds (area.removeFromLeft (area.getWidth() / 2 + 2));
    meterArea = area.reduced (1, 4);
}

void MixerStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    Theme::paintMetal (g, bounds, 5.0f, juce::Colour (0xe62c3048), juce::Colour (0xe61a1d30), false);

    g.setFont (Theme::font (10.5f, true));
    g.setColour (colour);
    g.drawFittedText (labelOf (processor, source), getLocalBounds().withHeight (16).withTrimmedTop (3), juce::Justification::centred, 1, 0.7f);

    paintMeterBars (g, meterArea, meterDb.data(), holdDb.data(), 2, colour);

    // Gain i dB under faderna.
    const auto db = (float) fader.getValue();
    g.setFont (Theme::font (9.5f));
    g.setColour (Theme::text);
    g.drawText (db <= Mixer::minGainDb + 0.05f ? juce::String ("-inf") : juce::String (db, 1),
                getLocalBounds().withTrimmedBottom (20).removeFromBottom (14), juce::Justification::centred);
}

//==============================================================================
MasterStrip::MasterStrip (PluginBase& p) : processor (p)
{
    styleFader (fader, Theme::master);
    fader.setTooltip ("OUTPUT - The volume of the whole plugin, after everything else. " + Help::forParameter (ParamID::gain));
    fader.setPopupDisplayEnabled (true, true, nullptr);
    fader.textFromValueFunction = [] (double v)
    {
        return v <= 0.001 ? juce::String ("-inf dB") : juce::String (juce::Decibels::gainToDecibels ((float) v), 1) + " dB";
    };
    addAndMakeVisible (fader);
    faderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamID::gain, fader);
    fader.setDoubleClickReturnValue (true, 0.8);
}

void MasterStrip::updateMeter()
{
    ballistics (meterDb, holdDb, processor.getMasterSection().outputPeakDb.load());
    repaint (meterArea);
}

void MasterStrip::resized()
{
    auto area = getLocalBounds().reduced (2, 2);
    area.removeFromTop (13 + 30);
    area.removeFromBottom (16 + 2 + 13);
    fader.setBounds (area.removeFromLeft (area.getWidth() / 2 + 2));
    meterArea = area.reduced (1, 4);
}

void MasterStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    Theme::paintMetal (g, bounds, 5.0f, juce::Colour (0xe6402a2e), juce::Colour (0xe62a1a1e), false);

    g.setFont (Theme::font (10.5f, true));
    g.setColour (Theme::master);
    g.drawText ("MASTER", getLocalBounds().withHeight (16).withTrimmedTop (3), juce::Justification::centred);

    const float level[2] { meterDb, meterDb }, hold[2] { holdDb, holdDb };
    paintMeterBars (g, meterArea, level, hold, 2, Theme::master);

    const auto gain = (float) fader.getValue();
    g.setFont (Theme::font (9.5f));
    g.setColour (Theme::text);
    g.drawText (gain <= 0.001f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (gain), 1),
                getLocalBounds().withTrimmedBottom (20).removeFromBottom (14), juce::Justification::centred);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (8.5f));
    g.drawText ("OUTPUT", getLocalBounds().removeFromTop (46).removeFromBottom (24), juce::Justification::centred);
}

//==============================================================================
MixerView::MixerView (PluginBase& p) : processor (p), master (p)
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);
    for (int s = 0; s < Mixer::numSources; ++s)
    {
        strips.push_back (std::make_unique<MixerStrip> (p, s));
        content.addAndMakeVisible (*strips.back());
    }
    addAndMakeVisible (master);
    startVisualTimer (Settings::Visual::mixer, 30);
}

void MixerStrip::refreshColour()
{
    colour = mixerSourceColour (source);
    pan.setColour (juce::Slider::rotarySliderFillColourId, colour);
    fader.setColour (juce::Slider::trackColourId, colour);
    for (auto* button : { &duckButton, &maxButton })
        button->setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.65f));
    repaint();
}

void MixerView::timerCallback()
{
    followVisualRate();
    if (! isShowing())
        return;
    if (const auto colourVersion = processor.layerStructureVersion.load(); colourVersion != lastColourVersion)
    {
        lastColourVersion = colourVersion;
        InstrumentMenu::applyColours (processor);
        for (auto& strip : strips)
            strip->refreshColour();
    }
    const auto fxMask = processor.fxChannelUsageMask();
    if (processor.activeInstruments() != lastActive || fxMask != lastFxMask)
    {
        lastActive = processor.activeInstruments();
        lastFxMask = fxMask;
        resized();
    }
    for (size_t i = 0; i < strips.size(); ++i)
        if (strips[i]->isVisible())
            strips[i]->updateMeter();
    master.updateMeter();
}

void MixerView::resized()
{
    auto area = getLocalBounds();
    master.setBounds (area.removeFromRight (stripWidth));
    area.removeFromRight (8);
    viewport.setBounds (area);

    const auto fxMask = processor.fxChannelUsageMask();
    auto count = 0, gaps = 0;
    for (size_t i = 0; i < strips.size(); ++i)
        if (inUse (processor, (int) i, fxMask))
            ++count;
    for (int i = Mixer::firstFxSource; i < Mixer::numSources; ++i)
        if (inUse (processor, i, fxMask))
        {
            gaps = 8;
            break;
        }
    const auto wanted = count * stripWidth + gaps;
    const auto scrolling = wanted > area.getWidth();
    const auto height = area.getHeight() - (scrolling ? viewport.getScrollBarThickness() : 0);
    content.setSize (std::max (wanted, area.getWidth()), height);

    // Kick och instrument, ett litet glapp, sedan effektkanalerna som används.
    auto x = 0;
    auto gapDone = false;
    for (size_t i = 0; i < strips.size(); ++i)
    {
        const auto used = inUse (processor, (int) i, fxMask);
        strips[i]->setVisible (used);
        if (! used)
            continue;
        if (! gapDone && (int) i >= Mixer::firstFxSource)
        {
            x += 8;
            gapDone = true;
        }
        strips[i]->setBounds (x, 0, stripWidth, height);
        x += stripWidth;
    }
}

//==============================================================================
SourceToggles::SourceToggles (PluginBase& p, const juce::String& fieldIn, const juce::String& titleIn) : processor (p), field (fieldIn), title (titleIn)
{
    if (field != "Duck") // kicken ducks inte sig själv
    {
        kickButton = std::make_unique<juce::TextButton> (labelOf (p, Mixer::kickSource));
        styleToggle (*kickButton, mixerSourceColour (Mixer::kickSource), "KICK - " + Help::forParameter (Mixer::paramID (Mixer::kickSource, field)));
        addAndMakeVisible (*kickButton);
        kickAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Mixer::paramID (Mixer::kickSource, field), *kickButton);
    }

    instrumentsButton.setColour (juce::TextButton::buttonColourId, Theme::layerColour (0).withAlpha (0.25f));
    instrumentsButton.setTooltip ("The instruments: click to choose which of them this applies to. The menu stays open so you can tick several.");
    fxButton.setColour (juce::TextButton::buttonColourId, Theme::effects.withAlpha (0.25f));
    fxButton.setTooltip ("The effect channels (FX1-FX12): click to choose which of them this applies to. The menu stays open so you can tick several.");
    for (auto* button : { &instrumentsButton, &fxButton })
    {
        button->setColour (juce::TextButton::textColourOffId, Theme::text);
        addAndMakeVisible (*button);
    }
    instrumentsButton.onClick = [this] { showMenu (true); };
    fxButton.onClick = [this] { showMenu (false); };
    startTimerHz (4);
}

int SourceToggles::onCount (bool instruments) const
{
    auto count = 0;
    const auto first = instruments ? Mixer::firstBassSource : Mixer::firstFxSource;
    const auto end = instruments ? Mixer::firstBassSource + processor.activeInstruments() : Mixer::numSources;
    for (int s = first; s < end; ++s)
        if (processor.apvts.getRawParameterValue (Mixer::paramID (s, field))->load() >= 0.5f)
            ++count;
    return count;
}

void SourceToggles::updateTexts()
{
    const auto arrow = juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe"));
    instrumentsButton.setButtonText ("INSTRUMENTS  " + juce::String (lastInstrumentsOn) + "/" + juce::String (processor.activeInstruments()) + "  " + arrow);
    fxButton.setButtonText ("FX CHANNELS  " + juce::String (lastFxOn) + "/" + juce::String (Mixer::numSources - Mixer::firstFxSource) + "  " + arrow);
}

void SourceToggles::showMenu (bool instruments, int initiallySelected)
{
    const auto mask = processor.fxChannelUsageMask();
    const auto first = instruments ? Mixer::firstBassSource : Mixer::firstFxSource;
    const auto end = instruments ? Mixer::firstBassSource + processor.activeInstruments() : Mixer::numSources;
    juce::PopupMenu menu;
    for (int s = first; s < end; ++s)
    {
        const auto used = instruments || (mask & (1 << (s - Mixer::firstFxSource))) != 0;
        menu.addItem (s + 1, labelOf (processor, s) + (used ? "" : "   (not used yet)"), true,
                      processor.apvts.getRawParameterValue (Mixer::paramID (s, field))->load() >= 0.5f);
    }
    menu.addSeparator();
    menu.addItem (1000, "All on");
    menu.addItem (1001, "All off");

    juce::Component::SafePointer<SourceToggles> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (instruments ? &instrumentsButton : &fxButton).withInitiallySelectedItem (initiallySelected), [safeThis, instruments, first, end] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        auto set = [safeThis] (int source, bool on)
        {
            if (auto* parameter = safeThis->processor.apvts.getParameter (Mixer::paramID (source, safeThis->field)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
                parameter->endChangeGesture();
            }
        };
        if (result >= 1000)
        {
            for (int s = first; s < end; ++s)
                set (s, result == 1000);
        }
        else
        {
            const auto source = result - 1;
            set (source, safeThis->processor.apvts.getRawParameterValue (Mixer::paramID (source, safeThis->field))->load() < 0.5f);
        }
        safeThis->timerCallback();
        juce::MessageManager::callAsync ([safeThis, instruments, result] { if (safeThis != nullptr) safeThis->showMenu (instruments, result); }); // öppnas igen: flera kan kryssas i
    });
}

void SourceToggles::timerCallback()
{
    const auto instrumentsOn = onCount (true), fxOn = onCount (false);
    if (instrumentsOn != lastInstrumentsOn || fxOn != lastFxOn || processor.activeInstruments() != lastActive)
    {
        lastInstrumentsOn = instrumentsOn;
        lastFxOn = fxOn;
        lastActive = processor.activeInstruments();
        updateTexts();
    }
}

void SourceToggles::paint (juce::Graphics& g)
{
    g.setFont (Theme::font (10.0f, true));
    g.setColour (Theme::textDim);
    g.drawText (title, getLocalBounds().removeFromLeft (150), juce::Justification::centredLeft);
}

void SourceToggles::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft (150);
    if (kickButton != nullptr)
        kickButton->setBounds (area.removeFromLeft (62).reduced (2, 0));
    instrumentsButton.setBounds (area.removeFromLeft (170).reduced (2, 0));
    fxButton.setBounds (area.removeFromLeft (170).reduced (2, 0));
    if (lastFxOn < 0)
        timerCallback();
}
