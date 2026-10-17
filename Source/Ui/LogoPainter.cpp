#include "LogoPainter.h"

namespace
{
    const juce::Colour pink { 0xffff3fa8 }, magenta { 0xffd23cff }, blue { 0xff4d7dff }, cyan { 0xff3fe6ff };

    constexpr float domeTop = 3.0f, domeHeight = 52.0f, plateHeight = 4.5f;
    constexpr float padding = 12.0f, iconSize = 44.0f, fontSize = 32.0f;

    // Färgskalan över texten, med stoppen som glider sakta.
    juce::ColourGradient textGradient (juce::Rectangle<float> bounds, double now, float alpha)
    {
        const auto drift = 0.05f * (float) std::sin (now * 0.35);
        juce::ColourGradient gradient (pink.withAlpha (alpha), bounds.getX(), 0.0f, cyan.withAlpha (alpha), bounds.getRight(), 0.0f, false);
        gradient.addColour (juce::jlimit (0.1, 0.9, 0.42 + drift), magenta.withAlpha (alpha));
        gradient.addColour (juce::jlimit (0.1, 0.95, 0.72 + drift), blue.withAlpha (alpha));
        return gradient;
    }
}

LogoPainter::LogoPainter (const juce::String& subtitleIn) : subtitle (subtitleIn)
{
    build();
}

void LogoPainter::build()
{
    const auto font = Theme::font (fontSize, true).withExtraKerningFactor (0.06f);
    const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, "KICKBASE");
    const auto centreY = domeTop + domeHeight * 0.5f + 1.0f;

    iconRect = juce::Rectangle<float> (iconSize, iconSize).withCentre ({ padding + iconSize * 0.5f, centreY });
    const auto textX = iconRect.getRight() + 10.0f;
    const auto domeWidth = textX + textWidth + padding + 4.0f;
    domeRect = { 0.0f, domeTop, domeWidth, domeHeight };
    plateRect = { -5.0f, domeTop + domeHeight - 1.0f, domeWidth + 10.0f, plateHeight };

    dome.clear();
    dome.addRoundedRectangle (domeRect.getX(), domeRect.getY(), domeRect.getWidth(), domeRect.getHeight(), domeWidth * 0.2f, domeHeight * 0.95f, true, true, false, false);

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font, "KICKBASE", textX, centreY + fontSize * 0.36f);
    textPath.clear();
    glyphs.createPath (textPath);
    textBounds = textPath.getBounds();

    // Ikonen: en ring med ett B och en puls inuti.
    ringPath.clear();
    ringPath.addEllipse (iconRect.reduced (2.5f));
    const auto inner = iconRect.reduced (8.5f);
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

    width = (int) std::ceil (domeWidth + 5.0f);
    if (subtitle.isNotEmpty())
        width += 10 + juce::GlyphArrangement::getStringWidthInt (Theme::font (15.0f, true), subtitle);
}

