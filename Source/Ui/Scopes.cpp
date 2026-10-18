#include "Scopes.h"

namespace
{
    float mixSample (const std::vector<float>& l, const std::vector<float>& r, int channel, size_t i)
    {
        return channel == 0 ? l[i] : channel == 1 ? r[i] : 0.5f * (l[i] + r[i]);
    }
}

//==============================================================================
ScopeView::ScopeView (PluginBase& p, std::vector<Trace> tracesIn, Mode modeIn)
    : processor (p), traces (std::move (tracesIn)), mode (modeIn)
{
    data.resize (traces.size());
    startTimerHz (Settings::animationHz (30));
}

double ScopeView::windowSeconds() const
{
    // Fyra slag vid DAW:ens tempo (halv rullhastighet, så att man hinner se), men högst så mycket som uttaget rymmer.
    const auto seconds = 4.0 * 60.0 / std::max (40.0, processor.transportStatus.bpm.load());
    const auto rate = std::max (8000.0, processor.getSampleRate());
    return std::min (seconds, (double) SignalTap::capacity / rate * 0.95);
}

void ScopeView::timerCallback()
{
    if (! isShowing())
        return;

    windowSamples = (int) (windowSeconds() * std::max (8000.0, processor.getSampleRate()));
    std::vector<float> l, r;
    for (size_t t = 0; t < traces.size(); ++t)
    {
        traces[t].tap->copyLatest (windowSamples, l, r);
        data[t].resize (l.size());
        for (size_t i = 0; i < l.size(); ++i)
            data[t][i] = mode == Mode::gainReduction ? l[i] : mixSample (l, r, traces[t].channel, i);
    }
    repaint();
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = bounds.reduced (6.0f, 8.0f);

    // Ett lodrätt streck per slag.
    const auto beats = 4;
    for (int beat = 1; beat < beats; ++beat)
    {
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) beat / beats), area.getY(), area.getBottom());
    }

    if (mode == Mode::gainReduction)
    {
        // Förstärkningsminskning i dB: 0 överst, -12 nederst.
        constexpr float range = 12.0f;
        for (const auto db : { 3.0f, 6.0f, 9.0f })
        {
            g.setColour (Theme::grid);
            g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * db / range), area.getX(), area.getRight());
        }
        for (size_t t = 0; t < traces.size(); ++t)
        {
            const auto& samples = data[t];
            if (samples.empty())
                continue;
            juce::Path path;
            const auto n = (int) samples.size();
            const auto width = (int) area.getWidth();
            path.startNewSubPath (area.getX(), area.getY());
            for (int px = 0; px < width; ++px)
            {
                const auto from = px * n / width, to = std::max (from + 1, (px + 1) * n / width);
                auto lowest = 1.0f;
                for (int i = from; i < std::min (to, n); ++i)
                    lowest = std::min (lowest, samples[(size_t) i]);
                const auto db = juce::jlimit (-range, 0.0f, juce::Decibels::gainToDecibels (lowest, -range));
                path.lineTo (area.getX() + (float) px, area.getY() - db / range * area.getHeight());
            }
            path.lineTo (area.getRight(), area.getY());
            path.closeSubPath();
            g.setColour (traces[t].colour.withAlpha (0.45f));
            g.fillPath (path);
            g.setColour (traces[t].colour);
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
        g.setFont (Theme::font (9.5f, true));
        g.setColour (Theme::textDim);
        g.drawText (gainTitle, bounds.reduced (8.0f, 3.0f), juce::Justification::topLeft);
        return;
    }

    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    if (guideLevel)
    {
        const auto level = juce::jlimit (0.0f, 1.0f, guideLevel());
        for (const auto sign : { -1.0f, 1.0f })
        {
            g.setColour (Theme::master.withAlpha (0.5f));
            const auto y = area.getCentreY() - sign * level * area.getHeight() * 0.5f * 0.92f;
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        }
    }

    const auto half = area.getHeight() * 0.5f * 0.92f;
    for (size_t t = 0; t < traces.size(); ++t)
    {
        const auto& samples = data[t];
        if (samples.empty() || (traces[t].instrument >= 0 && traces[t].instrument >= processor.activeInstruments()))
            continue;

        const auto n = (int) samples.size();
        const auto width = (int) area.getWidth();

        if (traces[t].edge)
        {
            // Ducking: från över- och underkanten inåt, så långt som ljudet tas bort (som maximizerns GR).
            juce::Path top, bottom;
            top.startNewSubPath (area.getX(), area.getY());
            bottom.startNewSubPath (area.getX(), area.getBottom());
            for (int px = 0; px < width; ++px)
            {
                const auto from = px * n / width, to = std::max (from + 1, (px + 1) * n / width);
                auto lowest = 1.0f;
                for (int i = from; i < std::min (to, n); ++i)
                    lowest = std::min (lowest, samples[(size_t) i]);
                const auto reach = (1.0f - juce::jlimit (0.0f, 1.0f, lowest)) * area.getHeight() * 0.5f;
                top.lineTo (area.getX() + (float) px, area.getY() + reach);
                bottom.lineTo (area.getX() + (float) px, area.getBottom() - reach);
            }
            top.lineTo (area.getRight(), area.getY());
            top.closeSubPath();
            bottom.lineTo (area.getRight(), area.getBottom());
            bottom.closeSubPath();
            g.setColour (traces[t].colour.withAlpha (0.16f));
            g.fillPath (top);
            g.fillPath (bottom);
            g.setColour (traces[t].colour.withAlpha (0.95f));
            g.strokePath (top, juce::PathStrokeType (1.6f));
            g.strokePath (bottom, juce::PathStrokeType (1.6f));
            continue;
        }

        juce::Path path;
        std::vector<juce::Point<float>> lower;
        for (int px = 0; px < width; ++px)
        {
            const auto from = px * n / width, to = std::max (from + 1, (px + 1) * n / width);
            auto lo = 0.0f, hi = 0.0f;
            for (int i = from; i < std::min (to, n); ++i)
            {
                lo = std::min (lo, samples[(size_t) i]);
                hi = std::max (hi, samples[(size_t) i]);
            }
            const auto x = area.getX() + (float) px;
            const juce::Point<float> top { x, area.getCentreY() - juce::jlimit (-1.0f, 1.0f, hi) * half };
            if (px == 0)
                path.startNewSubPath (top);
            else
                path.lineTo (top);
            lower.push_back ({ x, area.getCentreY() - juce::jlimit (-1.0f, 1.0f, lo) * half });
        }
        for (auto it = lower.rbegin(); it != lower.rend(); ++it)
            path.lineTo (*it);
        path.closeSubPath();

        if (traces[t].filled)
        {
            g.setColour (traces[t].colour.withAlpha (0.55f));
            g.fillPath (path);
        }
        g.setColour (traces[t].colour.withAlpha (traces[t].filled ? 0.9f : 0.7f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

    // Namn på vågformerna.
    const auto legendRow = bounds.reduced (8.0f, 3.0f).withHeight (12.0f);
    auto legend = legendRow;
    g.setFont (Theme::font (9.5f, true));
    for (const auto& trace : traces)
    {
        if (trace.instrument >= 0 && trace.instrument >= processor.activeInstruments())
            continue;
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (Theme::font (9.5f, true), trace.name) + 14.0f;
        if (legend.getWidth() < width && legend.getX() > legendRow.getX())
            legend = legendRow.withY (legend.getBottom() + 1.0f); // får inte plats: ny rad (med många instrument blir det flera)
        g.setColour (trace.colour);
        g.fillRoundedRectangle (legend.getX(), legend.getCentreY() - 3.0f, 6.0f, 6.0f, 2.0f);
        g.drawText (trace.name, legend.withTrimmedLeft (10.0f).withWidth (width), juce::Justification::centredLeft);
        legend.removeFromLeft (width + 8.0f);
    }
}

//==============================================================================
SpectrumView::SpectrumView (PluginBase& p) : processor (p)
{
    const auto& master = processor.getMasterSection();
    bands.push_back ({ &processor.kickTap, 2, Theme::kick, "KICK", {} });
    bands.push_back ({ &processor.instrumentAfterDuckTap, 2, Theme::bass, "INSTRUMENTS", {} });
    bands.push_back ({ &master.outputTap, 2, Theme::text, "MASTER", {} });
    for (auto& band : bands)
        band.level.fill (-120.0f);
    peakHold.fill (-120.0f);

    setTooltip ("SPECTRUM - Which frequencies play and how loud, live: the kick, the instruments and the master output. "
                "The thin line is the master's peak hold.");
    startTimerHz (Settings::animationHz (30));
}

void SpectrumView::analyse (Band& band)
{
    band.tap->copyLatest (size, left, right);
    for (int i = 0; i < size; ++i)
        fftData[(size_t) i] = band.channel == 2 ? 0.5f * (left[(size_t) i] + right[(size_t) i]) : left[(size_t) i];
    std::fill (fftData.begin() + size, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), size);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // Normaliserat så att en sinus i full skala blir 0 dB.
    constexpr float norm = 4.0f / (float) size;
    for (int b = 0; b < bins; ++b)
    {
        const auto db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * norm, -120.0f);
        auto& level = band.level[(size_t) b];
        level = db > level ? db : level + (db - level) * 0.25f; // snabbt upp, mjukt ner
    }
}

