#include "SpectrumPage.h"
#include "../Core/ToolLayer.h"
#include "../Core/Settings.h"

namespace
{
    constexpr float lowHz = 20.0f, highHz = 20000.0f;
    constexpr float dbTop = 0.0f, dbBottom = -90.0f;
    constexpr float collisionFloor = -66.0f; // under den nivån räknas en källa inte som "spelar"
    constexpr int stripHeight = 24;
    constexpr int panelWidth = 350;

    juce::Colour severityColour (MixAnalysis::Severity severity)
    {
        return severity == MixAnalysis::Severity::problem ? juce::Colour (0xffff4d5e)
             : severity == MixAnalysis::Severity::warning ? juce::Colour (0xffffb347)
                                                          : juce::Colour (0xff6bb8ff);
    }
}

//==============================================================================
void ReportContent::setReport (const MixAnalysis::Report& newReport)
{
    report = newReport;
    selected = -1;
    layoutRows();
}

void ReportContent::setNotes (std::vector<PluginBase::AiNote> newNotes)
{
    notes = std::move (newNotes);
    layoutRows();
}

void ReportContent::setComparison (const juce::String& text)
{
    comparison = text;
    layoutRows();
}

void ReportContent::setWidth (int newWidth)
{
    width = std::max (120, newWidth);
    layoutRows();
}

void ReportContent::layoutRows()
{
    rows.clear();
    auto top = 4;
    auto add = [&] (juce::AttributedString text, int kind, int finding, bool applied = false, int extra = 0)
    {
        Row row;
        row.layout.createLayout (text, (float) (width - 26));
        row.top = top;
        row.height = (int) std::ceil (row.layout.getHeight()) + 14 + extra;
        row.kind = kind;
        row.finding = finding;
        row.applied = applied;
        top += row.height + 4;
        rows.push_back (std::move (row));
    };

    if (! report.valid)
    {
        juce::AttributedString text;
        text.append (report.error.isEmpty() ? "Press ANALYSE to play the pattern once through and get a report of what to look at. The report can then be saved with everything that was measured (EXPORT), "
                                              "and an AI can read it and write suggestions here."
                                            : report.error,
                     Theme::font (11.0f), Theme::textDim);
        add (text, 0, -1);
    }
    else
    {
        juce::AttributedString summary;
        summary.append (juce::String (report.bars) + " bars at " + juce::String (juce::roundToInt (report.bpm)) + " BPM"
                            + (report.kickTailHz > 0.0f ? ", kick tail " + juce::String (report.kickTailHz, 1) + " Hz" : juce::String())
                            + (report.referenceName.isNotEmpty() ? ", reference: " + report.referenceName : juce::String()) + ". Click a finding to select it, double-click to see it in the spectrum.",
                        Theme::font (10.5f), Theme::textDim);
        add (summary, 0, -1);

        if (comparison.isNotEmpty())
        {
            juce::AttributedString compare;
            compare.append (comparison, Theme::font (10.5f), Theme::text);
            add (compare, 0, -1);
        }

        if (! report.afterMaster.empty())
        {
            juce::AttributedString header;
            header.append ("AFTER MASTER\n", Theme::font (10.5f, true), Theme::mix);
            header.append ("What each source adds to the final mix, in dB compared with its own signal, per band (Hz). Red = the mixer and master take level, blue = it gets louder. "
                           "Left: own level, then level in the final mix.", Theme::font (10.0f), Theme::textDim);
            add (header, 3, -1, false, (int) report.afterMaster.size() * 18 + 20);
        }
        if (report.findings.empty())
        {
            juce::AttributedString none;
            none.append ("Nothing to report at this sensitivity. Raise it (SENS) to see smaller things.", Theme::font (11.0f, true), Theme::mix);
            add (none, 0, -1);
        }
        auto number = 0;
        for (const auto& finding : report.findings)
        {
            juce::AttributedString text;
            text.append (juce::String (++number) + "  ", Theme::font (10.0f, true), Theme::textDim);
            text.append (finding.severity == MixAnalysis::Severity::problem ? "PROBLEM  " : finding.severity == MixAnalysis::Severity::warning ? "WARNING  " : "INFO  ",
                         Theme::font (10.0f, true), severityColour (finding.severity));
            text.append (finding.title + "\n", Theme::font (11.5f, true), Theme::text);
            text.append (finding.detail, Theme::font (10.5f), Theme::textDim.brighter (0.25f));
            if (! finding.bars.empty())
            {
                juce::String bars = "\nBars: ";
                for (size_t i = 0; i < finding.bars.size(); ++i)
                    bars << (i > 0 ? ", " : "") << finding.bars[i];
                text.append (bars, Theme::font (10.5f, true), severityColour (finding.severity).withAlpha (0.9f));
            }
            add (text, 1, number - 1);
        }
    }

    // En AI:s förslag och det den fixat (verktyget add_report_note).
    if (! notes.empty())
    {
        juce::AttributedString header;
        header.append ("AI SUGGESTIONS AND FIXES", Theme::font (10.5f, true), Theme::mix);
        add (header, 0, -1);
        for (const auto& note : notes)
        {
            juce::AttributedString text;
            text.append (note.applied ? "APPLIED  " : "SUGGESTION  ", Theme::font (10.0f, true), note.applied ? Theme::lfo : Theme::mix);
            text.append (note.title + "\n", Theme::font (11.5f, true), Theme::text);
            text.append (note.text, Theme::font (10.5f), Theme::textDim.brighter (0.25f));
            if (note.finding >= 1 && note.finding <= (int) report.findings.size())
                text.append ("\nAbout finding " + juce::String (note.finding) + ": " + report.findings[(size_t) note.finding - 1].title, Theme::font (10.0f, true), Theme::textDim);
            add (text, 2, note.finding >= 1 && note.finding <= (int) report.findings.size() ? note.finding - 1 : -1, note.applied);
        }
    }
    setSize (width, top + 4);
    repaint();
}

void ReportContent::paint (juce::Graphics& g)
{
    const auto clip = g.getClipBounds();
    for (const auto& row : rows)
    {
        if (row.top + row.height < clip.getY() || row.top > clip.getBottom())
            continue;
        const auto bounds = juce::Rectangle<float> (2.0f, (float) row.top, (float) width - 4.0f, (float) row.height);
        const auto isSelected = row.finding >= 0 && selected == row.finding;
        auto colour = Theme::textDim;
        if (row.kind == 1 && row.finding < (int) report.findings.size())
            colour = severityColour (report.findings[(size_t) row.finding].severity);
        else if (row.kind == 2)
            colour = row.applied ? Theme::lfo : Theme::mix;
        g.setColour (juce::Colours::black.withAlpha (isSelected && row.kind == 1 ? 0.55f : 0.3f));
        g.fillRoundedRectangle (bounds, 5.0f);
        if (row.kind != 0)
        {
            g.setColour (colour.withAlpha (isSelected ? 0.95f : 0.7f));
            g.fillRoundedRectangle (bounds.withWidth (4.0f), 2.0f);
            if (isSelected && row.kind == 1)
                g.drawRoundedRectangle (bounds, 5.0f, 1.2f);
        }
        row.layout.draw (g, bounds.reduced (12.0f, 7.0f));
        if (row.kind == 3)
        {
            const auto textHeight = (float) row.layout.getHeight();
            auto y = bounds.getY() + 7.0f + textHeight + 6.0f;
            const auto nameWidth = 78.0f, levelWidth = 92.0f;
            const auto cellsLeft = bounds.getX() + 12.0f + nameWidth + levelWidth;
            const auto cellWidth = std::max (24.0f, (bounds.getRight() - 8.0f - cellsLeft) / (float) MixAnalysis::shareBands);
            g.setFont (Theme::font (9.0f));
            g.setColour (Theme::textDim);
            for (int b = 0; b < MixAnalysis::shareBands; ++b)
                g.drawText (MixAnalysis::shareBandName (b), juce::Rectangle<float> (cellsLeft + (float) b * cellWidth, y, cellWidth, 12.0f), juce::Justification::centred);
            y += 14.0f;
            for (const auto& share : report.afterMaster)
            {
                g.setFont (Theme::font (10.5f, true));
                g.setColour (Theme::text);
                g.drawText (share.name, juce::Rectangle<float> (bounds.getX() + 12.0f, y, nameWidth - 4.0f, 16.0f), juce::Justification::centredLeft, true);
                g.setFont (Theme::font (10.0f));
                g.setColour (Theme::textDim);
                g.drawText (juce::String (share.heardRmsDb, 0) + " > " + juce::String (share.masterRmsDb, 0),
                            juce::Rectangle<float> (bounds.getX() + 12.0f + nameWidth, y, levelWidth - 4.0f, 16.0f), juce::Justification::centredLeft);
                for (int b = 0; b < MixAnalysis::shareBands; ++b)
                {
                    const auto cell = juce::Rectangle<float> (cellsLeft + (float) b * cellWidth, y, cellWidth - 2.0f, 16.0f);
                    const auto active = share.heardBandDb[(size_t) b] > share.heardRmsDb - 24.0f && share.heardRmsDb > -70.0f;
                    const auto delta = share.masterBandDb[(size_t) b] - share.heardBandDb[(size_t) b];
                    const auto strength = juce::jlimit (0.0f, 1.0f, std::abs (delta) / 12.0f);
                    g.setColour ((delta < 0.0f ? juce::Colour (0xffff4d5e) : juce::Colour (0xff6bb8ff)).withAlpha (active ? 0.15f + 0.6f * strength : 0.0f));
                    g.fillRoundedRectangle (cell, 3.0f);
                    g.setColour (active ? Theme::text : Theme::textDim.withAlpha (0.4f));
                    g.drawText (active ? juce::String (delta, 1) : "-", cell, juce::Justification::centred);
                }
                y += 18.0f;
            }
        }
    }
}

