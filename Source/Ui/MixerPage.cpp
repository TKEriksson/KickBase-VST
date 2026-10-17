#include "MixerPage.h"
#include "Help.h"

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
    for (int s = 0; s < Mixer::numSources; ++s)
    {
        strips.push_back (std::make_unique<MixerStrip> (p, s));
        addAndMakeVisible (*strips.back());
    }
    addAndMakeVisible (master);
    startTimerHz (Settings::animationHz (30));
}

void MixerView::timerCallback()
{
    if (! isShowing())
        return;
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
    const auto fxMask = processor.fxChannelUsageMask();
    auto count = 1;
    for (size_t i = 0; i < strips.size(); ++i)
        if (inUse (processor, (int) i, fxMask))
            ++count;
    const auto width = std::min (70, (area.getWidth() - 16) / count);

    // Kick och instrument, ett litet glapp, sedan effektkanalerna som används och mastern.
    auto gapDone = false;
    for (size_t i = 0; i < strips.size(); ++i)
    {
        const auto used = inUse (processor, (int) i, fxMask);
        strips[i]->setVisible (used);
        if (! used)
            continue;
        if (! gapDone && (int) i >= Mixer::firstFxSource)
        {
            area.removeFromLeft (8);
            gapDone = true;
        }
        strips[i]->setBounds (area.removeFromLeft (width));
    }
    area.removeFromLeft (8);
    master.setBounds (area.removeFromLeft (width));
}

//==============================================================================
SourceToggles::SourceToggles (PluginBase& p, const juce::String& fieldIn, const juce::String& titleIn) : processor (p), field (fieldIn), title (titleIn)
{
    for (int s = 0; s < Mixer::firstFxSource; ++s)
    {
        if (field == "Duck" && ! Mixer::hasDuck (s))
            continue;

        auto button = std::make_unique<juce::TextButton> (labelOf (p, s));
        styleToggle (*button, mixerSourceColour (s), Mixer::sourceName (s) + " - " + Help::forParameter (Mixer::paramID (s, field)));
        addAndMakeVisible (*button);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Mixer::paramID (s, field), *button));
        buttons.push_back (std::move (button));
        sourceOfButton.push_back (s);
    }

    fxButton.setColour (juce::TextButton::buttonColourId, Theme::effects.withAlpha (0.25f));
    fxButton.setColour (juce::TextButton::textColourOffId, Theme::text);
    fxButton.setTooltip ("The effect channels (FX1-FX11): click to choose which of them this applies to. The menu stays open so you can tick several.");
    fxButton.onClick = [this] { showFxMenu(); };
    addAndMakeVisible (fxButton);
    startTimerHz (4);
}

int SourceToggles::fxOnCount() const
{
    auto count = 0;
    for (int s = Mixer::firstFxSource; s < Mixer::numSources; ++s)
        if (processor.apvts.getRawParameterValue (Mixer::paramID (s, field))->load() >= 0.5f)
            ++count;
    return count;
}

void SourceToggles::showFxMenu()
{
    const auto mask = processor.fxChannelUsageMask();
    juce::PopupMenu menu;
    for (int s = Mixer::firstFxSource; s < Mixer::numSources; ++s)
    {
        const auto used = (mask & (1 << (s - Mixer::firstFxSource))) != 0;
        menu.addItem (s + 1, Mixer::sourceName (s) + (used ? "" : "   (not used yet)"), true,
                      processor.apvts.getRawParameterValue (Mixer::paramID (s, field))->load() >= 0.5f);
    }
    menu.addSeparator();
    menu.addItem (1000, "All on");
    menu.addItem (1001, "All off");

    juce::Component::SafePointer<SourceToggles> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fxButton), [safeThis] (int result)
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
            for (int s = Mixer::firstFxSource; s < Mixer::numSources; ++s)
                set (s, result == 1000);
        }
        else
        {
            const auto source = result - 1;
            set (source, safeThis->processor.apvts.getRawParameterValue (Mixer::paramID (source, safeThis->field))->load() < 0.5f);
        }
        safeThis->timerCallback();
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showFxMenu(); }); // öppnas igen: flera kan kryssas i
    });
}

void SourceToggles::timerCallback()
{
    const auto fxOn = fxOnCount();
    if (fxOn != lastFxOn)
    {
        lastFxOn = fxOn;
        fxButton.setButtonText ("FX CHANNELS  " + juce::String (fxOn) + "/" + juce::String (Mixer::numSources - Mixer::firstFxSource) + "  " + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")));
    }
    if (processor.activeInstruments() == lastActive && processor.layerNamesVersion.load() == lastNamesVersion)
        return;
    lastActive = processor.activeInstruments();
    lastNamesVersion = processor.layerNamesVersion.load();
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setButtonText (labelOf (processor, sourceOfButton[i]));
    resized();
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
    constexpr int fxWidth = 150;
    auto used = 0;
    for (size_t i = 0; i < buttons.size(); ++i)
        used += inUse (processor, sourceOfButton[i], 0) ? 1 : 0;
    const auto width = std::min (62, (area.getWidth() - fxWidth) / std::max (1, used));
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto shown = inUse (processor, sourceOfButton[i], 0);
        buttons[i]->setVisible (shown);
        if (shown)
            buttons[i]->setBounds (area.removeFromLeft (width).reduced (2, 0));
    }
    area.removeFromLeft (6);
    fxButton.setBounds (area.removeFromLeft (fxWidth).reduced (2, 0));
    if (lastFxOn < 0)
        timerCallback();
}
