#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>
#include <vector>
#include <cmath>

// Färger och mått för gränssnittet. Mörk slate/charcoal hårdvarubas med modulspeficika accenter.
namespace Theme
{
    inline const juce::Colour backgroundTop    { 0xff1e2133 };
    inline const juce::Colour backgroundBottom { 0xff0e1018 };
    inline const juce::Colour panel            { 0xff161826 };
    inline const juce::Colour panelBorder      { 0xff282c42 };
    inline const juce::Colour knobBody         { 0xff2a2e45 };
    inline const juce::Colour knobBodyDark     { 0xff121420 };
    inline const juce::Colour track            { 0xff222538 };
    inline const juce::Colour text             { 0xffedf0ff };
    inline const juce::Colour textDim          { 0xff868cae };
    inline const juce::Colour grid             { 0xff23273e };

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

    // Marginal runt rattarna där LFO-ringarna ritas.
    inline constexpr float knobRingMargin = 8.0f;

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Högkvalitativ borstad satin-metall för chassit: mycket fina, jämna vågräta mikrostreck
    // utan grova repor, sömlöst upprepningsbar.
    inline const juce::Image& brushedTexture()
    {
        static const juce::Image image = []
        {
            constexpr int size = 512;
            juce::Image texture (juce::Image::ARGB, size, size, true);
            juce::Graphics g (texture);
            juce::Random random (4711);
            for (int y = 0; y < size; ++y)
            {
                for (int streak = 0; streak < 10; ++streak)
                {
                    const auto x = random.nextInt (size);
                    const auto length = 40 + random.nextInt (220);
                    const auto light = random.nextBool();
                    g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.006f + random.nextFloat() * 0.012f));
                    g.fillRect (x, y, length, 1);
                    if (x + length > size)
                        g.fillRect (0, y, x + length - size, 1);
                }
            }
            return texture;
        }();
        return image;
    }

    // Koncentriskt borstad ring-struktur ("lathe-turned circular brushed metal"):
    // Skapar autentiska mikroräfflor och dubbla anisotropa spegellober vinkelrätt
    // mot ljusvinkeln, precis som maskinbearbetade aluminiumrattar på studioutrustning.
    inline const juce::Image& circularBrushedTexture (int size)
    {
        static std::unordered_map<int, juce::Image> cache;
        auto it = cache.find (size);
        if (it != cache.end())
            return it->second;

        juce::Image img (juce::Image::ARGB, size, size, true);
        const float radius = (float) size * 0.5f;
        const float cx = radius, cy = radius;

        juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
        juce::Random rng (4711 + size);

        std::vector<float> radialGrooves ((size_t) size + 2);
        for (size_t i = 0; i < radialGrooves.size(); ++i)
        {
            const float r = (float) i;
            radialGrooves[i] = std::sin (r * 1.7f) * 0.045f
                             + std::sin (r * 3.9f) * 0.030f
                             + std::sin (r * 7.7f) * 0.020f
                             + (rng.nextFloat() - 0.5f) * 0.035f;
        }

        const float lightAngle = -juce::MathConstants<float>::pi * 0.25f; // ljus uppe till vänster (~ -45°)

        for (int y = 0; y < size; ++y)
        {
            auto* row = (juce::uint32*) data.getLinePointer (y);
            for (int x = 0; x < size; ++x)
            {
                const float dx = (float) x - cx;
                const float dy = (float) y - cy;
                const float dist = std::sqrt (dx * dx + dy * dy);

                if (dist > radius)
                {
                    row[x] = 0;
                    continue;
                }

                const float normDist = dist / radius;
                const float angle = std::atan2 (dy, dx);
                const float angleToLight = angle - lightAngle;

                // 1. Riktat diffust ljus
                float diffuse = std::cos (angleToLight) * 0.14f;

                // 2. Anisotrop spegelglans: två motstående lober vinkelrätt mot spåren
                float aniso = std::abs (std::sin (angle - lightAngle));
                aniso = std::pow (aniso, 2.0f) * 0.32f;

                // 3. Koncentriska svarvräfflor ("ring-struktur")
                const int rIndex = juce::jlimit (0, (int) radialGrooves.size() - 1, (int) dist);
                const float groove = radialGrooves[(size_t) rIndex];

                // 4. Yttre fasning (bevel)
                if (dist > radius - 2.5f)
                {
                    const float t = (radius - dist) / 2.5f;
                    const float rimSpec = std::cos (angle - lightAngle);
                    diffuse += rimSpec * 0.40f * (1.0f - t);
                }

                // 5. Mjuk mitt
                const float centerFade = juce::jlimit (0.0f, 1.0f, dist / 4.0f);

                const float baseLum = 0.44f - normDist * 0.08f;
                const float lum = juce::jlimit (0.0f, 1.0f, (baseLum + diffuse + aniso + groove) * (0.85f + 0.15f * centerFade));

                // Sval maskinstål-ton (#5c627d -> #232637)
                const float rVal = juce::jlimit (0.0f, 1.0f, lum * 0.95f);
                const float gVal = juce::jlimit (0.0f, 1.0f, lum * 0.98f);
                const float bVal = juce::jlimit (0.0f, 1.0f, lum * 1.08f);

                const float aa = juce::jlimit (0.0f, 1.0f, (radius - dist) * 1.8f);
                const auto c = juce::Colour::fromFloatRGBA (rVal, gVal, bVal, aa);
                row[x] = c.getARGB();
            }
        }

        cache[size] = img;
        return cache[size];
    }

    // Borstad metall i en yta: skulpterad metallbädd med mjuk skugga, subtil gradient och fasade kanter.
    inline void paintMetal (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour top, juce::Colour bottom,
                            bool bevel = true)
    {
        // Yttre subtil skugga
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), corner);

        g.setGradientFill (juce::ColourGradient (top, area.getX(), area.getY(), bottom, area.getX(), area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (area, corner);
            g.reduceClipRegion (clip);
            g.setTiledImageFill (brushedTexture(), 0, 0, 1.0f);
            g.fillRect (area);
        }

        if (bevel)
        {
            // Ljus överkant (specular highlight)
            g.setColour (juce::Colours::white.withAlpha (0.20f));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);

            // Mörk nedsänkt underkant
            juce::Path edge;
            edge.addRoundedRectangle (area.reduced (0.5f), corner);
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (area.getSmallestIntegerContainer().withTop ((int) (area.getBottom() - corner - 2.0f)));
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.strokePath (edge, juce::PathStrokeType (1.2f));
        }
    }

    // Fönstrets bakgrund: djup skiffer- och marinblå bas med mjukt studioljus i mitten och borstad textur.
    inline void paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const auto a = area.toFloat();
        g.setGradientFill (juce::ColourGradient (backgroundTop, a.getX(), a.getY(),
                                                 backgroundBottom, a.getRight(), a.getBottom(), false));
        g.fillRect (area);

        // Mjukt omgivande ljussken uppe i mitten som på professionella rackpaneler
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.045f), a.getCentreX(), a.getY() + 40.0f,
                                                 juce::Colours::transparentWhite, a.getCentreX(), a.getY() + a.getHeight() * 0.65f, true));
        g.fillRect (area);

        g.setTiledImageFill (brushedTexture(), 0, 0, 1.0f);
        g.fillRect (area);

        // Subtil yttre ram
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRect (area, 1);
    }

    // Loggans färger: silver ("KICK") och koppar ("BASE" och ikonen), som borstad metall.
    inline const juce::Colour silverLight  { 0xfff5f6fa };
    inline const juce::Colour silverDark   { 0xff9a9cb0 };
    inline const juce::Colour copperLight  { 0xfff7b78f };
    inline const juce::Colour copperDark   { 0xffa85532 };

    inline juce::Font logoFont() { return font (24.0f, true).withExtraKerningFactor (0.07f); }

    // Ikonen: cirkulär metall-badge med ring-struktur, puls och skarpt 3D-B i koppar.
    inline void paintLogoIcon (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const auto size = std::min (r.getWidth(), r.getHeight());
        const auto badge = juce::Rectangle<float> (size, size).withCentre (r.getCentre());
        const auto centre = badge.getCentre();

        // 1. Cirkulär badge med ring-struktur
        const int badgePx = juce::jmax (16, (int) std::round (size));
        g.drawImage (circularBrushedTexture (badgePx), badge);

        // Skarp fasad metallring runt brickan
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (badge.reduced (0.5f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (badge.reduced (2.0f), 1.0f);

        // 2. Mörk nedsänkt displaykärna
        const auto inner = badge.reduced (3.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xee10121d), centre.x, inner.getY(),
                                                 juce::Colour (0xee181b2a), centre.x, inner.getBottom(), false));
        g.fillEllipse (inner);

        // 3. Kick-pulsen som går in i ett skarpt, facetterat B (från Bild 2)
        const auto w = inner.getWidth();
        const auto h = inner.getHeight();
        const auto x = inner.getX();
        const auto y = inner.getY();
        const auto mid = y + h * 0.5f;
        const auto stem = x + w * 0.50f;

        juce::Path pulse;
        pulse.startNewSubPath (x + w * 0.08f, mid);
        pulse.lineTo (x + w * 0.20f, mid);
        pulse.lineTo (x + w * 0.26f, y + h * 0.32f);
        pulse.lineTo (x + w * 0.33f, y + h * 0.74f);
        pulse.lineTo (x + w * 0.40f, y + h * 0.16f);
        pulse.lineTo (x + w * 0.46f, y + h * 0.62f);
        pulse.lineTo (stem, mid);

        juce::Path b;
        // Vertikal stam
        b.startNewSubPath (stem, y + h * 0.15f);
        b.lineTo (stem, y + h * 0.85f);
        // Övre vinge med skarp facett
        b.startNewSubPath (stem, y + h * 0.15f);
        b.lineTo (x + w * 0.78f, y + h * 0.15f);
        b.lineTo (x + w * 0.90f, y + h * 0.32f);
        b.lineTo (x + w * 0.74f, mid);
        b.lineTo (stem, mid);
        // Nedre vinge med skarp facett
        b.startNewSubPath (x + w * 0.74f, mid);
        b.lineTo (x + w * 0.94f, y + h * 0.68f);
        b.lineTo (x + w * 0.80f, y + h * 0.85f);
        b.lineTo (stem, y + h * 0.85f);

        // Varm kopparglöd under linjerna
        g.setColour (copperLight.withAlpha (0.35f));
        g.strokePath (pulse, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath (b, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3D-kopparstråk med metallgradient
        g.setGradientFill (juce::ColourGradient (copperLight, x, y, copperDark, x + w, y + h, false));
        const juce::PathStrokeType stroke (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.strokePath (pulse, stroke);
        g.strokePath (b, stroke);

        // Skarp ljusreflex i överkanten
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre ({ x + w * 0.40f, y + h * 0.16f }));
    }

    // Bredden loggan tar (ikon + text + ev. undertitel).
    inline int logoWidth (const juce::String& subtitle = {})
    {
        auto width = 36 + 12 + juce::GlyphArrangement::getStringWidthInt (logoFont(), "KICKBASE");
        if (subtitle.isNotEmpty())
            width += 10 + juce::GlyphArrangement::getStringWidthInt (font (15.0f, true), subtitle);
        return width;
    }

    // Logotypen: cirkulär ring-badge och "KICK" i 3D-silver + "BASE" i 3D-koppar.
    inline void paintLogo (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& subtitle = {})
    {
        auto row = area.toFloat();
        paintLogoIcon (g, row.removeFromLeft (36.0f).withSizeKeepingCentre (34.0f, 34.0f));
        row.removeFromLeft (10.0f);

        const auto f = logoFont();
        g.setFont (f);
        const auto kickWidth = (float) juce::GlyphArrangement::getStringWidthInt (f, "KICK");
        const auto baseWidth = (float) juce::GlyphArrangement::getStringWidthInt (f, "BASE");
        const auto top = row.getCentreY() - 13.0f, bottom = row.getCentreY() + 13.0f;

        // Subtil skugga under texten för 3D-lyft
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawText ("KICK", row.removeFromLeft (kickWidth).translated (0.0f, 1.5f), juce::Justification::centredLeft);
        g.drawText ("BASE", row.removeFromLeft (baseWidth).translated (0.0f, 1.5f), juce::Justification::centredLeft);
        row.setLeft (row.getX() - (kickWidth + baseWidth)); // återställ för framsidan

        // KICK i 3D-borstat silver
        g.setGradientFill (juce::ColourGradient (silverLight, 0.0f, top, silverDark, 0.0f, bottom, false));
        g.drawText ("KICK", row.removeFromLeft (kickWidth), juce::Justification::centredLeft);

        // BASE i 3D-borstad koppar med varm glöd
        g.setGradientFill (juce::ColourGradient (copperLight, 0.0f, top, copperDark, 0.0f, bottom, false));
        g.drawText ("BASE", row.removeFromLeft (baseWidth), juce::Justification::centredLeft);

        if (subtitle.isNotEmpty())
        {
            row.removeFromLeft (10.0f);
            g.setColour (copperLight.withAlpha (0.75f));
            g.setFont (font (11.0f, true).withExtraKerningFactor (0.12f));
            g.drawText (subtitle.toUpperCase(), row, juce::Justification::centredLeft);
        }
    }
}

