#pragma once

#include "../Core/InstrumentSynth.h"
#include "../Core/PluginBase.h"
#include "Components.h"

// Tidsdiagrammet (l): en bana per motor (OSC 1-3, SAMPLE, NOISE, SUB, FM) på en gemensam tidsaxel och längst ner den gemensamma konturen
// (volym-envelopen). Fasen syns som NUDGE (banornas start flyttas) och POLARITY (konturen vänds). Den valda motorns bana är högre och markerad;
// i redigerbart läge flyttar drag i den banan POS DELAY (vänsterkanten) eller LENGTH (högerkanten), och ett klick på en annan bana väljer den motorn.
// Skrivskyddat (INSTR): inga drag; enkelklick väljer motor (visas störst), dubbelklick meddelar onOpen. Valet är UI-läge, ingen parameter.
class TimelineChart : public juce::Component, private juce::Timer
{
public:
    TimelineChart (PluginBase& p, int layerIn, juce::Colour accentIn, bool editableIn)
        : processor (p), layer (layerIn), accent (accentIn), editable (editableIn), refs (p.apvts)
    {
        refresh();
        startTimerHz (30);
    }

    int selected = 0;                  // vald motor 0-6
    std::function<void (int)> onSelect; // anropas när användaren klickar på en bana
    std::function<void (int)> onOpen;   // dubbelklick på en bana (INSTR: gå till TIMING)

    void setSelected (int engine)
    {
        selected = juce::jlimit (0, ParamID::numTimingEngines - 1, engine);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::chartBackground);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        const auto time = timeArea();
        const auto xOf = [&] (double t) { return time.getX() + (float) juce::jlimit (0.0, 1.0, t / spanSeconds) * time.getWidth(); };

        // Linjal: några jämna tider, resten av ytan får rutnätslinjer.
        const auto tickSeconds = niceTick (spanSeconds / 6.0);
        g.setFont (Theme::font (12.0f));
        for (double t = 0.0; t <= spanSeconds; t += tickSeconds)
        {
            g.setColour (Theme::textDim.withAlpha (0.18f));
            g.drawVerticalLine ((int) xOf (t), (float) time.getY(), (float) time.getBottom());
            g.setColour (Theme::textDim);
            g.drawText (formatTime (t), (int) xOf (t) + 2, getHeight() - rulerHeight, 60, rulerHeight, juce::Justification::centredLeft);
        }

        for (int e = 0; e < ParamID::numTimingEngines; ++e)
            paintLane (g, e, laneBounds (e), xOf);
        paintContour (g, contourBounds(), xOf);

        const auto noteOff = (double) processor.noteOffTime[(size_t) layer].load (std::memory_order_relaxed);
        const auto playTime = (double) processor.notePlayTime[(size_t) layer].load (std::memory_order_relaxed);
        const auto top = (float) time.getY(), bottom = (float) contourBounds().getBottom();
        if (playTime >= 0.0 && playTime <= spanSeconds)
        {
            g.setColour (Theme::text.withAlpha (0.85f));
            g.drawVerticalLine ((int) xOf (playTime), top, bottom);
        }
        if (noteOff >= 0.0 && noteOff <= spanSeconds)
        {
            g.setColour (Theme::fuchsia.withAlpha (0.8f));
            for (float y = top; y < bottom; y += 4.0f)
                g.drawVerticalLine ((int) xOf (noteOff), y, juce::jmin (y + 2.0f, bottom));
        }

