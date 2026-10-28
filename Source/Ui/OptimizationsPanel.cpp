#include "OptimizationsPanel.h"

namespace
{
    void styleLabel (juce::Label& label, float size, juce::Colour colour, bool bold = false)
    {
        label.setFont (Theme::font (size, bold));
        label.setColour (juce::Label::textColourId, colour);
        label.setMinimumHorizontalScale (0.7f);
    }

    int defaultFpsStep (Settings::Visual visual) { return visual == Settings::Visual::analyzer ? 3 : 4; } // samma som i Settings.cpp

    constexpr int rowHeight = 24, gap = 6;
    struct Widths { int fps, analyses, fill; };
    constexpr Widths wide { 150, 160, 74 }, narrow { 84, 84, 58 };

    struct Spec
    {
        Settings::Visual visual;
        const char* name;
        const char* tip;
        bool fill, analyses;
    };
}

OptimizationsPanel::OptimizationsPanel (PluginBase& processor, std::function<void()> onBack)
{
    backButton.setTooltip ("Back to the other settings");
    backButton.onClick = std::move (onBack);
    addAndMakeVisible (backButton);

    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::optimizations, "the OPTIMIZATIONS page");
    addAndMakeVisible (helpButton);

    styleLabel (globalLabel, 11.0f, Theme::text, true);
    globalLabel.setText ("GLOBAL", juce::dontSendNotification);
    addAndMakeVisible (globalLabel);
    reduceAnimationsToggle.setClickingTogglesState (true);
    reduceAnimationsToggle.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    reduceAnimationsToggle.setTooltip ("REDUCE ANIMATIONS - Uses less CPU everywhere: every view below is redrawn half as often and the logo's glow is off. "
                                       "Applies to every Kickbase window at once and is remembered.");
    reduceAnimationsToggle.onClick = [this] { Settings::setReduceAnimations (reduceAnimationsToggle.getToggleState()); };
    addAndMakeVisible (reduceAnimationsToggle);
    collisionGradientToggle.setClickingTogglesState (true);
    collisionGradientToggle.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    collisionGradientToggle.setTooltip ("COLLISION GRADIENT - The colour field over frequency and bar on ANALYZER > COLLISIONS. Off shows only the list and the markings, "
                                        "which draws faster. Remembered.");
    collisionGradientToggle.onClick = [this] { Settings::setCollisionGradient (collisionGradientToggle.getToggleState()); };
    collisionGradientToggle.setVisible (! processor.isEffectVersion()); // Kickbase FX har ingen ANALYZER-flik
    addChildComponent (collisionGradientToggle);
    styleLabel (globalHint, 10.5f, Theme::textDim);
    globalHint.setText ("Halves every rate below. Filled areas are the most expensive part of drawing: FILLED off saves the most.", juce::dontSendNotification);
    addAndMakeVisible (globalHint);

    const Spec specs[] {
        { Settings::Visual::analyzer, "ANALYZER: live spectrum", "The live spectrum on the ANALYZER tab (while ANALYSER is on). It is the most expensive view.", true, true },
        { Settings::Visual::sidechain, "SIDECHAIN: scope + meters", "The scope, the volume curve and the instruments' meters on the SIDECHAIN tab.", true, false },
        { Settings::Visual::maximizer, "MASTER: maximizer", "The maximizer's scope, gain reduction and meters on the MASTER tab.", true, false },
        { Settings::Visual::soundstage, "SOUNDSTAGE: spectrum + stereo", "The spectrum and the stereo picture on the SOUNDSTAGE tab.", true, false },
        { Settings::Visual::effects, "EFFECTS: EQ + compressor", "The EQ's live spectrum and the compressor's scopes on the EFFECTS tab.", true, false },
        { Settings::Visual::mixer, "MASTER: mixer meters", "The level meters of the mixer on the MASTER tab.", false, false },
        { Settings::Visual::topBar, "TOP BAR: OUT meter + logo", "The OUT meter and the logo's glow in the top bar (always visible).", false, false },
        { Settings::Visual::other, "EVERYTHING ELSE", "The playhead in the pattern and the piano roll, the knobs' modulation rings, the LFO, envelope, kick and wavetable pictures.", false, false },
    };

    for (const auto& spec : specs)
    {
        if (spec.visual == Settings::Visual::analyzer && processor.isEffectVersion())
            continue; // Kickbase FX har ingen ANALYZER-flik

        auto row = std::make_unique<Row>();
        row->visual = spec.visual;
        styleLabel (row->name, 11.0f, Theme::text, true);
        row->name.setText (spec.name, juce::dontSendNotification);
        row->name.setTooltip (spec.tip);
        addAndMakeVisible (row->name);

        for (int step = 1; step <= 5; ++step)
            row->fps.addItem (juce::String (step), step); // texten sätts i setTexts
        row->fps.setTooltip (juce::String (spec.name) + " - How many times per second it is redrawn: 5, 10, 20, 30 or 40 frames per second ("
                             + juce::String (Settings::fpsForStep (defaultFpsStep (spec.visual))) + " is the default). Lower uses less CPU, higher moves more smoothly. "
                             + spec.tip);
        const auto visual = spec.visual;
        auto* fps = &row->fps;
        row->fps.onChange = [visual, fps] { Settings::setVisualFpsStep (visual, fps->getSelectedId()); };
        addAndMakeVisible (row->fps);

        if (spec.analyses)
        {
            row->analyses = std::make_unique<juce::ComboBox>();
            for (int step = 1; step <= 5; ++step)
                row->analyses->addItem (juce::String (step), step);
            row->analyses->setTooltip ("ANALYZER: live spectrum - How many times per second the sound is analysed: 4, 8, 15, 30 or 40 (at most once per drawn frame). "
                                       "Fewer uses less CPU and is calmer to read. The same setting as SPEED on the ANALYZER tab.");
            auto* analyses = row->analyses.get();
            row->analyses->onChange = [analyses] { Settings::setSpectrumAnalysisStep (analyses->getSelectedId()); };
            addAndMakeVisible (*row->analyses);
        }

        if (spec.fill)
        {
            row->fill = std::make_unique<juce::TextButton> ("FILLED");
            row->fill->setClickingTogglesState (true);
            row->fill->setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
            row->fill->setTooltip (juce::String (spec.name) + " - FILLED: the areas under the curves and waves are filled. Off draws only the outlines, "
                                   "which costs much less CPU.");
            auto* fill = row->fill.get();
            row->fill->onClick = [visual, fill] { Settings::setVisualFill (visual, fill->getToggleState()); };
            addAndMakeVisible (*row->fill);
        }
        rows.push_back (std::move (row));
    }

    refresh();
}

