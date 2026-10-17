#include "SpectrumPage.h"

namespace
{
    constexpr float lowHz = 20.0f, highHz = 20000.0f;
    constexpr float dbTop = 0.0f, dbBottom = -90.0f;
    constexpr float collisionFloor = -66.0f; // under den nivån räknas en källa inte som "spelar"
    constexpr int stripHeight = 24;
}

SpectrumPage::SpectrumPage (PluginBase& p) : processor (p)
{
    sources[kickSource].colour = Theme::kick;
    sources[kickSource].name = "KICK";
    for (int i = 0; i < ParamID::numBassLayers; ++i)
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
                 "ANALYSER - Starts the analysis. It is off by default because it uses some CPU while it runs; it stops by itself when you leave this tab.");
    analyserButton.onClick = [this] { updateRunning(); };
    setUpToggle (collisionsButton, Theme::master, true,
                 "COLLISIONS - Marks in red where two or more of the shown sources play the same frequency at the same time. "
                 "The stronger the red, the louder they are and the more of them there are. Mixing: move, filter or retune one of them.");
    collisionsButton.onClick = [this] { repaint(); };
    setUpToggle (harmonicsButton, Theme::lfo, false,
                 "HARMONICS - Marks each shown source's fundamental and its overtones (2x, 3x, 4x ...) with ticks in its own colour, "
                 "so you can see which of the overtones line up with another source's.");
    harmonicsButton.onClick = [this] { repaint(); };

    for (size_t i = 0; i < chips.size(); ++i)
    {
        auto& chip = chips[i];
        setUpToggle (chip, sources[i].colour, true,
                     i == (size_t) kickSource ? "Show or hide the kick." : i == (size_t) masterSource ? "Show or hide the master output (a thin white line)."
                                                                                                      : "Show or hide this instrument.");
        chip.onClick = [this, i] { sources[i].selected = chips[i].getToggleState(); repaint(); };
    }

    for (auto& source : sources)
        source.level.fill (-120.0f);

    updateSources();

    if (processor.snapshotSpectrum)
    {
        analyserButton.setToggleState (true, juce::dontSendNotification);
        harmonicsButton.setToggleState (juce::SystemStats::getEnvironmentVariable ("KB_HARMONICS", "0").getIntValue() != 0, juce::dontSendNotification);
        updateRunning();
        timerCallback();
    }
}

SpectrumPage::~SpectrumPage()
{
    processor.instrumentTapsOn = false;
}

void SpectrumPage::updateRunning()
{
    const auto shouldRun = analyserButton.getToggleState() && (isShowing() || processor.snapshotSpectrum);
    if (shouldRun == running)
    {
        repaint();
        return;
    }
    running = shouldRun;
    processor.instrumentTapsOn = running;
    if (running)
    {
        for (auto& source : sources)
        {
            source.level.fill (-120.0f);
            source.fundamental = 0.0f;
        }
        startTimerHz (30);
    }
    else
    {
        stopTimer();
    }
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

    for (int i = 0; i < ParamID::numBassLayers; ++i)
    {
        auto& source = sources[(size_t) (firstInstrument + i)];
        source.present = i < count;
        source.name = processor.getLayerName (i).toUpperCase();
        chips[(size_t) (firstInstrument + i)].setButtonText (source.name);
        chips[(size_t) (firstInstrument + i)].setVisible (source.present);
    }
    for (size_t i = 0; i < chips.size(); ++i)
        chips[i].setButtonText (sources[i].name);
    resized();
}

void SpectrumPage::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    auto top = area.removeFromTop (stripHeight);
    analyserButton.setBounds (top.removeFromLeft (92));
    top.removeFromLeft (8);
    collisionsButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (4);
    harmonicsButton.setBounds (top.removeFromLeft (100));

    area.removeFromTop (4);
    auto row = area.removeFromTop (stripHeight);
    const auto font = Theme::font (11.0f, true);
    for (size_t i = 0; i < chips.size(); ++i)
    {
        if (! chips[i].isVisible())
            continue;
        const auto width = juce::jmax (52, juce::GlyphArrangement::getStringWidthInt (font, chips[i].getButtonText()) + 22);
        chips[i].setBounds (row.removeFromLeft (width));
        row.removeFromLeft (4);
    }
}

