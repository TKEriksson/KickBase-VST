#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Färger och mått för gränssnittet. Mörk blå/violett bas med en egen accentfärg per modul.
namespace Theme
{
    inline const juce::Colour backgroundTop    { 0xff1f2245 };
    inline const juce::Colour backgroundBottom { 0xff10112a };
    inline const juce::Colour panel            { 0xc0202449 };
    inline const juce::Colour panelBorder      { 0xff353a70 };
    inline const juce::Colour knobBody         { 0xff262a55 };
    inline const juce::Colour knobBodyDark     { 0xff14162f };
    inline const juce::Colour track            { 0xff30345f };
    inline const juce::Colour text             { 0xffe8eaff };
    inline const juce::Colour textDim          { 0xff8b90c0 };
    inline const juce::Colour grid             { 0xff2a2e58 };

    inline const juce::Colour kick    { 0xffff9a3c };
    inline const juce::Colour bass    { 0xff3fd2ff };
    inline const juce::Colour pattern { 0xffff5fa8 };
    inline const juce::Colour mix     { 0xff8cf5a6 };
    inline const juce::Colour gate    { 0xffb98cff };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline void paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setGradientFill (juce::ColourGradient (backgroundTop, 0.0f, 0.0f,
                                                 backgroundBottom, 0.0f, (float) area.getHeight(), false));
        g.fillRect (area);
    }

    // Logotypen: ritad text med en gradient från kickens orange till mönstrets rosa.
    inline void paintLogo (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& subtitle = {})
    {
        const auto title = juce::String ("MyOwnFruit");
        g.setFont (font (26.0f, true));
        g.setGradientFill (juce::ColourGradient (kick, (float) area.getX(), 0.0f,
                                                 pattern, (float) area.getX() + 160.0f, 0.0f, false));
        g.drawText (title, area, juce::Justification::centredLeft);

        if (subtitle.isNotEmpty())
        {
            const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (font (26.0f, true), title);
            g.setColour (gate);
            g.setFont (font (18.0f, true));
            g.drawText (subtitle, area.withTrimmedLeft (titleWidth + 8), juce::Justification::centredLeft);
        }
    }
}