        paintPhaseMarks (g, xOf);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto engine = engineAt (e.position);
        if (engine >= 0 && onOpen)
            onOpen (engine);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragEdge = Edge::none;
        const auto engine = engineAt (e.position);
        if (engine < 0)
            return;
        if (engine != selected)
        {
            selected = engine;
            if (onSelect)
                onSelect (engine);
            repaint();
            return;
        }
        if (! editable)
            return;
        // Närmast vänster- eller högerkant avgör vad draget ändrar.
        const auto time = timeArea();
        const auto x = [&] (double t) { return time.getX() + (float) juce::jlimit (0.0, 1.0, t / spanSeconds) * time.getWidth(); };
        const auto delay = delaySeconds (selected), end = delay + lengthSeconds (selected);
        // LENGTH 0 = ingen slutkant (öppen): då ligger båda kanterna på samma x, och ett klick till höger om den drar ut LENGTH.
        const auto openEnded = lengthSeconds (selected) <= 0.0 && e.position.x > x (delay);
        dragEdge = ! openEnded && std::abs (e.position.x - x (delay)) <= std::abs (e.position.x - x (end)) ? Edge::delay : Edge::length;
        const auto field = dragEdge == Edge::delay ? 0 : 2;
        const auto sync = dragEdge == Edge::delay ? params.delaySync[(size_t) selected] : params.lengthSync[(size_t) selected];
        if (sync > 0)
        {
            dragEdge = Edge::none; // SYNC-värden ändras på sina egna reglage
            return;
        }
        dragParam = processor.apvts.getParameter (ParamID::bass (layer, ParamID::timingField (selected, field)));
        if (dragParam != nullptr)
            dragParam->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragEdge == Edge::none || dragParam == nullptr)
            return;
        const auto time = timeArea();
        if (time.getWidth() <= 0)
            return;
        const auto t = juce::jlimit (0.0, spanSeconds, (double) (e.position.x - time.getX()) / (double) time.getWidth() * spanSeconds);
        // delaySeconds() räknar in NUDGE; fältet POS DELAY gör det inte, så NUDGE dras av.
        const auto seconds = dragEdge == Edge::delay ? juce::jmax (0.0, t - nudgeMs / 1000.0) : juce::jmax (0.0, t - delaySeconds (selected));
        dragParam->setValueNotifyingHost (dragParam->convertTo0to1 ((float) (seconds * 1000.0)));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragParam != nullptr)
            dragParam->endChangeGesture();
        dragParam = nullptr;
        dragEdge = Edge::none;
    }

