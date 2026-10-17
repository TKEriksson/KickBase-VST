#include "LogoPainter.h"

namespace
{
    const juce::Colour pink { 0xffff3fa8 }, magenta { 0xffd23cff }, blue { 0xff4d7dff }, cyan { 0xff3fe6ff };

    constexpr float iconSize = 52.0f, fontSize = 38.0f, leftPadding = 4.0f;

    // Färgskalan över texten: rosa till blått.
    juce::ColourGradient textGradient (juce::Rectangle<float> bounds, float alpha)
    {
        juce::ColourGradient gradient (pink.withAlpha (alpha), bounds.getX(), 0.0f, cyan.withAlpha (alpha), bounds.getRight(), 0.0f, false);
        gradient.addColour (0.42, magenta.withAlpha (alpha));
        gradient.addColour (0.72, blue.withAlpha (alpha));
        return gradient;
    }

    juce::PathStrokeType stroke (float widthPx) { return juce::PathStrokeType (widthPx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); }
}

LogoPainter::LogoPainter (const juce::String& subtitleIn) : subtitle (subtitleIn)
{
    build();
}

void LogoPainter::build()
{
    const auto font = Theme::font (fontSize, true).withExtraKerningFactor (0.06f);
    const auto centreY = 33.0f;

    iconRect = juce::Rectangle<float> (iconSize, iconSize).withCentre ({ leftPadding + iconSize * 0.5f, centreY });
    const auto textX = iconRect.getRight() + 12.0f;
    floorY = iconRect.getBottom() + 1.0f;

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, "KICKBASE", textX, centreY + fontSize * 0.36f);
    textPath.clear();
    glyphs.createPath (textPath);
    textBounds = textPath.getBounds();

    // Ikonen: en ring med ett B och en puls inuti.
    ringPath.clear();
    ringPath.addEllipse (iconRect.reduced (2.5f));
    const auto inner = iconRect.reduced (10.0f);
    const auto w = inner.getWidth(), h = inner.getHeight(), x = inner.getX(), y = inner.getY(), mid = y + h * 0.5f, stem = x + w * 0.52f;

    pulsePath.clear();
    pulsePath.startNewSubPath (x - w * 0.08f, mid);
    pulsePath.lineTo (x + w * 0.16f, mid);
    pulsePath.lineTo (x + w * 0.24f, y + h * 0.28f);
    pulsePath.lineTo (x + w * 0.33f, y + h * 0.74f);
    pulsePath.lineTo (x + w * 0.42f, y + h * 0.14f);
    pulsePath.lineTo (x + w * 0.48f, y + h * 0.62f);
    pulsePath.lineTo (stem, mid);

    bPath.clear();
    bPath.startNewSubPath (stem, y + h * 0.10f);
    bPath.lineTo (stem, y + h * 0.90f);
    bPath.startNewSubPath (stem, y + h * 0.10f);
    bPath.lineTo (x + w * 0.80f, y + h * 0.10f);
    bPath.lineTo (x + w * 0.96f, y + h * 0.30f);
    bPath.lineTo (x + w * 0.78f, mid);
    bPath.lineTo (stem, mid);
    bPath.startNewSubPath (x + w * 0.78f, mid);
    bPath.lineTo (x + w * 1.02f, y + h * 0.70f);
    bPath.lineTo (x + w * 0.84f, y + h * 0.90f);
    bPath.lineTo (stem, y + h * 0.90f);

    glowPath.clear();
    glowPath.addPath (ringPath);
    glowPath.addPath (textPath);

    width = (int) std::ceil (textBounds.getRight() + 8.0f);
    if (subtitle.isNotEmpty())
        width += 10 + juce::GlyphArrangement::getStringWidthInt (Theme::font (16.0f, true), subtitle);
    cacheLeft = -6;
    cacheWidth = width + 12;
    cache = {};
    neonCache = {};
}

