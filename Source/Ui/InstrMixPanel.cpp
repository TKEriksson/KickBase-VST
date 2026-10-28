#include "InstrMixPanel.h"

#include "HelpDialog.h"

InstrMixPanel::InstrMixPanel (PluginBase& p, int layerIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), accent (accentIn), refs (p.apvts)
{
    using F = InstrumentField;
    const F panA = F::aPan, panB = F::bPan, panC = F::cPan, panSample = F::samplePan, panNoise = F::noisePan, panFm = F::fmPan, widthNoise = F::noiseWidth, subPhase = F::subPhaseDeg, fmPhase = F::fmPhaseDeg;
    const F widthA = F::aWidth, widthB = F::bWidth, widthC = F::cWidth, widthSample = F::sampleWidth, widthFm = F::fmWidth;
    addStrip ("OSC 1", F::aLevel, &panA, &widthA, 0);
    addStrip ("OSC 2", F::bLevel, &panB, &widthB, 0);
    addStrip ("OSC 3", F::cLevel, &panC, &widthC, 0);
    addStrip ("SAMPLE", F::sampleLevel, &panSample, &widthSample, 0);
    addStrip ("NOISE", F::noise, &panNoise, &widthNoise, 0);
    addStrip ("SUB", F::sub, nullptr, nullptr, 0, &subPhase);
    addStrip ("FM", F::fmLevel, &panFm, &widthFm, 0, &fmPhase);
    timingButton.setClickingTogglesState (false);
    timingButton.setTooltip ("Show the timing of each engine instead of its levels: POS DELAY (or SYNC), LENGTH and CURVE. Click again to go back to LEVEL / PAN / WIDTH.");
    timingButton.onClick = [this] { setTiming (! timing); };
    addAndMakeVisible (timingButton);
    for (size_t e = 0; e < strips.size(); ++e)
    {
        auto& strip = *strips[e];
        const auto make = [&] (int which, const char* label)
        {
            auto knob = std::make_unique<KnobComponent> (processor.apvts, ParamID::bass (layer, ParamID::timingField ((int) e, which)), label, accent);
            knob->setVisible (false);
            addChildComponent (*knob);
            return knob;
        };
        strip.delay = make (0, "POS DELAY");
        strip.length = make (2, "LENGTH");
        strip.curve = make (3, "CURVE");
        strip.sync = std::make_unique<ChoiceComponent> (processor.apvts, ParamID::bass (layer, ParamID::timingField ((int) e, 1)), "SYNC");
        strip.sync->setVisible (false);
        addChildComponent (*strip.sync);
        const auto layerIndex = layer;
        auto& apvts = processor.apvts;
        const auto engine = (int) e;
        strip.delay->setWarning ([&apvts, layerIndex, engine]() -> juce::String
        {
            const auto read = [&] (InstrumentField f) { auto* raw = apvts.getRawParameterValue (ParamID::bass (layerIndex, f)); return raw != nullptr ? raw->load() : 0.0f; };
            if ((int) read (ParamID::timingField (engine, 1)) > 0 || read (InstrumentField::sustain) > 0.001f)
                return {}; // SYNC-längden och en hållen ton kan vi inte bedöma här
            const auto noteMs = read (InstrumentField::attack) + read (InstrumentField::decay) + read (InstrumentField::release);
            return read (ParamID::timingField (engine, 0)) > noteMs ? "POS DELAY is longer than the note" : juce::String();
        });
    }
    wideButton.setTooltip ("Give the combined waveform the full width of the panel (hides the knobs). Click again to bring them back.");
    wideButton.onClick = [this] { setWide (! wide); };
    scrollBar.setAutoHide (false);
    scrollBar.addListener (this);
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::mix, "the MIX tab (levels, pan, width, meters, waveform zoom)");
    addAndMakeVisible (wideButton);
    addAndMakeVisible (helpButton);
    addAndMakeVisible (scrollBar);
    startTimerHz (4);
}

void InstrMixPanel::setTiming (bool on)
{
    timing = on;
    timingButton.setButtonText (timing ? "LEVELS" : "TIMING");
    setWide (wide); // sätter om synligheten
}

