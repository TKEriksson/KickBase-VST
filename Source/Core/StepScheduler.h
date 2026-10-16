#pragma once

#include <array>
#include "Pattern.h"
#include "TransportClock.h"

struct StepEvent
{
    enum class Type { kick, bass };

    int offset;     // sample i blocket
    Type type;
    int step;       // steg i mönstret (kick: sextondel, bas: tick)
    int layer = 0;  // baslager (bara för bas)
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

    const StepEvent* begin() const { return events.data(); }
    const StepEvent* end() const   { return events.data() + numEvents; }

    // Senast passerade steg i mönstret, för gränssnittets uppspelningsmarkör.
    int getLastKickStep() const          { return lastKickStep; }
    int getLastBassStep (int layer) const { return lastBassSteps[(size_t) layer]; }

private:
    void addKick (const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position);
    void addBass (int layer, const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position);
    void add (StepEvent event);

    int numLayers = Pattern::numLayers;
    StepCursor kickCursor { (double) Pattern::kickStepsPerBar / 4.0 };
    // Baslagrens toner ligger fritt i tiden, så markören går i ticks (96 per fjärdedel).
    static constexpr double bassTicksPerQuarter = Pattern::ticksPerQuarter;
    std::array<StepCursor, Pattern::numLayers> bassCursors { StepCursor { bassTicksPerQuarter }, StepCursor { bassTicksPerQuarter },
                                                             StepCursor { bassTicksPerQuarter } };

    std::array<StepEvent, 512> events {};
    int numEvents = 0;
    int lastKickStep = -1;
    std::array<int, Pattern::numLayers> lastBassSteps { -1, -1, -1 };
};
