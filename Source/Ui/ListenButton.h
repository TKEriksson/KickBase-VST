#pragma once

#include "Theme.h"

// En högtalarknapp för att lyssna. Håll ner musen för att höra (det loopar), släpp för att sluta. Ett nytt tryck börjar om från
// början. Dubbelklicka så fortsätter det tills man klickar igen. `onStart` och `onStop` gör själva lyssnandet.
class ListenButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onStart, onStop;

    // Ljudet spelar (från motorn): knappen lyser även när ljudet startats på annat håll.
    void setActive (bool shouldBeActive)
    {
        if (shouldBeActive != active)
        {
            active = shouldBeActive;
            repaint();
        }
    }

    void setAccent (juce::Colour colour) { accent = colour; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto lit = active || held || latched;
        g.setColour (lit ? accent.withAlpha (0.55f) : juce::Colour (hover ? 0xff2c3148 : 0xff23263a));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (lit ? accent : Theme::panelBorder.withAlpha (hover ? 1.0f : 0.8f));
        g.drawRoundedRectangle (bounds, 5.0f, lit ? 1.6f : 1.0f);

        // Högtalare med ljudvågor, centrerad.
        const auto c = bounds.getCentre();
        const auto s = std::min (bounds.getWidth(), bounds.getHeight()) / 26.0f;
        juce::Path body;
        body.startNewSubPath (c.x - 8.0f * s, c.y - 3.5f * s);
        body.lineTo (c.x - 4.0f * s, c.y - 3.5f * s);
        body.lineTo (c.x + 1.0f * s, c.y - 8.0f * s);
        body.lineTo (c.x + 1.0f * s, c.y + 8.0f * s);
        body.lineTo (c.x - 4.0f * s, c.y + 3.5f * s);
        body.lineTo (c.x - 8.0f * s, c.y + 3.5f * s);
        body.closeSubPath();
        g.setColour (lit ? juce::Colours::white : Theme::text.withAlpha (0.9f));
        g.fillPath (body);
        for (const auto radius : { 4.5f, 8.0f })
        {
            juce::Path wave;
            wave.addCentredArc (c.x + 1.0f * s, c.y, radius * s, radius * s, 0.0f, 0.7f, 2.45f, true);
            g.strokePath (wave, juce::PathStrokeType (1.5f * std::max (0.8f, s), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (latched)
        {
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withPosition (bounds.getRight() - 6.0f, bounds.getY() + 2.0f)); // fastlåst
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (latched) // ett klick medan den är fastlåst stänger av den
        {
            latched = false;
            held = false;
            if (onStop)
                onStop();
            repaint();
            return;
        }
        held = true;
        if (onStart)
            onStart(); // börjar om från början, även om det redan spelar
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (held && ! latched)
        {
            held = false;
            if (onStop)
                onStop();
        }
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        latched = true; // fortsätter spela tills man klickar igen
        repaint();
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    bool active = false, held = false, latched = false, hover = false;
    juce::Colour accent = Theme::pattern;
};

// En liten på/av-knapp med loop-pilar (en rads loop-inställning). Kopplas till en bool-parameter med en ButtonAttachment.
class LoopToggle : public juce::Button
{
public:
    explicit LoopToggle (juce::Colour accentColour = Theme::pattern) : juce::Button ("loop"), accent (accentColour)
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto on = getToggleState();
        g.setColour (on ? accent.withAlpha (0.45f) : juce::Colour (highlighted ? 0xff2c3148 : 0xff23263a));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (on ? accent : Theme::panelBorder.withAlpha (highlighted ? 1.0f : 0.8f));
        g.drawRoundedRectangle (bounds, 5.0f, on ? 1.6f : 1.0f);

        // Två pilar som går runt.
        const auto c = bounds.getCentre();
        const auto r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.28f;
        g.setColour (on ? juce::Colours::white : Theme::textDim);
        for (int i = 0; i < 2; ++i)
        {
            const auto from = i == 0 ? 0.35f : 0.35f + juce::MathConstants<float>::pi;
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0.0f, from, from + 2.3f, true);
            g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const auto angle = from + 2.3f;
            const juce::Point<float> tip { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
            juce::Path head;
            head.addTriangle (tip.x, tip.y,
                              tip.x - 4.0f * std::cos (angle) - 2.0f * std::sin (angle), tip.y - 4.0f * std::sin (angle) + 2.0f * std::cos (angle),
                              tip.x + 1.0f * std::cos (angle) - 4.0f * std::sin (angle), tip.y + 1.0f * std::sin (angle) + 4.0f * std::cos (angle));
            g.fillPath (head);
        }
    }

private:
    juce::Colour accent;
};