void ReportContent::mouseDown (const juce::MouseEvent& event)
{
    for (const auto& row : rows)
        if (event.y >= row.top && event.y < row.top + row.height && row.kind != 0 && row.finding >= 0 && row.finding < (int) report.findings.size())
        {
            if (onSelect)
                onSelect (row.finding);
            return;
        }
}

void ReportContent::mouseDoubleClick (const juce::MouseEvent& event)
{
    for (const auto& row : rows)
        if (event.y >= row.top && event.y < row.top + row.height && row.kind != 0 && row.finding >= 0 && row.finding < (int) report.findings.size())
        {
            if (onOpen)
                onOpen (row.finding);
            return;
        }
}

//==============================================================================
SpectrumPage::SpectrumPage (PluginBase& p) : processor (p), analyser (p.getMixAnalyser())
{
    sources[kickSource].colour = Theme::kick;
    sources[kickSource].name = "KICK";
    for (int i = 0; i < ParamID::numInstrumentLayers; ++i)
        sources[(size_t) (firstInstrument + i)].colour = Theme::layerColour (i);
    sources[masterSource].colour = Theme::text;
    sources[masterSource].name = "MASTER";

    auto setUpToggle = [this] (juce::TextButton& button, juce::Colour colour, bool on, const juce::String& tip)
    {
        button.setClickingTogglesState (true);
        button.setToggleState (on, juce::dontSendNotification);
        button.setColour (juce::TextButton::buttonOnColourId, colour);
        button.setTooltip (tip);
        addAndMakeVisible (button);
    };

    setUpToggle (analyserButton, Theme::mix, false,
                 "ANALYSER - Starts the live spectrum. It is off by default because it uses some CPU while it runs; it stops by itself when you leave this tab.");
    analyserButton.onClick = [this] { updateRunning(); };
    setUpToggle (collisionsButton, Theme::master, true,
                 "COLLISIONS - Marks in red where two or more of the shown sources play the same frequency at the same time. "
                 "The stronger the red, the louder they are and the more of them there are. Mixing: move, filter or retune one of them.");
    collisionsButton.onClick = [this] { repaint(); };
    setUpToggle (harmonicsButton, Theme::lfo, false,
                 "HARMONICS - Marks each shown source's fundamental and its overtones (2x, 3x, 4x ...) with ticks in its own colour, "
                 "so you can see which of the overtones line up with another source's.");
    harmonicsButton.onClick = [this] { repaint(); };
    setUpToggle (eqButton, Theme::effects, false,
                 "EQ - Shows the EQ of the chosen source (pick it in the list) as a curve in the diagram, and lets you drag its four bands: "
                 "drag a dot to change frequency and gain, mouse wheel on a dot changes its width (Q), double-click resets it. "
                 "The two triangles at the bottom are the high-pass and low-pass. The curve is drawn around the -45 dB line (1 dB = 1 dB on the scale). "
                 "Every kick, instrument and the master has its own EQ.");
    eqButton.onClick = [this] { resized(); repaint(); };

    eqTargetBox.setTooltip ("Which source the EQ edits.");
    eqTargetBox.onChange = [this]
    {
        eqSource = juce::jlimit (0, numSources - 1, eqTargetBox.getSelectedId() - 1);
        eqTargetBox.setColour (juce::ComboBox::outlineColourId, sources[(size_t) eqSource].colour);
        eqTargetBox.setColour (juce::ComboBox::textColourId, sources[(size_t) eqSource].colour);
        repaint();
    };
    addChildComponent (eqTargetBox);

    showButton.setTooltip ("SHOW - Which sources the diagram shows at the same time: the kick, each instrument and the master. Click to tick or untick them "
                           "(the list stays open so you can pick several).");
    showButton.onClick = [this] { showSourceMenu(); };
    addAndMakeVisible (showButton);

    // ANALYSE, referens och känslighet.
    analyseButton.setColour (juce::TextButton::buttonColourId, Theme::mix.withAlpha (0.35f));
    analyseButton.setTooltip ("ANALYSE - Plays the pattern once through offline (the plugin keeps working while it runs) and writes a report of what to look at: "
                              "collisions between the kick and the instruments (and between instruments), an instrument covering the kick, a kick that is tuned too high or too low, "
                              "instruments that are out of tune with each other or the kick, rumble under 30 Hz and a master limiter that works too hard. "
                              "It lists the bars and frequencies. Click a finding to show it in the diagram. An AI can read the same report (tool analyse_mix).");
    analyseButton.onClick = [this] { startAnalysis(); };
    addAndMakeVisible (analyseButton);

    referenceButton.setTooltip ("REFERENCE - Load a track to compare against (WAV, FLAC, MP3 ...). Its average spectrum is drawn behind the master "
                                "(level-matched), and the report tells where your master has more or less energy than the reference.");
    referenceButton.onClick = [this] { chooseReference(); };
    addAndMakeVisible (referenceButton);

    for (int step = 1; step <= 5; ++step) // samma värde som SETTINGS > OPTIMIZATIONS (Settings::spectrumAnalysisStep)
        speedBox.addItem ("SPEED: " + juce::String (Settings::spectrumAnalysesPerSecond (step)) + "/S", step);
    speedBox.setSelectedId (Settings::spectrumAnalysisStep(), juce::dontSendNotification);
    speedBox.setTooltip ("SPEED - How many times per second the live spectrum is analysed: 4, 8, 15, 30 or 40, at most once per drawn frame. A slower speed uses less CPU and is easier to read. "
                         "15 is the default. The same setting is in SETTINGS > OPTIMIZATIONS, together with how often the spectrum is drawn and whether its curves are filled.");
    speedBox.onChange = [this]
    {
        Settings::setSpectrumAnalysisStep (speedBox.getSelectedId());
        updateRunning();
    };
    addAndMakeVisible (speedBox);

    sensitivityBox.addItem ("SENS: LOW", 1);
    sensitivityBox.addItem ("SENS: NORMAL", 2);
    sensitivityBox.addItem ("SENS: HIGH", 3);
    sensitivityBox.setSelectedId (2, juce::dontSendNotification);
    sensitivityBox.setTooltip ("SENSITIVITY - How much the analysis reports. Low: only clear problems. High: also smaller overlaps. It sets how close in level two sources must be "
                               "to count as colliding, how loud they must be and for how long in a bar.");
    addAndMakeVisible (sensitivityBox);

    tapBox.addItem ("AS HEARD", 1);
    tapBox.addItem ("DRY", 2);
    tapBox.addItem ("AFTER MASTER", 3);
    tapBox.setSelectedId (1, juce::dontSendNotification);
    tapBox.setTooltip ("MEASURE - As heard: after sidechain ducking and EQ (default). Dry: the analysis copy has ducking off and EQ neutral on the kick and every instrument, "
                       "so you see the real collisions the sidechain and EQ hide. After master (advanced, slower: one extra offline render per source): as heard, plus a table of what each source "
                       "adds to the final mix after the master chain, per frequency band. Your sound is not changed.");
    addAndMakeVisible (tapBox);

    exportButton.setTooltip ("EXPORT - Copies the report as text, or saves the whole analysis as a JSON file for an AI or for studying by hand. In the list you choose what to include: "
                             "findings, an overview per source, band levels and harmonic series and 1/12-octave spectra for every source and every bar, the kick hits with the ducking, "
                             "the level of every band in every frame through the loop, and all the synth's settings (oscillators, wavetables, filters, effects, modulation, pattern, notes).");
    exportButton.onClick = [this] { showExportMenu(); };
    addChildComponent (exportButton);

    for (auto* button : { &viewSpectrumButton, &viewReportButton })
    {
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::lfo.withAlpha (0.55f));
        addAndMakeVisible (*button);
    }
    keepButton.setTooltip ("KEEP - Remember this report. Change something, analyse again, and COMPARE shows what changed: level and peak per source, and which findings are gone or new.");
    keepButton.onClick = [this]
    {
        keptReport = content.getReport();
        keepButton.setButtonText (keptReport.valid ? "KEPT" : "KEEP");
        updateComparison();
    };
    compareButton.setClickingTogglesState (true);
    compareButton.setColour (juce::TextButton::buttonOnColourId, Theme::lfo.withAlpha (0.55f));
    compareButton.setTooltip ("COMPARE - Shows, on top of the report, how it differs from the report you kept with KEEP.");
    compareButton.onClick = [this] { updateComparison(); };
    addChildComponent (keepButton);
    addChildComponent (compareButton);
    showSpectrumButton.setTooltip ("SHOW IN SPECTRUM - Opens the selected finding in the spectrum view (the sources and the frequency range are highlighted). Select a finding in the list first.");
    showSpectrumButton.onClick = [this] { if (content.selected >= 0) selectFinding (content.selected, true); };
    addChildComponent (showSpectrumButton);
    viewSpectrumButton.setTooltip ("SPECTRUM - The live spectrum of the kick, the instruments and the master, with harmonics, collisions and the EQ.");
    viewReportButton.setTooltip ("ANALYSIS + REPORT - Analyse the whole pattern and read the report: what to look at, and what an AI suggests or has fixed.");
    viewSpectrumButton.onClick = [this] { setView (false); };
    viewReportButton.onClick = [this] { setView (true); };
    exportOptions = MixAnalysis::ExportOptions::fromString (Settings::get ("exportContent"));

    content.onSelect = [this] (int index) { selectFinding (index); };
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addChildComponent (viewport);

    for (auto& source : sources)
        source.low.fill (-120.0f);

    updateSources();
    setView (false);
    startTimerHz (2); // anteckningar från en AI dyker upp även när inget annat rör sig

    // Referensen från förra gången.
    if (const auto path = Settings::get ("referencePath"); path.isNotEmpty() && juce::File (path).existsAsFile())
        loadReferenceFile (juce::File (path));

    // Skärmbildsverktyget: analysen på, och (KB_REPORT) rapporten färdig.
    if (processor.snapshotSpectrum)
    {
        analyserButton.setToggleState (true, juce::dontSendNotification);
        harmonicsButton.setToggleState (juce::SystemStats::getEnvironmentVariable ("KB_HARMONICS", "0").getIntValue() != 0, juce::dontSendNotification);
        eqButton.setToggleState (juce::SystemStats::getEnvironmentVariable ("KB_EQ", "0").getIntValue() != 0, juce::dontSendNotification);
        updateRunning();
        timerCallback();
        if (const auto mode = juce::SystemStats::getEnvironmentVariable ("KB_REPORT", "0").getIntValue(); mode > 0)
        {
            startAnalysis();
            while (analyser.isRunning())
                juce::Thread::sleep (20);
            takeReport();
            analysing = false;
            analyseButton.setButtonText ("ANALYSE");
            if (mode > 1 && ! content.getReport().findings.empty())
                selectFinding (0, true);
        }
        if (juce::SystemStats::getEnvironmentVariable ("KB_VIEW", "0").getIntValue() != 0)
            setView (true);
    }
}