//==============================================================================
// Neonet: ikonen och texten som lysande rör, med glöd runtomkring.
void LogoPainter::paintNeon (juce::Graphics& g, double now, float intensity, bool detailed) const
{
    const auto stroke = [] (float widthPx) { return juce::PathStrokeType (widthPx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
    const auto breathing = intensity * (0.92f + 0.08f * (float) std::sin (now * 1.3));

    // Ikonen: mörk skiva, rosa ring, blått B.
    {
        const auto disc = iconRect.reduced (3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1030), disc.getCentreX(), disc.getY(), juce::Colour (0xff070611), disc.getCentreX(), disc.getBottom(), false));
        g.fillEllipse (disc);

        juce::ColourGradient ring (pink, iconRect.getX(), iconRect.getY(), magenta, iconRect.getRight(), iconRect.getBottom(), false);
        const auto layers = detailed ? std::initializer_list<std::pair<float, float>> { { 9.0f, 0.15f }, { 5.5f, 0.3f }, { 3.4f, 0.65f }, { 2.0f, 1.0f } }
                                     : std::initializer_list<std::pair<float, float>> { { 5.5f, 0.3f }, { 2.0f, 1.0f } };
        for (const auto& [widthPx, alpha] : layers)
        {
            auto gradient = ring;
            gradient.multiplyOpacity (alpha * breathing);
            g.setGradientFill (gradient);
            g.strokePath (ringPath, stroke (widthPx));
        }
        if (detailed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.5f * breathing));
            g.strokePath (ringPath, stroke (0.8f));
        }

        juce::ColourGradient letter (blue, iconRect.getX(), iconRect.getBottom(), cyan, iconRect.getRight(), iconRect.getY(), false);
        const auto letterLayers = detailed ? std::initializer_list<std::pair<float, float>> { { 6.0f, 0.18f }, { 3.6f, 0.4f }, { 1.9f, 1.0f } }
                                           : std::initializer_list<std::pair<float, float>> { { 3.6f, 0.4f }, { 1.9f, 1.0f } };
        for (const auto& [widthPx, alpha] : letterLayers)
        {
            auto gradient = letter;
            gradient.multiplyOpacity (alpha * breathing);
            g.setGradientFill (gradient);
            g.strokePath (bPath, stroke (widthPx));
            g.strokePath (pulsePath, stroke (widthPx));
        }
        if (detailed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f * breathing));
            g.strokePath (bPath, stroke (0.7f));
            g.strokePath (pulsePath, stroke (0.7f));
        }
    }

    // Texten: ett mörkt rör med neon längs kanten, och ett litet djup (en skugga snett nedåt).
    if (detailed)
    {
        g.setColour (juce::Colour (0xff05030c).withAlpha (0.8f));
        g.strokePath (textPath, stroke (3.2f), juce::AffineTransform::translation (0.9f, 1.8f));
    }
    g.setColour (juce::Colour (0xff12081f).withAlpha (0.8f));
    g.fillPath (textPath);
    const auto layers = detailed ? std::initializer_list<std::pair<float, float>> { { 9.0f, 0.14f }, { 5.5f, 0.26f }, { 3.2f, 0.5f }, { 1.9f, 1.0f } }
                                 : std::initializer_list<std::pair<float, float>> { { 5.5f, 0.26f }, { 1.9f, 1.0f } };
    for (const auto& [widthPx, alpha] : layers)
    {
        auto gradient = textGradient (textBounds, now, 1.0f);
        gradient.multiplyOpacity (alpha * breathing);
        g.setGradientFill (gradient);
        g.strokePath (textPath, stroke (widthPx));
    }
    if (detailed)
    {
        // Den vitglödande kärnan i röret.
        g.setColour (juce::Colours::white.withAlpha (0.42f * breathing));
        g.strokePath (textPath, stroke (0.8f));
    }
}

