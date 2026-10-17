#include "ChannelViews.h"

namespace
{
    constexpr float lowHz = 20.0f, highHz = 20000.0f, rangeDb = 15.0f;
    constexpr float grab = 10.0f;
    const char* roman[] { "HP", "I", "II", "III", "IV", "LP" };
}

EqView::EqView (PluginBase& p) : processor (p)
{
    level.fill (-120.0f);
    hold.fill (-120.0f);
    setTooltip ("EQ - Drag the nodes: left/right = frequency, up/down = boost or cut. HP and LP are high and low pass (drag them sideways). "
                "Scroll on a bell (II, III) to change its width (Q); double-click a node to reset it. "
                "The orange area is the sound reaching the EQ, so you can see which frequencies to fix.");
    startTimerHz (Settings::animationHz (30));
}

void EqView::setChannel (int newChannel)
{
    channel = newChannel;
    level.fill (-120.0f);
    hold.fill (-120.0f);
    repaint();
}

juce::RangedAudioParameter* EqView::freqParam (int node) const
{
    if (node == 0) return processor.apvts.getParameter (Effects::channelParamID (channel, "EqHp"));
    if (node == numNodes - 1) return processor.apvts.getParameter (Effects::channelParamID (channel, "EqLp"));
    return processor.apvts.getParameter (Effects::channelParamID (channel, "Eq" + juce::String (node) + "F"));
}

juce::RangedAudioParameter* EqView::gainParam (int node) const
{
    return node >= 1 && node <= Effects::eqBands
               ? processor.apvts.getParameter (Effects::channelParamID (channel, "Eq" + juce::String (node) + "G")) : nullptr;
}

juce::RangedAudioParameter* EqView::qParam (int node) const
{
    return node == 2 || node == 3
               ? processor.apvts.getParameter (Effects::channelParamID (channel, "Eq" + juce::String (node) + "Q")) : nullptr;
}

void EqView::setValue (juce::RangedAudioParameter* parameter, float value)
{
    if (parameter == nullptr)
        return;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    parameter->endChangeGesture();
}

juce::Rectangle<float> EqView::plot() const { return getLocalBounds().toFloat().reduced (28.0f, 14.0f); }
float EqView::xForHz (float hz) const { return plot().getX() + std::log (hz / lowHz) / std::log (highHz / lowHz) * plot().getWidth(); }
float EqView::hzForX (float x) const { return lowHz * std::pow (highHz / lowHz, juce::jlimit (0.0f, 1.0f, (x - plot().getX()) / plot().getWidth())); }
float EqView::yForDb (float db) const { return plot().getCentreY() - juce::jlimit (-rangeDb, rangeDb, db) / rangeDb * plot().getHeight() * 0.5f; }
float EqView::dbForY (float y) const { return juce::jlimit (-rangeDb, rangeDb, (plot().getCentreY() - y) / (plot().getHeight() * 0.5f) * rangeDb); }

juce::Point<float> EqView::nodePosition (int node, const Effects::EqSettings& s) const
{
    if (node == 0) return { xForHz (std::max (lowHz, s.hp)), yForDb (0.0f) };
    if (node == numNodes - 1) return { xForHz (std::min (highHz, s.lp)), yForDb (0.0f) };
    return { xForHz (s.freq[node - 1]), yForDb (s.gain[node - 1]) };
}

int EqView::nodeAt (juce::Point<float> p) const
{
    const auto s = Effects::readEq (processor.apvts, channel);
    for (int n = 0; n < numNodes; ++n)
        if (nodePosition (n, s).getDistanceFrom (p) < grab)
            return n;
    return -1;
}

juce::String EqView::describe (int node, const Effects::EqSettings& s) const
{
    auto hz = [] (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
    if (node == 0) return "HIGH PASS  " + (s.hp <= 21.0f ? juce::String ("off") : hz (s.hp));
    if (node == numNodes - 1) return "LOW PASS  " + (s.lp >= 19500.0f ? juce::String ("off") : hz (s.lp));
    const auto b = node - 1;
    auto text = "BAND " + juce::String (roman[node]) + "  " + hz (s.freq[b]) + "  " + (s.gain[b] > 0.0f ? "+" : "") + juce::String (s.gain[b], 1) + " dB";
    if (node == 2 || node == 3)
        text << "  Q " << juce::String (s.q[b], 2);
    return text;
}

void EqView::timerCallback()
{
    if (! isShowing())
        return;

    processor.getEffectRack().eqInputTap[channel].copyLatest (size, left, right);
    for (int i = 0; i < size; ++i)
        fftData[(size_t) i] = 0.5f * (left[(size_t) i] + right[(size_t) i]);
    std::fill (fftData.begin() + size, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), size);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    for (int b = 0; b < bins; ++b)
    {
        const auto db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * 4.0f / (float) size, -120.0f);
        auto& v = level[(size_t) b];
        v = db > v ? db : v + (db - v) * 0.25f;
        hold[(size_t) b] = std::max (v, hold[(size_t) b] - 0.4f);
    }
    repaint();
}

