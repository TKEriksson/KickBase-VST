#pragma once

#include <array>
#include "Pattern.h"
#include "TransportClock.h"

struct StepEvent
{
    enum class Type { kick, bass };

    int offset;     // sample i blocket
    Type type;
    int step;       // tonens tick i mönstret (kick och bas)
    int layer = 0;  // baslager (bara för bas)
    juce::uint32 sequence = 0; // i vilken ordning händelserna schemalades: avgör ordningen när två hamnar på samma sample (oberoende av blockens delning)
};

// Tar reda på vilka kick- och bassteg som ska spelas i en del av ett block.
// Delas av instrumentet och FX-versionen.
class StepScheduler
{
public:
    // Hur många baslager som spelas (FX-versionen: 1).
    void setNumLayers (int n) { numLayers = juce::jlimit (1, Pattern::numLayers, n); }

    // Samlar händelser i [startSample, endSample), sorterade efter sample (kick före bas
    // på samma sample). Steg på eller efter `stopAtPpq` hoppas över.
    void collect (const TransportClock& clock, const Pattern& pattern, const PatternLengths& lengths,
                  int startSample, int endSample, double stopAtPpq);

    // Samlar steg som passerades precis före `sampleOffset` (se TransportClock::takeStepJustPassed).
    void collectJustPassed (const TransportClock& clock, const Pattern& pattern, const PatternLengths& lengths,
                            int sampleOffset, int toleranceSamples);

    // Spolning och loop för förhandsspelningen av en enskild rad: i stället för radens vanliga plats spelas ticken från `from` (eller runt i loopen).
    struct Window
    {
        int row = -1; // notlager eller kickLayer, -1 = ingen
        bool allRows = false; // ALL: alla rader följer samma start och loop
        int from = 0, loopStart = -1, loopEnd = -1;
        juce::int64 bar0 = 0; // takten där förhandsspelningen började
    };
    void setWindow (const Window& newWindow) { window = newWindow; }

    // Radens tick under förhandsspelningen (publik så att testerna kan nå den).
    int windowTick (int rowTicks, int totalTicks, StepPosition position) const;

    const StepEvent* begin() const { return events.data(); }
    const StepEvent* end() const   { return events.data() + numEvents; }

    // Senast passerade steg i mönstret, för gränssnittets uppspelningsmarkör.
    int getLastKickStep() const          { return lastKickStep; }
    int getLastInstrumentStep (int layer) const { return lastInstrumentSteps[(size_t) layer]; }

private:
    void addKick (const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position);
    void addBass (int layer, const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position);
    void add (StepEvent event);

    Window window;
    int numLayers = Pattern::numLayers;
    // Kickens och baslagrens toner ligger fritt i tiden, så markörerna går i ticks (96 per fjärdedel).
    StepCursor kickCursor { (double) Pattern::ticksPerQuarter };
    static constexpr double instrumentTicksPerQuarter = Pattern::ticksPerQuarter;
    std::array<StepCursor, Pattern::numLayers> instrumentCursors {}; // alla med instrumentTicksPerQuarter (standardvärdet)

    std::array<StepEvent, 512> events {};
    int numEvents = 0;
    int lastKickStep = -1;
    std::array<int, Pattern::numLayers> lastInstrumentSteps = filledArray<int, Pattern::numLayers> (-1);
};