void SpectrumView::timerCallback()
{
    if (! isShowing())
        return;

    for (auto& band : bands)
        analyse (band);

    const auto& master = bands.back();
    for (int b = 0; b < bins; ++b)
        peakHold[(size_t) b] = std::max (master.level[(size_t) b], peakHold[(size_t) b] - 0.4f);
    repaint();
}

float SpectrumView::xFor (float hz, juce::Rectangle<float> area) const
{
    constexpr float low = 20.0f, high = 20000.0f;
    return area.getX() + std::log (hz / low) / std::log (high / low) * area.getWidth();
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = bounds.reduced (8.0f, 10.0f).withTrimmedBottom (10.0f);
    constexpr float dbTop = 0.0f, dbBottom = -90.0f;
    auto yFor = [&] (float db) { return area.getY() + juce::jlimit (0.0f, 1.0f, (dbTop - db) / (dbTop - dbBottom)) * area.getHeight(); };

    g.setFont (Theme::font (9.0f));
    for (const auto hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xFor (hz, area);
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (Theme::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 14.0f, area.getBottom() + 1.0f, 28.0f, 10.0f), juce::Justification::centred);
    }
    for (const auto db : { -20.0f, -40.0f, -60.0f })
    {
        g.setColour (Theme::grid);
        g.drawHorizontalLine ((int) yFor (db), area.getX(), area.getRight());
        g.setColour (Theme::textDim.withAlpha (0.7f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (area.getX() + 2.0f, yFor (db) - 10.0f, 24.0f, 10.0f), juce::Justification::left);
    }

    const auto rate = std::max (8000.0, processor.getSampleRate());
    auto pathFor = [&] (const std::array<float, bins>& level, bool close)
    {
        juce::Path path;
        const auto width = (int) area.getWidth();
        path.startNewSubPath (area.getX(), area.getBottom());
        for (int px = 0; px < width; ++px)
        {
            // Frekvensen vid bildpunkten, och den starkaste bin som hamnar i den.
            const auto hz0 = 20.0f * std::pow (1000.0f, (float) px / (float) width);
            const auto hz1 = 20.0f * std::pow (1000.0f, (float) (px + 1) / (float) width);
            const auto b0 = (int) (hz0 / (rate * 0.5) * bins), b1 = std::max (b0 + 1, (int) (hz1 / (rate * 0.5) * bins));
            auto db = -120.0f;
            for (int b = b0; b < std::min (b1, bins); ++b)
                db = std::max (db, level[(size_t) b]);
            path.lineTo (area.getX() + (float) px, yFor (db));
        }
        if (close)
        {
            path.lineTo (area.getRight(), area.getBottom());
            path.closeSubPath();
        }
        return path;
    };

    for (size_t i = 0; i + 1 < bands.size(); ++i)
    {
        const auto path = pathFor (bands[i].level, true);
        g.setColour (bands[i].colour.withAlpha (0.35f));
        g.fillPath (path);
        g.setColour (bands[i].colour);
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }
    g.setColour (Theme::text.withAlpha (0.9f));
    g.strokePath (pathFor (bands.back().level, false), juce::PathStrokeType (1.6f));
    g.setColour (Theme::text.withAlpha (0.35f));
    g.strokePath (pathFor (peakHold, false), juce::PathStrokeType (1.0f));

    auto legend = bounds.reduced (8.0f, 3.0f).withHeight (12.0f).withTrimmedLeft (30.0f);
    g.setFont (Theme::font (9.5f, true));
    for (const auto& band : bands)
    {
        g.setColour (band.colour);
        g.fillRoundedRectangle (legend.getX(), legend.getCentreY() - 3.0f, 6.0f, 6.0f, 2.0f);
        const auto width = juce::jmax (54.0f, (float) juce::GlyphArrangement::getStringWidthInt (Theme::font (9.5f, true), band.name) + 4.0f); // INSTRUMENTS fick inte plats på 60 px
        g.drawText (band.name, legend.withTrimmedLeft (10.0f).withWidth (width), juce::Justification::centredLeft);
        legend.removeFromLeft (width + 14.0f);
    }
}

