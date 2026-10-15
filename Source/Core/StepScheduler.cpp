#include "StepScheduler.h"

void StepScheduler::setGrid (int layer, BassGrid newGrid)
{
    if (newGrid == grids[(size_t) layer])
        return;

    grids[(size_t) layer] = newGrid;
    bassCursors[(size_t) layer] = StepCursor { Pattern::bassStepsPerQuarter (newGrid) };
}

void StepScheduler::collect (const TransportClock& clock, const Pattern& pattern, int patternBars,
                             int startSample, int endSample, double stopAtPpq)
{
    numEvents = 0;

    auto beforeStop = [&] (int offset) { return clock.getPpqAt (offset) < stopAtPpq - 1.0e-6; };

    clock.forEachStep (kickCursor, startSample, endSample, [&] (int offset, StepPosition position)
    {
        if (beforeStop (offset))
            addKick (pattern, patternBars, offset, position);
    });

    for (int layer = 0; layer < numLayers; ++layer)
    {
        clock.forEachStep (bassCursors[(size_t) layer], startSample, endSample, [&] (int offset, StepPosition position)
        {
            if (beforeStop (offset))
                addBass (layer, pattern, patternBars, offset, position);
        });
    }
}

void StepScheduler::collectJustPassed (const TransportClock& clock, const Pattern& pattern, int patternBars,
                                       int sampleOffset, int toleranceSamples)
{
    numEvents = 0;

    if (const auto position = clock.takeStepJustPassed (kickCursor, sampleOffset, toleranceSamples))
        addKick (pattern, patternBars, sampleOffset, *position);

    for (int layer = 0; layer < numLayers; ++layer)
        if (const auto position = clock.takeStepJustPassed (bassCursors[(size_t) layer], sampleOffset, toleranceSamples))
            addBass (layer, pattern, patternBars, sampleOffset, *position);
}

void StepScheduler::addKick (const Pattern& pattern, int patternBars, int offset, StepPosition position)
{
    const auto step = position.stepInPattern (Pattern::kickStepsPerBar, patternBars);
    lastKickStep = step;

    if (pattern.isKickOn (step))
        add ({ offset, StepEvent::Type::kick, step });
}

void StepScheduler::addBass (int layer, const Pattern& pattern, int patternBars, int offset, StepPosition position)
{
    const auto grid = grids[(size_t) layer];
    const auto step = position.stepInPattern (Pattern::bassStepsPerBar (grid), patternBars);
    lastBassSteps[(size_t) layer] = step;

    if (pattern.bassStep (layer, grid, step).on.load())
        add ({ offset, StepEvent::Type::bass, step, layer });
}

void StepScheduler::add (StepEvent event)
{
    if (numEvents >= (int) events.size())
        return;

    // Insättningssortering: listorna är korta och nästan alltid redan i ordning.
    auto i = numEvents++;
    while (i > 0 && (events[(size_t) i - 1].offset > event.offset
                     || (events[(size_t) i - 1].offset == event.offset && event.type == StepEvent::Type::kick
                         && events[(size_t) i - 1].type == StepEvent::Type::bass)))
    {
        events[(size_t) i] = events[(size_t) i - 1];
        --i;
    }
    events[(size_t) i] = event;
}
