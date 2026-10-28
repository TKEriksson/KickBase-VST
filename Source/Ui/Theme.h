#pragma once

#include "../Core/Settings.h"
#include "../Core/Modulation.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>
#include <vector>
#include <cmath>

// Färger och mått för gränssnittet. Mörk slate/charcoal hårdvarubas med modulspeficika accenter.
namespace Theme
{
    // E1 (skissen): bakgrund #0b0d14, paneler #0e111a/#121622, kantlinjer #232a3b/#1e2636, cyan huvudaccent, fuchsia andra accent.
    inline const juce::Colour backgroundTop    { 0xff0d1018 };
    inline const juce::Colour backgroundBottom { 0xff0b0d14 };
    inline const juce::Colour panel            { 0xff0e111a };
    inline const juce::Colour panelAlt         { 0xff121622 };
    inline const juce::Colour panelBorder      { 0xff232a3b };
    inline const juce::Colour panelBorderDim   { 0xff1e2636 };
    inline const juce::Colour insetStrip       { 0xff0d1017 };
    inline const juce::Colour chartBackground  { 0xff07090e };
    inline const juce::Colour cyan             { 0xff22d3ee };
    inline const juce::Colour cyanLight        { 0xff67e8f9 };
    inline const juce::Colour fuchsia          { 0xffe879f9 };
    inline const juce::Colour slate400         { 0xff94a3b8 };
    inline const juce::Colour slate500         { 0xff64748b };
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
    // Egna färger per instrument (0 = standard). Fylls av editorn från projektets state (PluginBase::getLayerColour).
    inline std::array<juce::uint32, 10>& layerColourOverrides()
    {
        static std::array<juce::uint32, 10> overrides {};
        return overrides;
    }

    inline juce::Colour layerColour (int index)
    {
        if (const auto custom = layerColourOverrides()[(size_t) juce::jlimit (0, 9, index)]; custom != 0)
            return juce::Colour (custom);
        const juce::Colour colours[] { bass, juce::Colour (0xff8f9bff), juce::Colour (0xff5fffb0), juce::Colour (0xffff7ac8), juce::Colour (0xffc3ff6b),
                                       juce::Colour (0xff6bb8ff), juce::Colour (0xffd49bff), juce::Colour (0xff5ff0e0), juce::Colour (0xffffd66b),
                                       juce::Colour (0xffff8a8a) };
        return colours[juce::jlimit (0, 9, index)];
    }

    // Varje LFO har en egen färg, så att man ser vilken LFO som rör en ratt.
    inline juce::Colour lfoColour (int index)
    {
        const juce::Colour colours[] { lfo, juce::Colour (0xffc6ff5e), juce::Colour (0xffff7ad9), juce::Colour (0xff5eb8ff), juce::Colour (0xffffb05e),
                                       juce::Colour (0xffa98cff), juce::Colour (0xff5effa0), juce::Colour (0xffff6464), juce::Colour (0xfffff05e), juce::Colour (0xff5ee0ff) };
        return colours[juce::jlimit (0, 9, index)];
    }

    // Färgen för vilken modulationskälla som helst (LFO, makro, instrumentets envelopes): ringarna på rattarna och handtagen.
    inline juce::Colour sourceColour (int source)
    {
        if (Modulation::isLfoSource (source)) // LFO 1-3 (källa 0-2) och LFO 4-10 (källa 47-53)
            return lfoColour (Modulation::lfoIndexOfSource (source));
        if (source < 7)
        {
            const juce::Colour macros[] { juce::Colour (0xfff2a65a), juce::Colour (0xffe9d35c), juce::Colour (0xfff08a6a), juce::Colour (0xffe08cff) };
            return macros[juce::jlimit (0, 3, source - 3)];
        }
        if (Modulation::isCodeSource (source))
            return lfoColour (source - Modulation::firstCodeSource).withMultipliedSaturation (0.75f);
        const auto layer = Modulation::layerOfSource (source), kind = Modulation::kindOfSource (source);
        const auto base = layerColour (layer);
        if (kind >= Modulation::instrumentSources) // AMP ENV ljus (som ENV 1:s handtag), FILT ENV mellan lagrets färg och ljus
            return kind == Modulation::instrumentSources ? text : base.interpolatedWith (text, 0.5f);
        return kind == 0 ? base : kind == 1 ? base.brighter (0.35f) : kind == 2 ? base.withMultipliedSaturation (0.45f) : base.darker (0.35f);
    }

