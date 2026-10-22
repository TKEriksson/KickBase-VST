#pragma once

#include "../Core/MixAnalysis.h"
#include "../Core/PluginBase.h"
#include "Theme.h"

// ANALYZER > KICK+BASS: hur kick och bas läggs ihop. Vågformen för ett slag (kick, bas och summan, summan färgad efter fas: grönt när
// de drar åt samma håll, rött när de släcker ut varandra), korrelationen per 16-del som en stapelrad (grönt = adderar, blått = neutralt,
// rött = släcker ut) och spektrumet 20-300 Hz. Under bilden står åtgärdstips ur måtten.
// Rent läsande: visar den senaste analysens mått (MixAnalysis::PhaseMetrics), ANALYSE startar en ny. Varje analys sparas automatiskt
// som en ruta (högst sex, CLEAR rensar); SNAPSHOT sparar en extra kopia. Klicka på en ruta för att se den, med senaste analysen som spöke.
// Timern går bara när vyn syns.
class KickBassView : public juce::Component, private juce::Timer
{
public:
    explicit KickBassView (PluginBase& p) : processor (p), analyser (p.getMixAnalyser())
    {
        analyseButton.setTooltip ("Play the whole pattern offline and measure how the kick and the bass add up (the same analysis as REPORT).");
        snapshotButton.setTooltip ("Save what is shown now as an extra snapshot. Every new analysis is also added automatically (up to " + juce::String (maxSnapshots) + " total). Click a box to view it; the latest analysis is the rightmost box.");
        clearButton.setTooltip ("Remove all saved snapshots.");
        analyseButton.onClick = [this] { startOrCancel(); };
        snapshotButton.onClick = [this] { takeSnapshot(); };
        clearButton.onClick = [this]
        {
            snapshots.clear();
            selected = -1;
            updateButtons();
            repaint();
        };
        addAndMakeVisible (analyseButton);
        addAndMakeVisible (snapshotButton);
        addAndMakeVisible (clearButton);
        updateButtons();
    }

    ~KickBassView() override { stopTimer(); }

    void visibilityChanged() override { updateRunning(); }
    void parentHierarchyChanged() override { updateRunning(); }

    void resized() override
    {
        auto top = getLocalBounds().removeFromTop (28).reduced (4, 2);
        analyseButton.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (6);
        snapshotButton.setBounds (top.removeFromLeft (100));
        top.removeFromLeft (6);
        clearButton.setBounds (top.removeFromLeft (64));
        thumbsX = top.getX() + 10;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < (int) snapshots.size(); ++i)
            if (thumbRect (i).contains (e.getPosition()))
            {
                selected = selected == i ? -1 : i;
                updateButtons();
                repaint();
                return;
            }
    }

    void paint (juce::Graphics& g) override
    {
        paintThumbs (g);
        auto area = getLocalBounds().withTrimmedTop (30).reduced (6);
        const auto viewing = selected >= 0 && selected < (int) snapshots.size();
        const auto& m = viewing ? snapshots[(size_t) selected].phase : phase;
        const auto* ghost = viewing && phase.valid && ! phase.scopeKick.empty() ? &phase : nullptr;
        if (! m.valid || m.scopeKick.empty())
        {
            g.setColour (Theme::textDim);
            g.setFont (14.0f);
            g.drawFittedText (analysing ? "Analysing..." : "Press ANALYSE to measure how the kick and the bass add up.", area, juce::Justification::centred, 2);
            return;
        }
        auto tipsArea = area.removeFromBottom (maxTips * tipHeight);
        auto summary = area.removeFromBottom (18);
        auto scope = area.removeFromTop (area.getHeight() * 42 / 100);
        area.removeFromTop (6);
        auto correlation = area.removeFromTop (46);
        area.removeFromTop (6);
        paintScope (g, scope, m, ghost);
        paintCorrelation (g, correlation, m);
        paintSpectrum (g, area, m, ghost);

        g.setColour (m.sumVsMaxDb < -3.0f ? Theme::master : Theme::textDim);
        g.setFont (12.0f);
        const auto prefix = viewing ? "SNAPSHOT #" + juce::String (snapshots[(size_t) selected].number) + " (click it again for the latest)    " : juce::String();
        g.drawText (prefix + "Sum vs the largest single peak: " + juce::String (m.sumVsMaxDb, 1) + " dB    Clicks: " + juce::String (m.clickHits)
                        + "    Kick peak spread: " + juce::String (m.peakSpreadDb, 1) + " dB",
                    summary, juce::Justification::centredLeft);
        paintTips (g, tipsArea, m);
    }

