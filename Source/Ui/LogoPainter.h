#pragma once

#include "Theme.h"

// Loggan: KICKBASE och ikonen som glasrör med neon längs kanten (rosa till blått), med en spegling i golvet under. Glöden som följer ljudet
// (kicken åt höger, instrumenten åt vänster) glider genom glaset: bokstäverna tänds inifrån och ett ljusband speglas i dem.
class LogoPainter
{
public:
    static constexpr int height = 72;

    struct Sweep
    {
        float position = 0.0f; // 0..1 över loggan
        float strength = 0.0f; // 0..1
        juce::Colour colour;
    };

    explicit LogoPainter (const juce::String& subtitle = {});

    int getWidth() const { return width; }

    // `origin` är loggans övre vänstra hörn.
    // `glow` (0..1) är hur mycket neonet lyser: 0 när inget ljud spelas (släckt glas), upp mot 1 med ljudets nivå, så att man "ser" ljuden.
    void paint (juce::Graphics&, juce::Point<float> origin, float glow, const Sweep& kick, const Sweep& bass);

private:
    void build();
    void paintGlassLogo (juce::Graphics&, bool detailed, bool body, bool neon) const;
    void paintReflection (juce::Graphics&, float scale);
    void renderCache (float scale);

    juce::String subtitle;
    int width = 0;

    juce::Rectangle<float> iconRect, textBounds;
    float floorY = 60.0f;
    juce::Path textPath, ringPath, bPath, pulsePath, glowPath;
    juce::Image reflection, cache, neonCache; // cache: det släckta glaset, neonCache: neonet och spegelbilden (läggs ovanpå med glödens styrka)
    float cacheScale = 0.0f;
    int cacheLeft = -8, cacheWidth = 0;
};
