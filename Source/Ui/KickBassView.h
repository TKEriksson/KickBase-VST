#pragma once

#include "../Core/MixAnalysis.h"
#include "../Core/PluginBase.h"
#include "Theme.h"

// ANALYZER > KICK+BASS: hur kick och bas läggs ihop. Vågformen för ett slag (kick, bas och summan, summan färgad efter fas: grönt när
// de drar åt samma håll, rött när de släcker ut varandra), korrelationen per 16-del som en stapelrad, och spektrumet 20-300 Hz.
// Rent läsande: visar den senaste analysens mått (MixAnalysis::PhaseMetrics), ANALYSE startar en ny. Timern går bara när vyn syns.
class KickBassView : public juce::Component, private juce::Timer
{
public:
    explicit KickBassView (PluginBase& p) : processor (p), analyser (p.getMixAnalyser())
    {
        analyseButton.setTooltip ("Play the whole pattern offline and measure how the kick and the bass add up (the same analysis as REPORT).");
        freezeButton.setTooltip ("Keep what is shown now, even when a new analysis finishes. Useful when comparing before and after a change.");
        freezeButton.setClickingTogglesState (true);
        analyseButton.onClick = [this] { startOrCancel(); };
        addAndMakeVisible (analyseButton);
        addAndMakeVisible (freezeButton);
    }

    ~KickBassView() override { stopTimer(); }

    void visibilityChanged() override { updateRunning(); }
    void parentHierarchyChanged() override { updateRunning(); }

    void resized() override
    {
        auto top = getLocalBounds().removeFromTop (28).reduced (4, 2);
        analyseButton.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (6);
        freezeButton.setBounds (top.removeFromLeft (90));
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().withTrimmedTop (30).reduced (6);
        if (! phase.valid || phase.scopeKick.empty())
        {
            g.setColour (Theme::textDim);
            g.setFont (14.0f);
            g.drawFittedText (analysing ? "Analysing..." : "Press ANALYSE to measure how the kick and the bass add up.", area, juce::Justification::centred, 2);
            return;
        }
        auto summary = area.removeFromBottom (22);
        auto scope = area.removeFromTop (area.getHeight() * 45 / 100);
        area.removeFromTop (6);
        auto correlation = area.removeFromTop (46);
        area.removeFromTop (6);
        paintScope (g, scope);
        paintCorrelation (g, correlation);
        paintSpectrum (g, area);

        g.setColour (phase.sumVsMaxDb < -3.0f ? Theme::master : Theme::textDim);
        g.setFont (12.0f);
        g.drawText ("Sum vs the largest single peak: " + juce::String (phase.sumVsMaxDb, 1) + " dB    Clicks: " + juce::String (phase.clickHits)
                        + "    Kick peak spread: " + juce::String (phase.peakSpreadDb, 1) + " dB",
                    summary, juce::Justification::centredLeft);
    }

private:
    void updateRunning()
    {
        if (isShowing())
            startTimerHz (10);
        else
            stopTimer();
    }

    void startOrCancel()
    {
        if (analyser.isRunning())
        {
            analyser.cancel();
            return;
        }
        processor.requestAudition (PluginBase::previewStop);
        MixAnalysis::Options options;
        if (analyser.start (processor, options, nullptr, {}))
        {
            analysing = true;
            analyseButton.setButtonText ("CANCEL");
            repaint();
        }
    }

    void timerCallback() override
    {
        if (analysing)
            analyseButton.setButtonText ("CANCEL " + juce::String (juce::roundToInt (analyser.getProgress() * 100.0f)) + " %");
        if (analysing && ! analyser.isRunning())
        {
            analysing = false;
            analyseButton.setButtonText ("ANALYSE");
        }
        if (analyser.isRunning() || analyser.getVersion() == shownVersion)
        {
            if (! analyser.isRunning() && ! analysing)
                repaint();
            return;
        }
        shownVersion = analyser.getVersion();
        if (freezeButton.getToggleState())
            return;
        const auto report = analyser.getReport();
        if (report.valid && report.capture != nullptr)
        {
            capture = report.capture; // håller rådatan vid liv medan vyn visar den
            phase = capture->phase;
        }
        repaint();
    }

    static juce::Colour correlationColour (float c)
    {
        return c >= 0.0f ? Theme::mix.interpolatedWith (juce::Colour (0xffe8d35c), 1.0f - juce::jlimit (0.0f, 1.0f, c * 1.25f))
                         : juce::Colour (0xffe8d35c).interpolatedWith (Theme::master, juce::jlimit (0.0f, 1.0f, -c * 2.0f));
    }