SpectrumPage::~SpectrumPage()
{
    processor.instrumentTapsOn = false;
}

void SpectrumPage::updateRunning()
{
    const auto shouldRun = analyserButton.getToggleState() && ! analysing && ! reportView && (isShowing() || processor.snapshotSpectrum); // (pausad under en analys och i rapportvyn)
    if (shouldRun != running)
    {
        running = shouldRun;
        processor.instrumentTapsOn = running;
        if (running)
            for (auto& source : sources)
            {
                source.column.clear();
                source.target.clear();
                source.low.fill (-120.0f);
                source.fundamental = 0.0f;
            }
    }
    if (running)
    {
        // Bildrutorna går i egen takt (SETTINGS > OPTIMIZATIONS: 5-40 per sekund, halverat med REDUCE ANIMATIONS) och analysen var n:te ruta:
        // SPEED styr bara hur ofta analysen körs (som mest en gång per bildruta). Fyllningen läses också här, inte i paint (fråga 2).
        displayHz = Settings::visualHz (Settings::Visual::analyzer, 30);
        fillCurves = Settings::visualFill (Settings::Visual::analyzer);
        fps = Settings::spectrumAnalysesPerSecond (Settings::spectrumAnalysisStep());
        speedBox.setSelectedId (Settings::spectrumAnalysisStep(), juce::dontSendNotification);
        smoothing = 1.0f - std::pow (0.7f, 30.0f / (float) fps); // kurvorna faller lika fort i sekunden oavsett takt
        const auto analysesPerSecond = Settings::reduceAnimations() ? std::min (fps, 8) : fps;
        analysisEvery = std::max (1, (int) std::lround ((double) displayHz / (double) analysesPerSecond));
        startTimerHz (displayHz);
    }
    else
        startTimerHz (analysing ? 10 : 2);
    repaint();
}

void SpectrumPage::updateSources()
{
    const auto count = processor.activeInstruments();
    const auto version = processor.layerNamesVersion.load();
    if (count == instrumentsShown && version == namesVersion)
        return;
    instrumentsShown = count;
    namesVersion = version;

    for (int i = 0; i < ParamID::numInstrumentLayers; ++i)
    {
        auto& source = sources[(size_t) (firstInstrument + i)];
        source.present = i < count;
        source.name = processor.getLayerName (i).toUpperCase();
    }
    updateShowText();

    eqTargetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < numSources; ++i)
        if (sources[(size_t) i].present)
            eqTargetBox.addItem (sources[(size_t) i].name, i + 1);
    if (! sources[(size_t) eqSource].present)
        eqSource = 0;
    eqTargetBox.setSelectedId (eqSource + 1, juce::dontSendNotification);
    resized();
}

void SpectrumPage::setEmbedded()
{
    embedded = true;
    viewSpectrumButton.setVisible (false);
    viewReportButton.setVisible (false);
    resized();
}

void SpectrumPage::setCollisions (bool on)
{
    collisionsButton.setToggleState (on, juce::dontSendNotification);
    repaint();
}

void SpectrumPage::setView (bool report)
{
    reportView = report;
    viewSpectrumButton.setToggleState (! report, juce::dontSendNotification);
    viewReportButton.setToggleState (report, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &analyserButton, &collisionsButton, &harmonicsButton, &speedBox, &showButton, &eqButton })
        c->setVisible (! report);
    for (auto* c : std::initializer_list<juce::Component*> { &analyseButton, &referenceButton, &sensitivityBox, &tapBox, &exportButton, &keepButton, &compareButton, &showSpectrumButton })
        c->setVisible (report);
    viewport.setVisible (report);
    updateRunning();
    resized();
    repaint();
}

juce::Rectangle<int> SpectrumPage::overlayBounds() const
{
    return reportView ? viewport.getBounds() : plotBody;
}

void SpectrumPage::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    auto top = area.removeFromTop (stripHeight);
    if (! embedded)
    {
        viewSpectrumButton.setBounds (top.removeFromLeft (92));
        top.removeFromLeft (3);
        viewReportButton.setBounds (top.removeFromLeft (150));
        top.removeFromLeft (14);
    }

    if (reportView)
    {
        analyseButton.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (4);
        sensitivityBox.setBounds (top.removeFromLeft (110));
        top.removeFromLeft (4);
        tapBox.setBounds (top.removeFromLeft (96));
        top.removeFromLeft (4);
        referenceButton.setBounds (top.removeFromLeft (112));
        top.removeFromLeft (4);
        exportButton.setBounds (top.removeFromLeft (76));
        top.removeFromLeft (4);
        keepButton.setBounds (top.removeFromLeft (62));
        top.removeFromLeft (4);
        compareButton.setBounds (top.removeFromLeft (84));
        top.removeFromLeft (4);
        showSpectrumButton.setBounds (top.removeFromLeft (150));
        eqTargetBox.setVisible (false);
    }
    else
    {
        analyserButton.setBounds (top.removeFromLeft (76));
        top.removeFromLeft (4);
        collisionsButton.setBounds (top.removeFromLeft (88));
        top.removeFromLeft (4);
        harmonicsButton.setBounds (top.removeFromLeft (88));
        top.removeFromLeft (4);
        speedBox.setBounds (top.removeFromLeft (92));
        top.removeFromLeft (4);
        showButton.setBounds (top.removeFromLeft (150));
        top.removeFromLeft (6);
        eqButton.setBounds (top.removeFromLeft (38));
        top.removeFromLeft (4);
        eqTargetBox.setVisible (eqButton.getToggleState());
        eqTargetBox.setBounds (top.removeFromLeft (100));
    }

    area.removeFromTop (4);
    if (reportView)
    {
        viewport.setBounds (area);
        content.setWidth (viewport.getMaximumVisibleWidth());
        plotBody = {};
    }
    else
    {
        plotBody = area;
    }

    // Ändrad storlek: kurvorna räknas om till de nya bildpunkterna direkt (annars är de tomma tills nästa analys).
    if (running)
    {
        for (int i = 0; i < numSources; ++i)
            if (sources[(size_t) i].present && sources[(size_t) i].selected)
                analyse (i);
        smoothColumns();
    }
}

juce::Rectangle<float> SpectrumPage::plotArea() const
{
    return plotBody.toFloat().reduced (4.0f, 6.0f).withTrimmedLeft (22.0f).withTrimmedBottom (14.0f);
}

float SpectrumPage::xFor (float hz) const
{
    const auto area = plotArea();
    return area.getX() + std::log (hz / lowHz) / std::log (highHz / lowHz) * area.getWidth();
}

float SpectrumPage::hzFor (float x) const
{
    const auto area = plotArea();
    return lowHz * std::pow (highHz / lowHz, (x - area.getX()) / area.getWidth());
}

float SpectrumPage::dbToY (float db) const
{
    const auto area = plotArea();
    return area.getY() + juce::jlimit (0.0f, 1.0f, (dbTop - db) / (dbTop - dbBottom)) * area.getHeight();
}

