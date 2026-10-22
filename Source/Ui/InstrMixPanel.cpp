#include "InstrMixPanel.h"

InstrMixPanel::InstrMixPanel (PluginBase& p, int layerIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), accent (accentIn), refs (p.apvts)
{
    using F = InstrumentField;
    const F panA = F::aPan, panB = F::bPan, panC = F::cPan, panSample = F::samplePan, panNoise = F::noisePan, panFm = F::fmPan, widthNoise = F::noiseWidth;
    addStrip ("OSC 1", F::aLevel, &panA, nullptr, 0);
    addStrip ("OSC 2", F::bLevel, &panB, nullptr, 0);
    addStrip ("OSC 3", F::cLevel, &panC, nullptr, 0);
    addStrip ("SAMPLE", F::sampleLevel, &panSample, nullptr, 0);
    addStrip ("NOISE", F::noise, &panNoise, &widthNoise, 0);
    addStrip ("SUB", F::sub, nullptr, nullptr, 0);
    addStrip ("FM", F::fmLevel, &panFm, nullptr, 0);
    startTimerHz (4);
}

void InstrMixPanel::addStrip (const char* name, InstrumentField level, const InstrumentField* pan, const InstrumentField* width, int)
{
    auto strip = std::make_unique<Strip>();
    strip->name = name;
    auto make = [&] (InstrumentField field, const char* label)
    {
        auto knob = std::make_unique<KnobComponent> (processor.apvts, ParamID::bass (layer, field), label, accent);
        addAndMakeVisible (*knob);
        return knob;
    };
    strip->level = make (level, "LEVEL");
    if (pan != nullptr)
        strip->pan = make (*pan, "PAN");
    if (width != nullptr)
        strip->width = make (*width, "WIDTH");
    strips.push_back (std::move (strip));
}

void InstrMixPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    waveArea = area.removeFromLeft (juce::jmin (260, area.getWidth() / 3));
    area.removeFromLeft (8);
    // Två rader: OSC 1-3 + SAMPLE överst, NOISE, SUB och FM under.
    const auto rowHeight = area.getHeight() / 2;
    auto topRow = area.removeFromTop (rowHeight);
    auto bottomRow = area;
    const auto place = [] (Strip& strip, juce::Rectangle<int> cell)
    {
        strip.frame = cell.reduced (2);
        auto inner = strip.frame.withTrimmedTop (14).reduced (2, 0);
        const auto count = 1 + (strip.pan != nullptr ? 1 : 0) + (strip.width != nullptr ? 1 : 0);
        const auto knobWidth = inner.getWidth() / count;
        strip.level->setBounds (inner.removeFromLeft (knobWidth));
        if (strip.pan != nullptr)
            strip.pan->setBounds (inner.removeFromLeft (knobWidth));
        if (strip.width != nullptr)
            strip.width->setBounds (inner.removeFromLeft (knobWidth));
    };
    const auto topWidth = topRow.getWidth() / 4;
    for (size_t i = 0; i < 4; ++i)
        place (*strips[i], topRow.removeFromLeft (topWidth));
    // NOISE har tre ratter och får dubbel bredd mot SUB.
    const auto unit = bottomRow.getWidth() / 5;
    place (*strips[4], bottomRow.removeFromLeft (unit * 2));
    place (*strips[5], bottomRow.removeFromLeft (unit));
    place (*strips[6], bottomRow);
}

void InstrMixPanel::renderWave()
{
    // Några cykler vid 110 Hz en stund in i tonen, så att attacken inte styr bilden.
    constexpr double rate = 48000.0;
    constexpr int skip = 4800, length = 1309;
    auto params = refs.readInstrument (layer, 36).withTempo (processor.transportStatus.bpm.load(), false);
    params.rootHz = 110.0f;
    params.level = 1.0f;
    synth.prepare (rate);
    synth.setSeed (11);
    synth.setParams (params);
    synth.noteOn (0, 0.8f, (int) rate);
    for (int i = 0; i < skip; ++i)
        synth.nextSample();
    const auto columns = juce::jmax (1, waveArea.getWidth() - 8);
    peaks.assign ((size_t) columns * 2, 0.0f);
    std::vector<float> wave ((size_t) length);
    auto maxAbs = 1.0e-6f;
    for (auto& s : wave)
    {
        s = synth.nextSample();
        maxAbs = std::max (maxAbs, std::abs (s));
    }
    waveGain = 1.0f / maxAbs;
    for (int x = 0; x < columns; ++x)
    {
        const auto from = (size_t) ((double) x * length / columns), to = std::max (from + 1, (size_t) ((double) (x + 1) * length / columns));
        auto lo = 0.0f, hi = 0.0f;
        for (auto i = from; i < std::min (to, wave.size()); ++i)
        {
            lo = std::min (lo, wave[i]);
            hi = std::max (hi, wave[i]);
        }
        peaks[(size_t) x * 2] = lo;
        peaks[(size_t) x * 2 + 1] = hi;
    }
}

void InstrMixPanel::timerCallback()
{
    if (! isShowing())
        return;
    // Bara när något i instrumentet ändrats: nyckeln är alla parametervärden i lagret.
    juce::String key;
    for (int f = 0; f < (int) InstrumentField::count; ++f)
        if (auto* raw = processor.apvts.getRawParameterValue (ParamID::bass (layer, (InstrumentField) f)))
            key << juce::String (raw->load(), 4) << ',';
    key << processor.transportStatus.bpm.load();
    if (key == lastKey && ! peaks.empty())
        return;
    lastKey = key;
    renderWave();
    repaint (waveArea);
}

void InstrMixPanel::paint (juce::Graphics& g)
{
    auto wave = waveArea.toFloat();
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    g.drawText ("COMBINED WAVEFORM (all sources, 110 Hz)", waveArea.withHeight (14), juce::Justification::centredLeft);
    wave.removeFromTop (16.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (wave, 3.0f);
    const auto mid = wave.getCentreY();
    g.setColour (Theme::textDim.withAlpha (0.4f));
    g.drawHorizontalLine ((int) mid, wave.getX(), wave.getRight());
    g.setColour (accent);
    const auto columns = (int) (peaks.size() / 2);
    for (int x = 0; x < columns; ++x)
    {
        const auto lo = peaks[(size_t) x * 2] * waveGain, hi = peaks[(size_t) x * 2 + 1] * waveGain;
        const auto px = wave.getX() + 4.0f + (float) x;
        g.drawVerticalLine ((int) px, mid - hi * (wave.getHeight() * 0.45f), mid - lo * (wave.getHeight() * 0.45f) + 1.0f);
    }

    for (const auto& strip : strips)
    {
        g.setColour (Theme::textDim.withAlpha (0.35f));
        g.drawRoundedRectangle (strip->frame.toFloat(), 4.0f, 1.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (strip->name + (strip->name == "SUB" ? "  (always mono)" : ""), strip->frame.withHeight (14).reduced (6, 0), juce::Justification::centredLeft);
    }
}
