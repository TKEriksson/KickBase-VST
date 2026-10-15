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
    inline const juce::Colour effects { 0xffffc857 };
    inline const juce::Colour master  { 0xffff6b6b };
    inline const juce::Colour lfo     { 0xff5ef2d6 };

    // Baslagren: blå, violett och mint, så att raderna i mönstret skiljs åt.
    inline juce::Colour layerColour (int index)
    {
        const juce::Colour colours[] { bass, juce::Colour (0xff8f9bff), juce::Colour (0xff5fffb0) };
        return colours[juce::jlimit (0, 2, index)];
    }

    // Varje LFO har en egen färg, så att man ser vilken LFO som rör en ratt.
    inline juce::Colour lfoColour (int index)
    {
        const juce::Colour colours[] { lfo, juce::Colour (0xffc6ff5e), juce::Colour (0xffff7ad9) };
        return colours[juce::jlimit (0, 2, index)];
    }

    // Marginal runt rattarna där LFO-ringarna ritas (utanför värdebågen, så att de inte
    // är i vägen när man vrider på ratten).
    inline constexpr float knobRingMargin = 8.0f;

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

    // Loggans färger: silver ("KICK") och koppar ("BASE" och ikonen), som borstad metall.
    inline const juce::Colour silverLight  { 0xfff2f2f6 };
    inline const juce::Colour silverDark   { 0xff9c9cae };
    inline const juce::Colour copperLight  { 0xfff3b089 };
    inline const juce::Colour copperDark   { 0xffb05f3a };

    inline juce::Font logoFont() { return font (25.0f, true).withExtraKerningFactor (0.06f); }

    // Ikonen: en pulslinje (kickens svep) som går in i ett B.
    inline void paintLogoIcon (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        const auto mid = y + h * 0.5f;
        const auto stem = x + w * 0.52f;

        juce::Path pulse;
        pulse.startNewSubPath (x, mid);
        pulse.lineTo (x + w * 0.14f, mid);
        pulse.lineTo (x + w * 0.20f, y + h * 0.30f);
        pulse.lineTo (x + w * 0.28f, y + h * 0.78f);
        pulse.lineTo (x + w * 0.36f, y + h * 0.10f);
        pulse.lineTo (x + w * 0.44f, y + h * 0.66f);
        pulse.lineTo (stem, mid);

        juce::Path b;
        b.startNewSubPath (stem, y + h * 0.06f);
        b.lineTo (stem, y + h * 0.94f);
        b.startNewSubPath (stem, y + h * 0.06f);
        b.lineTo (x + w * 0.78f, y + h * 0.06f);
        b.quadraticTo (x + w * 0.96f, y + h * 0.06f, x + w * 0.96f, y + h * 0.27f);
        b.quadraticTo (x + w * 0.96f, mid, x + w * 0.76f, mid);
        b.lineTo (stem, mid);
        b.startNewSubPath (x + w * 0.76f, mid);
        b.quadraticTo (x + w * 1.0f, mid, x + w * 1.0f, y + h * 0.72f);
        b.quadraticTo (x + w * 1.0f, y + h * 0.94f, x + w * 0.78f, y + h * 0.94f);
        b.lineTo (stem, y + h * 0.94f);

        const juce::PathStrokeType stroke (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (copperLight.withAlpha (0.25f));
        g.strokePath (pulse, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (b, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (copperLight, x, y, copperDark, x + w, y + h, false));
        g.strokePath (pulse, stroke);
        g.strokePath (b, stroke);
    }

    // Bredden loggan tar (ikon + text + ev. undertitel), så att knapparna bredvid hamnar rätt.
    inline int logoWidth (const juce::String& subtitle = {})
    {
        auto width = 34 + 10 + juce::GlyphArrangement::getStringWidthInt (logoFont(), "KICKBASE");
        if (subtitle.isNotEmpty())
            width += 8 + juce::GlyphArrangement::getStringWidthInt (font (18.0f, true), subtitle);
        return width;
    }

    // Logotypen: ikonen och "KICK" i silver + "BASE" i koppar.
    inline void paintLogo (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& subtitle = {})
    {
        auto row = area.toFloat();
        paintLogoIcon (g, row.removeFromLeft (34.0f).withSizeKeepingCentre (32.0f, 26.0f));
        row.removeFromLeft (10.0f);

        const auto f = logoFont();
        g.setFont (f);
        const auto kickWidth = (float) juce::GlyphArrangement::getStringWidthInt (f, "KICK");
        const auto baseWidth = (float) juce::GlyphArrangement::getStringWidthInt (f, "BASE");
        const auto top = row.getCentreY() - 12.0f, bottom = row.getCentreY() + 12.0f;

        g.setGradientFill (juce::ColourGradient (silverLight, 0.0f, top, silverDark, 0.0f, bottom, false));
        g.drawText ("KICK", row.removeFromLeft (kickWidth), juce::Justification::centredLeft);
        g.setGradientFill (juce::ColourGradient (copperLight, 0.0f, top, copperDark, 0.0f, bottom, false));
        g.drawText ("BASE", row.removeFromLeft (baseWidth), juce::Justification::centredLeft);

        if (subtitle.isNotEmpty())
        {
            row.removeFromLeft (8.0f);
            g.setColour (gate);
            g.setFont (font (18.0f, true));
            g.drawText (subtitle, row, juce::Justification::centredLeft);
        }
    }
}