float SpectrumPage::yToDb (float y) const
{
    const auto area = plotArea();
    return dbTop - (y - area.getY()) / area.getHeight() * (dbTop - dbBottom);
}

juce::String SpectrumPage::describe (float hz) const
{
    const auto note = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
    return (hz < 100.0f ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz))) + " Hz  " + juce::MidiMessage::getMidiNoteName (note, true, true, 5);
}

//==============================================================================
// ANALYSE, rapport och referens

void SpectrumPage::startAnalysis()
{
    if (analyser.isRunning())
    {
        analyser.cancel();
        return;
    }
    processor.requestAudition (PluginBase::previewStop); // förhandslyssningar stoppas först, så att inget annat spelar under analysen
    MixAnalysis::Options options;
    options.sensitivity = juce::jlimit (0, 2, sensitivityBox.getSelectedId() - 1);
    options.tap = tapBox.getSelectedId() == 2 ? 1 : tapBox.getSelectedId() == 3 ? 2 : 0;
    // Alla inställningar när analysen startar följer med i rapporten (oscillatorer, wavetables, effekter, modulering, mönster, toner).
    ToolLayer tools (processor);
    auto stateArgs = juce::DynamicObject::Ptr (new juce::DynamicObject());
    stateArgs->setProperty ("all_parameters", true);
    const auto state = tools.call ("get_state", juce::var (stateArgs.get()));
    processor.clearAiNotes();
    if (! analyser.start (processor, options, reference, state["data"]))
        return;
    analysing = true;
    analyseButton.setButtonText ("CANCEL");
    for (auto* c : std::initializer_list<juce::Component*> { &referenceButton, &sensitivityBox, &tapBox, &eqButton, &eqTargetBox })
        c->setEnabled (false);
    updateRunning();
}

void SpectrumPage::updateComparison()
{
    content.setComparison (compareButton.getToggleState() ? MixAnalysis::compareReports (keptReport, content.getReport()).toText() : juce::String());
}

void SpectrumPage::takeReport()
{
    content.setReport (analyser.getReport());
    reportShown = true;
    highlightLow = highlightHigh = 0.0f;
    reportVersion = analyser.getVersion();
    exportButton.setEnabled (content.getReport().valid);
    updateComparison();
    resized();
    viewport.setViewPosition (0, 0);
    repaint();
}

void SpectrumPage::selectFinding (int index, bool showSpectrum)
{
    const auto& report = content.getReport();
    if (index < 0 || index >= (int) report.findings.size())
        return;
    const auto& finding = report.findings[(size_t) index];
    content.selected = index;
    content.repaint();
    highlightLow = finding.lowHz;
    highlightHigh = finding.highHz;

    // Visa de inblandade källorna, och sätt EQ:n på den första (helst ett instrument).
    auto first = -1;
    for (int i = 0; i < numSources; ++i)
    {
        if (! sources[(size_t) i].present)
            continue;
        auto involved = false;
        for (const auto& name : finding.sources)
            involved = involved || (i == kickSource && name == "Kick") || (i == masterSource && name == "Master")
                       || (i >= firstInstrument && i < masterSource && name.equalsIgnoreCase (processor.getLayerName (i - firstInstrument)));
        sources[(size_t) i].selected = involved || finding.sources.isEmpty();
        if (involved && (first < 0 || (first == kickSource && i != masterSource)))
            first = i;
    }
    updateShowText();
    if (first >= 0)
    {
        eqSource = first;
        eqTargetBox.setSelectedId (eqSource + 1, juce::dontSendNotification);
    }
    if (! analyserButton.getToggleState())
        analyserButton.setToggleState (true, juce::dontSendNotification);
    if (showSpectrum)
        setView (false); // dubbelklick: visa fyndet i spektrumet
    repaint();
}

void SpectrumPage::copyReport()
{
    auto text = content.getReport().toText();
    const auto notes = processor.getAiNotes();
    if (! notes.empty())
    {
        text << "\nAI suggestions and fixes:\n";
        for (const auto& note : notes)
            text << (note.applied ? "[APPLIED] " : "[SUGGESTION] ") << note.title << "\n    " << note.text << "\n";
    }
    juce::SystemClipboard::copyTextToClipboard (text);
}

// EXPORT: kopiera som text, eller spara allt som mättes i en fil. Listan väljer vad som tas med och ligger kvar öppen.
void SpectrumPage::showExportMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Copy the report as text");
    menu.addItem (2, "Save the analysis to a file ...");
    menu.addSeparator();
    menu.addSectionHeader ("WHAT THE FILE CONTAINS");
    struct Entry { int id; const char* text; bool MixAnalysis::ExportOptions::* flag; };
    const Entry entries[] { { 10, "Findings", &MixAnalysis::ExportOptions::findings },
                            { 11, "Overview per source", &MixAnalysis::ExportOptions::sources },
                            { 12, "Band levels per source and bar", &MixAnalysis::ExportOptions::bars },
                            { 13, "Harmonic series per source and bar", &MixAnalysis::ExportOptions::harmonics },
                            { 14, "1/12-octave spectrum per source and bar", &MixAnalysis::ExportOptions::spectra },
                            { 15, "Band levels in every frame (large)", &MixAnalysis::ExportOptions::frames },
                            { 16, "Kick hits and ducking", &MixAnalysis::ExportOptions::kicks },
                            { 17, "All synth settings, flat list and the notes", &MixAnalysis::ExportOptions::synth },
                            { 19, "All settings arranged (oscillators A/B/C, filters, effects ...)", &MixAnalysis::ExportOptions::overview },
                            { 20, "Loaded presets", &MixAnalysis::ExportOptions::presets },
                            { 21, "Levels, brightness and energy ranges (dynamics)", &MixAnalysis::ExportOptions::dynamics },
                            { 22, "Modulation, limiter and ducking through the loop", &MixAnalysis::ExportOptions::modulation },
                            { 18, "Explanation for an AI (the plugin, the data, how to analyse)", &MixAnalysis::ExportOptions::about } };
    for (const auto& entry : entries)
        menu.addItem (entry.id, entry.text, true, exportOptions.*entry.flag);
    menu.addSeparator();
    menu.addItem (30, "Only the sources chosen in SHOW", true, exportOnlyShown);

    juce::Component::SafePointer<SpectrumPage> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton), [safeThis, entries] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        if (result == 1)
        {
            safeThis->copyReport();
            return;
        }
        if (result == 2)
        {
            safeThis->saveExportFile();
            return;
        }
        for (const auto& entry : entries)
            if (entry.id == result)
                safeThis->exportOptions.*entry.flag = ! (safeThis->exportOptions.*entry.flag);
        if (result == 30)
            safeThis->exportOnlyShown = ! safeThis->exportOnlyShown;
        Settings::set ("exportContent", safeThis->exportOptions.toString());
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showExportMenu(); });
    });
}

void SpectrumPage::saveExportFile()
{
    if (! content.getReport().valid)
        return;
    const auto folder = Settings::rootFolder().getChildFile ("Analysis");
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save the analysis", folder.getChildFile ("kickbase-analysis-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H%M%S") + ".json"), "*.json");
    juce::Component::SafePointer<SpectrumPage> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || fc.getResult() == juce::File())
            return;
        auto report = safeThis->analyser.getReport();
        report.aiNotes = safeThis->processor.getAiNotesVar();
        const auto file = fc.getResult().withFileExtension ("json");
        MixAnalysis::ExportFilter filter;
        if (safeThis->exportOnlyShown)
            for (const auto& source : safeThis->sources)
                if (source.present && source.selected)
                    filter.sources.add (source.name);
        if (report.saveExport (file, safeThis->exportOptions, filter))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Analysis saved", file.getFullPathName() + "\n(" + juce::String (file.getSize() / 1024) + " kB)");
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Analysis", "Could not write " + file.getFullPathName());
    });
}

void SpectrumPage::updateShowText()
{
    juce::StringArray names;
    auto presentCount = 0;
    for (const auto& source : sources)
    {
        presentCount += source.present ? 1 : 0;
        if (source.present && source.selected)
            names.add (source.name);
    }
    juce::String text;
    if (names.isEmpty())
        text = "SHOW: NOTHING";
    else if (names.size() == presentCount)
        text = "SHOW: ALL";
    else
        text = "SHOW: " + names.joinIntoString (", ");
    showButton.setButtonText (text);
    showButton.setTooltip ("SHOW - " + (names.isEmpty() ? juce::String ("nothing is shown") : names.joinIntoString (", ") + " " + (names.size() == 1 ? "is" : "are") + " shown")
                           + " in the diagram. Click to tick or untick the kick, each instrument and the master (the list stays open so you can pick several).");
}

void SpectrumPage::showSourceMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1000, "All", true, false);
    menu.addItem (1001, "None", true, false);
    menu.addSeparator();
    for (int i = 0; i < numSources; ++i)
    {
        if (! sources[(size_t) i].present)
            continue;
        juce::PopupMenu::Item item (sources[(size_t) i].name);
        item.setID (i + 1).setTicked (sources[(size_t) i].selected).setColour (sources[(size_t) i].colour);
        menu.addItem (item);
    }

    juce::Component::SafePointer<SpectrumPage> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&showButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        if (result == 1000 || result == 1001)
            for (auto& source : safeThis->sources)
                source.selected = result == 1000;
        else
            safeThis->sources[(size_t) (result - 1)].selected = ! safeThis->sources[(size_t) (result - 1)].selected;
        safeThis->updateShowText();
        safeThis->repaint();
        // Listan öppnas igen: man väljer ofta flera.
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showSourceMenu(); });
    });
}