void InstrMixPanel::setWide (bool on)
{
    wide = on;
    wideButton.setButtonText (wide ? "NARROW" : "WIDE");
    for (auto& strip : strips)
    {
        for (auto* knob : { strip->level.get(), strip->pan.get(), strip->width.get(), strip->phase.get() })
            if (knob != nullptr)
                knob->setVisible (! wide && ! timing);
        for (auto* knob : { strip->delay.get(), strip->length.get(), strip->curve.get() })
            knob->setVisible (! wide && timing);
        strip->sync->setVisible (! wide && timing);
    }
    timingButton.setVisible (! wide);
    resized();
    renderWave();
    repaint();
}

void InstrMixPanel::updateScrollBar()
{
    scrollBar.setRangeLimits (0.0, (double) maxPanSamples + (double) (waveArea.getWidth() - 8) * samplesPerColumn());
    scrollBar.setCurrentRange ((double) panSamples, (double) (waveArea.getWidth() - 8) * samplesPerColumn(), juce::dontSendNotification);
}

void InstrMixPanel::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    panSamples = juce::jlimit (0, maxPanSamples, (int) newRangeStart);
    renderWave();
    repaint (waveArea);
}

void InstrMixPanel::addStrip (const char* name, InstrumentField level, const InstrumentField* pan, const InstrumentField* width, int, const InstrumentField* phase)
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
    if (phase != nullptr)
        strip->phase = make (*phase, "START");
    strips.push_back (std::move (strip));
}

void InstrMixPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    waveArea = wide ? area : area.removeFromLeft (juce::jmin (260, area.getWidth() / 3));
    wideButton.setBounds (waveArea.getRight() - 56, waveArea.getY(), 56, 14);
    helpButton.setBounds (waveArea.getRight() - 56 - 4 - 40, waveArea.getY(), 40, 14);
    timingButton.setBounds (helpButton.getX() - 4 - 56, waveArea.getY(), 56, 14);
    scrollBar.setBounds (waveArea.getX(), waveArea.getBottom() - 28, waveArea.getWidth(), 12);
    if (wide)
        return;
    area.removeFromLeft (8);
    // Två rader: OSC 1-3 + SAMPLE överst, NOISE, SUB och FM under.
    const auto rowHeight = area.getHeight() / 2;
    auto topRow = area.removeFromTop (rowHeight);
    auto bottomRow = area;
    const auto place = [] (Strip& strip, juce::Rectangle<int> cell)
    {
        strip.frame = cell.reduced (2);
        auto inner = strip.frame.withTrimmedTop (14).reduced (2, 0);
        strip.meter = inner.removeFromRight (10).withTrimmedBottom (4);
        if (strip.delay->isVisible() || strip.sync->isVisible())
        {
            // TIMING: POS DELAY, SYNC (valruta), LENGTH, CURVE
            const auto cell = inner.getWidth() / 4;
            strip.delay->setBounds (inner.removeFromLeft (cell));
            strip.sync->setBounds (inner.removeFromLeft (cell).reduced (2, 0).withSizeKeepingCentre (cell - 4, juce::jmin (inner.getHeight(), 44)));
            strip.length->setBounds (inner.removeFromLeft (cell));
            strip.curve->setBounds (inner);
            return;
        }
        const auto count = 1 + (strip.pan != nullptr ? 1 : 0) + (strip.width != nullptr ? 1 : 0) + (strip.phase != nullptr ? 1 : 0);
        const auto knobWidth = inner.getWidth() / count;
        strip.level->setBounds (inner.removeFromLeft (knobWidth));
        if (strip.pan != nullptr)
            strip.pan->setBounds (inner.removeFromLeft (knobWidth));
        if (strip.width != nullptr)
            strip.width->setBounds (inner.removeFromLeft (knobWidth));
        if (strip.phase != nullptr)
            strip.phase->setBounds (inner.removeFromLeft (knobWidth));
    };
    const auto topWidth = topRow.getWidth() / 4;
    for (size_t i = 0; i < 4; ++i)
        place (*strips[i], topRow.removeFromLeft (topWidth));
    // NOISE har tre ratter och får dubbel bredd mot SUB.
    const auto unit = bottomRow.getWidth() / 9;
    place (*strips[4], bottomRow.removeFromLeft (unit * 3));
    place (*strips[5], bottomRow.removeFromLeft (unit * 2));
    place (*strips[6], bottomRow);
}

