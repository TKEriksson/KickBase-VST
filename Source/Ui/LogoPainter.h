#pragma once

#include "Theme.h"

// Loggan: ett neonskylt (ikonen och KICKBASE) under en glaskupol på en blank sockel, med en spegling i golvet under. Glöden som följer
// ljudet (kicken åt höger, instrumenten åt vänster) glider genom neonet och reflekteras i glaset.
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
    void paint (juce::Graphics&, juce::Point<float> origin, double now, const Sweep& kick, const Sweep& bass);

private:
    void build();
    void paintNeon (juce::Graphics&, double now, float intensity, bool detailed) const;
    void paintGlass (juce::Graphics&, const Sweep& kick, const Sweep& bass) const;
    void paintReflection (juce::Graphics&, double now, float scale);

    juce::String subtitle;
    int width = 0;

    juce::Rectangle<float> domeRect, plateRect, iconRect, textBounds;
    juce::Path dome, textPath, ringPath, bPath, pulsePath, glowPath;
    juce::Image reflection;
    float reflectionScale = 0.0f;
};