//==============================================================================
// Glasrören: ikonen och texten. Ett mörkt, genomskinligt glas med en ljus skärning överst, en böjd glans, neon längs kanten och en
// liten skuggning och högdager inuti kanten, så att det ser ut som tjockt glas.
void LogoPainter::paintGlassLogo (juce::Graphics& g, bool detailed, bool body, bool neon) const
{
    // --- Ikonen
    if (body)
    {
        const auto disc = iconRect.reduced (3.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff261840), disc.getCentreX(), disc.getY(), juce::Colour (0xff070611), disc.getCentreX(), disc.getBottom(), false));
        g.fillEllipse (disc);
        if (detailed)
        {
            // Glasets glans: en ljus båge överst och en svag reflektion nere.
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addEllipse (disc);
            g.reduceClipRegion (clip);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.36f), 0.0f, disc.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, disc.getCentreY(), false));
            g.fillEllipse (disc.getX() + 3.0f, disc.getY() + 1.5f, disc.getWidth() - 6.0f, disc.getHeight() * 0.62f);
            g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.0f), 0.0f, disc.getCentreY(), cyan.withAlpha (0.18f), 0.0f, disc.getBottom(), false));
            g.fillRect (disc);
        }

        // Släckt: bara en svag kontur, så att formen syns.
        juce::ColourGradient dimRing (pink.withAlpha (0.3f), iconRect.getX(), iconRect.getY(), magenta.withAlpha (0.3f), iconRect.getRight(), iconRect.getBottom(), false);
        g.setGradientFill (dimRing);
        g.strokePath (ringPath, stroke (2.4f));
        g.setColour (blue.withAlpha (0.32f));
        g.strokePath (bPath, stroke (1.8f));
        g.strokePath (pulsePath, stroke (1.8f));
    }

    if (neon)
    {
        juce::ColourGradient ring (pink, iconRect.getX(), iconRect.getY(), magenta, iconRect.getRight(), iconRect.getBottom(), false);
        const auto layers = detailed ? std::initializer_list<std::pair<float, float>> { { 11.0f, 0.18f }, { 7.0f, 0.3f }, { 4.4f, 0.6f }, { 2.6f, 1.0f } }
                                     : std::initializer_list<std::pair<float, float>> { { 7.0f, 0.3f }, { 2.6f, 1.0f } };
        for (const auto& [widthPx, alpha] : layers)
        {
            auto gradient = ring;
            gradient.multiplyOpacity (alpha);
            g.setGradientFill (gradient);
            g.strokePath (ringPath, stroke (widthPx));
        }
        if (detailed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.strokePath (ringPath, stroke (0.9f));
        }

        juce::ColourGradient letter (blue, iconRect.getX(), iconRect.getBottom(), cyan, iconRect.getRight(), iconRect.getY(), false);
        const auto letterLayers = detailed ? std::initializer_list<std::pair<float, float>> { { 7.0f, 0.2f }, { 4.2f, 0.45f }, { 2.2f, 1.0f } }
                                           : std::initializer_list<std::pair<float, float>> { { 4.2f, 0.45f }, { 2.2f, 1.0f } };
        for (const auto& [widthPx, alpha] : letterLayers)
        {
            auto gradient = letter;
            gradient.multiplyOpacity (alpha);
            g.setGradientFill (gradient);
            g.strokePath (bPath, stroke (widthPx));
            g.strokePath (pulsePath, stroke (widthPx));
        }
        if (detailed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.strokePath (bPath, stroke (0.8f));
            g.strokePath (pulsePath, stroke (0.8f));
        }
    }

    // --- Texten
    if (body && detailed)
    {
        g.setColour (juce::Colour (0xff05030c).withAlpha (0.75f));
        g.strokePath (textPath, stroke (3.4f), juce::AffineTransform::translation (0.9f, 2.0f)); // djup: en skugga snett nedåt
    }

    // Glaset: mörkt och genomskinligt, med färgen i botten och en ljus skärning överst.
    if (body)
    {
        g.setColour (juce::Colour (0xff130a24).withAlpha (0.72f));
        g.fillPath (textPath);
        auto tint = textGradient (textBounds, 1.0f);
        tint.multiplyOpacity (detailed ? 0.26f : 0.2f);
        g.setGradientFill (tint);
        g.fillPath (textPath);

        // Släckt: en svag kontur längs kanten.
        auto dim = textGradient (textBounds, 1.0f);
        dim.multiplyOpacity (0.32f);
        g.setGradientFill (dim);
        g.strokePath (textPath, stroke (1.6f));
    }
    if (body && detailed)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (textPath);

        // Skärningen överst och en diagonal glans över bokstäverna.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.42f), 0.0f, textBounds.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, textBounds.getY() + textBounds.getHeight() * 0.55f, false));
        g.fillRect (textBounds);
        const auto sheenX = textBounds.getX() + textBounds.getWidth() * 0.22f;
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), sheenX - 14.0f, 0.0f, juce::Colours::white.withAlpha (0.0f), sheenX + 14.0f, 0.0f, false);
        sheen.addColour (0.5, juce::Colours::white.withAlpha (0.22f));
        g.setGradientFill (sheen);
        juce::Path band;
        band.startNewSubPath (sheenX - 10.0f + 14.0f, textBounds.getY());
        band.lineTo (sheenX + 10.0f + 14.0f, textBounds.getY());
        band.lineTo (sheenX + 10.0f - 14.0f, textBounds.getBottom());
        band.lineTo (sheenX - 10.0f - 14.0f, textBounds.getBottom());
        band.closeSubPath();
        g.fillPath (band);

        // Inuti kanten: en skuggning nere till höger och en högdager uppe till vänster (tjockt glas).
        g.setColour (juce::Colour (0xff02010a).withAlpha (0.55f));
        g.strokePath (textPath, stroke (2.4f), juce::AffineTransform::translation (-0.9f, -1.3f));
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.strokePath (textPath, stroke (1.1f), juce::AffineTransform::translation (1.0f, 1.4f));
    }

    // Neon längs kanten.
    if (! neon)
        return;
    const auto layers = detailed ? std::initializer_list<std::pair<float, float>> { { 11.0f, 0.15f }, { 6.5f, 0.28f }, { 3.8f, 0.55f }, { 2.2f, 1.0f } }
                                 : std::initializer_list<std::pair<float, float>> { { 6.5f, 0.28f }, { 2.2f, 1.0f } };
    for (const auto& [widthPx, alpha] : layers)
    {
        auto gradient = textGradient (textBounds, 1.0f);
        gradient.multiplyOpacity (alpha);
        g.setGradientFill (gradient);
        g.strokePath (textPath, stroke (widthPx));
    }
    if (detailed)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.strokePath (textPath, stroke (0.8f));
    }
}

