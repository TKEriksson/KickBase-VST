#pragma once

#include "Modules.h"

// JOG / LP (D3b): ett skivspelarhjul som bara styr pluginets egna klocka (förhandsspelning, free run). Värdens uppspelning påverkas aldrig.
// Hjulet skriver `jogManual` (0..4, hastighet relativt tempot) så länge man håller i det; annars gäller parametern jogSpeed (som kan
// automatiseras i DAW:en eller modulas av en LFO, precis som vilken ratt som helst).
class JogPanel : public juce::Component, private juce::Timer
{
public:
    explicit JogPanel (PluginBase& p) : processor (p), speedKnob (p.apvts, ParamID::jogSpeed, "JOG SPEED", Theme::pattern)
    {
        setSize (380, 284);

        for (auto* b : { &stopMode, &nudgeMode, &recMode })
        {
            b->setRadioGroupId (4711);
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
            addAndMakeVisible (*b);
        }
        stopMode.setTooltip ("STOP - Touching the wheel stops the music. Turn it forward to play at the speed you turn it; let go and it plays on at normal speed.");
        nudgeMode.setTooltip ("KEEP GOING - The music plays on while you touch the wheel. Turn it forward to speed up, back to slow down (it never plays backwards).");
        recMode.setTooltip ("REC - Like STOP, but the wheel writes JOG SPEED as a parameter gesture, so the DAW can record your scratch (Write/Touch/Latch). "
                            "JOG SPEED springs back to 1x when you let go.");
        for (auto* b : { &kindJog, &kindScrub, &kindSpeed })
        {
            b->setRadioGroupId (4712);
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
            addAndMakeVisible (*b);
        }
        kindScrub.setTooltip ("SCRUB - Grab the wheel and the music leaves the DAW's position and follows the wheel: forward plays, backward rewinds, standing still is silence. "
                              "Let go and it follows the DAW again. Default in new projects.");
        kindJog.setTooltip ("JOG - The older wheel: STOP, KEEP GOING or REC (see the buttons below). Kept so old projects sound the same.");
        kindSpeed.setTooltip ("SPEED - The wheel is a big knob for JOG SPEED: turn it forward for faster, back for slower. The speed stays where you leave it (RESET sets 1x).");
        (processor.jogKind.load() == PluginBase::jogKindSpeed ? kindSpeed : processor.jogKind.load() == PluginBase::jogKindScrub ? kindScrub : kindJog)
            .setToggleState (true, juce::dontSendNotification);
        kindJog.onClick = [this] { setKind (PluginBase::jogKindJog); };
        kindScrub.onClick = [this] { setKind (PluginBase::jogKindScrub); };
        kindSpeed.onClick = [this] { setKind (PluginBase::jogKindSpeed); };
        updateKindVisibility();

        (processor.jogMode.load() == 2 ? recMode : processor.jogMode.load() == 1 ? nudgeMode : stopMode).setToggleState (true, juce::dontSendNotification);
        stopMode.onClick = [this] { processor.jogMode = 0; };
        nudgeMode.onClick = [this] { processor.jogMode = 1; };
        recMode.onClick = [this] { processor.jogMode = 2; };

        speedKnob.setDescription ("JOG SPEED - Playback speed of the plugin's own clock, 1 = normal, 0 = stopped, 4 = four times as fast. "
                                  "Right-click for DAW automation; drag an LFO here to modulate it.");
        addAndMakeVisible (speedKnob);

        removeButton.setTooltip ("Sets JOG SPEED back to 1x. LFO routes are not touched (remove them in the modulation matrix); DAW automation is removed in the DAW.");
        removeButton.onClick = [this] { resetSpeed(); };
        addAndMakeVisible (removeButton);

        brakeButton.setTooltip ("BRAKE - One-shot record-player stop: the plugin's clock slows to a halt, rests a moment, then spins back up to the JOG SPEED you had. "
                                "Touching the wheel cancels it. Only the plugin's own clock is affected.");
        brakeButton.onClick = [this] { startBrake(); };
        addAndMakeVisible (brakeButton);

        brakeCurveBox.addItemList ({ "VINYL", "TAPE", "HARD", "SLOW" }, 1);
        brakeCurveBox.setSelectedItemIndex (juce::jlimit (0, 3, processor.brakeCurve.load()), juce::dontSendNotification);
        brakeCurveBox.setTooltip ("BRAKE curve. VINYL: record player. TAPE: long, pitch sinks like a tape stop. HARD: short and straight. SLOW: slow stop and slow start.");
        brakeCurveBox.onChange = [this] { processor.brakeCurve = brakeCurveBox.getSelectedItemIndex(); };
        addAndMakeVisible (brakeCurveBox);

        // LOOP (flyttad hit från rutan i toppbaren): panelen byggs om vid varje öppning, så läget läses från processorn.
        loopButton.setClickingTogglesState (true);
        loopButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        loopButton.setToggleState (processor.globalLoop.load(), juce::dontSendNotification);
        loopButton.setTooltip ("LOOP - The plugin loops the bars shown next to it on its own clock and ignores the DAW's position. "
                               "A plugin cannot stop the DAW, so stop it yourself in the DAW.");
        loopButton.onClick = [this]
        {
            applyLoopBars();
            const auto on = loopButton.getToggleState();
            processor.globalLoop = on;
            if (on)
                processor.seekBarRequest = processor.globalLoopStartBar.load();
        };
        addAndMakeVisible (loopButton);
        for (auto* label : { &loopFromLabel, &loopToLabel })
        {
            label->setFont (Theme::font (13.0f, true));
            label->setJustificationType (juce::Justification::centred);
            label->setColour (juce::Label::textColourId, Theme::text);
            label->setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0b0d18));
            label->setColour (juce::Label::textWhenEditingColourId, Theme::text);
            label->setColour (juce::Label::outlineColourId, Theme::text.withAlpha (0.25f));
            label->setEditable (false, true, false);
            label->onTextChange = [this] { applyLoopBars(); };
            addAndMakeVisible (*label);
        }
        loopFromLabel.setText (juce::String (processor.globalLoopStartBar.load() + 1), juce::dontSendNotification);
        loopToLabel.setText (juce::String (processor.globalLoopEndBar.load()), juce::dontSendNotification);
        loopFromLabel.setTooltip ("LOOP start bar (double-click to type).");
        loopToLabel.setTooltip ("LOOP end bar, included (double-click to type).");

        statusLabel.setFont (Theme::font (11.0f, false));
        statusLabel.setColour (juce::Label::textColourId, Theme::textDim);
        statusLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statusLabel);

        startTimerHz (30);
        timerCallback();
    }

    ~JogPanel() override // ett hjul som inte hunnit släppas får inte hålla fast tempot eller lämna en öppen gest
    {
        processor.jogManual = -1.0f;
        processor.scrubActive = false;
        endGesture();
        endSpeedGesture();
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
        g.setColour (juce::Colour (0xff15161c)); // svart etikett som på förebilden, med en tunn färgring
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
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto top = area.removeFromTop (150);
        top.removeFromLeft (150);
        auto kinds = top.removeFromTop (24);
        kindJog.setBounds (kinds.removeFromLeft (56).reduced (0, 1));
        kinds.removeFromLeft (4);
        kindScrub.setBounds (kinds.removeFromLeft (70).reduced (0, 1));
        kinds.removeFromLeft (4);
        kindSpeed.setBounds (kinds.reduced (0, 1));
        top.removeFromTop (3);
        auto modes = top.removeFromTop (24);
        stopMode.setBounds (modes.removeFromLeft (56).reduced (0, 1));
        modes.removeFromLeft (4);
        nudgeMode.setBounds (modes.removeFromLeft (84).reduced (0, 1));
        modes.removeFromLeft (4);
        recMode.setBounds (modes.reduced (0, 1));
        top.removeFromTop (2);
        const auto knobArea = top.withSizeKeepingCentre (70, juce::jmin (90, top.getHeight()));
        speedKnob.setBounds (knobArea);
        removeButton.setBounds (juce::Rectangle<int> (52, 24).withPosition (knobArea.getRight() + 6, knobArea.getCentreY() - 12));
        brakeButton.setBounds (removeButton.getBounds().translated (0, 28));
        brakeCurveBox.setBounds (brakeButton.getBounds().translated (0, 28).withWidth (72));
        area.removeFromTop (4);
        statusLabel.setBounds (area.removeFromTop (34));
        area.removeFromTop (6);
        auto loopRow = area.removeFromTop (26);
        loopButton.setBounds (loopRow.removeFromLeft (52));
        loopRow.removeFromLeft (8);
        loopFromLabel.setBounds (loopRow.removeFromLeft (34));
        loopRow.removeFromLeft (14);
        loopToLabel.setBounds (loopRow.removeFromLeft (34));
    }

    void applyLoopBars()
    {
        const auto from = std::max (1, loopFromLabel.getText().retainCharacters ("0123456789").getIntValue());
        const auto to = std::max (from, loopToLabel.getText().retainCharacters ("0123456789").getIntValue());
        loopFromLabel.setText (juce::String (from), juce::dontSendNotification);
        loopToLabel.setText (juce::String (to), juce::dontSendNotification);
        processor.globalLoopStartBar = from - 1;
        processor.globalLoopEndBar = to; // slutet exklusivt, 0-räknat = sista inräknade takten 1-räknad
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
            return;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        touching = true;
        const auto kind = processor.jogKind.load();
        if (kind == PluginBase::jogKindScrub)
            processor.scrubActive = true;
        else if (kind == PluginBase::jogKindSpeed)
        {
            if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
            {
                parameter->beginChangeGesture();
                speedGestureOpen = true;
            }
        }
        else if (processor.jogMode.load() == 2)
            if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
            {
                parameter->beginChangeGesture();
                gestureOpen = true;
            }
        braking = false; // en hand på skivan avbryter bromsen
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
            // Ett varv på 1,33 s (0,75 varv/s) = normalt tempo.
            const auto instant = delta / dt / (juce::MathConstants<float>::twoPi * 0.75f);
            velocity = 0.6f * velocity + 0.4f * instant;
            lastAngle = angle;
            lastTime = now;
        }
        shownAngle += delta;

        const auto kind = processor.jogKind.load();
        if (kind == PluginBase::jogKindScrub && delta < 0.0f)
        {
            // Bakåt: klockan flyttas bakåt, ett varv = en takt (4 fjärdedelar).
            const auto ppq = (double) delta / juce::MathConstants<double>::twoPi * 4.0;
            auto expected = processor.scrubNudgePpq.load();
            while (! processor.scrubNudgePpq.compare_exchange_weak (expected, expected + ppq)) {}
        }
        else if (kind == PluginBase::jogKindSpeed && speedGestureOpen)
        {
            // Ett varv = 1x ändring av hastigheten.
            if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
            {
                const auto current = parameter->convertFrom0to1 (parameter->getValue());
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (0.0f, 4.0f, current + delta / juce::MathConstants<float>::twoPi)));
            }
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
        endGesture();
        endSpeedGesture();
        repaint();
    }

