#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>

// Basens rutnät. Kicken går alltid i sextondelar.
enum class BassGrid
{
    sixteenth,        // 16 steg per takt: rullande, galopp
    eighthTriplet,    // 12 steg per takt: K-B-B per slag
    sixteenthTriplet  // 24 steg per takt
};

enum class PatternTemplate { rolling, galloping, triplet, offbeat, clear };

// Stegen i mönstret. Gränssnittet skriver och ljudtråden läser, därför är varje fält atomiskt:
// ingen av dem behöver låsa eller vänta på den andra.
struct Pattern
{
    static constexpr int maxBars = 4;
    static constexpr int kickStepsPerBar = 16;
    static constexpr int maxBassStepsPerBar = 24;
    static constexpr int numGrids = 3;
    static constexpr int maxKickSteps = maxBars * kickStepsPerBar;
    static constexpr int maxBassSteps = maxBars * maxBassStepsPerBar;
    static constexpr int minPitch = -24;
    static constexpr int maxPitch = 24;

    static int bassStepsPerBar (BassGrid grid);
    static double bassStepsPerQuarter (BassGrid grid);
    static juce::StringArray gridNames()     { return { "1/16", "1/8 T", "1/16 T" }; }
    static juce::StringArray templateNames() { return { "Rolling", "Galloping", "Triplet", "Offbeat", "Clear" }; }

    struct BassStep
    {
        std::atomic<bool> on { false };
        std::atomic<int> pitch { 0 };        // halvtoner från tonarten
        std::atomic<bool> slide { false };   // glid till nästa steg (303-stil)
        std::atomic<float> gate { 0.7f };    // andel av steget som tonen hålls
        std::atomic<float> velocity { 0.8f };
    };

    Pattern() { applyTemplate (PatternTemplate::rolling); }

    std::atomic<bool>& kickStep (int index)                { return kick[(size_t) index]; }
    BassStep& bassStep (BassGrid grid, int index)          { return bass[(size_t) grid][(size_t) index]; }
    const BassStep& bassStep (BassGrid grid, int index) const { return bass[(size_t) grid][(size_t) index]; }
    bool isKickOn (int index) const                        { return kick[(size_t) index].load(); }

    // Fyller mönstret enligt en mall. Returnerar det rutnät mallen är skriven för.
    BassGrid applyTemplate (PatternTemplate t);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    static inline const juce::Identifier treeType { "Pattern" };

private:
    std::array<std::atomic<bool>, maxKickSteps> kick {};
    std::array<std::array<BassStep, maxBassSteps>, numGrids> bass;
};
