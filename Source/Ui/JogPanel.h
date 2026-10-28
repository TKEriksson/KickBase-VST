#pragma once

#include "Modules.h"

// JOG / LP: ett skivspelarhjul som bara styr pluginets egna klocka (förhandsspelning, free run), plus LOOP och testtempo.
// Värdens uppspelning påverkas aldrig. Inga parametrar: allt här är körtidsläge (loopen sparas inte).
namespace JogConfig
{
    constexpr int panelWidth = 380, panelHeight = 262;
    constexpr int minBpm = 60, maxBpm = 220, defaultBpm = 140;
    constexpr int defaultStepIndex = 2; // 1/16
    constexpr int stepTicks[] = { 16, 8, 4, 2, 1 }; // 1/4, 1/8, 1/16, 1/32, 1/64 (1 tick = 1/64 takt)
    constexpr const char* stepNames[] = { "1/4", "1/8", "1/16", "1/32", "1/64" };
    constexpr float revolutionsPerSecondAtNormalTempo = 0.75f; // ett varv på 1,33 s = normalt tempo
    constexpr float maxRate = 4.0f;
}

// bar.beat.sixteenth.tick (1-räknat utom tick), 1 tick = 1/64 takt = 1/16 fjärdedel.
namespace JogTicks
{
    inline juce::String format (juce::int64 t)
    {
        t = std::max ((juce::int64) 0, t);
        return juce::String (t / TransportClock::ticksPerBar + 1) + "." + juce::String ((t % TransportClock::ticksPerBar) / TransportClock::ticksPerBeat + 1)
             + "." + juce::String ((t % TransportClock::ticksPerBeat) / TransportClock::ticksPerSixteenth + 1) + "." + juce::String (t % TransportClock::ticksPerSixteenth);
    }

    // Delar som saknas blir 1/1/1/0. Delar utanför sina gränser klipps (beat 1-4, sixteenth 1-4, tick 0-3).
    inline juce::int64 parse (const juce::String& text)
    {
        const auto parts = juce::StringArray::fromTokens (text, ".", "");
        const auto part = [&] (int i, int fallback, int lo, int hi)
        {
            return i < parts.size() && parts[i].containsAnyOf ("0123456789") ? juce::jlimit (lo, hi, parts[i].getIntValue()) : fallback;
        };
        const auto bar = (juce::int64) std::max (1, part (0, 1, 1, 100000));
        return (bar - 1) * TransportClock::ticksPerBar + (part (1, 1, 1, 4) - 1) * TransportClock::ticksPerBeat
             + (part (2, 1, 1, 4) - 1) * TransportClock::ticksPerSixteenth + part (3, 0, 0, TransportClock::ticksPerSixteenth - 1);
    }
}

// Glasdisplay där man skriver en tid: klick eller Enter börjar redigera, siffror och punkt skrivs, Enter bekräftar, Esc avbryter.
// Upp/ned flyttar ett STEP.
class TickField : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onCommit;   // ny tid i ticks
    std::function<int()> stepTicks;       // aktuellt STEP i ticks

    TickField() { setWantsKeyboardFocus (true); setMouseCursor (juce::MouseCursor::IBeamCursor); }

    void setTicks (int t) { ticks = t; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        Theme::paintGlassDisplay (g, area);
        const auto text = editing ? buffer + "_" : JogTicks::format (ticks);
        Theme::drawGlowText (g, text, area.reduced (6.0f, 0.0f), Theme::monoFont (14.0f, true), 0.14f, juce::Justification::centred,
                             editing ? Theme::text : Theme::pattern);
        if (hasKeyboardFocus (false) && ! editing)
        {
            g.setColour (Theme::pattern.withAlpha (0.5f));
            g.drawRoundedRectangle (area.reduced (1.0f), 6.0f, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); beginEdit(); }
    void focusLost (FocusChangeType) override { cancelEdit(); }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)
        {
            if (editing)
                commit();
            else
                beginEdit();
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            if (! editing)
                return false;
            cancelEdit();
            return true;
        }
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            if (editing)
                commit();
            const auto step = stepTicks ? stepTicks() : 1;
            if (onCommit)
                onCommit (std::max (0, ticks + (key == juce::KeyPress::upKey ? step : -step)));
            return true;
        }
        if (key == juce::KeyPress::backspaceKey)
        {
            if (! editing)
                return false;
            buffer = buffer.dropLastCharacters (1);
            repaint();
            return true;
        }
        const auto c = key.getTextCharacter();
        if (juce::CharacterFunctions::isDigit (c) || c == '.')
        {
            if (! editing)
                beginEdit();
            if (buffer.length() < 14)
                buffer += juce::String::charToString (c);
            repaint();
            return true;
        }
        return false;
    }

