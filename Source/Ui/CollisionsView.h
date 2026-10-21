#pragma once

#include "../Core/MixAnalysis.h"
#include "../Core/PluginBase.h"
#include "../Core/Settings.h"
#include "Theme.h"

// ANALYZER > COLLISIONS: fynden från senaste analysen som en lista (grön = info, gul = varning, röd = problem), en textlogg i varje rad,
// och till höger var det händer: frekvensbandet och takterna för den valda raden, över ett färgfält (frekvens x takt) där minst två
// källor ligger inom 12 dB från varandra. Färgfältet kan stängas av i SETTINGS > OPTIMIZATIONS. Rent läsande.
class CollisionsView : public juce::Component, private juce::Timer
{
public:
    explicit CollisionsView (PluginBase& p) : processor (p), analyser (p.getMixAnalyser())
    {
        analyseButton.setTooltip ("Play the whole pattern offline and list where the sounds collide (the same analysis as REPORT).");
        analyseButton.onClick = [this] { startOrCancel(); };
        addAndMakeVisible (analyseButton);
    }

    ~CollisionsView() override { stopTimer(); }

    void visibilityChanged() override { updateRunning(); }
    void parentHierarchyChanged() override { updateRunning(); }

    void resized() override
    {
        analyseButton.setBounds (getLocalBounds().removeFromTop (28).reduced (4, 2).removeFromLeft (110));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto row = (e.y - listArea().getY() + scroll) / rowHeight;
        if (listArea().contains (e.getPosition()) && juce::isPositiveAndBelow (row, (int) report.findings.size()))
            selected = row;
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const auto maxScroll = std::max (0, (int) report.findings.size() * rowHeight - listArea().getHeight());
        scroll = juce::jlimit (0, maxScroll, scroll - juce::roundToInt (wheel.deltaY * 120.0f));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! report.valid)
        {
            g.setColour (Theme::textDim);
            g.setFont (14.0f);
            g.drawFittedText (analysing ? "Analysing..." : "Press ANALYSE to list where the sounds collide.", getLocalBounds().withTrimmedTop (30).reduced (6),
                              juce::Justification::centred, 2);
            return;
        }
        paintList (g);
        paintField (g);
    }