//==============================================================================
void LogoPainter::paintReflection (juce::Graphics& g, float scale)
{
    const auto reflectionHeight = (float) height - floorY;
    const auto reflectionWidth = (float) cacheWidth;
    const auto pixelsW = juce::jmax (8, (int) std::ceil (reflectionWidth * scale)), pixelsH = juce::jmax (4, (int) std::ceil (reflectionHeight * scale));

    if (reflection.isNull() || reflection.getWidth() != pixelsW || reflection.getHeight() != pixelsH)
        reflection = juce::Image (juce::Image::ARGB, pixelsW, pixelsH, true);
    else
        reflection.clear (reflection.getBounds());

    {
        juce::Graphics gi (reflection);
        // Spegelvänd och hoptryckt runt golvlinjen; bildens övre kant är golvet.
        gi.addTransform (juce::AffineTransform::translation ((float) -cacheLeft, 0.0f).followedBy (juce::AffineTransform::scale (1.0f, -0.36f)).translated (0.0f, floorY * 0.36f).scaled (scale));
        paintGlassLogo (gi, false, false, true);
    }

    // Tonar ut nedåt.
    for (int y = 0; y < pixelsH; ++y)
    {
        const auto t = (float) y / (float) pixelsH;
        const auto alpha = 0.6f * std::pow (1.0f - t, 1.4f);
        for (int x = 0; x < pixelsW; ++x)
            reflection.multiplyAlphaAt (x, y, alpha);
    }
    g.drawImage (reflection, juce::Rectangle<float> ((float) cacheLeft, floorY, reflectionWidth, reflectionHeight));
}