void InstrMixPanel::renderWave()
{
    // Några cykler vid 110 Hz en stund in i tonen, så att attacken inte styr bilden; panoreringen flyttar startpunkten framåt.
    constexpr double rate = 48000.0;
    constexpr int skip = 4800;
    const auto columns = juce::jmax (1, waveArea.getWidth() - 8);
    const auto length = juce::jmax (64, (int) ((double) columns * samplesPerColumn()));
    updateScrollBar();
    auto params = refs.readInstrument (layer, 36).withTempo (processor.transportStatus.bpm.load(), false);
    params.rootHz = 110.0f;
    params.level = 1.0f;
    synth.prepare (rate);
    synth.setSeed (11);
    synth.setParams (params);
    synth.noteOn (0, 0.8f, (int) rate);
    for (int i = 0; i < skip + panSamples; ++i)
        synth.nextSample();
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

// Varje källa ensam genom hela kedjan (filter, envelope, pan, bredd): toppen av vänster/höger i dB.
void InstrMixPanel::renderMeters()
{
    constexpr double rate = 48000.0;
    const auto base = refs.readInstrument (layer, 36).withTempo (processor.transportStatus.bpm.load(), false);
    for (int source = 0; source < 7; ++source)
    {
        auto params = base;
        params.rootHz = 110.0f;
        params.level = 1.0f;
        params.oscA.on = base.oscA.on && source == 0;
        params.oscB.on = base.oscB.on && source == 1;
        params.oscC.on = base.oscC.on && source == 2;
        params.sampleOn = base.sampleOn && source == 3;
        params.noise = source == 4 ? base.noise : 0.0f;
        params.sub = source == 5 ? base.sub : 0.0f;
        params.fmOn = base.fmOn && source == 6;
        meterSynth.prepare (rate);
        meterSynth.setSeed (11);
        meterSynth.setParams (params);
        meterSynth.noteOn (0, 0.8f, (int) rate);
        for (int i = 0; i < 4800; ++i)
            meterSynth.next();
        auto peak = 0.0f;
        for (int i = 0; i < 2400; ++i)
        {
            const auto o = meterSynth.next();
            peak = std::max ({ peak, std::abs (o.left * o.amplitude), std::abs (o.right * o.amplitude) });
        }
        peakDb[(size_t) source] = peak > 1.0e-5f ? juce::Decibels::gainToDecibels (peak) : -100.0f;
    }
}

void InstrMixPanel::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! waveArea.contains (event.getPosition()) || wheel.deltaY == 0.0f)
        return;
    const auto factor = wheel.deltaY > 0.0f ? 1.25f : 0.8f;
    if (event.mods.isShiftDown())
        zoomY = juce::jlimit (0.25f, 8.0f, zoomY * factor);
    else
    {
        // Zooma runt musen: samplet under pekaren stannar kvar.
        const auto column = (double) (event.x - waveArea.getX() - 4);
        const auto sampleUnderMouse = (double) panSamples + column * samplesPerColumn();
        zoomX = juce::jlimit (0.1f, 32.0f, zoomX * factor);
        panSamples = juce::jlimit (0, maxPanSamples, (int) (sampleUnderMouse - column * samplesPerColumn()));
    }
    renderWave();
    repaint (waveArea);
}

void InstrMixPanel::mouseDown (const juce::MouseEvent& event)
{
    panAtDragStart = panSamples;
    juce::ignoreUnused (event);
}

void InstrMixPanel::mouseDrag (const juce::MouseEvent& event)
{
    if (! event.mods.isMiddleButtonDown() || ! waveArea.contains (event.getMouseDownPosition()))
        return;
    panSamples = juce::jlimit (0, maxPanSamples, panAtDragStart - (int) ((double) event.getDistanceFromDragStartX() * samplesPerColumn()));
    renderWave();
    repaint (waveArea);
}

void InstrMixPanel::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! waveArea.contains (event.getPosition()))
        return;
    zoomX = zoomY = 1.0f;
    panSamples = 0;
    renderWave();
    repaint (waveArea);
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
    renderMeters();
    repaint();
}