juce::Rectangle<float> SpectrumPage::plotArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 6.0f).withTrimmedTop ((float) (stripHeight * 2 + 12)).withTrimmedBottom (14.0f).withTrimmedLeft (22.0f);
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

juce::String SpectrumPage::describe (float hz) const
{
    const auto note = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
    return (hz < 100.0f ? juce::String (hz, 1) : juce::String (juce::roundToInt (hz))) + " Hz  " + juce::MidiMessage::getMidiNoteName (note, true, true, 5);
}

void SpectrumPage::mouseMove (const juce::MouseEvent& e)
{
    hoverX = (float) e.x;
    if (running)
        repaint();
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

    for (int i = 0; i < size; ++i)
        fftData[(size_t) i] = 0.5f * (left[(size_t) i] + right[(size_t) i]);
    std::fill (fftData.begin() + size, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), size);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // Normaliserat så att en sinus i full skala blir 0 dB.
    constexpr float norm = 4.0f / (float) size;
    auto& level = sources[(size_t) index].level;
    for (int b = 0; b < bins; ++b)
    {
        const auto db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * norm, -120.0f);
        level[(size_t) b] = db > level[(size_t) b] ? db : level[(size_t) b] + (db - level[(size_t) b]) * 0.3f; // snabbt upp, mjukt ner
    }
}