void SpectrumPage::chooseReference()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Load a reference track ...");
    if (reference != nullptr)
        menu.addItem (2, "Remove the reference (" + reference->name + ")");
    juce::Component::SafePointer<SpectrumPage> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&referenceButton), [safeThis] (int result)
    {
        if (safeThis == nullptr)
            return;
        if (result == 2)
        {
            safeThis->reference.reset();
            Settings::set ("referencePath", {});
            safeThis->referenceButton.setButtonText ("REFERENCE");
            safeThis->repaint();
        }
        else if (result == 1)
        {
            safeThis->chooser = std::make_unique<juce::FileChooser> ("Choose a reference track", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                                     "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
            safeThis->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safeThis] (const juce::FileChooser& fc)
            {
                if (safeThis != nullptr && fc.getResult().existsAsFile())
                    safeThis->loadReferenceFile (fc.getResult());
            });
        }
    });
}

void SpectrumPage::loadReferenceFile (const juce::File& file)
{
    referenceButton.setButtonText ("LOADING ...");
    juce::Component::SafePointer<SpectrumPage> safeThis (this);
    juce::Thread::launch ([safeThis, file]
    {
        juce::String error;
        auto loaded = MixAnalysis::loadReference (file, error);
        juce::MessageManager::callAsync ([safeThis, file, loaded, error]
        {
            if (safeThis == nullptr)
                return;
            safeThis->reference = loaded;
            safeThis->referenceError = error;
            if (loaded != nullptr)
            {
                Settings::set ("referencePath", file.getFullPathName());
                safeThis->referenceButton.setButtonText ("REF: " + loaded->name.substring (0, 9).toUpperCase());
            }
            else
            {
                safeThis->referenceButton.setButtonText ("REFERENCE");
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Reference", error);
            }
            safeThis->repaint();
        });
    });
}

//==============================================================================
// EQ i diagrammet

juce::String SpectrumPage::eqKey (int node, const char* field) const
{
    return SourceEq::prefixFor (eqSource) + "Eq" + juce::String (node + 1) + field;
}

float SpectrumPage::eqParameter (const juce::String& suffix) const
{
    if (const auto* value = processor.apvts.getRawParameterValue (SourceEq::prefixFor (eqSource) + suffix))
        return value->load();
    return 0.0f;
}

void SpectrumPage::setEqParameter (const juce::String& suffix, float value)
{
    if (auto* parameter = processor.apvts.getParameter (SourceEq::prefixFor (eqSource) + suffix))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::vector<SpectrumPage::EqNode> SpectrumPage::eqNodes() const
{
    std::vector<EqNode> nodes;
    const auto area = plotArea();
    for (int b = 0; b < Effects::eqBands; ++b)
    {
        const auto freq = eqParameter ("Eq" + juce::String (b + 1) + "F"), gain = eqParameter ("Eq" + juce::String (b + 1) + "G");
        nodes.push_back ({ { xFor (freq), dbToY (eqCentreDb + gain) }, b });
    }
    nodes.push_back ({ { xFor (std::max (lowHz, eqParameter ("EqHp"))), area.getBottom() - 7.0f }, 4 });
    nodes.push_back ({ { xFor (std::min (highHz, eqParameter ("EqLp"))), area.getBottom() - 7.0f }, 5 });
    return nodes;
}

int SpectrumPage::eqHit (juce::Point<float> position) const
{
    auto best = -1;
    auto bestDistance = 11.0f;
    for (const auto& node : eqNodes())
    {
        const auto distance = node.position.getDistanceFrom (position);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = node.id;
        }
    }
    return best;
}

void SpectrumPage::paintEq (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto colour = sources[(size_t) eqSource].colour;
    const auto settings = SourceEq::read (processor.apvts, SourceEq::prefixFor (eqSource));
    const auto rate = std::max (8000.0, processor.getSampleRate());
    const auto coefficients = Effects::eqCoefficients (rate, settings);

    g.setColour (Theme::effects.withAlpha (0.35f));
    const float dashes[] { 4.0f, 4.0f };
    g.drawDashedLine (juce::Line<float> (area.getX(), dbToY (eqCentreDb), area.getRight(), dbToY (eqCentreDb)), dashes, 2, 1.0f);

    juce::Path curve;
    const auto width = juce::jmax (1, (int) area.getWidth());
    const auto pixelsPerDb = area.getHeight() / (dbTop - dbBottom);
    for (int px = 0; px <= width; ++px)
    {
        const auto hz = lowHz * std::pow (highHz / lowHz, (float) px / (float) width);
        auto magnitude = 1.0f;
        for (const auto& c : coefficients)
            magnitude = (float) (magnitude * c->getMagnitudeForFrequency (hz, rate));
        const auto db = juce::jlimit (-60.0f, 30.0f, juce::Decibels::gainToDecibels (magnitude, -60.0f));
        const auto y = dbToY (eqCentreDb) - db * pixelsPerDb;
        if (px == 0)
            curve.startNewSubPath (area.getX(), y);
        else
            curve.lineTo (area.getX() + (float) px, y);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (curve, juce::PathStrokeType (4.0f));
    g.setColour (colour.brighter (0.3f));
    g.strokePath (curve, juce::PathStrokeType (2.2f));

    g.setFont (Theme::font (9.5f, true));
    for (const auto& node : eqNodes())
    {
        const auto active = dragNode == node.id;
        if (node.id < Effects::eqBands)
        {
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (node.position));
            g.setColour (active ? juce::Colours::white : colour);
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (node.position));
            g.setColour (juce::Colours::black);
            g.drawText (juce::String (node.id + 1), juce::Rectangle<float> (14.0f, 14.0f).withCentre (node.position), juce::Justification::centred);
        }
        else
        {
            juce::Path triangle;
            const auto p = node.position;
            if (node.id == 4)
                triangle.addTriangle (p.x - 6.0f, p.y + 6.0f, p.x - 6.0f, p.y - 6.0f, p.x + 6.0f, p.y);
            else
                triangle.addTriangle (p.x + 6.0f, p.y + 6.0f, p.x + 6.0f, p.y - 6.0f, p.x - 6.0f, p.y);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.strokePath (triangle, juce::PathStrokeType (3.0f));
            g.setColour (active ? juce::Colours::white : Theme::effects);
            g.fillPath (triangle);
            g.setColour (Theme::textDim);
            const auto text = node.id == 4 ? (settings.hp <= 21.0f ? juce::String ("HP off") : "HP " + juce::String (juce::roundToInt (settings.hp)) + " Hz")
                                           : (settings.lp >= 19500.0f ? juce::String ("LP off") : "LP " + juce::String (settings.lp / 1000.0f, 1) + " kHz");
            g.drawText (text, juce::Rectangle<float> (60.0f, 11.0f).withCentre ({ p.x + (node.id == 4 ? 36.0f : -36.0f), p.y - 1.0f }), juce::Justification::centred);
        }
    }

    if (dragNode >= 0 && dragNode < Effects::eqBands)
    {
        const auto text = "BAND " + juce::String (dragNode + 1) + "   " + describe (settings.freq[dragNode]) + "   " + (settings.gain[dragNode] > 0 ? "+" : "")
                          + juce::String (settings.gain[dragNode], 1) + " dB   Q " + juce::String (settings.q[dragNode], 2);
        g.setColour (Theme::knobBodyDark.withAlpha (0.92f));
        g.fillRoundedRectangle (area.getX() + 6.0f, area.getY() + 6.0f, 270.0f, 18.0f, 4.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (10.5f, true));
        g.drawText (text, juce::Rectangle<float> (area.getX() + 12.0f, area.getY() + 6.0f, 262.0f, 18.0f), juce::Justification::centredLeft);
    }
}

void SpectrumPage::mouseDown (const juce::MouseEvent& event)
{
    if (reportView || ! eqButton.getToggleState() || ! plotArea().expanded (8.0f).contains (event.position))
        return;
    dragNode = eqHit (event.position);
    if (dragNode < 0)
        return;
    gestureParameters.clear();
    const auto prefix = SourceEq::prefixFor (eqSource);
    auto add = [this, &prefix] (const juce::String& suffix)
    {
        if (auto* parameter = processor.apvts.getParameter (prefix + suffix))
        {
            parameter->beginChangeGesture();
            gestureParameters.push_back (parameter);
        }
    };
    if (dragNode < Effects::eqBands)
    {
        add ("Eq" + juce::String (dragNode + 1) + "F");
        add ("Eq" + juce::String (dragNode + 1) + "G");
    }
    else
    {
        add (dragNode == 4 ? "EqHp" : "EqLp");
    }
    repaint();
}

