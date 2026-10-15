#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

// Duckkurvan: hur mycket basen trycks undan över tid efter varje kick.
//
// Användaren redigerar punkter (x = tid 0..1 av kurvans längd, y = hur mycket som duckas
// 0..1, tension = segmentets böjning). Punkterna ägs av message thread; ljudtråden läser
// en uppslagstabell som byggs om när punkterna ändras.
class SidechainCurve
{
public:
    struct Point
    {
        float x = 0.0f, y = 0.0f;
        float tension = 0.0f; // böjning för segmentet fram till nästa punkt, -1..1
    };

    static constexpr int maxPoints = 16;
    static constexpr int tableSize = 512;

    SidechainCurve() { reset(); }

    // Standardkurva: full duck direkt, sedan snabb återhämtning som planar ut.
    void reset();

    // Bara message thread.
    const std::vector<Point>& getPoints() const { return points; }
    void setPoints (std::vector<Point> newPoints);
    float evaluate (float x) const; // från punkterna (för gränssnittet)

    // Ljudtråden: värdet 0..1 vid position `phase` 0..1 (tabell med interpolation).
    float lookup (float phase) const;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);
    static inline const juce::Identifier treeType { "SidechainCurve" };

    std::atomic<int> version { 0 }; // ökas vid varje ändring, så att gränssnittet ritar om

    static float shape (float t, float tension);

private:
    void rebuildTable();

    std::vector<Point> points;
    std::array<std::atomic<float>, tableSize> table {};
};

// Spelar duckkurvan från början vid varje kick.
class DuckCurvePlayer
{
public:
    void trigger() { position = 0.0; playing = true; }

    // `lengthSamples`: kurvans längd. Returnerar hur mycket som duckas, 0..1.
    float next (const SidechainCurve& curve, double lengthSamples)
    {
        if (! playing)
            return 0.0f;

        const auto phase = (float) (position / std::max (1.0, lengthSamples));
        if (phase >= 1.0f)
        {
            playing = false;
            return curve.lookup (1.0f);
        }

        position += 1.0;
        return curve.lookup (phase);
    }

private:
    double position = 0.0;
    bool playing = false;
};

namespace Sidechain
{
    enum class Mode { kickEnvelope, curve };
    enum class Position { afterEffects, beforeEffects };

    juce::StringArray lengthNames();                      // "Kick", "1/16", ...
    double lengthInQuarters (int index);                   // 0 för "Kick" (kickens egen längd)
    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
}
