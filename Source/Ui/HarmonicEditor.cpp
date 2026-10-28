#include "HarmonicEditor.h"

namespace
{
    constexpr float minDb = -48.0f, maxDb = 12.0f, topPad = 22.0f, gap = 14.0f, waveHeight = 190.0f;
}

HarmonicEditor::HarmonicEditor (PluginBase& p, int layerIn, int oscIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), osc (oscIn), accent (accentIn)
{
    available = processor.wavetableSource (layer, osc, baseline, steps, tableName);

    frameBox.addItem ("All frames", 1);
    for (int f = 0; f < Wavetable::numFrames; ++f)
        frameBox.addItem ("Frame " + juce::String (f + 1), f + 2);
    frameBox.setSelectedId (1, juce::dontSendNotification);
    frameBox.onChange = [this] { refreshBackdrop(); refreshPreview(); changed(); repaint(); };
    frameBox.setTooltip ("Which wave of the table the edit applies to. The waveform above shows this wave (the first one for All frames).");
    addAndMakeVisible (frameBox);

    brushBox.addItem ("DRAW", draw);
    brushBox.addItem ("LINE", line);
    brushBox.addItem ("SMOOTH", smooth);
    brushBox.addItem ("RANDOM", random);
    brushBox.setSelectedId (draw, juce::dontSendNotification);
    brushBox.setTooltip ("Brush. DRAW: set bars under the mouse. LINE: drag a straight line from where you pressed. SMOOTH: average each bar with its neighbours. RANDOM: random values in a sensible range.");
    addAndMakeVisible (brushBox);

    presetBox.addItem ("PRESET...", 1);
    presetBox.addItem ("SAW", 2);
    presetBox.addItem ("SQUARE", 3);
    presetBox.addItem ("TRIANGLE", 4);
    presetBox.addItem ("ODD", 5);
    presetBox.addItem ("EVEN", 6);
    presetBox.addItem ("BASS WARM", 7);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId()); presetBox.setSelectedId (1, juce::dontSendNotification); };
    presetBox.setTooltip ("Shape the harmonics into a classic spectrum (relative to the loudest harmonic of the table you opened). You can keep editing afterwards.");
    addAndMakeVisible (presetBox);

    zoomBox.addItem ("16 harmonics", 16);
    zoomBox.addItem ("32 harmonics", 32);
    zoomBox.addItem ("64 harmonics", 64);
    zoomBox.setSelectedId (64, juce::dontSendNotification);
    zoomBox.onChange = [this] { setZoom (zoomBox.getSelectedId()); };
    zoomBox.setTooltip ("How many harmonics are shown (bars get wider). Ctrl + mouse wheel changes it too.");
    addAndMakeVisible (zoomBox);

    refreshBackdrop();
    refreshPreview();
    setSize (900, 580);
}

void HarmonicEditor::resized()
{
    auto top = getLocalBounds().reduced (8, 4).removeFromTop (18);
    frameBox.setBounds (top.removeFromRight (120));
    top.removeFromRight (6);
    zoomBox.setBounds (top.removeFromRight (120));
    top.removeFromRight (6);
    brushBox.setBounds (top.removeFromRight (90));
    top.removeFromRight (6);
    presetBox.setBounds (top.removeFromRight (110));
}

// Sätter gainDb/phaseDeg så att spektrumet i vald frame blir det valda (1/k osv.), mätt mot den starkaste övertonen. Fasen följer sin (kx + p).
void HarmonicEditor::applyPreset (int id)
{
    if (! available || id < 2)
        return;
    const auto reference = *std::max_element (baseAmp.begin(), baseAmp.end());
    if (reference < 1.0e-6f)
        return;
    for (int k = 1; k <= shown; ++k)
    {
        const auto odd = (k % 2) == 1;
        float target = 0.0f, targetPhase = 0.0f;
        switch (id)
        {
            case 2: target = 1.0f / (float) k; break;
            case 3: target = odd ? 1.0f / (float) k : 0.0f; break;
            case 4: target = odd ? 1.0f / (float) (k * k) : 0.0f; targetPhase = ((k / 2) % 2) == 1 ? juce::MathConstants<float>::pi : 0.0f; break;
            case 5: target = odd ? 1.0f / std::sqrt ((float) k) : 0.0f; break;
            case 6: target = (k == 1 || ! odd) ? 1.0f / (float) k : 0.0f; break;
            default: target = k <= 4 ? 1.0f / (float) (1 << (k - 1)) : 0.0f; break;
        }
        const auto i = (size_t) k - 1;
        if (target <= 0.0f)
            gainDb[i] = -120.0f;
        else if (baseAmp[i] < reference * 1.0e-4f)
            gainDb[i] = 0.0f; // finns ingen energi att skala upp
        else
        {
            const auto db = juce::Decibels::gainToDecibels (target * reference / baseAmp[i]);
            gainDb[i] = std::abs (db) < 0.6f ? 0.0f : juce::jlimit (minDb + 2.0f, maxDb, db);
        }
        auto delta = juce::radiansToDegrees (targetPhase - basePhase[i]);
        delta = std::fmod (delta + 540.0f, 360.0f) - 180.0f;
        phaseDeg[i] = std::abs (delta) < 4.0f ? 0.0f : std::round (delta);
    }
    refreshPreview();
    changed();
    repaint();
}