void SpectrumPage::mouseDrag (const juce::MouseEvent& event)
{
    if (dragNode < 0)
        return;
    const auto area = plotArea();
    const auto hz = hzFor (juce::jlimit (area.getX(), area.getRight(), event.position.x));
    if (dragNode < Effects::eqBands)
    {
        const auto key = "Eq" + juce::String (dragNode + 1);
        setEqParameter (key + "F", hz);
        setEqParameter (key + "G", juce::jlimit (-15.0f, 15.0f, yToDb (event.position.y) - eqCentreDb));
    }
    else if (dragNode == 4)
    {
        setEqParameter ("EqHp", hz < 24.0f ? 20.0f : hz);
    }
    else
    {
        setEqParameter ("EqLp", hz > 18500.0f ? 20000.0f : hz);
    }
    repaint();
}

void SpectrumPage::mouseUp (const juce::MouseEvent&)
{
    for (auto* parameter : gestureParameters)
        parameter->endChangeGesture();
    gestureParameters.clear();
    dragNode = -1;
    repaint();
}

void SpectrumPage::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (reportView || ! eqButton.getToggleState())
        return;
    const auto hit = eqHit (event.position);
    if (hit < 0)
        return;
    auto reset = [this] (const juce::String& suffix)
    {
        if (auto* parameter = processor.apvts.getParameter (SourceEq::prefixFor (eqSource) + suffix))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    };
    if (hit < Effects::eqBands)
    {
        const auto key = "Eq" + juce::String (hit + 1);
        reset (key + "F");
        reset (key + "G");
        reset (key + "Q");
    }
    else
    {
        reset (hit == 4 ? "EqHp" : "EqLp");
    }
    repaint();
}

void SpectrumPage::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (reportView || ! eqButton.getToggleState())
        return;
    const auto hit = eqHit (event.position);
    if (hit < 0 || hit >= Effects::eqBands)
        return;
    const auto key = "Eq" + juce::String (hit + 1) + "Q";
    if (auto* parameter = processor.apvts.getParameter (SourceEq::prefixFor (eqSource) + key))
    {
        const auto q = juce::jlimit (0.3f, 8.0f, eqParameter (key) * std::exp (wheel.deltaY * 0.6f));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (q));
        parameter->endChangeGesture();
        repaint();
    }
}

//==============================================================================
void SpectrumPage::mouseMove (const juce::MouseEvent& e)
{
    hoverX = (float) e.x;
    hoverPoint = e.position;
    if (running)
        repaint (plotBody);
}

void SpectrumPage::mouseExit (const juce::MouseEvent&)
{
    hoverX = -1.0f;
    repaint();
}

void SpectrumPage::analyse (int index)
{
    if (index == kickSource)
        processor.kickTap.copyLatest (size, left, right);
    else if (index == masterSource)
        processor.getMasterSection().outputTap.copyLatest (size, left, right);
    else
        processor.instrumentTaps[(size_t) (index - firstInstrument)].copyLatest (size, left, right);

    auto silent = true;
    for (int i = 0; i < size; ++i)
    {
        fftData[(size_t) i] = 0.5f * (left[(size_t) i] + right[(size_t) i]);
        silent = silent && fftData[(size_t) i] == 0.0f;
    }

    // Normaliserat så att en sinus i full skala blir 0 dB. Analysen görs direkt i bildpunkter (en logaritm per punkt, inte per steg).
    constexpr float norm = 4.0f / (float) size;
    auto& source = sources[(size_t) index];
    const auto width = juce::jmax (1, (int) plotArea().getWidth());
    ensureMapping (width, std::max (8000.0, processor.getSampleRate()));
    if ((int) source.target.size() != width)
        source.target.assign ((size_t) width, -120.0f);

    if (silent)
    {
        // En tyst källa (ett instrument som inte spelar): samma resultat som en FFT av nollor (-120 dB överallt), utan att räkna den.
        std::fill (source.target.begin(), source.target.end(), -120.0f);
        for (auto& level : source.low)
            level = -120.0f > level ? -120.0f : level + (-120.0f - level) * smoothing;
        return;
    }

    std::fill (fftData.begin() + size, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), size);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    for (int px = 0; px < width; ++px)
    {
        const auto first = pixelFirst[(size_t) px], last = pixelLast[(size_t) px];
        auto magnitude = 0.0f;
        if (last <= first)
        {
            magnitude = fftData[(size_t) first] + (fftData[(size_t) first + 1] - fftData[(size_t) first]) * pixelFraction[(size_t) px]; // under ett steg per punkt: interpolera
        }
        else
        {
            for (int b = first; b <= last; ++b)
                magnitude = std::max (magnitude, fftData[(size_t) b]);
        }
        source.target[(size_t) px] = juce::Decibels::gainToDecibels (magnitude * norm, -120.0f);
    }
    for (int b = 0; b < lowBinCount; ++b)
    {
        const auto db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * norm, -120.0f);
        source.low[(size_t) b] = db > source.low[(size_t) b] ? db : source.low[(size_t) b] + (db - source.low[(size_t) b]) * smoothing;
    }
}

void SpectrumPage::smoothColumns()
{
    // Uppåt: närma sig snabbt (så att slag syns direkt men inte hoppar). Neråt: en jämn fart i dB per sekund, oberoende av hur ofta analysen körs,
    // så att kurvan tonar ut mellan analyserna i stället för att stå still och hoppa.
    constexpr float riseFraction = 0.55f, fallDbPerSecond = 40.0f;
    const auto fall = fallDbPerSecond / (float) displayHz;
    for (auto& source : sources)
    {
        if (! source.present || ! source.selected || source.target.empty())
            continue;
        if (source.column.size() != source.target.size())
            source.column = source.target;
        for (size_t px = 0; px < source.target.size(); ++px)
        {
            const auto goal = source.target[px];
            auto& value = source.column[px];
            value = goal > value ? value + (goal - value) * riseFraction : std::max (goal, value - fall);
        }
    }
}

void SpectrumPage::ensureMapping (int width, double rate)
{
    if (width == mappedWidth && rate == mappedRate)
        return;
    mappedWidth = width;
    mappedRate = rate;
    pixelFirst.assign ((size_t) width, 0);
    pixelLast.assign ((size_t) width, 0);
    pixelFraction.assign ((size_t) width, 0.0f);
    for (int px = 0; px < width; ++px)
    {
        const auto hz0 = lowHz * std::pow (highHz / lowHz, (float) px / (float) width);
        const auto hz1 = lowHz * std::pow (highHz / lowHz, (float) (px + 1) / (float) width);
        const auto exact0 = (float) (hz0 / (rate * 0.5) * bins), exact1 = (float) (hz1 / (rate * 0.5) * bins);
        const auto b0 = (int) exact0, b1 = (int) exact1;
        if (b1 <= b0)
        {
            const auto position = 0.5f * (exact0 + exact1) - 0.5f;
            const auto lower = juce::jlimit (0, bins - 2, (int) std::floor (position));
            pixelFirst[(size_t) px] = pixelLast[(size_t) px] = lower;
            pixelFraction[(size_t) px] = juce::jlimit (0.0f, 1.0f, position - (float) lower);
        }
        else
        {
            pixelFirst[(size_t) px] = juce::jlimit (0, bins - 2, b0);
            pixelLast[(size_t) px] = juce::jlimit (pixelFirst[(size_t) px] + 1, bins - 1, b1);
        }
    }
}

// Grundtonen: den lägsta toppen mellan 28 och 400 Hz som ligger inom 10 dB från den starkaste (en bas har ofta en starkare 2:a tonen).
void SpectrumPage::findFundamental (Source& source) const
{
    const auto rate = std::max (8000.0, processor.getSampleRate());
    const auto binHz = (float) rate * 0.5f / (float) bins;
    const auto first = std::max (2, (int) (28.0f / binHz)), last = std::min (lowBinCount - 2, (int) (400.0f / binHz));
    auto strongest = -120.0f;
    for (int b = first; b <= last; ++b)
        strongest = std::max (strongest, source.low[(size_t) b]);
    if (strongest < -58.0f)
    {
        source.fundamental = 0.0f;
        return;
    }
    for (int b = first; b <= last; ++b)
    {
        const auto level = source.low[(size_t) b];
        if (level >= strongest - 10.0f && level >= source.low[(size_t) b - 1] && level >= source.low[(size_t) b + 1])
        {
            const auto a = source.low[(size_t) b - 1], c = source.low[(size_t) b + 1];
            const auto denominator = a - 2.0f * level + c;
            const auto delta = std::abs (denominator) > 1.0e-3f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denominator) : 0.0f;
            source.fundamental = ((float) b + delta) * binHz;
            return;
        }
    }
    source.fundamental = 0.0f;
}

