#pragma once

#include <array>
#include "Pattern.h"
#include "TransportClock.h"

struct StepEvent
{
    enum class Type { kick, bass };

    int offset;   // sample i blocket
    Type type;
    int step;     // steg i mönstret
};

// Tar reda på vilka kick- och bassteg som ska spelas i en del av ett block.
// Delas av instrumentet och FX-versionen.
class StepScheduler
{
public:
    void setGrid (BassGrid newGrid);
    BassGrid getGrid() const { return grid; }

    // Samlar händelser i [startSample, endSample), sorterade efter sample (kick före bas
    // på samma sample). Steg på eller efter `stopAtPpq` hoppas över.
    void collect (const TransportClock& clock, const Pattern& pattern, int patternBars,
                  int startSample, int endSample, double stopAtPpq);

    // Samlar steg som passerades precis före `sampleOffset` (se TransportClock::takeStepJustPassed).
    void collectJustPassed (const TransportClock& clock, const Pattern& pattern, int patternBars,
                            int sampleOffset, int toleranceSamples);

    const StepEvent* begin() const { return events.data(); }
    const StepEvent* end() const   { return events.data() + numEvents; }

    // Senast passerade steg i mönstret, för gränssnittets uppspelningsmarkör.
    int getLastKickStep() const { return lastKickStep; }
    int getLastBassStep() const { return lastBassStep; }

private:
    void addKick (const Pattern& pattern, int patternBars, int offset, StepPosition position);
    void addBass (const Pattern& pattern, int patternBars, int offset, StepPosition position);
    void add (StepEvent event);

    BassGrid grid = BassGrid::sixteenth;
    StepCursor kickCursor { (double) Pattern::kickStepsPerBar / 4.0 };
    StepCursor bassCursor { Pattern::bassStepsPerQuarter (BassGrid::sixteenth) };

    std::array<StepEvent, 256> events {};
    int numEvents = 0;
    int lastKickStep = -1, lastBassStep = -1;
};