private:
    enum class Edge { none, delay, length };
    static constexpr int rulerHeight = 16, contourHeight = 26, laneGap = 2, headerHeight = 22;
    static constexpr int labelWidth = 58;

    void timerCallback() override
    {
        if (! isShowing())
            return;
        if (tick++ % 8 == 0)
            refresh();
        repaint();
    }

    void refresh()
    {
        bpm = processor.transportStatus.bpm.load();
        if (bpm <= 0.0)
            bpm = 120.0;
        params = refs.readInstrument (layer, 36).withTempo (bpm, false);
        if (auto* raw = processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::nudgeMs)))
            nudgeMs = raw->load();
        double span = (params.attackMs + params.decayMs + params.releaseMs) / 1000.0;
        for (int e = 0; e < ParamID::numTimingEngines; ++e)
            span = juce::jmax (span, delaySeconds (e) + lengthSeconds (e));
        spanSeconds = juce::jlimit (0.5, 8.0, span * 1.15);
    }

    double delaySeconds (int e) const { return juce::jmax (0.0, InstrumentSynth::engineDelaySeconds (params, e, bpm) + nudgeMs / 1000.0); }
    double lengthSeconds (int e) const { return InstrumentSynth::engineLengthSeconds (params, e, bpm); }

    juce::Rectangle<int> timeArea() const { return getLocalBounds().reduced (4, 4).withTrimmedLeft (labelWidth).withTrimmedTop (headerHeight).withTrimmedBottom (rulerHeight); }

    // Rubrikraden: NUDGE, START och POLARITY som små etiketter, och motsvarande markeringar i banorna.
    // NUDGE är högst +-2 ms medan spannet är hundratals ms, så förskjutningen är under en pixel: pilen har därför en minsta längd och ms-värdet i klartext.
    template <typename XOf>
    void paintPhaseMarks (juce::Graphics& g, const XOf& xOf) const
    {
        const auto time = timeArea();
        const auto inverted = params.invertPolarity;
        const auto degrees = (double) params.phaseDeg;

        // START: en period av vågen, förskjuten med startfasen (och vänd av POLARITY), i början av de banor som startfasen påverkar (OSC 1-3 och SUB).
        for (const auto engine : { 0, 1, 2, 5 })
        {
            const auto lane = laneBounds (engine).toFloat();
            const auto amp = juce::jlimit (2.0f, 7.0f, lane.getHeight() * 0.32f);
            const auto width = 26.0f;
            const auto x0 = xOf (delaySeconds (engine)) + 3.0f;
            const auto midY = lane.getCentreY();
            juce::Path wave;
            const auto steps = 26;
            for (int i = 0; i <= steps; ++i)
            {
                const auto phase = degrees / 360.0 + (double) i / (double) steps;
                const auto s = (float) std::sin (juce::MathConstants<double>::twoPi * phase) * (inverted ? -1.0f : 1.0f);
                const auto p = juce::Point<float> (x0 + width * (float) i / (float) steps, midY - amp * s);
                i == 0 ? wave.startNewSubPath (p) : wave.lineTo (p);
            }
            g.setColour (Theme::text.withAlpha (0.9f));
            g.strokePath (wave, juce::PathStrokeType (1.2f));
            const auto startS = (float) std::sin (juce::MathConstants<double>::twoPi * degrees / 360.0) * (inverted ? -1.0f : 1.0f);
            g.setColour (Theme::fuchsia);
            g.fillEllipse (x0 - 2.0f, midY - amp * startS - 2.0f, 4.0f, 4.0f); // startpunkten
        }

        // NUDGE: nollpunkten som streckad linje och en pil åt det håll banorna flyttats.
        const auto top = (float) time.getY(), bottom = (float) contourBounds().getBottom();
        const auto zeroX = xOf (0.0);
        if (std::abs (nudgeMs) >= 0.005f)
        {
            const auto dir = nudgeMs > 0.0f ? 1.0f : -1.0f;
            const auto len = juce::jmax (14.0f, std::abs (xOf (nudgeMs / 1000.0) - zeroX));
            const auto y = top - 4.0f;
            g.setColour (Theme::fuchsia);
            g.drawLine (zeroX, y, zeroX + dir * len, y, 1.6f);
            juce::Path head;
            head.addTriangle (zeroX + dir * len, y, zeroX + dir * (len - 5.0f), y - 3.0f, zeroX + dir * (len - 5.0f), y + 3.0f);
            g.fillPath (head);
            for (float yy = top; yy < bottom; yy += 4.0f)
                g.drawVerticalLine ((int) zeroX, yy, juce::jmin (yy + 1.5f, bottom));
        }

        // Rubrikraden (höger): värdena i klartext.
        auto header = getLocalBounds().reduced (6, 2).withHeight (headerHeight - 4);
        g.setFont (Theme::font (12.0f, true));
        const auto chip = [&] (const juce::String& text, bool active)
        {
            const auto w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 12;
            auto r = header.removeFromRight (w).toFloat().reduced (0.0f, 1.0f);
            header.removeFromRight (4);
            g.setColour (active ? Theme::fuchsia.withAlpha (0.28f) : Theme::insetStrip);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (active ? Theme::text : Theme::textDim);
            g.drawText (text, r.toNearestInt(), juce::Justification::centred);
        };
        chip (juce::String (juce::CharPointer_UTF8 ("POLARITY ")) + (inverted ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92 inverted")) : "+"), inverted);
        chip ("START " + juce::String (degrees, 0) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")), degrees > 0.05);
        chip ("NUDGE " + juce::String (nudgeMs, 2) + " ms", std::abs (nudgeMs) >= 0.005f);
    }

    // Den valda banan får tre gånger höjden.
    juce::Rectangle<int> laneBounds (int engine) const
    {
        auto area = timeArea().withTrimmedBottom (contourHeight + laneGap);
        const auto weights = ParamID::numTimingEngines + 2;
        const auto unit = (float) (area.getHeight() - laneGap * (ParamID::numTimingEngines - 1)) / (float) weights;
        float y = (float) area.getY();
        for (int i = 0; i < engine; ++i)
            y += unit * (i == selected ? 3.0f : 1.0f) + laneGap;
        return { area.getX(), (int) y, area.getWidth(), (int) (unit * (engine == selected ? 3.0f : 1.0f)) };
    }

    juce::Rectangle<int> contourBounds() const { return timeArea().removeFromBottom (contourHeight); }

    int engineAt (juce::Point<float> p) const
    {
        for (int e = 0; e < ParamID::numTimingEngines; ++e)
            if (laneBounds (e).withLeft (4).toFloat().contains (p))
                return e;
        return -1;
    }

    template <typename XOf>
    void paintLane (juce::Graphics& g, int engine, juce::Rectangle<int> lane, const XOf& xOf) const
    {
        static const char* const names[] = { "OSC 1", "OSC 2", "OSC 3", "SAMPLE", "NOISE", "SUB", "FM" };
        const auto isSelected = engine == selected;
        const auto laneF = lane.toFloat();
        g.setColour (isSelected ? accent.withAlpha (0.14f) : Theme::insetStrip);
        g.fillRoundedRectangle (laneF.withLeft (4.0f), 3.0f);
        if (isSelected)
        {
            g.setColour (accent.withAlpha (0.8f));
            g.drawRoundedRectangle (laneF.withLeft (4.0f).reduced (0.5f), 3.0f, 1.2f);
        }
        g.setColour (isSelected ? Theme::text : Theme::textDim);
        g.setFont (Theme::font (isSelected ? 12.5f : 11.5f, isSelected));
        g.drawText (names[engine], 6, lane.getY(), labelWidth - 4, lane.getHeight(), juce::Justification::centredLeft);

        const auto delay = delaySeconds (engine), length = lengthSeconds (engine);
        const auto curve = params.curve[(size_t) engine];
        const auto inner = laneF.reduced (0.0f, 2.0f);
        juce::Path path;
        path.startNewSubPath (inner.getX(), inner.getBottom());
        const auto steps = juce::jmax (2, (int) inner.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = spanSeconds * (double) i / (double) steps;
            path.lineTo (inner.getX() + inner.getWidth() * (float) i / (float) steps, inner.getBottom() - inner.getHeight() * InstrumentSynth::gateAt (t, delay, length, curve));
        }
        path.lineTo (inner.getRight(), inner.getBottom());
        path.closeSubPath();
        g.setColour (accent.withAlpha (isSelected ? 0.34f : 0.2f));
        g.fillPath (path);
        g.setColour (accent.withAlpha (isSelected ? 1.0f : 0.6f));
        g.strokePath (path, juce::PathStrokeType (isSelected ? 1.5f : 1.0f));

        if (isSelected && editable) // grepp i kanterna: det som drag flyttar
        {
            g.setColour (Theme::text.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (xOf (delay) - 1.0f, laneF.getY() + 3.0f, 2.0f, laneF.getHeight() - 6.0f));
            if (length > 0.0)
                g.fillRect (juce::Rectangle<float> (xOf (delay + length) - 1.0f, laneF.getY() + 3.0f, 2.0f, laneF.getHeight() - 6.0f));
        }
    }

    // Volym-envelopen (attack, decay, sustain, release med ton hållen halva spannet). POLARITY - vänder konturen under mittlinjen.
    template <typename XOf>
    void paintContour (juce::Graphics& g, juce::Rectangle<int> area, const XOf& xOf) const
    {
        const auto r = area.toFloat().reduced (0.0f, 2.0f);
        g.setColour (Theme::insetStrip);
        g.fillRoundedRectangle (area.toFloat().withLeft (4.0f), 3.0f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawText ("OUT", 6, area.getY(), labelWidth - 4, area.getHeight(), juce::Justification::centredLeft);

        const auto attack = params.attackMs / 1000.0, decay = params.decayMs / 1000.0, release = params.releaseMs / 1000.0;
        const auto hold = juce::jmax (attack + decay, spanSeconds * 0.5 - release);
        const auto level = [&] (double t)
        {
            if (t < attack)
                return attack > 0.0 ? t / attack : 1.0;
            const auto sustain = (double) params.sustain;
            const auto afterAttack = t - attack;
            const auto held = t <= hold ? sustain + (1.0 - sustain) * std::exp (-afterAttack / juce::jmax (0.001, decay / 3.0)) : 0.0;
            if (t <= hold)
                return held;
            const auto atRelease = sustain + (1.0 - sustain) * std::exp (-(hold - attack) / juce::jmax (0.001, decay / 3.0));
            return t - hold < release ? atRelease * (1.0 - (t - hold) / release) : 0.0;
        };
        const auto base = params.invertPolarity ? r.getY() : r.getBottom();
        const auto direction = params.invertPolarity ? 1.0f : -1.0f;
        juce::Path path;
        path.startNewSubPath (r.getX(), base);
        const auto steps = juce::jmax (2, (int) r.getWidth());
        for (int i = 0; i <= steps; ++i)
            path.lineTo (r.getX() + r.getWidth() * (float) i / (float) steps, base + direction * r.getHeight() * (float) level (spanSeconds * (double) i / (double) steps));
        path.lineTo (r.getRight(), base);
        path.closeSubPath();
        g.setColour (Theme::fuchsia.withAlpha (0.22f));
        g.fillPath (path);
        g.setColour (Theme::fuchsia.withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
        juce::ignoreUnused (xOf);
    }

    static double niceTick (double raw)
    {
        const double steps[] = { 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0 };
        for (const auto s : steps)
            if (s >= raw)
                return s;
        return 2.0;
    }

    static juce::String formatTime (double s) { return s < 1.0 ? juce::String (juce::roundToInt (s * 1000.0)) + " ms" : juce::String (s, 1) + " s"; }

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    const bool editable;
    ParamRefs refs;
    InstrumentParams params;
    double bpm = 120.0, spanSeconds = 1.0;
    float nudgeMs = 0.0f;
    int tick = 0;
    Edge dragEdge = Edge::none;
    juce::RangedAudioParameter* dragParam = nullptr;
};