//==============================================================================
// Glaset: en kupol med kant, högdagrar och reflektioner av glöden.
void LogoPainter::paintGlass (juce::Graphics& g, const Sweep& kick, const Sweep& bass) const
{
    const auto d = domeRect;

    // Kupolens insida: svagt färgat glas med ett mörkare djup.
    {
        juce::ColourGradient body (pink.withAlpha (0.10f), d.getX(), 0.0f, cyan.withAlpha (0.10f), d.getRight(), 0.0f, false);
        g.setGradientFill (body);
        g.fillPath (dome);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0.0f, d.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, d.getY() + d.getHeight() * 0.6f, false));
        g.fillPath (dome);
    }

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (dome);

    // Reflektioner av glöden som går genom neonet: ett snett ljusband i glaset som följer efter, och en kant som lyser.
    auto streak = [&] (const Sweep& sweep)
    {
        if (sweep.strength < 0.02f)
            return;
        const auto cx = d.getX() + sweep.position * d.getWidth();
        const auto skew = 14.0f;
        const auto colour = sweep.colour.brighter (0.35f);

        juce::Path band;
        band.startNewSubPath (cx - 9.0f + skew, d.getY());
        band.lineTo (cx + 11.0f + skew, d.getY());
        band.lineTo (cx + 11.0f - skew, d.getBottom());
        band.lineTo (cx - 9.0f - skew, d.getBottom());
        band.closeSubPath();
        juce::ColourGradient gradient (colour.withAlpha (0.0f), cx - 10.0f, 0.0f, colour.withAlpha (0.0f), cx + 12.0f, 0.0f, false);
        gradient.addColour (0.5, colour.withAlpha (juce::jlimit (0.0f, 1.0f, sweep.strength * 0.5f)));
        g.setGradientFill (gradient);
        g.fillPath (band);

        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, sweep.strength * 0.5f)));
        g.drawLine (cx + 1.0f + skew, d.getY(), cx + 1.0f - skew, d.getBottom(), 1.0f);

        // Kanten tänds där bandet passerar.
        juce::ColourGradient rim (colour.withAlpha (0.0f), cx - 36.0f, 0.0f, colour.withAlpha (0.0f), cx + 36.0f, 0.0f, false);
        rim.addColour (0.5, colour.withAlpha (juce::jlimit (0.0f, 1.0f, sweep.strength * 0.95f)));
        g.setGradientFill (rim);
        g.strokePath (dome, juce::PathStrokeType (3.2f));
    };
    streak (kick);
    streak (bass);

    // Den stora högdagern i kupolens överkant (en halvmåne) och en smal vid vänsterkanten.
    {
        juce::Path crescent;
        crescent.setUsingNonZeroWinding (false);
        crescent.addEllipse (d.getX() + 3.0f, d.getY() + 1.5f, d.getWidth() - 6.0f, d.getHeight() * 0.95f);
        crescent.addEllipse (d.getX() + 6.0f, d.getY() + 6.5f, d.getWidth() - 12.0f, d.getHeight() * 0.95f);
        juce::Graphics::ScopedSaveState top (g);
        g.reduceClipRegion (juce::Rectangle<int> ((int) d.getX(), (int) d.getY(), (int) d.getWidth(), (int) (d.getHeight() * 0.42f)));
        juce::ColourGradient gradient (juce::Colours::white.withAlpha (0.42f), d.getX(), 0.0f, juce::Colours::white.withAlpha (0.06f), d.getRight(), 0.0f, false);
        gradient.addColour (0.5, juce::Colours::white.withAlpha (0.2f));
        g.setGradientFill (gradient);
        g.fillPath (crescent);

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0.0f, d.getY() + 8.0f, juce::Colours::white.withAlpha (0.0f), 0.0f, d.getBottom(), false));
        g.fillRoundedRectangle (d.getX() + 4.0f, d.getY() + 9.0f, 2.2f, d.getHeight() - 16.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre ({ d.getX() + 11.0f, d.getY() + 12.0f }));
    }

    // Svag reflektion nere i kanten (ljuset från sockeln) och högerkantens skimmer.
    g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.0f), 0.0f, d.getY() + d.getHeight() * 0.6f, cyan.withAlpha (0.16f), 0.0f, d.getBottom(), false));
    g.fillRect (d);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), d.getRight() - 14.0f, 0.0f, juce::Colours::white.withAlpha (0.12f), d.getRight() - 2.0f, 0.0f, false));
    g.fillRect (juce::Rectangle<float> (d.getRight() - 14.0f, d.getY() + 14.0f, 12.0f, d.getHeight() - 14.0f));
}

//==============================================================================
void LogoPainter::paintReflection (juce::Graphics& g, double now, float scale)
{
    const auto baseY = plateRect.getBottom();
    const auto reflectionHeight = (float) height - baseY;
    const auto reflectionWidth = plateRect.getWidth();
    const auto pixelsW = juce::jmax (8, (int) std::ceil (reflectionWidth * scale)), pixelsH = juce::jmax (4, (int) std::ceil (reflectionHeight * scale));

    if (reflection.isNull() || reflection.getWidth() != pixelsW || reflection.getHeight() != pixelsH)
        reflection = juce::Image (juce::Image::ARGB, pixelsW, pixelsH, true);
    else
        reflection.clear (reflection.getBounds());

    {
        juce::Graphics gi (reflection);
        // Spegelvänd runt sockelns underkant; bildens övre kant är golvlinjen.
        gi.addTransform (juce::AffineTransform::translation (-plateRect.getX(), 0.0f).followedBy (juce::AffineTransform::scale (1.0f, -0.36f)).translated (0.0f, baseY * 0.36f).scaled (scale));
        paintNeon (gi, now, 1.0f, false);
    }

    // Tonar ut nedåt.
    for (int y = 0; y < pixelsH; ++y)
    {
        const auto t = (float) y / (float) pixelsH;
        const auto alpha = 0.62f * std::pow (1.0f - t, 1.4f);
        for (int x = 0; x < pixelsW; ++x)
            reflection.multiplyAlphaAt (x, y, alpha);
    }
    g.drawImage (reflection, juce::Rectangle<float> (plateRect.getX(), baseY, reflectionWidth, reflectionHeight));
}