// Det som inte rör sig ritas en gång till två bilder: det släckta glaset (med undertiteln), och neonet med spegelbilden som läggs ovanpå med glödens
// styrka. Varje bild ritas bara bilderna och glöden som följer ljudet.
void LogoPainter::renderCache (float scale)
{
    const auto pixelsW = juce::jmax (8, (int) std::ceil ((float) cacheWidth * scale)), pixelsH = juce::jmax (8, (int) std::ceil ((float) height * scale));
    cacheScale = scale;

    cache = juce::Image (juce::Image::ARGB, pixelsW, pixelsH, true);
    {
        juce::Graphics g (cache);
        g.addTransform (juce::AffineTransform::translation ((float) -cacheLeft, 0.0f).scaled (scale));
        paintGlassLogo (g, true, true, false);
        if (subtitle.isNotEmpty())
        {
            g.setFont (Theme::font (16.0f, true));
            g.setColour (cyan.withAlpha (0.9f));
            g.drawText (subtitle.toUpperCase(), juce::Rectangle<float> (textBounds.getRight() + 10.0f, textBounds.getY(), (float) width - textBounds.getRight() - 10.0f, textBounds.getHeight()),
                        juce::Justification::centredLeft);
        }
    }

    neonCache = juce::Image (juce::Image::ARGB, pixelsW, pixelsH, true);
    {
        juce::Graphics g (neonCache);
        g.addTransform (juce::AffineTransform::translation ((float) -cacheLeft, 0.0f).scaled (scale));
        paintReflection (g, scale);
        paintGlassLogo (g, true, false, true);
    }
}

void LogoPainter::paint (juce::Graphics& g, juce::Point<float> origin, float glow, const Sweep& kick, const Sweep& bass)
{
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::translation (origin));
    const auto scale = juce::jlimit (1.0f, 3.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
    if (cache.isNull() || neonCache.isNull() || std::abs (cacheScale - scale) > 0.01f)
        renderCache (scale);
    const juce::Rectangle<float> imageBounds ((float) cacheLeft, 0.0f, (float) cacheWidth, (float) height);
    g.drawImage (cache, imageBounds);
    if (glow > 0.01f)
    {
        g.setOpacity (juce::jlimit (0.0f, 1.0f, glow));
        g.drawImage (neonCache, imageBounds);
    }

    // Glöden som följer ljudet: glasbokstäverna tänds inifrån, neonet lyser starkare, och ett ljusband speglas i glaset.
    const auto bounds = glowPath.getBounds();
    auto sweep = [&] (const Sweep& s)
    {
        if (s.strength < 0.01f)
            return;
        const auto centreX = bounds.getX() + s.position * bounds.getWidth();
        const auto half = bounds.getWidth() * 0.26f;
        const auto strength = juce::jlimit (0.0f, 1.0f, s.strength * 1.35f);

        // Inifrån: färgen fyller glaset.
        juce::ColourGradient fill (s.colour.withAlpha (0.0f), centreX - half, 0.0f, s.colour.withAlpha (0.0f), centreX + half, 0.0f, false);
        fill.addColour (0.5, s.colour.withAlpha (juce::jlimit (0.0f, 1.0f, strength * 0.8f)));
        g.setGradientFill (fill);
        g.fillPath (glowPath);

        // Neonet och glöden runt.
        for (const auto [widthPx, alpha] : { std::pair<float, float> { 16.0f, 0.16f }, { 9.0f, 0.3f }, { 4.0f, 0.65f }, { 2.0f, 1.0f } })
        {
            juce::ColourGradient gradient (s.colour.withAlpha (0.0f), centreX - half, 0.0f, s.colour.withAlpha (0.0f), centreX + half, 0.0f, false);
            gradient.addColour (0.5, s.colour.brighter (0.3f).withAlpha (juce::jlimit (0.0f, 1.0f, strength * alpha * 1.6f)));
            g.setGradientFill (gradient);
            g.strokePath (glowPath, stroke (widthPx));
        }

        // Reflektionen: ett snett vitt ljusband som bara syns i glaset (inuti bokstäverna och ikonen).
        juce::Graphics::ScopedSaveState clipState (g);
        g.reduceClipRegion (glowPath);
        const auto skew = 10.0f;
        juce::Path band;
        band.startNewSubPath (centreX - 7.0f + skew, bounds.getY() - 2.0f);
        band.lineTo (centreX + 9.0f + skew, bounds.getY() - 2.0f);
        band.lineTo (centreX + 9.0f - skew, bounds.getBottom() + 2.0f);
        band.lineTo (centreX - 7.0f - skew, bounds.getBottom() + 2.0f);
        band.closeSubPath();
        juce::ColourGradient shine (juce::Colours::white.withAlpha (0.0f), centreX - 8.0f, 0.0f, juce::Colours::white.withAlpha (0.0f), centreX + 10.0f, 0.0f, false);
        shine.addColour (0.5, juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, strength * 0.85f)));
        g.setGradientFill (shine);
        g.fillPath (band);
    };
    sweep (kick);
    sweep (bass);
}