void SpectrumPage::timerCallback()
{
    // SETTINGS > OPTIMIZATIONS (eller SPEED) har ändrats: ny takt och fyllning direkt.
    if (const auto version = Settings::visualVersion(); version != settingsVersion)
    {
        settingsVersion = version;
        updateRunning();
    }
    updateSources();

    // Anteckningar från en AI: de visas i rapporten, och rapportvyn visas inte automatiskt (man kan vara i spektrumet).
    if (const auto version = processor.aiNotesVersion.load(); version != notesVersionShown)
    {
        notesVersionShown = version;
        const auto notes = processor.getAiNotes();
        content.setNotes (notes);
        content.setWidth (reportView ? viewport.getMaximumVisibleWidth() : content.getWidth());
        if (! reportView && ! notes.empty())
            viewReportButton.setButtonText ("ANALYSIS + REPORT *"); // en stjärna: något nytt i rapporten
    }
    if (reportView && viewReportButton.getButtonText().endsWith ("*"))
        viewReportButton.setButtonText ("ANALYSIS + REPORT");

    // En analys som en AI startade (eller en som redan finns när fliken öppnas) visas här som om den kördes härifrån.
    if (! analysing && analyser.isRunning())
    {
        analysing = true;
        analyseButton.setButtonText ("CANCEL");
        for (auto* c : std::initializer_list<juce::Component*> { &referenceButton, &sensitivityBox, &tapBox, &eqButton, &eqTargetBox })
            c->setEnabled (false);
        updateRunning();
    }
    else if (! analysing && analyser.getVersion() != reportVersion)
    {
        takeReport();
    }

    if (analysing)
    {
        if (! analyser.isRunning())
        {
            analysing = false;
            analyseButton.setButtonText ("ANALYSE");
            for (auto* c : std::initializer_list<juce::Component*> { &referenceButton, &sensitivityBox, &tapBox, &eqButton, &eqTargetBox })
                c->setEnabled (true);
            takeReport();
            updateRunning();
        }
        else
        {
            analyseButton.setButtonText ("CANCEL " + juce::String (juce::roundToInt (analyser.getProgress() * 100.0f)) + " %");
        }
    }

    if (running)
    {
        ++tick;
        if ((tick - 1) % analysisEvery == 0) // den första bildrutan analyserar direkt
        {
            ++analysisCount;
            for (int i = 0; i < numSources; ++i)
            {
                auto& source = sources[(size_t) i];
                if (! source.present || ! source.selected)
                    continue;
                analyse (i);
                if (i != masterSource && analysisCount % 3 == 1)
                    findFundamental (source);
            }
        }
        smoothColumns();
    }
    if (running)
        repaint (plotBody.expanded (2));
}

void SpectrumPage::paint (juce::Graphics& g)
{
    if (reportView)
    {
        g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
        g.fillRoundedRectangle (viewport.getBounds().toFloat().expanded (2.0f), 6.0f);
        return;
    }
    // Källväljaren har samma färg som källan (bara när den ändras: setColour ritar om).
    if (const auto tint = sources[(size_t) eqSource].colour; eqTargetBox.findColour (juce::ComboBox::outlineColourId) != tint)
    {
        eqTargetBox.setColour (juce::ComboBox::outlineColourId, tint);
        eqTargetBox.setColour (juce::ComboBox::textColourId, tint);
    }

    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (plotBody.toFloat(), 6.0f);

    const auto area = plotArea();
    auto yFor = [this] (float db) { return dbToY (db); };

    g.setFont (Theme::font (9.0f));
    for (const auto hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xFor (hz);
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (Theme::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 14.0f, area.getBottom() + 1.0f, 28.0f, 11.0f), juce::Justification::centred);
    }
    for (const auto db : { -20.0f, -40.0f, -60.0f, -80.0f })
    {
        g.setColour (Theme::grid);
        g.drawHorizontalLine ((int) yFor (db), area.getX(), area.getRight());
        g.setColour (Theme::textDim.withAlpha (0.7f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (area.getX() - 22.0f, yFor (db) - 5.0f, 20.0f, 10.0f), juce::Justification::right);
    }

    // Det rapporten pekar på: frekvensområdet.
    if (highlightHigh > 0.0f)
    {
        const auto x0 = xFor (std::max (lowHz, highlightLow)), x1 = xFor (std::min (highHz, highlightHigh));
        g.setColour (juce::Colour (0xffffe27a).withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (x0, area.getY(), std::max (2.0f, x1 - x0), area.getHeight()));
        g.setColour (juce::Colour (0xffffe27a).withAlpha (0.55f));
        g.drawVerticalLine ((int) x0, area.getY(), area.getBottom());
        g.drawVerticalLine ((int) std::max (x0 + 1.0f, x1), area.getY(), area.getBottom());
    }

    if (! running)
    {
        if (! eqButton.getToggleState())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (13.0f, true));
            g.drawFittedText (analyserButton.getToggleState() ? "Open this tab to see the spectrum." : "The live spectrum is off.\nPress ANALYSER to start it (it uses some CPU while it runs),\nor ANALYSE to get a report on the whole pattern.",
                              area.toNearestInt().reduced (20), juce::Justification::centred, 4);
        }
        else
        {
            paintEq (g, area);
        }
        return;
    }

    // Nivån (dB) per bildpunkt och källa kommer färdig från analysen (läses direkt, utan att kopieras varje bildruta).
    const auto width = juce::jmax (1, (int) area.getWidth());
    struct ColumnView
    {
        const std::vector<float>* values = nullptr;
        bool empty() const { return values == nullptr; }
        float operator[] (size_t px) const { return (*values)[px]; }
        const std::vector<float>& get() const { return *values; }
    };
    std::array<ColumnView, numSources> columns;
    for (int i = 0; i < numSources; ++i)
    {
        const auto& source = sources[(size_t) i];
        if (source.present && source.selected && (int) source.column.size() == width)
            columns[(size_t) i].values = &source.column;
    }

    // Krockar: under den näst starkaste källans nivå har minst två källor energi. Rött, starkare med nivå och antal.
    auto shown = 0;
    for (int i = 0; i < numSources; ++i)
        shown += (i != masterSource && ! columns[(size_t) i].empty()) ? 1 : 0;
    if (collisionsButton.getToggleState() && shown >= 2)
    {
        // Intilliggande bildpunkter med (nästan) samma rödhet och höjd ritas som en enda ruta.
        auto runStart = -1, runAlpha = 0, runTop = 0;
        auto flush = [&] (int end)
        {
            if (runStart >= 0 && runAlpha > 0)
                g.fillRect (juce::Rectangle<float> (area.getX() + (float) runStart, (float) runTop, (float) (end - runStart), area.getBottom() - (float) runTop));
            runStart = -1;
        };
        for (int px = 0; px < width; ++px)
        {
            auto best = -120.0f, second = -120.0f;
            auto count = 0;
            for (int i = 0; i < numSources; ++i)
            {
                if (i == masterSource || columns[(size_t) i].empty())
                    continue;
                const auto db = columns[(size_t) i][(size_t) px];
                count += db > collisionFloor ? 1 : 0;
                if (db > best)
                {
                    second = best;
                    best = db;
                }
                else if (db > second)
                {
                    second = db;
                }
            }
            auto alphaStep = 0, top = 0;
            if (second > collisionFloor)
            {
                const auto strength = juce::jlimit (0.0f, 1.0f, (second - collisionFloor) / 46.0f);
                const auto alpha = juce::jlimit (0.0f, 0.95f, 0.12f + 0.6f * strength + 0.1f * (float) std::max (0, count - 2));
                alphaStep = juce::roundToInt (alpha * 20.0f);
                top = ((int) yFor (second) / 3) * 3;
            }
            if (alphaStep != runAlpha || top != runTop)
            {
                flush (px);
                runAlpha = alphaStep;
                runTop = top;
                runStart = px;
                g.setColour (juce::Colour (0xffff2a3a).withAlpha ((float) alphaStep / 20.0f));
            }
        }
        flush (width);
    }

    // Referenslåten: medelspektrumet, nivån anpassad till mastern (långsamt, så att linjen inte hoppar).
    if (reference != nullptr)
    {
        const auto refRate = std::max (8000.0, reference->rate);
        const auto rebuild = refKey != reference.get() || (int) refColumn.size() != width;
        if (rebuild)
        {
            refKey = reference.get();
            refColumn.assign ((size_t) width, -120.0f);
        }
        for (int px = 0; rebuild && px < width; ++px)
        {
            const auto hz0 = lowHz * std::pow (highHz / lowHz, (float) px / (float) width);
            const auto hz1 = lowHz * std::pow (highHz / lowHz, (float) (px + 1) / (float) width);
            const auto e0 = (float) (hz0 / (refRate * 0.5) * bins), e1 = (float) (hz1 / (refRate * 0.5) * bins);
            const auto b0 = std::min (bins - 2, (int) e0), b1 = std::min (bins - 1, (int) e1);
            auto power = 0.0f;
            if (b1 <= b0)
            {
                const auto position = 0.5f * (e0 + e1) - 0.5f;
                const auto lower = juce::jlimit (0, bins - 2, (int) std::floor (position));
                const auto fraction = juce::jlimit (0.0f, 1.0f, position - (float) lower);
                power = reference->power[(size_t) lower] + (reference->power[(size_t) lower + 1] - reference->power[(size_t) lower]) * fraction;
            }
            else
            {
                for (int b = b0; b <= b1; ++b)
                    power = std::max (power, reference->power[(size_t) b]);
            }
            refColumn[(size_t) px] = 10.0f * std::log10 (power + 1.0e-12f);
        }
        if (! columns[(size_t) masterSource].empty())
        {
            auto sum = 0.0f;
            auto count = 0;
            for (int px = 0; px < width; ++px)
            {
                const auto hz = lowHz * std::pow (highHz / lowHz, (float) px / (float) width);
                if (hz >= 120.0f && hz <= 5000.0f && columns[(size_t) masterSource][(size_t) px] > -80.0f)
                {
                    sum += columns[(size_t) masterSource][(size_t) px] - refColumn[(size_t) px];
                    ++count;
                }
            }
            if (count > 20)
                referenceOffset += (sum / (float) count - referenceOffset) * 0.06f;
        }
        juce::Path path;
        for (int px = 0; px < width; ++px)
        {
            const auto y = yFor (refColumn[(size_t) px] + referenceOffset);
            if (px == 0)
                path.startNewSubPath (area.getX(), y);
            else
                path.lineTo (area.getX() + (float) px, y);
        }
        g.setColour (juce::Colour (0xffffe27a).withAlpha (0.8f));
        const float dashes[] { 5.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, path, dashes, 2);
        g.fillPath (dashed);
        g.setFont (Theme::font (9.5f, true));
        g.drawText ("REFERENCE: " + reference->name, juce::Rectangle<float> (area.getRight() - 260.0f, area.getY() + 2.0f, 254.0f, 12.0f), juce::Justification::centredRight);
    }

    const auto stride = width > 700 ? 2 : 1; // varannan punkt räcker för kurvorna när diagrammet är brett
    auto pathFor = [&] (const std::vector<float>& column, bool close)
    {
        juce::Path path;
        path.startNewSubPath (area.getX(), area.getBottom());
        for (int px = 0; px < width; px += stride)
            path.lineTo (area.getX() + (float) px, yFor (column[(size_t) px]));
        if (close)
        {
            path.lineTo (area.getX() + (float) width, area.getBottom());
            path.closeSubPath();
        }
        return path;
    };

    for (int i = 0; i < numSources; ++i)
    {
        if (columns[(size_t) i].empty())
            continue;
        const auto& colour = sources[(size_t) i].colour;
        if (i == masterSource)
        {
            g.setColour (colour.withAlpha (0.85f));
            g.strokePath (pathFor (columns[(size_t) i].get(), false), juce::PathStrokeType (1.4f));
            continue;
        }
        if (! fillCurves) // bara kanten: mycket billigare att rita
        {
            g.setColour (colour);
            g.strokePath (pathFor (columns[(size_t) i].get(), false), juce::PathStrokeType (1.3f));
            continue;
        }
        const auto path = pathFor (columns[(size_t) i].get(), true);
        g.setColour (colour.withAlpha (0.16f));
        g.fillPath (path);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.3f));
    }

    // Övertonsserier: grundtonen och dess multiplar, en rad med markeringar per källa högst upp.
    if (harmonicsButton.getToggleState())
    {
        constexpr float lane = 12.0f;
        auto row = 0;
        auto listed = 0;
        for (int i = 0; i < numSources; ++i)
            if (i != masterSource && ! columns[(size_t) i].empty() && sources[(size_t) i].fundamental > 0.0f)
                ++listed;

        // Namn, Hz och ton i en fast lista uppe till vänster (står still, hoppar inte med frekvensen). I själva spektrumet ritas bara
        // grundtonens och övertonernas streck i källans färg, i rader under listan.
        const auto listHeight = (float) listed * lane + 4.0f;
        if (listed > 0)
        {
            g.setColour (Theme::knobBodyDark.withAlpha (0.8f));
            g.fillRoundedRectangle (juce::Rectangle<float> (area.getX() + 4.0f, area.getY() + 2.0f, 200.0f, listHeight), 4.0f);
            g.setFont (Theme::font (10.0f, true));
            auto listRow = 0;
            for (int i = 0; i < numSources; ++i)
            {
                const auto& source = sources[(size_t) i];
                if (i == masterSource || columns[(size_t) i].empty() || source.fundamental <= 0.0f)
                    continue;
                g.setColour (source.colour);
                g.drawText (source.name + "  " + describe (source.fundamental),
                            juce::Rectangle<float> (area.getX() + 10.0f, area.getY() + 4.0f + (float) listRow * lane, 190.0f, lane), juce::Justification::centredLeft);
                ++listRow;
            }
        }

        for (int i = 0; i < numSources; ++i)
        {
            const auto& source = sources[(size_t) i];
            if (i == masterSource || columns[(size_t) i].empty() || source.fundamental <= 0.0f)
                continue;
            const auto y = area.getY() + 6.0f + listHeight + (float) row * lane;
            for (int n = 1; n <= 24; ++n)
            {
                const auto hz = source.fundamental * (float) n;
                if (hz > highHz)
                    break;
                const auto x = xFor (hz);
                g.setColour (source.colour.withAlpha (n == 1 ? 0.95f : 0.7f));
                g.fillRect (x - 0.5f, y + (n == 1 ? 0.0f : 3.0f), 1.5f, n == 1 ? lane - 2.0f : lane - 5.0f);
                if (shown <= 3)
                {
                    g.setColour (source.colour.withAlpha (0.12f));
                    g.drawVerticalLine ((int) x, y + lane, area.getBottom());
                }
            }
            ++row;
        }
    }

    if (eqButton.getToggleState())
        paintEq (g, area);

    // Muspekaren: frekvens, ton och nivå för varje visad källa.
    if (dragNode < 0 && hoverX >= area.getX() && hoverX <= area.getRight() && hoverPoint.y >= area.getY() && hoverPoint.y <= area.getBottom())
    {
        const auto hz = hzFor (hoverX);
        g.setColour (Theme::text.withAlpha (0.4f));
        g.drawVerticalLine ((int) hoverX, area.getY(), area.getBottom());

        juce::StringArray lines;
        lines.add (describe (hz));
        const auto px = juce::jlimit (0, width - 1, (int) (hoverX - area.getX()));
        for (int i = 0; i < numSources; ++i)
            if (! columns[(size_t) i].empty())
                lines.add (sources[(size_t) i].name + "  " + juce::String (columns[(size_t) i][(size_t) px], 1) + " dB");

        const auto boxWidth = 150.0f, boxHeight = 6.0f + (float) lines.size() * 12.0f;
        auto box = juce::Rectangle<float> (hoverX + 10.0f, area.getBottom() - boxHeight - 6.0f, boxWidth, boxHeight);
        if (box.getRight() > area.getRight())
            box.setX (hoverX - 10.0f - boxWidth);
        g.setColour (Theme::knobBodyDark.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setFont (Theme::font (10.0f));
        for (int l = 0; l < lines.size(); ++l)
        {
            auto colour = Theme::text;
            if (l > 0)
            {
                auto seen = 0;
                for (int i = 0; i < numSources; ++i)
                    if (! columns[(size_t) i].empty() && ++seen == l)
                        colour = sources[(size_t) i].colour;
            }
            g.setColour (colour);
            g.drawText (lines[l], box.reduced (6.0f, 3.0f).withTrimmedTop ((float) l * 12.0f).withHeight (12.0f), juce::Justification::centredLeft);
        }
    }

    if (collisionsButton.getToggleState() && shown >= 2)
    {
        g.setColour (juce::Colour (0xffff5060).withAlpha (0.9f));
        g.setFont (Theme::font (9.5f, true));
        g.drawText ("RED = SEVERAL SOURCES AT THE SAME FREQUENCY", area.withTrimmedLeft (area.getWidth() - 300.0f).withHeight (12.0f).translated (-4.0f, 14.0f),
                    juce::Justification::centredRight);
    }
}