void HarmonicEditor::setZoom (int count)
{
    visible = count;
    zoomBox.setSelectedId (count, juce::dontSendNotification);
    lastColumn = -1;
    repaint();
}

void HarmonicEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! e.mods.isCtrlDown() || wheel.deltaY == 0.0f)
        return;
    setZoom (wheel.deltaY > 0.0f ? juce::jmax (16, visible / 2) : juce::jmin (shown, visible * 2));
}

int HarmonicEditor::shownFrame() const
{
    return frameBox.getSelectedId() <= 1 ? 0 : frameBox.getSelectedId() - 2;
}

juce::Rectangle<float> HarmonicEditor::waveArea() const
{
    auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    area.removeFromTop (topPad + 4.0f);
    return area.removeFromTop (waveHeight);
}

juce::Rectangle<float> HarmonicEditor::ampLane() const
{
    auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    area.removeFromTop (topPad + 4.0f + waveHeight + 8.0f);
    area.removeFromBottom (14.0f);
    return area.removeFromTop ((area.getHeight() - gap) * 0.62f);
}

juce::Rectangle<float> HarmonicEditor::phaseLane() const
{
    auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    area.removeFromTop (topPad + 4.0f + waveHeight + 8.0f);
    area.removeFromBottom (14.0f);
    area.removeFromTop ((area.getHeight() - gap) * 0.62f + gap);
    return area;
}

int HarmonicEditor::columnAt (float x) const
{
    const auto lane0 = ampLane();
    return juce::jlimit (0, visible - 1, (int) ((x - lane0.getX()) / (lane0.getWidth() / (float) visible)));
}

void HarmonicEditor::refreshBackdrop()
{
    backdrop.fill (0.0f);
    if (! available)
        return;
    std::vector<float> amp, phase;
    WavetableEdit::analyse (baseline.data() + (size_t) shownFrame() * Wavetable::frameSize, shown, amp, phase);
    std::copy_n (amp.begin(), shown, baseAmp.begin());
    std::copy_n (phase.begin(), shown, basePhase.begin());
    const auto biggest = *std::max_element (amp.begin(), amp.end());
    if (biggest > 1.0e-6f)
        for (int k = 0; k < shown; ++k)
            backdrop[(size_t) k] = amp[(size_t) k] / biggest;
}

std::vector<WavetableEdit::Edit> HarmonicEditor::currentEdits() const
{
    std::vector<WavetableEdit::Edit> edits;
    for (int k = 0; k < shown; ++k)
        if (gainDb[(size_t) k] != 0.0f || phaseDeg[(size_t) k] != 0.0f)
            edits.push_back ({ k + 1, gainDb[(size_t) k] <= -119.0f ? 0.0f : juce::Decibels::decibelsToGain (gainDb[(size_t) k]),
                               juce::degreesToRadians (phaseDeg[(size_t) k]) });
    return edits;
}

// Vågformsvyn: en FFT på en frame, billigt nog att köras vid varje musrörelse (tabellbygget väntar på timern).
void HarmonicEditor::refreshPreview()
{
    if (! available)
        return;
    const auto frame = shownFrame();
    auto frames = baseline;
    WavetableEdit::apply (frames, frame, currentEdits(), true);
    std::copy_n (baseline.begin() + (size_t) frame * Wavetable::frameSize, Wavetable::frameSize, before.begin());
    std::copy_n (frames.begin() + (size_t) frame * Wavetable::frameSize, Wavetable::frameSize, after.begin());
}

void HarmonicEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (available ? "HARMONICS  " + tableName : juce::String ("HARMONICS  (nothing to edit here)"),
                8, 4, 330, 18, juce::Justification::centredLeft);

    // Vågformen: utgångspunkten svag, med ändringen i oscillatorns färg.
    const auto wave = waveArea();
    g.setColour (Theme::track);
    g.fillRect (wave);
    g.setColour (Theme::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) wave.getCentreY(), wave.getX(), wave.getRight());
    if (available)
    {
        auto path = [&] (const std::array<float, Wavetable::frameSize>& data)
        {
            juce::Path p;
            for (int i = 0; i < Wavetable::frameSize; ++i)
            {
                const auto x = wave.getX() + wave.getWidth() * (float) i / (float) (Wavetable::frameSize - 1);
                const auto y = wave.getCentreY() - wave.getHeight() * 0.45f * juce::jlimit (-1.2f, 1.2f, data[(size_t) i]);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };
        g.setColour (Theme::textDim.withAlpha (0.4f));
        g.strokePath (path (before), juce::PathStrokeType (1.0f));
        g.setColour (accent);
        g.strokePath (path (after), juce::PathStrokeType (2.0f));
    }
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f));
    g.drawText ("WAVEFORM  (grey = when this window opened)", wave.toNearestInt().reduced (4, 2), juce::Justification::topLeft);

    const auto amp = ampLane(), phase = phaseLane();
    const auto columnWidth = amp.getWidth() / (float) visible;
    const auto gridStep = visible / 8;
    for (const auto& laneArea : { amp, phase })
    {
        g.setColour (Theme::track);
        g.fillRect (laneArea);
        g.setColour (Theme::grid);
        for (int k = 0; k <= visible; k += gridStep)
            g.drawVerticalLine ((int) (laneArea.getX() + columnWidth * (float) k), laneArea.getY(), laneArea.getBottom());
    }

    auto ampY = [&] (float db) { return amp.getBottom() - amp.getHeight() * juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); };
    const auto zeroY = ampY (0.0f);
    g.setColour (Theme::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) zeroY, amp.getX(), amp.getRight());
    g.drawHorizontalLine ((int) phase.getCentreY(), phase.getX(), phase.getRight());

    for (int k = 0; k < visible; ++k)
    {
        const auto x = amp.getX() + columnWidth * (float) k;
        // Utgångspunktens amplitud (svag), så man ser var energin ligger.
        const auto level = backdrop[(size_t) k];
        if (level > 0.0f)
        {
            const auto h = amp.getHeight() * juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 60.0f) / 60.0f);
            g.setColour (Theme::textDim.withAlpha (0.18f));
            g.fillRect (x + 1.0f, amp.getBottom() - h, columnWidth - 2.0f, h);
        }

        const auto db = gainDb[(size_t) k];
        if (db <= -119.0f)
        {
            g.setColour (Theme::master);
            g.fillRect (x + 1.0f, amp.getBottom() - 3.0f, columnWidth - 2.0f, 3.0f);
        }
        else if (db != 0.0f)
        {
            const auto y = ampY (db);
            g.setColour (accent);
            g.fillRect (x + 2.0f, juce::jmin (y, zeroY), columnWidth - 4.0f, std::max (2.0f, std::abs (zeroY - y)));
        }

        if (phaseDeg[(size_t) k] != 0.0f)
        {
            const auto py = phase.getCentreY() - phase.getHeight() * 0.5f * phaseDeg[(size_t) k] / 180.0f;
            g.setColour (Theme::lfo);
            g.fillRect (x + 2.0f, juce::jmin (py, phase.getCentreY()), columnWidth - 4.0f, std::max (2.0f, std::abs (phase.getCentreY() - py)));
        }
    }

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f));
    g.drawText ("dB  (+12 / 0 / bottom = removed)", amp.toNearestInt().reduced (4, 2), juce::Justification::topLeft);
    g.drawText ("PHASE  (-180 .. +180 degrees)", phase.toNearestInt().reduced (4, 2), juce::Justification::topLeft);
    for (int k = 0; k < visible; k += gridStep)
        g.drawText (juce::String (k + 1), (int) (phase.getX() + columnWidth * (float) k), (int) phase.getBottom() + 1, 24, 12, juce::Justification::centredLeft);
}

void HarmonicEditor::mouseDown (const juce::MouseEvent& e)
{
    if (phaseLane().contains (e.position))
        lane = 1;
    else if (ampLane().contains (e.position))
        lane = 0;
    else
        return;
    lastColumn = -1;
    ++seed;
    gainStart = gainDb;
    phaseStart = phaseDeg;
    anchorColumn = columnAt (e.position.x);
    anchorValue = valueAt (e.position);
    setFromMouse (e.position);
}

