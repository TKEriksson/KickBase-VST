#include "HarmonicEditor.h"

namespace
{
    constexpr float minDb = -48.0f, maxDb = 12.0f, topPad = 22.0f, gap = 14.0f;
}

HarmonicEditor::HarmonicEditor (PluginBase& p, int layerIn, int oscIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), osc (oscIn), accent (accentIn)
{
    available = processor.wavetableSource (layer, osc, baseline, steps, tableName);

    frameBox.addItem ("All frames", 1);
    for (int f = 0; f < Wavetable::numFrames; ++f)
        frameBox.addItem ("Frame " + juce::String (f + 1), f + 2);
    frameBox.setSelectedId (1, juce::dontSendNotification);
    frameBox.onChange = [this] { refreshBackdrop(); changed(); repaint(); };
    frameBox.setTooltip ("Which wave of the table the edit applies to.");
    addAndMakeVisible (frameBox);

    zeroFundamental.setTooltip ("Removes harmonic 1 (the fundamental). Useful for a bass that fights the kick.");
    zeroFundamental.onClick = [this] { gainDb[0] = -120.0f; changed(); repaint(); };
    reset.setTooltip ("Back to the table as it was when this window opened.");
    reset.onClick = [this]
    {
        gainDb.fill (0.0f);
        phaseDeg.fill (0.0f);
        changed();
        repaint();
    };
    addAndMakeVisible (zeroFundamental);
    addAndMakeVisible (reset);

    refreshBackdrop();
    setSize (680, 330);
}

void HarmonicEditor::resized()
{
    auto top = getLocalBounds().reduced (8, 4).removeFromTop (18);
    reset.setBounds (top.removeFromRight (60));
    top.removeFromRight (6);
    zeroFundamental.setBounds (top.removeFromRight (110));
    top.removeFromRight (6);
    frameBox.setBounds (top.removeFromRight (110));
}

juce::Rectangle<float> HarmonicEditor::ampLane() const
{
    auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    area.removeFromTop (topPad + 8.0f);
    area.removeFromBottom (14.0f);
    return area.removeFromTop ((area.getHeight() - gap) * 0.62f);
}

juce::Rectangle<float> HarmonicEditor::phaseLane() const
{
    auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    area.removeFromTop (topPad + 8.0f);
    area.removeFromBottom (14.0f);
    area.removeFromTop ((area.getHeight() - gap) * 0.62f + gap);
    return area;
}

int HarmonicEditor::columnAt (float x) const
{
    const auto lane0 = ampLane();
    return juce::jlimit (0, shown - 1, (int) ((x - lane0.getX()) / (lane0.getWidth() / (float) shown)));
}

void HarmonicEditor::refreshBackdrop()
{
    backdrop.fill (0.0f);
    if (! available)
        return;
    const auto frame = frameBox.getSelectedId() <= 1 ? 0 : frameBox.getSelectedId() - 2;
    std::vector<float> amp, phase;
    WavetableEdit::analyse (baseline.data() + (size_t) frame * Wavetable::frameSize, shown, amp, phase);
    const auto biggest = *std::max_element (amp.begin(), amp.end());
    if (biggest > 1.0e-6f)
        for (int k = 0; k < shown; ++k)
            backdrop[(size_t) k] = amp[(size_t) k] / biggest;
}

void HarmonicEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (available ? "HARMONICS  " + tableName : juce::String ("HARMONICS  (nothing to edit here)"),
                8, 4, 330, 18, juce::Justification::centredLeft);

    const auto amp = ampLane(), phase = phaseLane();
    const auto columnWidth = amp.getWidth() / (float) shown;
    for (const auto& laneArea : { amp, phase })
    {
        g.setColour (Theme::track);
        g.fillRect (laneArea);
        g.setColour (Theme::grid);
        for (int k = 0; k <= shown; k += 8)
            g.drawVerticalLine ((int) (laneArea.getX() + columnWidth * (float) k), laneArea.getY(), laneArea.getBottom());
    }

    auto ampY = [&] (float db) { return amp.getBottom() - amp.getHeight() * juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); };
    const auto zeroY = ampY (0.0f);
    g.setColour (Theme::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) zeroY, amp.getX(), amp.getRight());
    g.drawHorizontalLine ((int) phase.getCentreY(), phase.getX(), phase.getRight());

    for (int k = 0; k < shown; ++k)
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
    for (int k = 0; k < shown; k += 8)
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
    setFromMouse (e.position);
}

void HarmonicEditor::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position); }

void HarmonicEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto k = (size_t) columnAt (e.position.x);
    if (phaseLane().contains (e.position))
        phaseDeg[k] = 0.0f;
    else if (ampLane().contains (e.position))
        gainDb[k] = 0.0f;
    else
        return;
    changed();
    repaint();
}

void HarmonicEditor::setFromMouse (juce::Point<float> position)
{
    if (! available)
        return;
    const auto k = (size_t) columnAt (position.x);
    if (lane == 0)
    {
        const auto area = ampLane();
        const auto db = minDb + (maxDb - minDb) * juce::jlimit (0.0f, 1.0f, (area.getBottom() - position.y) / area.getHeight());
        gainDb[k] = db <= minDb + 1.0f ? -120.0f : (std::abs (db) < 0.6f ? 0.0f : db); // nederst = borttagen, nära noll fastnar vid noll
    }
    else
    {
        const auto area = phaseLane();
        const auto deg = juce::jlimit (-180.0f, 180.0f, (area.getCentreY() - position.y) / (area.getHeight() * 0.5f) * 180.0f);
        phaseDeg[k] = std::abs (deg) < 4.0f ? 0.0f : std::round (deg);
    }
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

    std::vector<WavetableEdit::Edit> edits;
    for (int k = 0; k < shown; ++k)
        if (gainDb[(size_t) k] != 0.0f || phaseDeg[(size_t) k] != 0.0f)
            edits.push_back ({ k + 1, gainDb[(size_t) k] <= -119.0f ? 0.0f : juce::Decibels::decibelsToGain (gainDb[(size_t) k]),
                               juce::degreesToRadians (phaseDeg[(size_t) k]) });

    auto frames = baseline;
    WavetableEdit::apply (frames, frameBox.getSelectedId() <= 1 ? -1 : frameBox.getSelectedId() - 2, edits, true);
    processor.installWavetable (layer, osc, std::move (frames), steps, tableName);
}