private:
    void setKind (int kind)
    {
        processor.jogKind = kind;
        processor.markStateChanged();
        updateKindVisibility();
    }

    void updateKindVisibility()
    {
        const auto jog = processor.jogKind.load() == PluginBase::jogKindJog;
        for (auto* b : { &stopMode, &nudgeMode, &recMode })
            b->setVisible (jog);
    }

    // SPEED-läget: hastigheten blir kvar där hjulet lämnades, så gesten stängs utan fjädring.
    void endSpeedGesture()
    {
        if (! speedGestureOpen)
            return;
        if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
            parameter->endChangeGesture();
        speedGestureOpen = false;
    }

    // REC-läget: hjulet skriver jogSpeed inom en gest. Vid släpp skrivs 1x (fjädring) innan gesten stängs, så att värdens inspelning får med återgången
    // och inget skrivs över efteråt.
    void endGesture()
    {
        if (! gestureOpen)
            return;
        if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
        {
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
        gestureOpen = false;
    }

    // BRAKE (broms B): engångsbroms i tre steg (stanna, vila, snurra upp) som skriver jogManual; grundhastigheten (JOG SPEED) rörs aldrig.
    // Kurvorna (broms A): tider i ms och form på inbromsning (1 -> 0) och uppstart (0 -> 1), båda över x = 0..1.
    struct BrakeCurve
    {
        double stopMs, restMs, spinMs;
        float (*stopShape) (float);
        float (*spinShape) (float);
    };

    static const BrakeCurve& brakeCurveFor (int index)
    {
        static const BrakeCurve curves[] = {
            { 1200.0, 300.0, 900.0, [] (float x) { return 1.0f - x * x; }, [] (float x) { return x * (2.0f - x); } },  // VINYL
            { 2200.0, 150.0, 1400.0, [] (float x) { return (1.0f - x) * std::exp (-3.0f * x); },                          // TAPE
              [] (float x) { return (1.0f - std::exp (-4.0f * x)) / (1.0f - std::exp (-4.0f)); } },
            { 250.0, 80.0, 250.0, [] (float x) { return 1.0f - x; }, [] (float x) { return x; } },                        // HARD
            { 2500.0, 500.0, 2000.0, [] (float x) { return 1.0f - x * x * (3.0f - 2.0f * x); },                            // SLOW
              [] (float x) { return x * x * (3.0f - 2.0f * x); } }
        };
        return curves[juce::jlimit (0, 3, index)];
    }

    void startBrake()
    {
        if (touching)
            return;
        activeBrake = &brakeCurveFor (processor.brakeCurve.load());
        const auto manual = processor.jogManual.load();
        brakeFromRate = manual >= 0.0f ? manual : processor.apvts.getRawParameterValue (ParamID::jogSpeed)->load();
        brakeStart = juce::Time::getMillisecondCounterHiRes();
        braking = true;
    }

    void stepBrake()
    {
        const auto t = juce::Time::getMillisecondCounterHiRes() - brakeStart;
        const auto target = processor.apvts.getRawParameterValue (ParamID::jogSpeed)->load();
        const auto& c = *activeBrake;
        if (t >= c.stopMs + c.restMs + c.spinMs)
        {
            braking = false;
            processor.jogManual = -1.0f; // tillbaka till parametern
            return;
        }
        float rate = 0.0f;
        if (t < c.stopMs)
            rate = brakeFromRate * c.stopShape ((float) (t / c.stopMs));
        else if (t >= c.stopMs + c.restMs)
            rate = target * c.spinShape ((float) ((t - c.stopMs - c.restMs) / c.spinMs));
        processor.jogManual = juce::jlimit (0.0f, 4.0f, rate);
    }

    juce::Rectangle<int> wheelBounds() const { return { 10, 10, 140, 140 }; }

    float angleOf (juce::Point<float> p) const
    {
        const auto c = wheelBounds().toFloat().getCentre();
        return std::atan2 (p.y - c.y, p.x - c.x);
    }

    void apply()
    {
        const auto kind = processor.jogKind.load();
        if (kind == PluginBase::jogKindSpeed)
            return; // hjulet skriver parametern direkt i mouseDrag
        if (kind == PluginBase::jogKindScrub)
        {
            processor.jogManual = juce::jlimit (0.0f, 4.0f, velocity); // framåt: hjulets fart; stilla eller bakåt: klockan står still (bakåt flyttas den via scrubNudgePpq)
            return;
        }
        const auto mode = processor.jogMode.load();
        const auto rate = mode == 1 ? 1.0f + velocity : velocity;
        if (mode == 2 && gestureOpen)
        {
            if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (0.0f, 4.0f, rate)));
            processor.jogManual = -1.0f; // parametern bär hastigheten, så att inspelning och uppspelning ger samma ljud
            return;
        }
        processor.jogManual = juce::jlimit (0.0f, 4.0f, rate);
    }

    int speedIndex() const { return processor.getParameters().indexOf (processor.apvts.getParameter (ParamID::jogSpeed)); }

    void resetSpeed()
    {
        // RESET: bara hastigheten tillbaka till 1x, en gest (en undo). LFO-rutterna lämnas, de tas bort i modulationsmatrisen.
        if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    }

    void timerCallback() override
    {
        if (touching)
        {
            if (juce::Time::getMillisecondCounterHiRes() - lastTime > 90.0)
                velocity *= 0.7f; // handen står still: hjulet också
            apply();
        }
        else if (braking)
            stepBrake();

        const auto manual = processor.jogManual.load();
        currentRate = manual >= 0.0f ? manual : processor.apvts.getRawParameterValue (ParamID::jogSpeed)->load(); // grundvärdet (LFO-påverkan visas inte här)
        if (! touching)
            shownAngle += currentRate * 0.12f;
        const auto progress = processor.transportStatus.patternProgress.load();
        shownProgress = progress < shownProgress - 0.5f ? progress : shownProgress + 0.35f * (progress - shownProgress); // hoppar tillbaka vid loopen, glider annars

        const auto index = speedIndex();
        juce::StringArray sources;
        for (int r = 0; r < Modulation::maxRoutes; ++r)
        {
            const auto& route = processor.modRoutes.routes[(size_t) r];
            if (route.source.load() >= 0 && route.target.load() == index)
                sources.add (Modulation::sourceName (route.source.load()));
        }
        const auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed);
        const auto moved = parameter != nullptr && std::abs (parameter->convertFrom0to1 (parameter->getValue()) - 1.0f) > 0.001f;
        juce::String text = sources.isEmpty() ? "No LFO on JOG SPEED." : "Automated by " + sources.joinIntoString (", ") + ".";
        text += moved ? " Speed is not 1x." : "";
        text += processor.transportStatus.followingHost.load() ? "\nThe DAW is playing: JOG only moves the plugin's own clock (previews, free run)."
                                                                : "\nJOG moves the plugin's own clock only; the DAW is never touched.";
        statusLabel.setText (text, juce::dontSendNotification);
        removeButton.setEnabled (moved);
        removeButton.setColour (juce::TextButton::buttonColourId, moved ? juce::Colour (0xff6a2430) : juce::Colour (0xff2c3148));
        repaint();
    }

    PluginBase& processor;
    juce::TextButton stopMode { "STOP" }, nudgeMode { "KEEP GOING" }, removeButton { "RESET" }, recMode { "REC" }, brakeButton { "BRAKE" };
    juce::TextButton kindJog { "JOG" }, kindScrub { "SCRUB" }, kindSpeed { "SPEED" };
    KnobComponent speedKnob;
    juce::Label statusLabel, loopFromLabel, loopToLabel;
    juce::TextButton loopButton { "LOOP" };
    bool touching = false, gestureOpen = false, speedGestureOpen = false, braking = false;
    juce::ComboBox brakeCurveBox;
    const BrakeCurve* activeBrake = &brakeCurveFor (0);
    float brakeFromRate = 1.0f;
    double brakeStart = 0.0;
    float lastAngle = 0.0f, shownAngle = 0.0f, velocity = 0.0f, currentRate = 1.0f, shownProgress = 0.0f;
    double lastTime = 0.0;
};