//==============================================================================
StereoView::StereoView (PluginBase& p) : processor (p)
{
    setTooltip ("STEREO - The stereo image of the output. A vertical line is mono, a wide ball is wide stereo, horizontal means "
                "out of phase (bad in mono). The bar shows correlation: +1 = mono, 0 = wide, -1 = out of phase.");
    startTimerHz (Settings::animationHz (30));
}

void StereoView::timerCallback()
{
    if (! isShowing())
        return;

    processor.getMasterSection().outputTap.copyLatest (2048, left, right);

    double ll = 0.0, rr = 0.0, lr = 0.0, mm = 0.0, ss = 0.0;
    for (size_t i = 0; i < left.size(); ++i)
    {
        const auto l = left[i], r = right[i];
        ll += l * l;
        rr += r * r;
        lr += l * r;
        mm += 0.25 * (l + r) * (l + r);
        ss += 0.25 * (l - r) * (l - r);
    }
    const auto target = (ll > 1.0e-9 && rr > 1.0e-9) ? (float) (lr / std::sqrt (ll * rr)) : correlation;
    correlation += (target - correlation) * 0.2f;
    widthShare = (mm + ss > 1.0e-9) ? (float) (std::sqrt (ss) / (std::sqrt (mm) + std::sqrt (ss))) : widthShare;
    repaint();
}