void OptimizationsPanel::setTexts (bool shortTexts)
{
    for (auto& row : rows)
    {
        for (int step = 1; step <= 5; ++step)
        {
            const auto isDefault = step == defaultFpsStep (row->visual);
            row->fps.changeItemText (step, juce::String (Settings::fpsForStep (step)) + (shortTexts ? "/s" : " frames/s") + (isDefault ? (shortTexts ? " *" : "  (default)") : ""));
            if (row->analyses != nullptr)
                row->analyses->changeItemText (step, juce::String (Settings::spectrumAnalysesPerSecond (step)) + (shortTexts ? "/s" : " analyses/s")
                                                         + (step == 3 ? (shortTexts ? " *" : "  (default)") : ""));
        }
        if (row->fill != nullptr)
            row->fill->setButtonText (shortTexts ? "FILL" : "FILLED");
    }
    refresh(); // changeItemText byter bara listan: valet läses om så att den nya texten visas (getSelectedId jämför texten och ger 0 här)
}

void OptimizationsPanel::refresh()
{
    reduceAnimationsToggle.setToggleState (Settings::reduceAnimations(), juce::dontSendNotification);
    collisionGradientToggle.setToggleState (Settings::collisionGradient(), juce::dontSendNotification);
    for (auto& row : rows)
    {
        row->fps.setSelectedId (Settings::visualFpsStep (row->visual), juce::dontSendNotification);
        if (row->analyses != nullptr)
            row->analyses->setSelectedId (Settings::spectrumAnalysisStep(), juce::dontSendNotification);
        if (row->fill != nullptr)
            row->fill->setToggleState (Settings::visualFill (row->visual), juce::dontSendNotification);
    }
}

