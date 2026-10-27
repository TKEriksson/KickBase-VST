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
        setSize (330, 250);

        for (auto* b : { &stopMode, &nudgeMode })
        {
            b->setRadioGroupId (4711);
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
            addAndMakeVisible (*b);
        }
        stopMode.setTooltip ("STOP - Touching the wheel stops the music. Turn it forward to play at the speed you turn it; let go and it plays on at normal speed.");
        nudgeMode.setTooltip ("KEEP GOING - The music plays on while you touch the wheel. Turn it forward to speed up, back to slow down (it never plays backwards).");
        (processor.jogMode.load() == 1 ? nudgeMode : stopMode).setToggleState (true, juce::dontSendNotification);
        stopMode.onClick = [this] { processor.jogMode = 0; };
        nudgeMode.onClick = [this] { processor.jogMode = 1; };

        speedKnob.setDescription ("JOG SPEED - Playback speed of the plugin's own clock, 1 = normal, 0 = stopped, 4 = four times as fast. "
                                  "Right-click for DAW automation; drag an LFO here to modulate it.");
        addAndMakeVisible (speedKnob);

        removeButton.setTooltip ("Removes every LFO route to JOG SPEED and sets the speed back to 1. DAW automation is removed in the DAW.");
        removeButton.onClick = [this] { removeAutomation(); };
        addAndMakeVisible (removeButton);

        statusLabel.setFont (Theme::font (11.0f, false));
        statusLabel.setColour (juce::Label::textColourId, Theme::textDim);
        statusLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (statusLabel);

        startTimerHz (30);
        timerCallback();
    }

    ~JogPanel() override { processor.jogManual = -1.0f; } // ett hjul som inte hunnit släppas får inte hålla fast tempot

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff121420));
        const auto wheel = wheelBounds().toFloat();
        g.setColour (juce::Colour (0xff1c1f30));
        g.fillEllipse (wheel);
        g.setColour (touching ? Theme::pattern : juce::Colour (0xff2c3148));
        g.drawEllipse (wheel.reduced (1.0f), 2.0f);
        for (int i = 0; i < 12; ++i) // spår som i en skiva
        {
            const auto a = shownAngle + i * juce::MathConstants<float>::twoPi / 12.0f;
            const auto c = wheel.getCentre();
            const auto r = wheel.getWidth() * 0.5f;
            g.setColour (Theme::text.withAlpha (i == 0 ? 0.9f : 0.2f));
            g.drawLine (c.x + std::cos (a) * r * 0.55f, c.y + std::sin (a) * r * 0.55f,
                        c.x + std::cos (a) * r * 0.9f, c.y + std::sin (a) * r * 0.9f, i == 0 ? 3.0f : 1.0f);
        }
        g.setColour (Theme::text);
        g.setFont (Theme::font (13.0f, true));
        g.drawText (juce::String (currentRate, 2) + "x", wheel.reduced (wheel.getWidth() * 0.3f), juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto top = area.removeFromTop (150);
        top.removeFromLeft (150);
        auto modes = top.removeFromTop (28);
        stopMode.setBounds (modes.removeFromLeft (80).reduced (0, 2));
        modes.removeFromLeft (4);
        nudgeMode.setBounds (modes.removeFromLeft (92).reduced (0, 2));
        top.removeFromTop (6);
        speedKnob.setBounds (top.withSizeKeepingCentre (70, 90));
        area.removeFromTop (4);
        statusLabel.setBounds (area.removeFromTop (34));
        removeButton.setBounds (area.removeFromTop (26).removeFromLeft (170));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! wheelBounds().contains (e.getPosition()))
            return;
        touching = true;
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
        apply();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        touching = false;
        velocity = 0.0f;
        processor.jogManual = -1.0f;
        repaint();
    }

private:
    juce::Rectangle<int> wheelBounds() const { return { 10, 10, 140, 140 }; }

    float angleOf (juce::Point<float> p) const
    {
        const auto c = wheelBounds().toFloat().getCentre();
        return std::atan2 (p.y - c.y, p.x - c.x);
    }

    void apply()
    {
        const auto mode = processor.jogMode.load();
        const auto rate = mode == 1 ? 1.0f + velocity : velocity;
        processor.jogManual = juce::jlimit (0.0f, 4.0f, rate);
    }

    int speedIndex() const { return processor.getParameters().indexOf (processor.apvts.getParameter (ParamID::jogSpeed)); }

    void removeAutomation()
    {
        const auto index = speedIndex();
        const auto before = processor.snapshotMods();
        for (int r = Modulation::maxRoutes - 1; r >= 0; --r)
            if (processor.modRoutes.routes[(size_t) r].source.load() >= 0 && processor.modRoutes.routes[(size_t) r].target.load() == index)
                processor.modRoutes.remove (r);
        processor.markStateChanged();
        processor.recordModEdit (before, "Remove JOG automation");
        if (auto* parameter = processor.apvts.getParameter (ParamID::jogSpeed))
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }

    void timerCallback() override
    {
        if (touching)
        {
            if (juce::Time::getMillisecondCounterHiRes() - lastTime > 90.0)
                velocity *= 0.7f; // handen står still: hjulet också
            apply();
        }

        const auto manual = processor.jogManual.load();
        currentRate = manual >= 0.0f ? manual : processor.apvts.getRawParameterValue (ParamID::jogSpeed)->load(); // grundvärdet (LFO-påverkan visas inte här)
        if (! touching)
            shownAngle += currentRate * 0.12f;

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
        removeButton.setEnabled (! sources.isEmpty() || moved);
        removeButton.setColour (juce::TextButton::buttonColourId, ! sources.isEmpty() || moved ? juce::Colour (0xff6a2430) : juce::Colour (0xff2c3148));
        repaint();
    }

    PluginBase& processor;
    juce::TextButton stopMode { "STOP" }, nudgeMode { "KEEP GOING" }, removeButton { "REMOVE AUTOMATION" };
    KnobComponent speedKnob;
    juce::Label statusLabel;
    bool touching = false;
    float lastAngle = 0.0f, shownAngle = 0.0f, velocity = 0.0f, currentRate = 1.0f;
    double lastTime = 0.0;
};