void StereoView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    auto area = bounds.reduced (8.0f);
    auto meter = area.removeFromBottom (22.0f);
    area.removeFromBottom (4.0f);

    const auto side = std::min (area.getWidth(), area.getHeight());
    const auto square = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
    const auto centre = square.getCentre();
    const auto radius = side * 0.5f;

    g.setColour (Theme::grid);
    g.drawEllipse (square, 1.0f);
    g.drawLine (centre.x, square.getY(), centre.x, square.getBottom(), 1.0f);
    g.drawLine (square.getX(), centre.y, square.getRight(), centre.y, 1.0f);
    g.setColour (Theme::grid.withAlpha (0.6f));
    g.drawLine (square.getX(), square.getBottom(), square.getRight(), square.getY(), 1.0f);
    g.drawLine (square.getX(), square.getY(), square.getRight(), square.getBottom(), 1.0f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (9.0f, true));
    g.drawText ("L", juce::Rectangle<float> (square.getX() - 2.0f, square.getY() - 2.0f, 12.0f, 10.0f), juce::Justification::left);
    g.drawText ("R", juce::Rectangle<float> (square.getRight() - 10.0f, square.getY() - 2.0f, 12.0f, 10.0f), juce::Justification::right);

    // Punkterna: mitten uppåt, sidan åt sidorna; nyare punkter är ljusare.
    const auto n = (int) left.size();
    for (int i = 0; i < n; ++i)
    {
        const auto l = left[(size_t) i], r = right[(size_t) i];
        const auto x = centre.x + (l - r) * 0.7071f * radius;
        const auto y = centre.y - (l + r) * 0.7071f * radius;
        g.setColour (Theme::bass.withAlpha (0.08f + 0.5f * (float) i / (float) n));
        g.fillRect (x - 0.8f, y - 0.8f, 1.6f, 1.6f);
    }

    // Korrelation: -1 till +1 med en markör.
    g.setColour (Theme::panelBorder);
    g.fillRoundedRectangle (meter.withTrimmedBottom (10.0f), 3.0f);
    const auto track = meter.withTrimmedBottom (10.0f);
    const auto x = track.getX() + (correlation + 1.0f) * 0.5f * track.getWidth();
    g.setColour (correlation < 0.0f ? Theme::master : Theme::mix);
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, track.getHeight()).withCentre ({ x, track.getCentreY() }), 2.0f);
    g.setFont (Theme::font (8.5f));
    g.setColour (Theme::textDim);
    g.drawText ("-1", meter.withHeight (10.0f).withY (track.getBottom()), juce::Justification::left);
    g.drawText ("CORRELATION   WIDTH " + juce::String (juce::roundToInt (widthShare * 100.0f)) + " %", meter.withHeight (10.0f).withY (track.getBottom()), juce::Justification::centred);
    g.drawText ("+1", meter.withHeight (10.0f).withY (track.getBottom()), juce::Justification::right);
}