void EqView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0f1a));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = plot();
    const auto settings = Effects::readEq (processor.apvts, channel);
    const auto rate = std::max (8000.0, processor.getSampleRate());

    // Rutnät.
    g.setFont (Theme::font (9.0f));
    for (const auto hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xForHz (hz);
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (Theme::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 14.0f, area.getBottom() + 1.0f, 28.0f, 11.0f), juce::Justification::centred);
    }
    for (const auto db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        g.setColour (db == 0.0f ? Theme::textDim.withAlpha (0.5f) : Theme::grid);
        g.drawHorizontalLine ((int) yForDb (db), area.getX(), area.getRight());
        g.setColour (Theme::textDim.withAlpha (0.8f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (area.getX() - 26.0f, yForDb (db) - 6.0f, 22.0f, 12.0f), juce::Justification::right);
    }

    // Spektrum av ljudet som når EQ:n (orange yta, tunn håll-linje).
    auto spectrumPath = [&] (const std::array<float, bins>& data, bool close)
    {
        juce::Path path;
        const auto width = (int) area.getWidth();
        path.startNewSubPath (area.getX(), area.getBottom());
        for (int px = 0; px < width; ++px)
        {
            const auto hz0 = hzForX (area.getX() + (float) px), hz1 = hzForX (area.getX() + (float) px + 1.0f);
            const auto b0 = (int) (hz0 / (rate * 0.5) * bins), b1 = std::max (b0 + 1, (int) (hz1 / (rate * 0.5) * bins));
            auto db = -120.0f;
            for (int b = b0; b < std::min (b1, bins); ++b)
                db = std::max (db, data[(size_t) b]);
            const auto fraction = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 85.0f);
            path.lineTo (area.getX() + (float) px, area.getBottom() - fraction * area.getHeight());
        }
        if (close)
        {
            path.lineTo (area.getRight(), area.getBottom());
            path.closeSubPath();
        }
        return path;
    };
    g.setGradientFill (juce::ColourGradient (Theme::kick.withAlpha (0.55f), 0.0f, area.getY(), Theme::kick.withAlpha (0.05f), 0.0f, area.getBottom(), false));
    g.fillPath (spectrumPath (level, true));
    g.setColour (Theme::kick.withAlpha (0.3f));
    g.strokePath (spectrumPath (hold, false), juce::PathStrokeType (1.0f));

    // Kurvan: summan av alla filter.
    juce::Path curve;
    const auto coefficients = Effects::eqCoefficients (rate, settings);
    const auto width = (int) area.getWidth();
    for (int px = 0; px <= width; px += 2)
    {
        const auto hz = hzForX (area.getX() + (float) px);
        float magnitude = 1.0f;
        for (auto& c : coefficients)
            magnitude *= (float) c->getMagnitudeForFrequency (hz, rate);
        const auto point = juce::Point<float> (area.getX() + (float) px, yForDb (juce::Decibels::gainToDecibels (magnitude, -60.0f)));
        if (px == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }
    g.setColour (Theme::effects.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (juce::Colour (0xffffd98a));
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    // Noderna.
    for (int n = 0; n < numNodes; ++n)
    {
        const auto position = nodePosition (n, settings);
        const auto active = n == hovered || n == dragged;
        const auto isEnd = n == 0 || n == numNodes - 1;
        const auto off = (n == 0 && settings.hp <= 21.0f) || (n == numNodes - 1 && settings.lp >= 19500.0f);
        const auto radius = active ? 9.0f : 8.0f;
        g.setColour ((isEnd ? Theme::textDim : Theme::bass).withAlpha (off ? 0.35f : 0.3f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f + 6.0f, radius * 2.0f + 6.0f).withCentre (position));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6fa8ff), position.x, position.y - radius, juce::Colour (0xff1d4f9e), position.x, position.y + radius, false));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (position));
        g.setColour (juce::Colours::white.withAlpha (off ? 0.4f : 0.9f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (position), 1.2f);
        g.setFont (Theme::font (8.5f, true));
        g.drawText (roman[n], juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (position), juce::Justification::centred);
    }

    const auto shown = dragged >= 0 ? dragged : hovered;
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::text);
    g.drawText (shown >= 0 ? describe (shown, settings) : juce::String ("CHANNEL EQ   (orange = sound reaching the EQ)"),
                bounds.reduced (10.0f, 2.0f).withHeight (14.0f), juce::Justification::centredLeft);
}