private:
    void beginEdit() { editing = true; buffer.clear(); repaint(); }
    void cancelEdit() { if (editing) { editing = false; repaint(); } }

    void commit()
    {
        editing = false;
        if (onCommit && buffer.isNotEmpty())
            onCommit ((int) std::min ((juce::int64) 1000000, JogTicks::parse (buffer)));
        repaint();
    }

    int ticks = 0;
    bool editing = false;
    juce::String buffer;
};

class JogPanel : public juce::Component, private juce::Timer
{
public:
    explicit JogPanel (PluginBase& p) : processor (p)
    {
        setSize (JogConfig::panelWidth, JogConfig::panelHeight);

        // LOOP (panelen byggs om vid varje öppning, så läget läses från processorn).
        loopButton.setClickingTogglesState (true);
        loopButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        loopButton.setToggleState (processor.globalLoop.load(), juce::dontSendNotification);
        loopButton.setTooltip ("LOOP - The plugin loops the part between FROM and TO on its own clock and ignores the DAW's position. "
                               "A plugin cannot stop the DAW, so stop it yourself in the DAW.");
        loopButton.onClick = [this]
        {
            const auto on = loopButton.getToggleState();
            processor.globalLoop = on;
            if (on)
                processor.seekBarRequest = processor.globalLoopStartTick.load() / TransportClock::ticksPerBar;
        };
        addAndMakeVisible (loopButton);

        fromField.setTooltip ("LOOP start as bar.beat.sixteenth.tick (1 tick = 1/64 bar). Click or press Enter, type, Enter to accept, Esc to cancel. Up/Down move by STEP.");
        toField.setTooltip ("LOOP end as bar.beat.sixteenth.tick, the first position that is NOT played. Click or press Enter, type, Enter to accept, Esc to cancel. Up/Down move by STEP.");
        for (auto* f : { &fromField, &toField })
        {
            f->stepTicks = [this] { return processor.globalLoopStepTicks.load(); };
            addAndMakeVisible (*f);
        }
        fromField.onCommit = [this] (int t) { setLoopTicks (t, processor.globalLoopEndTick.load()); };
        toField.onCommit = [this] (int t) { setLoopTicks (processor.globalLoopStartTick.load(), t); };
        showLoopTicks();

        for (int i = 0; i < (int) std::size (JogConfig::stepNames); ++i)
            stepBox.addItem (JogConfig::stepNames[i], i + 1);
        stepBox.setSelectedItemIndex (stepIndexOf (processor.globalLoopStepTicks.load()), juce::dontSendNotification);
        stepBox.setTooltip ("STEP - How far Up/Down moves FROM and TO: a quarter note down to 1/64 of a bar.");
        stepBox.onChange = [this] { processor.globalLoopStepTicks = JogConfig::stepTicks[std::max (0, stepBox.getSelectedItemIndex())]; };
        addAndMakeVisible (stepBox);

        // TEST BPM: eget tempo för den egna klockan (gäller bara när värden inte spelar).
        const auto currentBpm = processor.freeRunBpm.load();
        testBpmButton.setToggleState (currentBpm > 0.0, juce::dontSendNotification);
        testBpmButton.setTooltip ("TEST BPM - Run the plugin's own clock at the tempo in the box, for testing without the DAW playing. "
                                  "When the DAW plays, its tempo is used.");
        testBpmButton.onClick = [this] { applyTestBpm(); };
        addAndMakeVisible (testBpmButton);

        bpmField.setText (juce::String (currentBpm > 0.0 ? juce::jlimit (JogConfig::minBpm, JogConfig::maxBpm, juce::roundToInt (currentBpm)) : JogConfig::defaultBpm),
                          juce::dontSendNotification);
        bpmField.setFont (Theme::font (13.0f, true));
        bpmField.setJustificationType (juce::Justification::centred);
        bpmField.setColour (juce::Label::textColourId, Theme::text);
        bpmField.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0b0d18));
        bpmField.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0b0d18));
        bpmField.setColour (juce::Label::textWhenEditingColourId, Theme::text);
        bpmField.setColour (juce::Label::outlineColourId, Theme::text.withAlpha (0.25f));
        bpmField.setEditable (false, true, false);
        bpmField.setTooltip ("TEST BPM tempo, " + juce::String (JogConfig::minBpm) + "-" + juce::String (JogConfig::maxBpm) + " (double-click to type).");
        bpmField.onTextChange = [this]
        {
            const auto value = juce::jlimit (JogConfig::minBpm, JogConfig::maxBpm, bpmField.getText().retainCharacters ("0123456789").getIntValue());
            bpmField.setText (juce::String (value), juce::dontSendNotification);
            testBpmKnob.setValue (value, juce::dontSendNotification);
            applyTestBpm();
        };
        addAndMakeVisible (bpmField);

        // Ratt för samma värde som rutan (freeRunBpm är inte en APVTS-parameter, så KnobComponent kan inte användas).
        testBpmKnob.setRange (JogConfig::minBpm, JogConfig::maxBpm, 1.0);
        testBpmKnob.setDoubleClickReturnValue (true, JogConfig::defaultBpm);
        testBpmKnob.setValue (bpmField.getText().getIntValue(), juce::dontSendNotification);
        testBpmKnob.setTooltip ("TEST BPM tempo, " + juce::String (JogConfig::minBpm) + "-" + juce::String (JogConfig::maxBpm)
                                + " (double-click to reset to " + juce::String (JogConfig::defaultBpm) + ").");
        testBpmKnob.onValueChange = [this]
        {
            bpmField.setText (juce::String (juce::roundToInt (testBpmKnob.getValue())), juce::dontSendNotification);
            applyTestBpm();
        };
        addAndMakeVisible (testBpmKnob);

        statusLabel.setFont (Theme::font (11.0f, false));
        statusLabel.setColour (juce::Label::textColourId, Theme::textDim);
        statusLabel.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (statusLabel);

        startTimerHz (30);
        timerCallback();
    }

    ~JogPanel() override // ett hjul som inte hunnit släppas får inte hålla fast tempot
    {
        processor.jogManual = -1.0f;
        processor.scrubActive = false;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff121420));
        const auto wheel = wheelBounds().toFloat();
        const auto c = wheel.getCentre();
        const auto r = wheel.getWidth() * 0.5f;
        const auto circle = [&] (float radius) { return juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };

        // Sockel i ljus metall med en mörk ring runt skivan.
        const auto plinth = wheel.expanded (8.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dbe0), plinth.getX(), plinth.getY(), juce::Colour (0xff9a9ea8), plinth.getRight(), plinth.getBottom(), false));
        g.fillRoundedRectangle (plinth, 6.0f);
        g.setColour (juce::Colour (0xff50535c));
        g.drawRoundedRectangle (plinth, 6.0f, 1.0f);
        g.setColour (juce::Colour (0xff22242b));
        g.fillEllipse (wheel.expanded (3.0f));

        // Vinylskiva: svart skiva, fina spårringar, kant som lyser när man håller i den.
        g.setColour (juce::Colour (0xff08090d));
        g.fillEllipse (wheel);
        for (float f = 0.40f; f < 0.97f; f += 0.028f)
        {
            g.setColour (juce::Colours::white.withAlpha ((int) (f * 100.0f) % 3 == 0 ? 0.07f : 0.035f));
            g.drawEllipse (circle (r * f), 0.7f);
        }
        g.setColour (touching ? Theme::pattern : juce::Colour (0xff2c3148));
        g.drawEllipse (wheel.reduced (1.0f), 2.0f);

        // Ljusreflex: står still medan skivan snurrar (två motstående kilar).
        for (int side = 0; side < 2; ++side)
        {
            juce::Path glint;
            const auto mid = -juce::MathConstants<float>::pi * 0.25f + side * juce::MathConstants<float>::pi;
            glint.addPieSegment (circle (r * 0.96f), mid - 0.09f, mid + 0.09f, 0.42f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillPath (glint);
        }

        // Etikett med hål och en markering som roterar med skivan (visar fart och riktning).
        g.setColour (juce::Colour (0xff15161c));
        g.fillEllipse (circle (r * 0.34f));
        g.setColour (Theme::pattern);
        g.drawEllipse (circle (r * 0.34f), 1.0f);
        const auto a = shownAngle;
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine (c.x + std::cos (a) * r * 0.10f, c.y + std::sin (a) * r * 0.10f, c.x + std::cos (a) * r * 0.31f, c.y + std::sin (a) * r * 0.31f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (c.x - std::cos (a) * r * 0.10f, c.y - std::sin (a) * r * 0.10f, c.x - std::cos (a) * r * 0.31f, c.y - std::sin (a) * r * 0.31f, 1.5f);
        g.setColour (juce::Colour (0xff121420));
        g.fillEllipse (circle (r * 0.05f));

        // Tonarm från hörnet uppe till höger. Nålen börjar vid skivans kant och åker inåt mot etiketten ju längre mönstret har spelats (som en riktig LP).
        const auto needleRadius = r * (0.95f - 0.60f * shownProgress);
        const juce::Point<float> pivot (wheel.getRight() - 4.0f, wheel.getY() + 6.0f), needle (c.x + std::cos (-0.38f) * needleRadius, c.y + std::sin (-0.38f) * needleRadius);
        g.setColour (juce::Colour (0xffb8bcc8));
        g.drawLine (pivot.x, pivot.y, needle.x, needle.y, 3.0f);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (pivot));
        g.setColour (juce::Colour (0xff6a6f80));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (pivot));
        g.setColour (touching ? Theme::pattern : juce::Colour (0xffe8e8f0));
        g.fillRoundedRectangle (juce::Rectangle<float> (7.0f, 11.0f).withCentre (needle), 2.0f);

        g.setColour (Theme::text);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (juce::String (currentRate, 2) + "x", juce::Rectangle<float> (60.0f, 16.0f).withCentre ({ c.x, c.y + r * 0.62f }), juce::Justification::centred);

        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.0f, true));
        g.drawText ("FROM", fromField.getX(), fromField.getY() - 13, fromField.getWidth(), 12, juce::Justification::centred);
        g.drawText ("TO", toField.getX(), toField.getY() - 13, toField.getWidth(), 12, juce::Justification::centred);
        g.drawText ("STEP", stepBox.getX(), stepBox.getY() - 13, stepBox.getWidth(), 12, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto top = area.removeFromTop (150);
        top.removeFromLeft (160);
        top.removeFromTop (50);
        auto bpmRow = top.removeFromTop (36);
        testBpmButton.setBounds (bpmRow.removeFromLeft (100).withSizeKeepingCentre (100, 26));
        bpmRow.removeFromLeft (4);
        testBpmKnob.setBounds (bpmRow.removeFromLeft (36));
        bpmRow.removeFromLeft (4);
        bpmField.setBounds (bpmRow.removeFromLeft (46).withSizeKeepingCentre (46, 26));

        area.removeFromTop (22); // plats för FROM/TO/STEP-texterna
        auto loopRow = area.removeFromTop (28);
        loopButton.setBounds (loopRow.removeFromLeft (52));
        loopRow.removeFromLeft (8);
        fromField.setBounds (loopRow.removeFromLeft (110));
        loopRow.removeFromLeft (8);
        toField.setBounds (loopRow.removeFromLeft (110));
        loopRow.removeFromLeft (8);
        stepBox.setBounds (loopRow.removeFromLeft (62));
        area.removeFromTop (8);
        statusLabel.setBounds (area);
    }

    // Handen över skivan visar att den kan gripas (JUCE:s DraggingHandCursor, ingen separat sluten hand finns).
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (wheelBounds().contains (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (! touching)
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! wheelBounds().contains (e.getPosition()))
        {
            grabKeyboardFocus(); // klick utanför fälten avslutar en pågående redigering
            return;
        }
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        touching = true;
        processor.scrubActive = true;
        lastAngle = angleOf (e.position);
        lastTime = juce::Time::getMillisecondCounterHiRes();
        velocity = 0.0f;
        apply();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! touching)
            return;
        const auto angle = angleOf (e.position);
        const auto now = juce::Time::getMillisecondCounterHiRes();
        auto delta = angle - lastAngle;
        if (delta > juce::MathConstants<float>::pi)  delta -= juce::MathConstants<float>::twoPi;
        if (delta < -juce::MathConstants<float>::pi) delta += juce::MathConstants<float>::twoPi;
        const auto dt = (float) (now - lastTime) * 0.001f;
        if (dt > 0.0005f)
        {
            const auto instant = delta / dt / (juce::MathConstants<float>::twoPi * JogConfig::revolutionsPerSecondAtNormalTempo);
            velocity = 0.6f * velocity + 0.4f * instant;
            lastAngle = angle;
            lastTime = now;
        }
        shownAngle += delta;

        if (delta < 0.0f)
        {
            // Bakåt: klockan flyttas bakåt, ett varv = en takt (4 fjärdedelar).
            const auto ppq = (double) delta / juce::MathConstants<double>::twoPi * 4.0;
            auto expected = processor.scrubNudgePpq.load();
            while (! processor.scrubNudgePpq.compare_exchange_weak (expected, expected + ppq)) {}
        }
        apply();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        touching = false;
        setMouseCursor (wheelBounds().contains (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        velocity = 0.0f;
        processor.jogManual = -1.0f;
        processor.scrubActive = false;
        repaint();
    }

private:
    static int stepIndexOf (int ticks)
    {
        for (int i = 0; i < (int) std::size (JogConfig::stepTicks); ++i)
            if (JogConfig::stepTicks[i] == ticks)
                return i;
        return JogConfig::defaultStepIndex;
    }

    void showLoopTicks()
    {
        fromField.setTicks (processor.globalLoopStartTick.load());
        toField.setTicks (processor.globalLoopEndTick.load());
    }

    // Slutet måste ligga efter början; annars flyttas slutet ett STEP efter början.
    void setLoopTicks (int start, int end)
    {
        const auto step = std::max (1, processor.globalLoopStepTicks.load());
        if (end <= start)
            end = start + step;
        processor.globalLoopStartTick = start;
        processor.globalLoopEndTick = end;
        showLoopTicks();
    }

    void applyTestBpm()
    {
        const auto value = juce::jlimit (JogConfig::minBpm, JogConfig::maxBpm, bpmField.getText().getIntValue());
        processor.freeRunBpm = testBpmButton.getToggleState() ? (double) value : 0.0;
    }

    juce::Rectangle<int> wheelBounds() const { return { 10, 10, 140, 140 }; }

    float angleOf (juce::Point<float> p) const
    {
        const auto c = wheelBounds().toFloat().getCentre();
        return std::atan2 (p.y - c.y, p.x - c.x);
    }

    // Framåt: hjulets fart; stilla eller bakåt: klockan står still (bakåt flyttas den via scrubNudgePpq).
    void apply() { processor.jogManual = juce::jlimit (0.0f, JogConfig::maxRate, velocity); }

    void timerCallback() override
    {
        if (touching)
        {
            if (juce::Time::getMillisecondCounterHiRes() - lastTime > 90.0)
                velocity *= 0.7f; // handen står still: hjulet också
            apply();
        }

        const auto manual = processor.jogManual.load();
        currentRate = manual >= 0.0f ? manual : 1.0f;
        if (! touching)
            shownAngle += currentRate * 0.12f;
        const auto progress = processor.transportStatus.patternProgress.load();
        shownProgress = progress < shownProgress - 0.5f ? progress : shownProgress + 0.35f * (progress - shownProgress); // hoppar tillbaka vid loopen, glider annars

        if (! toField.hasKeyboardFocus (false) && ! fromField.hasKeyboardFocus (false))
            showLoopTicks();
        loopButton.setToggleState (processor.globalLoop.load(), juce::dontSendNotification);

        const auto text = processor.transportStatus.followingHost.load()
                              ? juce::String ("The DAW is playing: the wheel and the loop only move the plugin's own clock (previews, free run). The DAW is never touched.")
                              : juce::String ("Grab the wheel: forward plays at the speed you turn, backward rewinds, standing still is silence. The DAW is never touched.");
        if (statusLabel.getText() != text)
            statusLabel.setText (text, juce::dontSendNotification);
        repaint();
    }

    PluginBase& processor;
    TickField fromField, toField;
    juce::ComboBox stepBox;
    juce::Label statusLabel, bpmField;
    juce::TextButton loopButton { "LOOP" };
    juce::ToggleButton testBpmButton { "TEST BPM" };
    juce::Slider testBpmKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    bool touching = false;
    float lastAngle = 0.0f, shownAngle = 0.0f, velocity = 0.0f, currentRate = 1.0f, shownProgress = 0.0f;
    double lastTime = 0.0;
};