    // Marginal runt rattarna där LFO-ringarna ritas.
    inline constexpr float knobRingMargin = 8.0f;

    // Gemensam avståndsskala (px). Panelinnehåll, rader, rattgrupper och rutor ska använda dessa, så att luften kan justeras på ett ställe.
    namespace Space
    {
        inline constexpr int xs = 3;        // tätt: mellan etikett och ratt
        inline constexpr int sm = 6;        // mellan knappar/rader i samma grupp
        inline constexpr int md = 10;       // mellan rattgrupper och rutor
        inline constexpr int lg = 14;       // mellan paneler/avdelningar
        inline constexpr int panelPad = 14; // inre marginal i en panel
        inline constexpr int rowGap = 8;    // mellan menyrader (nivå 1-3)
    }

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Monospace för avläsningar och brödsmulor (skissens font-mono).
    inline juce::Font monoFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Text med bokstavsavstånd (skissens tracking-wider) och versaler, centrerad eller vänsterställd i `area`.
    inline void drawTracked (juce::Graphics& g, const juce::String& textIn, juce::Rectangle<float> area, juce::Font f,
                             float tracking, juce::Justification just)
    {
        const auto s = textIn.toUpperCase();
        f.setExtraKerningFactor (tracking);
        g.setFont (f);
        g.drawText (s, area, just, false);
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

    // Gemensam kantglöd (å): tre tunna inre linjer med fallande styrka, inget blur. Billigt nog att ritas varje gång.
    // Används av alla paneler, kort och avdelare så att glöden ser likadan ut överallt. Inåt, så att grannar inte klipper den.
    inline void paintEdgeGlow (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour colour = juce::Colour (0xff38bdf8),
                               float strength = 1.0f)
    {
        static constexpr float alphas[3] = { 0.24f, 0.13f, 0.06f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (colour.withAlpha (alphas[i] * strength));
            g.drawRoundedRectangle (area.reduced (1.5f + (float) i * 1.5f), juce::jmax (1.0f, corner - (float) i), 1.5f);
        }
    }

    // Glödande avdelare: tunn linje med svag glöd på båda sidor.
    inline void paintDividerGlow (juce::Graphics& g, juce::Rectangle<float> line, bool vertical,
                                  juce::Colour colour = juce::Colour (0xff38bdf8))
    {
        const auto grow = [&] (float d) { return vertical ? line.expanded (d, 0.0f) : line.expanded (0.0f, d); };
        g.setColour (colour.withAlpha (0.07f));
        g.fillRect (grow (4.0f));
        g.setColour (colour.withAlpha (0.14f));
        g.fillRect (grow (2.0f));
        g.setColour (colour.withAlpha (0.55f));
        g.fillRect (line);
    }

    // Glasknapp (ä): mörkt glas med ljusreflex och tunn kant; tänd får färgad fyllning och glöd. Gemensam för PREVIEWS-knapparna m.fl.
    inline void paintGlassButton (juce::Graphics& g, juce::Rectangle<float> area, bool lit, bool hover, juce::Colour accent, float corner = 8.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (hover ? 0xff1d2233 : 0xff161a28), 0.0f, area.getY(),
                                                 juce::Colour (0xff0d1019), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);
        if (lit)
        {
            g.setColour (accent.withAlpha (0.30f));
            g.fillRoundedRectangle (area, corner);
        }
        juce::Path shine;
        shine.addRoundedRectangle (area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() * 0.45f,
                                   corner - 1.0f, corner - 1.0f, true, true, false, false);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), 0.0f, area.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, area.getY() + area.getHeight() * 0.45f, false));
        g.fillPath (shine);
        g.setColour (lit ? accent : (hover ? cyan.withAlpha (0.5f) : panelBorder));
        g.drawRoundedRectangle (area.reduced (0.5f), corner, lit ? 1.4f : 1.0f);
        if (lit)
            paintEdgeGlow (g, area, corner, accent, 1.4f);
    }

    // En panel: mjuk skugga, subtil gradient och fasade kanter (ingen textur).
    inline void paintMetal (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour top, juce::Colour bottom,
                            bool bevel = true)
    {
        juce::ignoreUnused (top, bottom);
        // E1: platt mörk panel (skissens gradient #161b26 -> #10131d) med en tunn kantlinje, ingen metallfasning.
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff161b26), area.getX(), area.getY(),
                                                 juce::Colour (0xff10131d), area.getX(), area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);

        if (bevel)
        {
            g.setColour (panelBorder);
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
            paintEdgeGlow (g, area, corner);
        }
    }

    // Diagramyta (vågform, EQ, envelope): #07090e med prickrutnät (cyan, 1 px var 12:e px, opacitet 0.10) och tunn kant.
    inline void paintChartBackground (juce::Graphics& g, juce::Rectangle<float> area, float corner = 8.0f)
    {
        g.setColour (chartBackground);
        g.fillRoundedRectangle (area, corner);
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (area, corner);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.10f));
            for (float y = area.getY() + 6.0f; y < area.getBottom(); y += 12.0f)
                for (float x = area.getX() + 6.0f; x < area.getRight(); x += 12.0f)
                    g.fillRect (x, y, 1.0f, 1.0f);
        }
        g.setColour (panelBorderDim);
        g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
    }

    // Glödande linje för kurvor: bred svag glöd under en tunn klar linje.
    inline void strokeGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width = 1.6f)
    {
        g.setColour (colour.withAlpha (0.18f));
        g.strokePath (path, juce::PathStrokeType (width + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (0.35f));
        g.strokePath (path, juce::PathStrokeType (width + 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Glasdisplay: mörk glasruta med ljusreflex i övre halvan och tunn kant. Texten ritas med drawGlowText.
    inline void paintGlassDisplay (juce::Graphics& g, juce::Rectangle<float> area, float corner = 7.0f)
    {
        g.setColour (chartBackground);
        g.fillRoundedRectangle (area, corner);
        juce::Path shine;
        shine.addRoundedRectangle (area.getX() + 1.0f, area.getY() + 1.0f, area.getWidth() - 2.0f, area.getHeight() * 0.5f,
                                   corner - 1.0f, corner - 1.0f, true, true, false, false);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0.0f, area.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0.0f, area.getCentreY(), false));
        g.fillPath (shine);
        g.setColour (cyan.withAlpha (0.25f));
        g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
    }

    // Text med mjuk glöd: några svaga kopior runt själva texten, sedan den klara texten överst.
    inline void drawGlowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, const juce::Font& font,
                              float tracking, juce::Justification just, juce::Colour colour)
    {
        static constexpr float offsets[4][2] = { { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } };
        g.setColour (colour.withAlpha (0.22f));
        for (const auto& o : offsets)
            drawTracked (g, text, area.translated (o[0], o[1]), font, tracking, just);
        g.setColour (colour);
        drawTracked (g, text, area, font, tracking, just);
    }

    // Fönstrets bakgrund: djup skiffer- och marinblå bas med mjukt studioljus i mitten.
    inline void paintBackground (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const auto a = area.toFloat();
        g.setGradientFill (juce::ColourGradient (backgroundTop, a.getX(), a.getY(),
                                                 backgroundBottom, a.getRight(), a.getBottom(), false));
        g.fillRect (area);

        // Subtil yttre ram
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRect (area, 1);
    }
}
