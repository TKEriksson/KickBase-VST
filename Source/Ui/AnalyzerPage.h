#pragma once

#include "CollisionsView.h"
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
            b.setColour (juce::TextButton::buttonOnColourId, Theme::lfo.withAlpha (0.55f));
            b.onClick = [this, i] { setCurrentView (i); };
            addAndMakeVisible (b);
        }
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
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto strip = area.removeFromTop (26);
        for (auto& b : buttons)
        {
            b.setBounds (strip.removeFromLeft (110));
            strip.removeFromLeft (4);
        }
        area.removeFromTop (2);
        kickBass.setBounds (area);
        collisions.setBounds (area);
        spectrum.setBounds (area);
    }

private:
    static constexpr int numViews = 4;
    std::array<juce::TextButton, numViews> buttons;
    KickBassView kickBass;
    CollisionsView collisions;
    SpectrumPage spectrum;
    int view = 2;
};