    void paintScope (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 4.0f);
        const auto n = (int) std::min (phase.scopeKick.size(), phase.scopeBass.size());
        if (n < 2)
            return;
        auto peak = 1.0e-4f;
        for (int i = 0; i < n; ++i)
            peak = std::max ({ peak, std::abs (phase.scopeKick[(size_t) i]), std::abs (phase.scopeBass[(size_t) i]), std::abs (phase.scopeKick[(size_t) i] + phase.scopeBass[(size_t) i]) });
        const auto plot = area.toFloat().reduced (6.0f, 4.0f);
        const auto mid = plot.getCentreY(), half = plot.getHeight() * 0.5f;
        g.setColour (Theme::grid);
        g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());
        auto xFor = [&] (int i) { return plot.getX() + plot.getWidth() * (float) i / (float) (n - 1); };
        auto yFor = [&] (float v) { return mid - half * v / peak; };

        juce::Path kickPath, bassPath;
        for (int i = 0; i < n; ++i)
        {
            const auto kx = xFor (i);
            if (i == 0)
            {
                kickPath.startNewSubPath (kx, yFor (phase.scopeKick[0]));
                bassPath.startNewSubPath (kx, yFor (phase.scopeBass[0]));
            }
            else
            {
                kickPath.lineTo (kx, yFor (phase.scopeKick[(size_t) i]));
                bassPath.lineTo (kx, yFor (phase.scopeBass[(size_t) i]));
            }
        }
        g.setColour (Theme::kick.withAlpha (0.55f));
        g.strokePath (kickPath, juce::PathStrokeType (1.0f));
        g.setColour (Theme::bass.withAlpha (0.55f));
        g.strokePath (bassPath, juce::PathStrokeType (1.0f));

        // Summan, färgad efter fas: samma tecken = grönt, motsatta = rött.
        for (int i = 1; i < n; ++i)
        {
            const auto a = phase.scopeKick[(size_t) i], b = phase.scopeBass[(size_t) i];
            g.setColour (a * b >= 0.0f ? Theme::mix : Theme::master);
            g.drawLine (xFor (i - 1), yFor (phase.scopeKick[(size_t) i - 1] + phase.scopeBass[(size_t) i - 1]), xFor (i), yFor (a + b), 2.0f);
        }
        g.setColour (Theme::textDim);
        g.setFont (11.0f);
        g.drawText ("KICK", area.reduced (8, 4), juce::Justification::topLeft);
        g.setColour (Theme::bass);
        g.drawText ("BASS", area.reduced (48, 4), juce::Justification::topLeft);
        g.setColour (Theme::mix);
        g.drawText ("SUM (green = in phase, red = cancelling)", area.reduced (90, 4), juce::Justification::topLeft);
        g.setColour (Theme::textDim);
        g.drawText (juce::String (juce::roundToInt ((float) n / (float) phase.scopeRate * 1000.0f)) + " ms", area.reduced (8, 4), juce::Justification::topRight);
    }

    void paintCorrelation (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        g.setColour (Theme::textDim);
        g.setFont (11.0f);
        g.drawText ("Kick/bass correlation per 16th (30-150 Hz): green = adds up, red = cancels", area.removeFromTop (14), juce::Justification::centredLeft);
        const auto cells = (int) phase.lowCorrelation.size();
        if (cells <= 0)
            return;
        const auto w = area.getWidth() / (float) cells;
        for (int i = 0; i < cells; ++i)
        {
            auto cell = juce::Rectangle<float> ((float) area.getX() + w * (float) i, (float) area.getY(), w - 1.0f, (float) area.getHeight()).reduced (0.0f, 1.0f);
            const auto active = phase.lowActive[(size_t) i] != 0;
            g.setColour (active ? correlationColour (phase.lowCorrelation[(size_t) i]) : Theme::track);
            g.fillRect (cell);
            if (i % 4 == 0)
            {
                g.setColour (Theme::backgroundBottom);
                g.fillRect (cell.withWidth (1.5f));
            }
        }
    }

    void paintSpectrum (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 4.0f);
        const auto count = (int) std::min ({ phase.spectrumKick.size(), phase.spectrumBass.size(), phase.spectrumSum.size() });
        if (count < 2)
            return;
        const auto plot = area.toFloat().reduced (30.0f, 8.0f);
        const auto lowHz = 20.0f, highHz = 300.0f;
        auto xFor = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / lowHz) / std::log (highHz / lowHz); };
        auto yFor = [&] (float db) { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f); };
        g.setFont (10.0f);
        for (const float hz : { 20.0f, 30.0f, 50.0f, 100.0f, 200.0f, 300.0f })
        {
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) xFor (hz), plot.getY(), plot.getBottom());
            g.setColour (Theme::textDim);
            g.drawText (juce::String ((int) hz), juce::Rectangle<float> (xFor (hz) - 14.0f, plot.getBottom() + 1.0f, 28.0f, 12.0f), juce::Justification::centred);
        }
        auto curve = [&] (const std::vector<float>& db, juce::Colour colour, float thickness)
        {
            juce::Path path;
            for (int i = 0; i < count; ++i)
            {
                const auto x = xFor (phase.spectrumHz0 + phase.spectrumStepHz * (float) i), y = yFor (db[(size_t) i]);
                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (thickness));
        };
        curve (phase.spectrumKick, Theme::kick.withAlpha (0.8f), 1.2f);
        curve (phase.spectrumBass, Theme::bass.withAlpha (0.8f), 1.2f);
        curve (phase.spectrumSum, Theme::text, 1.8f);
    }

    PluginBase& processor;
    MixAnalyser& analyser;
    juce::TextButton analyseButton { "ANALYSE" }, freezeButton { "FREEZE" };
    std::shared_ptr<const MixAnalysis::Capture> capture;
    MixAnalysis::PhaseMetrics phase;
    int shownVersion = 0;
    bool analysing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickBassView)
};