void LogoPainter::paint (juce::Graphics& g, juce::Point<float> origin, double now, const Sweep& kick, const Sweep& bass)
{
    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::translation (origin));
    const auto scale = juce::jlimit (1.0f, 3.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());

    paintReflection (g, now, scale);

    // Sockeln: blank svart skiva som tar upp färgerna uppifrån.
    {
        juce::Path plate;
        plate.addRoundedRectangle (plateRect.getX(), plateRect.getY(), plateRect.getWidth(), plateRect.getHeight(), 2.2f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d66), 0.0f, plateRect.getY(), juce::Colour (0xff0a0b14), 0.0f, plateRect.getBottom(), false));
        g.fillPath (plate);
        juce::ColourGradient glow (pink.withAlpha (0.55f), plateRect.getX(), 0.0f, cyan.withAlpha (0.55f), plateRect.getRight(), 0.0f, false);
        glow.addColour (0.5, magenta.withAlpha (0.45f));
        g.setGradientFill (glow);
        g.fillRect (juce::Rectangle<float> (plateRect.getX() + 3.0f, plateRect.getY(), plateRect.getWidth() - 6.0f, 1.2f));
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.fillRect (juce::Rectangle<float> (plateRect.getX() + 6.0f, plateRect.getY() + 0.6f, plateRect.getWidth() * 0.4f, 0.8f));
    }

    // Kupolens mörka bakgrund, så att neonet lyser.
    g.setColour (juce::Colour (0xff04030a).withAlpha (0.55f));
    g.fillPath (dome);

    paintNeon (g, now, 1.0f, true);

    // Glöden som följer ljudet längs neonet.
    {
        const auto bounds = glowPath.getBounds();
        auto sweep = [&] (const Sweep& s)
        {
            if (s.strength < 0.01f)
                return;
            const auto centreX = bounds.getX() + s.position * bounds.getWidth();
            const auto half = bounds.getWidth() * 0.24f;
            for (const auto [widthPx, alpha] : { std::pair<float, float> { 11.0f, 0.12f }, { 6.0f, 0.22f }, { 2.4f, 0.5f } })
            {
                juce::ColourGradient gradient (s.colour.withAlpha (0.0f), centreX - half, 0.0f, s.colour.withAlpha (0.0f), centreX + half, 0.0f, false);
                gradient.addColour (0.5, s.colour.withAlpha (juce::jlimit (0.0f, 1.0f, s.strength * alpha * 1.8f)));
                g.setGradientFill (gradient);
                g.strokePath (glowPath, juce::PathStrokeType (widthPx, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            juce::ColourGradient fill (s.colour.withAlpha (0.0f), centreX - half, 0.0f, s.colour.withAlpha (0.0f), centreX + half, 0.0f, false);
            fill.addColour (0.5, s.colour.withAlpha (juce::jlimit (0.0f, 1.0f, s.strength * 0.5f)));
            g.setGradientFill (fill);
            g.fillPath (glowPath);
        };
        sweep (kick);
        sweep (bass);
    }

    paintGlass (g, kick, bass);

    // Kupolens ytterkant ritas sist, över allt.
    {
        juce::ColourGradient rim (pink.withAlpha (0.75f), domeRect.getX(), 0.0f, cyan.withAlpha (0.75f), domeRect.getRight(), 0.0f, false);
        rim.addColour (0.5, magenta.withAlpha (0.55f));
        g.setGradientFill (rim);
        g.strokePath (dome, juce::PathStrokeType (1.5f));
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        juce::Path inner;
        inner.addRoundedRectangle (domeRect.getX() + 2.2f, domeRect.getY() + 2.2f, domeRect.getWidth() - 4.4f, domeRect.getHeight() - 2.2f, domeRect.getWidth() * 0.2f - 2.0f, domeRect.getHeight() * 0.95f - 2.0f, true, true, false, false);
        g.strokePath (inner, juce::PathStrokeType (0.8f));
    }

    if (subtitle.isNotEmpty())
    {
        g.setFont (Theme::font (15.0f, true));
        g.setColour (cyan.withAlpha (0.85f));
        g.drawText (subtitle.toUpperCase(), juce::Rectangle<float> (plateRect.getRight() + 8.0f, domeRect.getY(), (float) width - plateRect.getRight() - 8.0f, domeRect.getHeight()),
                    juce::Justification::centredLeft);
    }
}