// Under en pågående analys: en slöja och ett förlopp över diagrammet.
void SpectrumPage::paintOverChildren (juce::Graphics& g)
{
    // EQ-läget: en ram runt EQ-knappen och källväljaren, och en ring runt samma källas knapp i raden under, så att man ser vad EQ:n gäller.
    if (eqButton.getToggleState() && eqTargetBox.isVisible())
    {
        const auto colour = Theme::effects;
        const auto group = eqButton.getBounds().getUnion (eqTargetBox.getBounds()).toFloat().expanded (3.0f, 3.0f);
        g.setColour (colour.withAlpha (0.14f));
        g.fillRoundedRectangle (group, 7.0f);
        g.setColour (colour.withAlpha (0.95f));
        g.drawRoundedRectangle (group, 7.0f, 1.8f);

    }

    if (! analysing)
        return;
    const auto bounds = overlayBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto bar = juce::Rectangle<float> (300.0f, 10.0f).withCentre (bounds.getCentre().translated (0.0f, 18.0f));
    g.setColour (Theme::track);
    g.fillRoundedRectangle (bar, 5.0f);
    g.setColour (Theme::mix);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * analyser.getProgress()), 5.0f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (13.0f, true));
    g.drawText ("ANALYSING THE PATTERN ...  " + juce::String (juce::roundToInt (analyser.getProgress() * 100.0f)) + " %",
                juce::Rectangle<float> (400.0f, 20.0f).withCentre (bounds.getCentre().translated (0.0f, -6.0f)), juce::Justification::centred);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.5f));
    g.drawText (processor.transportStatus.patternActive.load() ? "The DAW is playing: stop it for a faster analysis (a plugin cannot stop the DAW itself). CANCEL stops the analysis."
                                                                : "The plugin keeps working. Press CANCEL to stop.",
                juce::Rectangle<float> (bounds.getWidth() - 20.0f, 16.0f).withCentre (bounds.getCentre().translated (0.0f, 42.0f)), juce::Justification::centred);
}