void EqView::mouseMove (const juce::MouseEvent& event)
{
    const auto n = nodeAt (event.position);
    if (n != hovered)
    {
        hovered = n;
        repaint();
    }
}

void EqView::mouseDown (const juce::MouseEvent& event) { dragged = nodeAt (event.position); }

void EqView::mouseDrag (const juce::MouseEvent& event)
{
    if (dragged < 0)
        return;

    const auto hz = hzForX (event.position.x);
    if (auto* f = freqParam (dragged))
    {
        // Frekvensen begränsas av parameterns omfång (varje band har sitt eget).
        const auto range = f->getNormalisableRange();
        setValue (f, juce::jlimit (range.start, range.end, hz));
    }
    if (auto* gain = gainParam (dragged))
        setValue (gain, dbForY (event.position.y));
    repaint();
}

void EqView::mouseUp (const juce::MouseEvent&) { dragged = -1; }

void EqView::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto n = nodeAt (event.position);
    if (n < 0)
        return;
    if (n == 0) setValue (freqParam (0), 20.0f);
    else if (n == numNodes - 1) setValue (freqParam (n), 20000.0f);
    else
    {
        setValue (gainParam (n), 0.0f);
        if (auto* q = qParam (n))
            setValue (q, 1.0f);
    }
}

void EqView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto n = nodeAt (event.position);
    if (auto* q = qParam (n))
        setValue (q, q->convertFrom0to1 (q->getValue()) * (wheel.deltaY > 0.0f ? 1.12f : 1.0f / 1.12f));
}

//==============================================================================
CompView::CompView (PluginBase& p, int channel)
    : scope (p, { { &p.getEffectRack().compInputTap[channel], 2, juce::Colours::white.withAlpha (0.6f), false, "IN" },
                  { &p.getEffectRack().compOutputTap[channel], 2, Theme::effects, true, "OUT" } }),
      gainScope (p, { { &p.getEffectRack().compGainTap[channel], 0, Theme::effects, true, "GAIN" } }, ScopeView::Mode::gainReduction)
{
    const std::pair<const char*, const char*> specs[] { { "CompThr", "THRESHOLD" }, { "CompRatio", "RATIO" }, { "CompAtt", "ATTACK" },
                                                        { "CompRel", "RELEASE" }, { "CompMakeup", "MAKEUP" } };
    const char* help[] { "The level above which the compressor starts turning the sound down.",
                         "How strongly it turns down what is above the threshold. 1:1 = no compression (the default).",
                         "How fast it reacts. Slow attack lets the punch through.",
                         "How fast it lets go again.",
                         "Volume added after compression." };
    for (int i = 0; i < 5; ++i)
    {
        knobs.push_back (std::make_unique<KnobComponent> (p.apvts, Effects::channelParamID (channel, specs[i].first), specs[i].second, Theme::effects));
        knobs.back()->setDescription (help[i]);
        addAndMakeVisible (*knobs.back());
    }
    scope.setTooltip ("COMPRESSOR - Live: the sound going in (white outline) and coming out (filled). Where the outline sticks out above "
                      "the fill, the compressor is turning it down.");
    gainScope.gainTitle = "GAIN REDUCTION (dB)";
    gainScope.setTooltip ("GAIN REDUCTION - How much the compressor turns the sound down, over time.");
    addAndMakeVisible (scope);
    addAndMakeVisible (gainScope);
}

void CompView::resized()
{
    auto area = getLocalBounds();
    auto knobArea = area.removeFromLeft (std::min (area.getWidth() / 2, 5 * 92));
    area.removeFromLeft (10);
    const auto width = knobArea.getWidth() / 5;
    for (auto& knob : knobs)
        knob->setBounds (knobArea.removeFromLeft (width).withSizeKeepingCentre (width, std::min (knobArea.getHeight(), 100)));

    scope.setBounds (area.removeFromTop (area.getHeight() * 7 / 10));
    area.removeFromTop (4);
    gainScope.setBounds (area);
}