void HarmonicEditor::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void HarmonicEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (waveArea().contains (e.position)) // hela ändringen tillbaka
    {
        gainDb.fill (0.0f);
        phaseDeg.fill (0.0f);
    }
    else
    {
        const auto k = (size_t) columnAt (e.position.x);
        if (phaseLane().contains (e.position))
            phaseDeg[k] = 0.0f;
        else if (ampLane().contains (e.position))
            gainDb[k] = 0.0f;
        else
            return;
    }
    refreshPreview();
    changed();
    repaint();
}

// Värdet under musen för den bana som drogs igång: dB eller grader, ännu inte avrundat.
float HarmonicEditor::valueAt (juce::Point<float> position) const
{
    if (lane == 0)
    {
        const auto area = ampLane();
        return minDb + (maxDb - minDb) * juce::jlimit (0.0f, 1.0f, (area.getBottom() - position.y) / area.getHeight());
    }
    const auto area = phaseLane();
    return juce::jlimit (-180.0f, 180.0f, (area.getCentreY() - position.y) / (area.getHeight() * 0.5f) * 180.0f);
}

void HarmonicEditor::setColumn (int column, float value)
{
    if (lane == 0)
        gainDb[(size_t) column] = value <= minDb + 1.0f ? -120.0f : (std::abs (value) < 0.6f ? 0.0f : value); // nederst = borttagen, nära noll fastnar vid noll
    else
        phaseDeg[(size_t) column] = std::abs (value) < 4.0f ? 0.0f : std::round (value);
}

// DRAW sätter värdet, SMOOTH tar medelvärdet av stapeln och dess grannar, RANDOM slumpar (samma frö och kolumn ger samma värde).
void HarmonicEditor::applyBrush (int column, float value)
{
    const auto brush = brushBox.getSelectedId();
    if (brush == smooth)
    {
        auto& values = lane == 0 ? gainDb : phaseDeg;
        auto at = [&] (int k) { return lane == 0 ? juce::jmax (minDb, values[(size_t) juce::jlimit (0, visible - 1, k)])
                                                  : values[(size_t) juce::jlimit (0, visible - 1, k)]; };
        setColumn (column, (at (column - 1) + at (column) + at (column + 1)) / 3.0f);
    }
    else if (brush == random)
    {
        juce::Random r (seed * 7919 + column * 31 + lane);
        setColumn (column, lane == 0 ? -24.0f + 24.0f * r.nextFloat() : -180.0f + 360.0f * r.nextFloat());
    }
    else
        setColumn (column, value);
}

void HarmonicEditor::setFromMouse (juce::Point<float> position)
{
    if (! available)
        return;
    const auto column = columnAt (position.x);
    const auto value = valueAt (position);

    if (brushBox.getSelectedId() == line)
    {
        // Rak linje från där draget började: börja om från banorna som de var då.
        gainDb = gainStart;
        phaseDeg = phaseStart;
        const auto from = juce::jmin (anchorColumn, column), to = juce::jmax (anchorColumn, column);
        for (int c = from; c <= to; ++c)
            setColumn (c, column == anchorColumn ? value
                                                 : anchorValue + (value - anchorValue) * (float) (c - anchorColumn) / (float) (column - anchorColumn));
    }
    else
    {
        // Snabb dragning hoppar över kolumner: gå igenom staplarna mellan förra och den här.
        if (lastColumn >= 0 && std::abs (column - lastColumn) > 1)
        {
            const auto step = column > lastColumn ? 1 : -1;
            for (int c = lastColumn + step; c != column; c += step)
                applyBrush (c, lastValue + (value - lastValue) * (float) (c - lastColumn) / (float) (column - lastColumn));
        }
        applyBrush (column, value);
    }
    lastColumn = column;
    lastValue = value;

    refreshPreview();
    changed();
    repaint();
}

void HarmonicEditor::changed()
{
    startTimer (100); // vänta tills reglaget lugnat sig: att bygga om tabellen är inte gratis
}

void HarmonicEditor::timerCallback()
{
    stopTimer();
    if (! available)
        return;

    auto frames = baseline;
    WavetableEdit::apply (frames, frameBox.getSelectedId() <= 1 ? -1 : frameBox.getSelectedId() - 2, currentEdits(), true);
    processor.installWavetable (layer, osc, std::move (frames), steps, tableName);
}