// Lodräta hjälplinjer med tid i ms sedan tonens start (48 kHz); avståndet är ett "snällt" steg som ger minst ~50 px mellan linjerna.
void InstrMixPanel::paintTimeAxis (juce::Graphics& g, juce::Rectangle<float> wave) const
{
    constexpr double samplesPerMs = 48.0;
    constexpr int skip = 4800;
    const auto msPerColumn = samplesPerColumn() / samplesPerMs;
    double step = 200.0;
    for (const double candidate : { 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0 })
        if (candidate / msPerColumn >= 50.0)
        {
            step = candidate;
            break;
        }
    const auto startMs = (double) (skip + panSamples) / samplesPerMs;
    const auto endMs = startMs + (double) (wave.getWidth() - 8.0f) * msPerColumn;
    g.setFont (Theme::font (9.0f));
    for (auto ms = std::ceil (startMs / step) * step; ms <= endMs; ms += step)
    {
        const auto x = wave.getX() + 4.0f + (float) ((ms - startMs) / msPerColumn);
        g.setColour (Theme::textDim.withAlpha (0.2f));
        g.drawVerticalLine ((int) x, wave.getY(), wave.getBottom());
        g.setColour (Theme::textDim);
        g.drawText (juce::String (ms, step < 1.0 ? 2 : (step < 10.0 ? 1 : 0)) + " ms", juce::Rectangle<float> (x + 2.0f, wave.getBottom() - 12.0f, 50.0f, 11.0f), juce::Justification::centredLeft);
    }
}

void InstrMixPanel::paint (juce::Graphics& g)
{
    auto wave = waveArea.toFloat();
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true));
    g.drawText ("COMBINED WAVEFORM (110 Hz)", waveArea.withHeight (14), juce::Justification::centredLeft);
    g.drawText ("ZOOM " + juce::String (zoomX, 2) + "x / " + juce::String (zoomY, 2) + "x  (wheel = time, Shift + wheel = height, middle button = pan)", waveArea.withTrimmedTop (waveArea.getHeight() - 14), juce::Justification::centredLeft);
    wave.removeFromTop (16.0f);
    wave.removeFromBottom (28.0f); // rullningslist och tidsaxel
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (wave, 3.0f);
    paintTimeAxis (g, wave);
    const auto mid = wave.getCentreY();
    g.setColour (Theme::textDim.withAlpha (0.4f));
    g.drawHorizontalLine ((int) mid, wave.getX(), wave.getRight());
    g.setColour (accent);
    const auto columns = (int) (peaks.size() / 2);
    for (int x = 0; x < columns; ++x)
    {
        const auto lo = peaks[(size_t) x * 2] * waveGain * zoomY, hi = peaks[(size_t) x * 2 + 1] * waveGain * zoomY;
        const auto px = wave.getX() + 4.0f + (float) x;
        const auto limit = wave.getHeight() * 0.5f - 1.0f;
        g.drawVerticalLine ((int) px, mid - juce::jmin (limit, hi * (wave.getHeight() * 0.45f)), mid - juce::jmax (-limit, lo * (wave.getHeight() * 0.45f)) + 1.0f);
    }

    if (wide)
        return;
    for (const auto& strip : strips)
    {
        g.setColour (Theme::textDim.withAlpha (0.35f));
        g.drawRoundedRectangle (strip->frame.toFloat(), 4.0f, 1.0f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (strip->name + (strip->name == "SUB" ? "  (always mono)" : ""), strip->frame.withHeight (14).reduced (6, 0), juce::Justification::centredLeft);
    }

    // dB-mätare: källan ensam, toppnivå från -60 till 0 dB (rött över -3 dB).
    for (size_t i = 0; i < strips.size() && i < peakDb.size(); ++i)
    {
        const auto meter = strips[i]->meter.toFloat();
        if (meter.isEmpty())
            continue;
        const auto db = peakDb[i];
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (meter, 2.0f);
        const auto filled = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -3.0f ? juce::Colour (0xffe05050) : accent);
        g.fillRect (meter.withTrimmedTop (meter.getHeight() * (1.0f - filled)).reduced (1.0f, 0.0f));
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (9.0f));
        g.drawText (db <= -99.0f ? juce::String ("-inf") : juce::String (juce::roundToInt (db)) + " dB", strips[i]->frame.withHeight (14).reduced (6, 0), juce::Justification::centredRight);
    }
}