// Grundtonen: den lägsta toppen mellan 28 och 400 Hz som ligger inom 10 dB från den starkaste (en bas har ofta en starkare 2:a tonen).
void SpectrumPage::findFundamental (Source& source) const
{
    const auto rate = std::max (8000.0, processor.getSampleRate());
    const auto binHz = (float) rate * 0.5f / (float) bins;
    const auto first = std::max (2, (int) (28.0f / binHz)), last = std::min (bins - 2, (int) (400.0f / binHz));
    auto strongest = -120.0f;
    for (int b = first; b <= last; ++b)
        strongest = std::max (strongest, source.level[(size_t) b]);
    if (strongest < -58.0f)
    {
        source.fundamental = 0.0f;
        return;
    }
    for (int b = first; b <= last; ++b)
    {
        const auto level = source.level[(size_t) b];
        if (level >= strongest - 10.0f && level >= source.level[(size_t) b - 1] && level >= source.level[(size_t) b + 1])
        {
            const auto a = source.level[(size_t) b - 1], c = source.level[(size_t) b + 1];
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
    updateSources();
    ++tick;
    for (int i = 0; i < numSources; ++i)
    {
        auto& source = sources[(size_t) i];
        if (! source.present || ! source.selected)
            continue;
        analyse (i);
        if (i != masterSource && tick % 3 == 1)
            findFundamental (source);
    }
    repaint();
}

void SpectrumPage::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds.withTrimmedTop ((float) (stripHeight * 2 + 10)), 6.0f);

    const auto area = plotArea();
    auto yFor = [&] (float db) { return area.getY() + juce::jlimit (0.0f, 1.0f, (dbTop - db) / (dbTop - dbBottom)) * area.getHeight(); };

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

    if (! running)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (13.0f, true));
        g.drawFittedText (analyserButton.getToggleState() ? "Open this tab to see the spectrum." : "The analyser is off.\nPress ANALYSER to start it. It uses some CPU while it runs.",
                          area.toNearestInt().reduced (20), juce::Justification::centred, 3);
        return;
    }

    // Starkaste nivån (dB) per bildpunkt och källa.
    const auto rate = std::max (8000.0, processor.getSampleRate());
    const auto width = juce::jmax (1, (int) area.getWidth());
    std::array<std::vector<float>, numSources> columns;
    for (int i = 0; i < numSources; ++i)
    {
        const auto& source = sources[(size_t) i];
        if (! source.present || ! source.selected)
            continue;
        auto& column = columns[(size_t) i];
        column.resize ((size_t) width);
        for (int px = 0; px < width; ++px)
        {
            const auto hz0 = lowHz * std::pow (highHz / lowHz, (float) px / (float) width);
            const auto hz1 = lowHz * std::pow (highHz / lowHz, (float) (px + 1) / (float) width);
            const auto exact0 = (float) (hz0 / (rate * 0.5) * bins), exact1 = (float) (hz1 / (rate * 0.5) * bins);
            const auto b0 = (int) exact0, b1 = (int) exact1;
            auto db = -120.0f;
            if (b1 <= b0)
            {
                // Under ett steg per bildpunkt (låga frekvenser): interpolera mellan stegen, så att kurvan inte blir trappig.
                const auto position = 0.5f * (exact0 + exact1) - 0.5f;
                const auto lower = juce::jlimit (0, bins - 2, (int) std::floor (position));
                const auto fraction = juce::jlimit (0.0f, 1.0f, position - (float) lower);
                db = source.level[(size_t) lower] + (source.level[(size_t) lower + 1] - source.level[(size_t) lower]) * fraction;
            }
            else
            {
                for (int b = std::max (0, b0); b <= std::min (b1, bins - 1); ++b)
                    db = std::max (db, source.level[(size_t) b]);
            }
            column[(size_t) px] = db;
        }
    }

    // Krockar: under den näst starkaste källans nivå har minst två källor energi. Rött, starkare med nivå och antal.
    auto shown = 0;
    for (int i = 0; i < numSources; ++i)
        shown += (i != masterSource && ! columns[(size_t) i].empty()) ? 1 : 0;
    if (collisionsButton.getToggleState() && shown >= 2)
    {
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
            if (second <= collisionFloor)
                continue;
            const auto strength = juce::jlimit (0.0f, 1.0f, (second - collisionFloor) / 46.0f);
            const auto alpha = juce::jlimit (0.0f, 0.95f, 0.12f + 0.6f * strength + 0.1f * (float) std::max (0, count - 2));
            const auto top = yFor (second);
            g.setColour (juce::Colour (0xffff2a3a).withAlpha (alpha));
            g.fillRect (area.getX() + (float) px, top, 1.0f, area.getBottom() - top);
        }
    }

    auto pathFor = [&] (const std::vector<float>& column, bool close)
    {
        juce::Path path;
        path.startNewSubPath (area.getX(), area.getBottom());
        for (int px = 0; px < width; ++px)
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
            g.strokePath (pathFor (columns[(size_t) i], false), juce::PathStrokeType (1.4f));
            continue;
        }
        const auto path = pathFor (columns[(size_t) i], true);
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
        g.setFont (Theme::font (9.0f, true));
        for (int i = 0; i < numSources; ++i)
        {
            const auto& source = sources[(size_t) i];
            if (i == masterSource || columns[(size_t) i].empty() || source.fundamental <= 0.0f)
                continue;
            const auto y = area.getY() + 2.0f + (float) row * lane;
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
                if (n == 2 || n == 3 || n == 4 || n == 6 || n == 8)
                {
                    g.setColour (source.colour.withAlpha (0.8f));
                    g.drawText (juce::String (n), juce::Rectangle<float> (x + 2.0f, y, 12.0f, 9.0f), juce::Justification::centredLeft);
                }
            }
            g.setColour (source.colour);
            g.drawText (source.name + "  " + describe (source.fundamental), juce::Rectangle<float> (xFor (source.fundamental) + 6.0f, y, 170.0f, 10.0f),
                        juce::Justification::centredLeft);
            ++row;
        }
    }

    // Muspekaren: frekvens, ton och nivå för varje visad källa.
    if (hoverX >= area.getX() && hoverX <= area.getRight())
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
        g.setFont (Theme::font (10.0f, true));
        g.drawText ("RED = SEVERAL SOURCES AT THE SAME FREQUENCY", bounds.reduced (10.0f, 6.0f).withHeight ((float) stripHeight), juce::Justification::centredRight);
    }
}
