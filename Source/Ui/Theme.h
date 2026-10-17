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

    // Instrumenten (upp till 10): kalla färger, skilda från kickens orange, så att raderna i mönstret skiljs åt.
    inline juce::Colour layerColour (int index)
    {
        const juce::Colour colours[] { bass, juce::Colour (0xff8f9bff), juce::Colour (0xff5fffb0), juce::Colour (0xffff7ac8), juce::Colour (0xffc3ff6b),
                                       juce::Colour (0xff6bb8ff), juce::Colour (0xffd49bff), juce::Colour (0xff5ff0e0), juce::Colour (0xffffd66b),
                                       juce::Colour (0xffff8a8a) };
        return colours[juce::jlimit (0, 9, index)];
    }

    // Varje LFO har en egen färg, så att man ser vilken LFO som rör en ratt.
    inline juce::Colour lfoColour (int index)
    {
        const juce::Colour colours[] { lfo, juce::Colour (0xffc6ff5e), juce::Colour (0xffff7ad9) };
        return colours[juce::jlimit (0, 2, index)];
    }

    // Färgen för vilken modulationskälla som helst (LFO, makro, instrumentets envelopes): ringarna på rattarna och handtagen.
    inline juce::Colour sourceColour (int source)
    {
        if (source < 3)
            return lfoColour (source);
        if (source < 7)
        {
            const juce::Colour macros[] { juce::Colour (0xfff2a65a), juce::Colour (0xffe9d35c), juce::Colour (0xfff08a6a), juce::Colour (0xffe08cff) };
            return macros[juce::jlimit (0, 3, source - 3)];
        }
        const auto layer = (source - 7) / 4, kind = (source - 7) % 4;
        const auto base = layerColour (layer);
        return kind == 0 ? base : kind == 1 ? base.brighter (0.35f) : kind == 2 ? base.withMultipliedSaturation (0.45f) : base.darker (0.35f);
    }

    // Marginal runt rattarna där LFO-ringarna ritas.
    inline constexpr float knobRingMargin = 8.0f;

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Texturerna ägs av en delad resurs som varje öppen editor håller i livet, i stället för av statiska bilder: en statisk
    // juce::Image rivs först när DLL:en laddas ur, efter att JUCE stängts ner, och det får värdprogram (FL Studio) att
    // krascha eller hänga sig vid avslut. Editorerna har en `juce::SharedResourcePointer<Theme::TextureCache>`.
    struct TextureCache
    {
        std::unordered_map<int, juce::Image> circular;
    };

    // Koncentriskt borstad ring-struktur ("lathe-turned circular brushed metal"):
    // Skapar autentiska mikroräfflor och dubbla anisotropa spegellober vinkelrätt
    // mot ljusvinkeln, precis som maskinbearbetade aluminiumrattar på studioutrustning.
    inline const juce::Image& circularBrushedTexture (int size)
    {
        juce::SharedResourcePointer<TextureCache> shared;
        auto& cache = shared->circular;
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

    // En panel: mjuk skugga, subtil gradient och fasade kanter (ingen textur).
    inline void paintMetal (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour top, juce::Colour bottom,
                            bool bevel = true)
    {
        // Yttre subtil skugga
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), corner);

        g.setGradientFill (juce::ColourGradient (top, area.getX(), area.getY(), bottom, area.getX(), area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);

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

    // Fönstrets bakgrund: djup skiffer- och marinblå bas med mjukt studioljus i mitten.
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

        // Subtil yttre ram
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRect (area, 1);
    }
}