void OptimizationsPanel::paint (juce::Graphics& g)
{
    // Samma ram som rutorna på SETTINGS-sidan, över hela sidan.
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121420), bounds.getX(), bounds.getY(), juce::Colour (0xff181b28), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff24283c));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (Theme::bass.withAlpha (0.92f));
    g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.08f));
    g.drawText ("OPTIMIZATIONS  -  HOW OFTEN EACH VIEW IS REDRAWN. FEWER FRAMES PER SECOND USE LESS CPU.", getLocalBounds().withHeight (16).withTrimmedLeft (8),
                juce::Justification::centredLeft);
}

void OptimizationsPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 0).withTrimmedTop (20).withTrimmedBottom (6);

    auto top = area.removeFromTop (24);
    backButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (gap);
    helpButton.setBounds (top.removeFromRight (44));
    top.removeFromRight (gap);
    globalLabel.setBounds (top.removeFromLeft (60));
    reduceAnimationsToggle.setBounds (top.removeFromLeft (150));
    top.removeFromLeft (gap);
    if (collisionGradientToggle.isVisible())
    {
        collisionGradientToggle.setBounds (top.removeFromLeft (150));
        top.removeFromLeft (gap);
    }
    globalHint.setBounds (top);
    area.removeFromTop (8);

    // Raderna i så många kolumner som behövs för att de ska få plats på höjden (Kickbase FX har ett lägre fönster).
    const auto count = (int) rows.size();
    const auto maxRows = std::max (1, area.getHeight() / rowHeight);
    const auto columns = (count + maxRows - 1) / maxRows;
    const auto perColumn = (count + columns - 1) / columns;
    const auto columnGap = 18;
    const auto columnWidth = (area.getWidth() - (columns - 1) * columnGap) / columns;
    const auto shortTexts = columnWidth < 470;
    if ((int) shortTexts != compact)
    {
        compact = (int) shortTexts;
        setTexts (shortTexts);
        globalHint.setText (juce::String ("Halves every rate below. ") + (shortTexts ? "* = default. " : "")
                                + "Filled areas are the most expensive part of drawing: " + (shortTexts ? "FILL" : "FILLED") + " off saves the most.",
                            juce::dontSendNotification);
    }
    const auto& w = shortTexts ? narrow : wide;
    auto columnHasAnalyses = [&] (int column) // (bara kolumnen med ANALYZER reserverar plats för analysmenyn)
    {
        for (int i = column * perColumn; i < std::min (count, (column + 1) * perColumn); ++i)
            if (rows[(size_t) i]->analyses != nullptr)
                return true;
        return false;
    };

    for (int i = 0; i < count; ++i)
    {
        auto& row = *rows[(size_t) i];
        const auto column = i / perColumn, line = i % perColumn;
        const auto hasAnalyses = columnHasAnalyses (column);
        const auto controls = w.fps + (hasAnalyses ? gap + w.analyses : 0) + gap + w.fill;
        auto cell = juce::Rectangle<int> (area.getX() + column * (columnWidth + columnGap), area.getY() + line * rowHeight, columnWidth, rowHeight - 4);
        row.name.setBounds (cell.removeFromLeft (std::max (60, cell.getWidth() - controls - gap)));
        cell.removeFromLeft (gap);
        row.fps.setBounds (cell.removeFromLeft (w.fps));
        cell.removeFromLeft (gap);
        if (hasAnalyses)
        {
            auto slot = cell.removeFromLeft (w.analyses);
            if (row.analyses != nullptr)
                row.analyses->setBounds (slot);
            cell.removeFromLeft (gap);
        }
        if (row.fill != nullptr)
            row.fill->setBounds (cell.removeFromLeft (w.fill));
    }
}