private:
    static constexpr int rowHeight = 34, bandsInField = 48;
    static constexpr float fieldLowHz = 30.0f, fieldHighHz = 16000.0f;

    juce::Rectangle<int> listArea() const { return getLocalBounds().withTrimmedTop (32).withTrimmedRight (getWidth() * 55 / 100).reduced (4, 0); }
    juce::Rectangle<int> fieldArea() const { return getLocalBounds().withTrimmedTop (32).withTrimmedLeft (getWidth() * 45 / 100).reduced (6, 0); }

    static juce::Colour severityColour (MixAnalysis::Severity s)
    {
        return s == MixAnalysis::Severity::problem ? Theme::master : s == MixAnalysis::Severity::warning ? juce::Colour (0xffe8d35c) : Theme::mix;
    }

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
        const auto gradient = Settings::collisionGradient();
        if (gradient != gradientShown)
        {
            gradientShown = gradient;
            buildField();
            repaint();
        }
        if (analyser.isRunning() || analyser.getVersion() == shownVersion)
            return;
        shownVersion = analyser.getVersion();
        report = analyser.getReport();
        selected = report.findings.empty() ? -1 : 0;
        scroll = 0;
        fixes = report.capture != nullptr ? MixAnalysis::describePhase (report.capture->phase).fixes : juce::Array<juce::var>();
        buildField();
        repaint();
    }

    // Färgfältet: för varje takt och logaritmiskt band, hur många källor (kick och instrument) som ligger inom 12 dB från den starkaste
    // och inte mer än 45 dB under den starkaste cellen i hela loopen. 0 = tomt, 1 = en ensam, 2+ = krock.
    void buildField()
    {
        field.clear();
        fieldBars = 0;
        if (! gradientShown || ! report.valid || report.capture == nullptr || report.capture->barPower.empty())
            return;
        const auto& c = *report.capture;
        const auto bins = MixAnalysis::fftSize / 2;
        const auto binHz = (float) c.rate / (float) MixAnalysis::fftSize;
        fieldBars = (int) (c.barPower.size() / ((size_t) MixAnalysis::numSources * (size_t) bins));
        std::vector<float> db ((size_t) fieldBars * MixAnalysis::masterSource * bandsInField, -200.0f);
        auto dbAt = [&] (int bar, int src, int band) -> float& { return db[((size_t) bar * MixAnalysis::masterSource + (size_t) src) * bandsInField + (size_t) band]; };
        auto topDb = -200.0f;
        for (int bar = 0; bar < fieldBars; ++bar)
            for (int src = 0; src < MixAnalysis::masterSource; ++src)
            {
                if (! c.present[(size_t) src])
                    continue;
                const auto* power = &c.barPower[((size_t) bar * MixAnalysis::numSources + (size_t) src) * (size_t) bins];
                for (int band = 0; band < bandsInField; ++band)
                {
                    const auto lo = fieldLowHz * std::pow (fieldHighHz / fieldLowHz, (float) band / bandsInField);
                    const auto hi = fieldLowHz * std::pow (fieldHighHz / fieldLowHz, (float) (band + 1) / bandsInField);
                    const auto first = std::max (1, (int) std::floor (lo / binHz)), last = std::min (bins - 1, std::max (first, (int) std::floor (hi / binHz)));
                    auto sum = 0.0f;
                    for (int k = first; k <= last; ++k)
                        sum += power[k];
                    const auto level = sum > 1.0e-20f ? 10.0f * std::log10 (sum) : -200.0f;
                    dbAt (bar, src, band) = level;
                    topDb = std::max (topDb, level);
                }
            }
        field.assign ((size_t) fieldBars * bandsInField, 0);
        for (int bar = 0; bar < fieldBars; ++bar)
            for (int band = 0; band < bandsInField; ++band)
            {
                auto loudest = -200.0f;
                for (int src = 0; src < MixAnalysis::masterSource; ++src)
                    loudest = std::max (loudest, dbAt (bar, src, band));
                if (loudest < topDb - 45.0f)
                    continue;
                int count = 0;
                for (int src = 0; src < MixAnalysis::masterSource; ++src)
                    if (dbAt (bar, src, band) > loudest - 12.0f)
                        ++count;
                field[(size_t) bar * bandsInField + (size_t) band] = (std::uint8_t) std::min (count, 4);
            }
    }

    void paintList (juce::Graphics& g) const
    {
        const auto area = listArea();
        g.saveState();
        g.reduceClipRegion (area);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 4.0f);
        if (report.findings.empty())
        {
            g.setColour (Theme::mix);
            g.setFont (13.0f);
            g.drawFittedText ("No collisions found.", area.reduced (8), juce::Justification::centred, 2);
        }
        for (int i = 0; i < (int) report.findings.size(); ++i)
        {
            const auto& f = report.findings[(size_t) i];
            auto row = juce::Rectangle<int> (area.getX(), area.getY() + i * rowHeight - scroll, area.getWidth(), rowHeight);
            if (row.getBottom() < area.getY() || row.getY() > area.getBottom())
                continue;
            if (i == selected)
            {
                g.setColour (Theme::lfo.withAlpha (0.25f));
                g.fillRect (row);
            }
            g.setColour (severityColour (f.severity));
            g.fillEllipse ((float) row.getX() + 8.0f, (float) row.getCentreY() - 5.0f, 10.0f, 10.0f);
            auto text = row.withTrimmedLeft (26).reduced (0, 2);
            g.setColour (Theme::text);
            g.setFont (Theme::font (12.0f, true));
            g.drawText (f.title, text.removeFromTop (15), juce::Justification::centredLeft, true);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (10.5f));
            g.drawText (f.detail, text, juce::Justification::centredLeft, true);
        }
        g.restoreState();
    }

    void paintField (juce::Graphics& g) const
    {
        const auto area = fieldArea();
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 4.0f);
        const auto showFixes = selected >= 0 && selected < (int) report.findings.size() && report.findings[(size_t) selected].category == "phase" && ! fixes.isEmpty();
        const auto fixHeight = showFixes ? 14 * std::min (fixes.size(), 6) + 20 : 0;
        const auto plot = area.toFloat().reduced (30.0f, 10.0f).withTrimmedBottom (8.0f + (float) fixHeight);
        auto xFor = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / fieldLowHz) / std::log (fieldHighHz / fieldLowHz); };
        const auto bars = std::max (1, fieldBars > 0 ? fieldBars : report.bars);
        auto yTop = [&] (int bar0) { return plot.getY() + plot.getHeight() * (float) bar0 / (float) bars; };

        if (fieldBars > 0)
        {
            const auto w = plot.getWidth() / (float) bandsInField, h = plot.getHeight() / (float) fieldBars;
            for (int bar = 0; bar < fieldBars; ++bar)
                for (int band = 0; band < bandsInField; ++band)
                {
                    const auto count = field[(size_t) bar * bandsInField + (size_t) band];
                    if (count == 0)
                        continue;
                    g.setColour (count == 1 ? Theme::track : count == 2 ? juce::Colour (0xffe8d35c).withAlpha (0.7f) : Theme::master.withAlpha (count == 3 ? 0.7f : 0.95f));
                    g.fillRect (plot.getX() + w * (float) band, plot.getY() + h * (float) bar, w + 0.5f, h + 0.5f);
                }
        }
        g.setFont (10.0f);
        for (const float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) xFor (hz), plot.getY(), plot.getBottom());
            g.setColour (Theme::textDim);
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz), juce::Rectangle<float> (xFor (hz) - 16.0f, plot.getBottom() + 1.0f, 32.0f, 10.0f),
                        juce::Justification::centred);
        }
        for (int bar = 0; bar < bars; ++bar)
        {
            g.setColour (Theme::textDim);
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (area.getX() + 2.0f, yTop (bar), 24.0f, plot.getHeight() / (float) bars), juce::Justification::centredRight);
        }
        if (selected < 0 || selected >= (int) report.findings.size())
            return;
        const auto& f = report.findings[(size_t) selected];
        if (showFixes)
        {
            auto strip = area.withTrimmedTop (area.getHeight() - fixHeight).reduced (8, 2);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (10.5f, true));
            g.drawText ("SUGGESTED FIXES - candidates to try with A/B; nothing is changed until you approve", strip.removeFromTop (16), juce::Justification::centredLeft, true);
            g.setFont (Theme::font (10.5f));
            for (int i = 0; i < std::min (fixes.size(), 6); ++i)
                g.drawText (juce::String (juce::CharPointer_UTF8 ("â¢ ")) + fixes[i].getProperty ("target", {}).toString() + " - " + fixes[i].getProperty ("why", {}).toString(),
                            strip.removeFromTop (14), juce::Justification::centredLeft, true);
        }
        const auto colour = severityColour (f.severity);
        if (f.highHz > f.lowHz && f.highHz > 0.0f)
        {
            const auto x0 = xFor (juce::jlimit (fieldLowHz, fieldHighHz, std::max (f.lowHz, 1.0f))), x1 = xFor (juce::jlimit (fieldLowHz, fieldHighHz, f.highHz));
            g.setColour (colour.withAlpha (0.18f));
            g.fillRect (juce::Rectangle<float> (x0, plot.getY(), std::max (2.0f, x1 - x0), plot.getHeight()));
            g.setColour (colour);
            g.drawVerticalLine ((int) x0, plot.getY(), plot.getBottom());
            g.drawVerticalLine ((int) x1, plot.getY(), plot.getBottom());
        }
        for (const auto bar1 : f.bars)
            if (bar1 >= 1 && bar1 <= bars)
            {
                g.setColour (colour);
                g.drawRect (juce::Rectangle<float> (plot.getX(), yTop (bar1 - 1), plot.getWidth(), plot.getHeight() / (float) bars), 1.5f);
            }
    }

    PluginBase& processor;
    MixAnalyser& analyser;
    juce::TextButton analyseButton { "ANALYSE" };
    MixAnalysis::Report report;
    juce::Array<juce::var> fixes; // suggested_fixes ur fasmåtten (visas när ett fasfynd är valt)
    std::vector<std::uint8_t> field; // [takt][band]: antal källor som ligger nära varandra (0-4)
    int fieldBars = 0, selected = -1, scroll = 0, shownVersion = 0;
    bool analysing = false, gradientShown = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollisionsView)
};