private:
    static constexpr int maxSnapshots = 6, maxTips = 3, tipHeight = 15, thumbWidth = 104;

    struct Snapshot
    {
        MixAnalysis::PhaseMetrics phase;
        int number = 0;
    };

    // Grönt = adderar, blått = neutralt, rött = släcker ut. Ingen gul: den läses som en varning.
    static juce::Colour neutralColour() { return juce::Colour (0xff5a8cff); }

    static juce::Colour correlationColour (float c)
    {
        if (c >= 0.0f)
            return neutralColour().interpolatedWith (Theme::mix, juce::jlimit (0.0f, 1.0f, c / 0.5f));
        return neutralColour().interpolatedWith (Theme::master, juce::jlimit (0.0f, 1.0f, (-c - 0.1f) / 0.3f));
    }

    static bool isRed (float c) { return c < -0.1f; }

    void updateRunning()
    {
        if (isShowing())
            startTimerHz (10);
        else
            stopTimer();
    }

    bool viewingSnapshot() const { return selected >= 0 && selected < (int) snapshots.size(); }

    void updateButtons()
    {
        snapshotButton.setEnabled (phase.valid || viewingSnapshot());
        clearButton.setEnabled (! snapshots.empty());
    }

    void takeSnapshot()
    {
        const auto& m = viewingSnapshot() ? snapshots[(size_t) selected].phase : phase;
        if (! m.valid)
            return;
        auto copy = m;
        if ((int) snapshots.size() >= maxSnapshots)
            eraseOldestSnapshot();
        snapshots.push_back ({ std::move (copy), ++snapshotCounter });
        selected = -1;
        updateButtons();
        repaint();
    }

    void eraseOldestSnapshot()
    {
        snapshots.erase (snapshots.begin());
        if (selected == 0)
            selected = -1;
        else if (selected > 0)
            --selected;
    }

    void rememberAnalysis (const MixAnalysis::PhaseMetrics& result)
    {
        if ((int) snapshots.size() >= maxSnapshots)
            eraseOldestSnapshot();
        snapshots.push_back ({ result, ++snapshotCounter });
        updateButtons();
    }

    juce::Rectangle<int> thumbRect (int i) const { return { thumbsX + i * (thumbWidth + 4), 3, thumbWidth, 24 }; }

    void paintThumbs (juce::Graphics& g) const
    {
        for (int i = 0; i < (int) snapshots.size(); ++i)
        {
            const auto& s = snapshots[(size_t) i];
            const auto r = thumbRect (i);
            g.setColour (Theme::panel);
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
            g.setColour (i == selected ? Theme::mix : Theme::grid);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, i == selected ? 1.6f : 1.0f);
            g.setColour (s.phase.sumVsMaxDb < -3.0f ? Theme::master : Theme::textDim);
            g.setFont (10.5f);
            const auto label = i == (int) snapshots.size() - 1 ? "LATEST " : "#" + juce::String (s.number) + " ";
            g.drawText (label + juce::String (s.phase.sumVsMaxDb, 1) + " dB", r.reduced (4, 1).removeFromTop (12), juce::Justification::centredLeft);
            const auto cells = (int) std::min (s.phase.lowCorrelation.size(), s.phase.lowActive.size());
            if (cells <= 0)
                continue;
            const auto strip = r.toFloat().reduced (4.0f, 0.0f).withTrimmedTop (14.0f).withTrimmedBottom (3.0f);
            const auto w = strip.getWidth() / (float) cells;
            for (int c = 0; c < cells; ++c)
            {
                g.setColour (s.phase.lowActive[(size_t) c] != 0 ? correlationColour (s.phase.lowCorrelation[(size_t) c]) : Theme::track);
                g.fillRect (strip.getX() + w * (float) c, strip.getY(), std::max (1.0f, w - 0.5f), strip.getHeight());
            }
        }
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
        const auto report = analyser.getReport();
        if (report.valid && report.capture != nullptr)
        {
            capture = report.capture; // håller rådatan vid liv medan vyn visar den
            phase = capture->phase;
            if (phase.valid && ! phase.scopeKick.empty())
                rememberAnalysis (phase);
            else
                updateButtons();
        }
        repaint();
    }

    // Åtgärdstips ur måtten (högst maxTips rader). Namnen är reglagen i gränssnittet.
    static juce::StringArray tipsFor (const MixAnalysis::PhaseMetrics& m)
    {
        juce::StringArray tips;
        auto active = 0, red = 0;
        for (size_t i = 0; i < m.lowCorrelation.size() && i < m.lowActive.size(); ++i)
            if (m.lowActive[i] != 0)
            {
                ++active;
                red += isRed (m.lowCorrelation[i]) ? 1 : 0;
            }
        if (m.sumVsMaxDb < -3.0f)
            tips.add ("Kick and bass cancel at the low end: change the kick start PHASE or the bass OSC phase (RAND off), or high-pass the bass in its EQ.");
        if (red > 0)
            tips.add (juce::String (red) + " of " + juce::String (active) + " 16ths are red: shift the bass note a little in time, shorten it there, or duck it with SIDECHAIN.");
        if (m.clickHits > 0)
            tips.add (juce::String (m.clickHits) + " kick(s) still sound above -60 dB when the next starts and click: let the kick AMP envelope reach 0 at its last node, or shorten the kick.");
        if (m.peakSpreadDb > 3.0f)
            tips.add ("The kick peak varies " + juce::String (m.peakSpreadDb, 1) + " dB between hits: use a fixed kick PHASE (RAND off) so every hit starts the same.");
        if (tips.isEmpty())
            tips.add ("No problems found: kick and bass add up. Change something, press ANALYSE again, and use SNAPSHOT to compare.");
        while (tips.size() > maxTips)
            tips.remove (maxTips);
        return tips;
    }

    void paintTips (juce::Graphics& g, juce::Rectangle<int> area, const MixAnalysis::PhaseMetrics& m) const
    {
        const auto problem = m.sumVsMaxDb < -3.0f || m.clickHits > 0 || m.peakSpreadDb > 3.0f;
        g.setFont (12.0f);
        g.setColour (problem ? Theme::text : Theme::textDim);
        for (const auto& tip : tipsFor (m))
            g.drawText ("- " + tip, area.removeFromTop (tipHeight), juce::Justification::centredLeft);
    }

    void paintScope (juce::Graphics& g, juce::Rectangle<int> area, const MixAnalysis::PhaseMetrics& m, const MixAnalysis::PhaseMetrics* ghost) const
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 4.0f);
        const auto n = (int) std::min (m.scopeKick.size(), m.scopeBass.size());
        if (n < 2)
            return;
        auto peak = 1.0e-4f;
        for (int i = 0; i < n; ++i)
            peak = std::max ({ peak, std::abs (m.scopeKick[(size_t) i]), std::abs (m.scopeBass[(size_t) i]), std::abs (m.scopeKick[(size_t) i] + m.scopeBass[(size_t) i]) });
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
                kickPath.startNewSubPath (kx, yFor (m.scopeKick[0]));
                bassPath.startNewSubPath (kx, yFor (m.scopeBass[0]));
            }
            else
            {
                kickPath.lineTo (kx, yFor (m.scopeKick[(size_t) i]));
                bassPath.lineTo (kx, yFor (m.scopeBass[(size_t) i]));
            }
        }
        g.setColour (Theme::kick.withAlpha (0.55f));
        g.strokePath (kickPath, juce::PathStrokeType (1.0f));
        g.setColour (Theme::bass.withAlpha (0.55f));
        g.strokePath (bassPath, juce::PathStrokeType (1.0f));

        // Spöket: den senaste analysens summa, tunn och grå, i samma skala som bilden.
        if (ghost != nullptr)
        {
            const auto gn = (int) std::min (ghost->scopeKick.size(), ghost->scopeBass.size());
            juce::Path ghostPath;
            for (int i = 0; i < gn; ++i)
            {
                const auto x = plot.getX() + plot.getWidth() * (float) i / (float) std::max (1, gn - 1);
                const auto y = yFor (ghost->scopeKick[(size_t) i] + ghost->scopeBass[(size_t) i]);
                if (i == 0)
                    ghostPath.startNewSubPath (x, y);
                else
                    ghostPath.lineTo (x, y);
            }
            g.setColour (Theme::text.withAlpha (0.45f));
            g.strokePath (ghostPath, juce::PathStrokeType (1.0f));
        }

        // Summan, färgad efter fas: samma tecken = grönt, motsatta = rött.
        for (int i = 1; i < n; ++i)
        {
            const auto a = m.scopeKick[(size_t) i], b = m.scopeBass[(size_t) i];
            g.setColour (a * b >= 0.0f ? Theme::mix : Theme::master);
            g.drawLine (xFor (i - 1), yFor (m.scopeKick[(size_t) i - 1] + m.scopeBass[(size_t) i - 1]), xFor (i), yFor (a + b), 2.0f);
        }
        g.setColour (Theme::kick);
        g.setFont (11.0f);
        g.drawText ("KICK", area.reduced (8, 4), juce::Justification::topLeft);
        g.setColour (Theme::bass);
        g.drawText ("BASS", area.reduced (48, 4), juce::Justification::topLeft);
        g.setColour (Theme::mix);
        g.drawText ("SUM = kick + bass (green = in phase, red = cancelling)", area.reduced (90, 4), juce::Justification::topLeft);
        g.setColour (Theme::textDim);
        g.drawText ((ghost != nullptr ? juce::String ("grey line = latest analysis    ") : juce::String())
                        + juce::String (juce::roundToInt ((float) n / (float) m.scopeRate * 1000.0f)) + " ms",
                    area.reduced (8, 4), juce::Justification::topRight);
    }

    void paintCorrelation (juce::Graphics& g, juce::Rectangle<int> area, const MixAnalysis::PhaseMetrics& m) const
    {
        auto label = area.removeFromTop (14);
        g.setColour (Theme::textDim);
        g.setFont (11.0f);
        g.drawText ("Kick/bass correlation per 16th (30-150 Hz):", label.removeFromLeft (250), juce::Justification::centredLeft);
        const std::pair<juce::Colour, juce::String> legend[] = { { Theme::mix, "adds up (good)" }, { neutralColour(), "neutral" }, { Theme::master, "cancels (problem)" }, { Theme::track, "no kick+bass here" } };
        for (const auto& [colour, text] : legend)
        {
            g.setColour (colour);
            g.fillRect (label.removeFromLeft (10).toFloat().reduced (0.0f, 3.0f));
            label.removeFromLeft (4);
            g.setColour (Theme::textDim);
            g.drawText (text, label.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 6), juce::Justification::centredLeft);
            label.removeFromLeft (10);
        }
        const auto cells = (int) std::min (m.lowCorrelation.size(), m.lowActive.size());
        if (cells <= 0)
            return;
        const auto w = area.getWidth() / (float) cells;
        for (int i = 0; i < cells; ++i)
        {
            auto cell = juce::Rectangle<float> ((float) area.getX() + w * (float) i, (float) area.getY(), w - 1.0f, (float) area.getHeight()).reduced (0.0f, 1.0f);
            g.setColour (m.lowActive[(size_t) i] != 0 ? correlationColour (m.lowCorrelation[(size_t) i]) : Theme::track);
            g.fillRect (cell);
            if (i % 4 == 0)
            {
                g.setColour (Theme::backgroundBottom);
                g.fillRect (cell.withWidth (1.5f));
            }
        }
    }

    void paintSpectrum (juce::Graphics& g, juce::Rectangle<int> area, const MixAnalysis::PhaseMetrics& m, const MixAnalysis::PhaseMetrics* ghost) const
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 4.0f);
        const auto count = (int) std::min ({ m.spectrumKick.size(), m.spectrumBass.size(), m.spectrumSum.size() });
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
        auto curve = [&] (const MixAnalysis::PhaseMetrics& source, const std::vector<float>& db, juce::Colour colour, float thickness)
        {
            juce::Path path;
            const auto points = (int) std::min (db.size(), (size_t) count);
            for (int i = 0; i < points; ++i)
            {
                const auto x = xFor (source.spectrumHz0 + source.spectrumStepHz * (float) i), y = yFor (db[(size_t) i]);
                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (thickness));
        };
        curve (m, m.spectrumKick, Theme::kick.withAlpha (0.8f), 1.2f);
        curve (m, m.spectrumBass, Theme::bass.withAlpha (0.8f), 1.2f);
        if (ghost != nullptr)
            curve (*ghost, ghost->spectrumSum, Theme::text.withAlpha (0.4f), 1.0f);
        curve (m, m.spectrumSum, Theme::text, 1.8f);

        g.setFont (11.0f);
        auto legend = area.reduced (34, 5).removeFromTop (14);
        g.setColour (Theme::kick);
        g.drawText ("kick", legend.removeFromLeft (34), juce::Justification::centredLeft);
        g.setColour (Theme::bass);
        g.drawText ("bass", legend.removeFromLeft (34), juce::Justification::centredLeft);
        g.setColour (Theme::text);
        g.drawText ("sum", legend.removeFromLeft (32), juce::Justification::centredLeft);
        g.setColour (Theme::textDim);
        g.drawText ("Where the sum dips under both curves, kick and bass cancel at that frequency.", legend, juce::Justification::centredLeft);
    }

    PluginBase& processor;
    MixAnalyser& analyser;
    juce::TextButton analyseButton { "ANALYSE" }, snapshotButton { "SNAPSHOT" }, clearButton { "CLEAR" };
    std::shared_ptr<const MixAnalysis::Capture> capture;
    MixAnalysis::PhaseMetrics phase;
    std::vector<Snapshot> snapshots;
    int selected = -1, snapshotCounter = 0, thumbsX = 290;
    int shownVersion = 0;
    bool analysing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickBassView)
};
