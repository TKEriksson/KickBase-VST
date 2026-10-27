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
            // Motorn har en enda aktiv källa: tar en annan över (eller ljudet stoppas) släpps den lokala fastlåsningen/hållningen,
            // annars lyser knappen kvar och nästa mouseUp skulle stoppa den nya källan.
            if (active && ! shouldBeActive)
                latched = held = false;
            active = shouldBeActive;
            repaint();
        }
    }

    void setAccent (juce::Colour colour) { accent = colour; repaint(); }

    // Knappen har själv startat ljudet (hållen eller fastlåst), till skillnad från att den bara lyser för att motorn spelar.
    bool isHeldOrLatched() const { return held || latched; }

    // Släpper den lokala hållningen/fastlåsningen utan att anropa onStop (den som släpper stoppar ljudet själv).
    void release() { if (held || latched) { held = latched = false; repaint(); } }

    // Spela/stopp-ikon (triangel i vila, kvadrat medan det spelar) i stället för högtalaren; högtalaren behålls för mute/lyssna på raderna.
    void setPlayIcon (bool shouldUsePlayIcon) { playIcon = shouldUsePlayIcon; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto lit = active || held || latched;
        g.setColour (lit ? accent.withAlpha (0.55f) : juce::Colour (hover ? 0xff2c3148 : 0xff23263a));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (lit ? accent : Theme::panelBorder.withAlpha (hover ? 1.0f : 0.8f));
        g.drawRoundedRectangle (bounds, 5.0f, lit ? 1.6f : 1.0f);

        const auto c = bounds.getCentre();
        const auto s = std::min (bounds.getWidth(), bounds.getHeight()) / 26.0f;
        if (playIcon)
        {
            g.setColour (lit ? juce::Colours::white : Theme::text.withAlpha (0.9f));
            if (lit)
                g.fillRect (juce::Rectangle<float> (10.0f * s, 10.0f * s).withCentre (c));
            else
                g.fillPath ([&] { juce::Path p; p.addTriangle (c.x - 4.5f * s, c.y - 6.5f * s, c.x - 4.5f * s, c.y + 6.5f * s, c.x + 6.5f * s, c.y); return p; }());
            if (latched)
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withPosition (bounds.getRight() - 6.0f, bounds.getY() + 2.0f));
            return;
        }

        // Högtalare med ljudvågor, centrerad.
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

    // JUCE skickar mouseDoubleClick efter det andra klickets mouseUp, så fastlåsningen måste ske redan här: annars stoppar mouseUp ljudet
    // innan mouseDoubleClick hinner låsa det (och knappen låter ingenting).
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (held && event.getNumberOfClicks() >= 2)
        {
            latched = true;
            held = false;
            repaint();
            return;
        }
        if (held && ! latched)
        {
            held = false;
            if (onStop)
                onStop();
        }
        repaint();
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    bool active = false, held = false, latched = false, hover = false, playIcon = false;
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

    void setAccent (juce::Colour colour) { accent = colour; repaint(); }

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

// PIANO ROLL-knappen: en pianoikon med ett plus när raden inte har någon pianorulle än (grön: lägg till en), utan plus när den har en
// (blå: öppna den). Medan pianorullen är öppen är knappen tänd.
class PianoRollButton : public juce::Button
{
public:
    PianoRollButton() : juce::Button ("piano roll") {}

    void setState (bool wantsPlus, bool isOpen)
    {
        if (wantsPlus != plus || isOpen != open)
        {
            plus = wantsPlus;
            open = isOpen;
            repaint();
        }
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = plus ? juce::Colour (0xff4be08a) : juce::Colour (0xff4aa8ff);
        const auto alpha = open ? 0.6f : (highlighted || down ? 0.38f : 0.24f);
        g.setColour (accent.withAlpha (alpha));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (accent.withAlpha (open || highlighted ? 1.0f : 0.7f));
        g.drawRoundedRectangle (bounds, 6.0f, open ? 1.8f : 1.2f);

        // Pianot: sju vita tangenter med fem svarta ovanpå.
        const auto iconHeight = std::min (bounds.getHeight() - 8.0f, 20.0f);
        const auto iconWidth = iconHeight * 1.5f;
        const auto iconArea = juce::Rectangle<float> (iconWidth, iconHeight).withCentre ({ bounds.getCentreX() + (plus ? 7.0f : 0.0f), bounds.getCentreY() });
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillRoundedRectangle (iconArea, 2.0f);
        const auto keyWidth = iconArea.getWidth() / 7.0f;
        g.setColour (juce::Colour (0xff10121c));
        for (int i = 1; i < 7; ++i)
            g.fillRect (juce::Rectangle<float> (iconArea.getX() + keyWidth * (float) i - 0.4f, iconArea.getY(), 0.8f, iconArea.getHeight()));
        for (const auto index : { 1, 2, 4, 5, 6 })
            g.fillRect (juce::Rectangle<float> (iconArea.getX() + keyWidth * (float) index - keyWidth * 0.3f, iconArea.getY(), keyWidth * 0.6f, iconArea.getHeight() * 0.58f));

        // Plus: lägg till en pianorulle.
        if (plus)
        {
            const auto centre = juce::Point<float> (iconArea.getX() - 10.0f, bounds.getCentreY());
            g.setColour (accent.brighter (0.4f));
            g.drawLine (centre.x - 5.0f, centre.y, centre.x + 5.0f, centre.y, 2.4f);
            g.drawLine (centre.x, centre.y - 5.0f, centre.x, centre.y + 5.0f, 2.4f);
        }
    }

private:
    bool plus = true, open = false;
};
