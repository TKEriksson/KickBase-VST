#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Egna muspekare, ritade som små ikoner: en högtalare (klicka för att lyssna), en penna och ett sudd (pianorullen).
// Skapas som medlemmar i komponenterna (inte som statiska objekt, som skulle leva kvar efter att JUCE stängts av).
namespace Cursors
{
    inline juce::MouseCursor fromDrawing (const std::function<void (juce::Graphics&)>& draw, int hotX, int hotY)
    {
        juce::Image image (juce::Image::ARGB, 32, 32, true);
        juce::Graphics g (image);
        draw (g);
        return juce::MouseCursor (image, hotX, hotY);
    }

    inline void outlined (juce::Graphics& g, const juce::Path& path, juce::Colour fill)
    {
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.strokePath (path, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (fill);
        g.fillPath (path);
    }

    // Högtalare med ljudvågor.
    inline juce::MouseCursor speaker()
    {
        return fromDrawing ([] (juce::Graphics& g)
        {
            juce::Path body;
            body.startNewSubPath (4.0f, 12.0f);
            body.lineTo (10.0f, 12.0f);
            body.lineTo (17.0f, 6.0f);
            body.lineTo (17.0f, 26.0f);
            body.lineTo (10.0f, 20.0f);
            body.lineTo (4.0f, 20.0f);
            body.closeSubPath();
            outlined (g, body, juce::Colours::white);

            for (const auto radius : { 6.0f, 11.0f })
            {
                juce::Path wave;
                wave.addCentredArc (17.0f, 16.0f, radius, radius, 0.0f, 0.6f, 2.5f, true);
                g.setColour (juce::Colours::black.withAlpha (0.85f));
                g.strokePath (wave, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colours::white);
                g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }, 6, 16);
    }

    // Penna: spetsen till vänster i mitten (där den ritar), kroppen snett uppåt höger, så att tonen hamnar mitt för pennan och till vänster.
    inline juce::MouseCursor pencil()
    {
        return fromDrawing ([] (juce::Graphics& g)
        {
            // Ritad vågrätt med spetsen vid (-8, 0), sedan vriden 25 grader så att spetsen hamnar till vänster i mitten.
            const auto rotate = juce::AffineTransform::rotation (-juce::MathConstants<float>::pi * 25.0f / 180.0f);
            auto tip = juce::Point<float> (-8.0f, 0.0f);
            tip.applyTransform (rotate);
            const auto place = rotate.followedBy (juce::AffineTransform::translation (4.0f - tip.x, 17.0f - tip.y));

            juce::Path body;
            body.addRectangle (0.0f, -3.5f, 17.0f, 7.0f);
            body.applyTransform (place);
            outlined (g, body, juce::Colour (0xffffd34d));

            juce::Path cone;
            cone.startNewSubPath (-8.0f, 0.0f);
            cone.lineTo (0.0f, -3.5f);
            cone.lineTo (0.0f, 3.5f);
            cone.closeSubPath();
            cone.applyTransform (place);
            outlined (g, cone, juce::Colour (0xfff2d4a8));

            juce::Path end;
            end.addRectangle (14.0f, -3.5f, 3.5f, 7.0f);
            end.applyTransform (place);
            g.setColour (juce::Colour (0xffff7a8a));
            g.fillPath (end);
        }, 4, 17);
    }

    // Sudd: en rosa klots, spetsen i mitten.
    inline juce::MouseCursor eraser()
    {
        return fromDrawing ([] (juce::Graphics& g)
        {
            const auto place = juce::AffineTransform::rotation (-juce::MathConstants<float>::pi / 5.0f, 0.0f, 0.0f)
                                   .followedBy (juce::AffineTransform::translation (13.0f, 24.0f));
            juce::Path block;
            block.addRoundedRectangle (-9.0f, -6.0f, 18.0f, 12.0f, 2.0f);
            block.applyTransform (place);
            outlined (g, block, juce::Colour (0xffff9eb5));

            juce::Path band;
            band.addRectangle (-9.0f, -6.0f, 7.0f, 12.0f);
            band.applyTransform (place);
            g.setColour (juce::Colour (0xff7aa7ff));
            g.fillPath (band);
        }, 8, 26);
    }
}
