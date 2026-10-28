#pragma once

#include "CollisionsView.h"
#include "HelpDialog.h"
#include "KickBassView.h"
#include "SpectrumPage.h"

// Toppfliken ANALYZER: underflikar [KICK+BASS] [COLLISIONS] [SPECTRUM] [REPORT]. SPECTRUM och REPORT är SpectrumPage:ns två vyer
// (oförändrade), COLLISIONS är CollisionsView (fynden som lista, band och takter markerade). KICK+BASS är KickBassView (vågform, fas, korrelation, 20-300 Hz).
class AnalyzerPage : public juce::Component
{
public:
    explicit AnalyzerPage (PluginBase& processor) : kickBass (processor), collisions (processor), spectrum (processor)
    {
        const char* names[] = { "KICK+BASS", "COLLISIONS", "SPECTRUM", "REPORT" };
        const char* tips[] = { "KICK+BASS - How the kick and the bass add up: waveform, phase and correlation per 16th.",
                               "COLLISIONS - A list of where the sounds collide or cancel each other (including phase problems between kick and bass), with the band and bars marked.",
                               "SPECTRUM - The live spectrum of the kick, the instruments and the master, with harmonics and the EQ.",
                               "REPORT - Analyse the whole pattern and read the report: what to look at, and what an AI suggests or has fixed." };
        for (int i = 0; i < numViews; ++i)
        {
            auto& b = buttons[(size_t) i];
            b.setButtonText (names[i]);
            b.setTooltip (tips[i]);
            b.setClickingTogglesState (false);
            b.getProperties().set ("subtab", true); // remsa-stil som instrumentets flikrad, texten krymper aldrig
            b.getProperties().set ("noshrink", true);
            b.setColour (juce::TextButton::buttonOnColourId, Theme::lfo.withAlpha (0.55f));
            b.onClick = [this, i] { setCurrentView (i); };
            addAndMakeVisible (b);
        }
        HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::analyzer, "the ANALYZER tab");
        addAndMakeVisible (helpButton);
        addChildComponent (kickBass);
        addChildComponent (collisions);
        spectrum.setEmbedded();
        addChildComponent (spectrum);
        setCurrentView (juce::SystemStats::getEnvironmentVariable ("KB_ANALYZER", "2").getIntValue()); // snapshot-verktyget väljer underflik
    }

    void setCurrentView (int index)
    {
        view = juce::jlimit (0, numViews - 1, index);
        for (int i = 0; i < numViews; ++i)
            buttons[(size_t) i].setToggleState (i == view, juce::dontSendNotification);
        kickBass.setVisible (view == 0);
        collisions.setVisible (view == 1);
        spectrum.setVisible (view >= 2);
        if (view >= 2)
            spectrum.setView (view == 3);
        resized();
        if (auto* parent = getParentComponent())
            parent->repaint(); // brödsmulan i rad 1 ritas av flikpanelen
    }

    void resized() override
    {
        auto area = getLocalBounds();
        stripArea = area.removeFromTop (36);
        auto strip = stripArea.reduced (6, 3);
        helpButton.setBounds (strip.removeFromRight (48));
        for (auto& b : buttons)
        {
            auto font = getLookAndFeel().getTextButtonFont (b, 30).boldened().withHeight (11.0f);
            font.setExtraKerningFactor (0.04f);
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (font, b.getButtonText().toUpperCase(), 0.0f, 0.0f);
            b.setBounds (strip.removeFromLeft ((int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 2 * 18 + 10));
            strip.removeFromLeft (8);
        }
        area.removeFromTop (4);
        kickBass.setBounds (area);
        collisions.setBounds (area);
        spectrum.setBounds (area);
    }

    void paint (juce::Graphics& g) override
    {
        const auto strip = stripArea.toFloat();
        g.setColour (Theme::insetStrip);
        g.fillRoundedRectangle (strip, 12.0f);
        g.setColour (Theme::panelBorderDim);
        g.drawRoundedRectangle (strip.reduced (0.5f), 12.0f, 1.0f);
    }

    juce::String currentViewName() const { return buttons[(size_t) view].getButtonText(); } // för brödsmulan i rad 1

private:
    juce::Rectangle<int> stripArea;
    static constexpr int numViews = 4;
    std::array<juce::TextButton, numViews> buttons;
    juce::TextButton helpButton;
    KickBassView kickBass;
    CollisionsView collisions;
    SpectrumPage spectrum;
    int view = 2;
};
