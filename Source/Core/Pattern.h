#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>

// Basens rutnät: upplösningen i stegsekvenseraren och förvalda "snap" i pianorullen.
// Själva tonerna ligger fritt i tid (se Pattern::Note), så ett lager kan ha 16-delar, 8-delar
// och 64-delar i samma mönster. Ordningen är den som visas i listan; nya rutnät läggs sist.
enum class BassGrid
{
    quarter,            //  4 steg per takt
    eighth,             //  8 steg per takt
    eighthTriplet,      // 12 steg per takt: K-B-B per slag
    sixteenth,          // 16 steg per takt: rullande, galopp
    sixteenthTriplet,   // 24 steg per takt
    thirtySecond,       // 32 steg per takt
    thirtySecondTriplet, // 48 steg per takt
    sixtyFourth         // 64 steg per takt
};

enum class PatternTemplate { rolling, galloping, triplet, offbeat, clear };

// Stegen i mönstret. Gränssnittet skriver och ljudtråden läser, därför är varje fält atomiskt:
// ingen av dem behöver låsa eller vänta på den andra.
struct Pattern
{
    static constexpr int maxBars = 4;
    static constexpr int kickStepsPerBar = 16;
    static constexpr int numGrids = 8;
    static constexpr int maxKickSteps = maxBars * kickStepsPerBar;
    static constexpr int minPitch = -24;
    static constexpr int maxPitch = 24;
    static constexpr int numLayers = 3; // baslager med var sitt mönster (FX-versionen använder bara det första)

    // Tiden räknas i "ticks": 96 per fjärdedel, så att alla rutnät (1/4 ... 1/64 och trioler) går jämnt upp.
    static constexpr int ticksPerQuarter = 96;
    static constexpr int ticksPerBar = ticksPerQuarter * 4;
    static constexpr int maxTicks = maxBars * ticksPerBar;
    static constexpr float defaultGate = 1.0f; // en ny ton fyller hela steget (lagrets GATE kortar den vid behov)

    static int bassStepsPerBar (BassGrid grid);
    static double bassStepsPerQuarter (BassGrid grid);
    static int ticksPerStep (BassGrid grid) { return ticksPerBar / bassStepsPerBar (grid); }
    static juce::StringArray gridNames()     { return { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32", "1/32 T", "1/64" }; }
    static juce::StringArray templateNames() { return { "Rolling", "Galloping", "Triplet", "Offbeat", "Clear" }; }

    // En ton i basens mönster. Den ligger på sin starttick; ett lager har högst en ton per tick.
    struct Note
    {
        std::atomic<bool> on { false };
        std::atomic<int> length { 24 };       // i ticks
        std::atomic<int> pitch { 0 };         // halvtoner från tonarten
        std::atomic<bool> slide { false };    // glid till nästa ton (303-stil)
        std::atomic<float> velocity { 0.8f };
    };

    Pattern() { applyTemplate (PatternTemplate::rolling); }

    std::atomic<bool>& kickStep (int index) { return kick[(size_t) index]; }
    bool isKickOn (int index) const         { return kick[(size_t) index].load(); }

    // Index utanför mönstret hamnar vid närmaste kant i stället för utanför minnet.
    Note& note (int layer, int tick)             { return notes[(size_t) juce::jlimit (0, numLayers - 1, layer)][(size_t) juce::jlimit (0, maxTicks - 1, tick)]; }
    const Note& note (int layer, int tick) const { return notes[(size_t) juce::jlimit (0, numLayers - 1, layer)][(size_t) juce::jlimit (0, maxTicks - 1, tick)]; }

    // Första tonen som startar i [fromTick, toTick), eller -1.
    int firstNoteIn (int layer, int fromTick, int toTick) const;

    // Antal ticks till nästa ton efter `tick` (cirkulärt i ett mönster på `patternTicks`), högst `limit`.
    int ticksToNextNote (int layer, int tick, int patternTicks, int limit) const;

    // Hur länge tonen hålls, i ticks. Med slide hålls den in i nästa ton (en aning över), annars gäller
    // tonens längd skalad med lagrets GATE.
    static constexpr double slideOverlap = 1.05;
    double holdTicks (int layer, int tick, int patternTicks, float gateScale) const;

    // Lägger en ny ton (eller ändrar den som redan ligger där). Returnerar den.
    Note& addNote (int layer, int tick, int length, int pitch = 0, float velocity = 0.8f, bool slide = false);
    void removeNote (int layer, int tick) { note (layer, tick).on = false; }
    void clearNotes (int layer);

    // Fyller hela mönstret enligt en mall: kicken och första baslagret, övriga lager töms.
    // Returnerar det rutnät mallen är skriven för.
    BassGrid applyTemplate (PatternTemplate t);

    // Fyller bara ett baslager enligt en mall (kicken och andra lager orörda).
    BassGrid applyLayerTemplate (int layer, PatternTemplate t);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    static inline const juce::Identifier treeType { "Pattern" };
    static constexpr int currentVersion = 4; // 4: fria toner (ticks); 3: baslager med steg per rutnät

private:
    std::array<std::atomic<bool>, maxKickSteps> kick {};
    std::array<std::array<Note, maxTicks>, numLayers> notes;
};
