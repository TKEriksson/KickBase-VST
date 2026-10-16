#include "StepScheduler.h"

void StepScheduler::collect (const TransportClock& clock, const Pattern& pattern, const PatternLengths& lengths,
                             int startSample, int endSample, double stopAtPpq)
{
    numEvents = 0;

    auto beforeStop = [&] (int offset) { return clock.getPpqAt (offset) < stopAtPpq - 1.0e-6; };

    clock.forEachStep (kickCursor, startSample, endSample, [&] (int offset, StepPosition position)
    {
        if (beforeStop (offset))
            addKick (pattern, lengths, offset, position);
    });

    for (int layer = 0; layer < numLayers; ++layer)
    {
        clock.forEachStep (bassCursors[(size_t) layer], startSample, endSample, [&] (int offset, StepPosition position)
        {
            if (beforeStop (offset))
                addBass (layer, pattern, lengths, offset, position);
        });
    }
}

void StepScheduler::collectJustPassed (const TransportClock& clock, const Pattern& pattern, const PatternLengths& lengths,
                                       int sampleOffset, int toleranceSamples)
{
    numEvents = 0;

    if (const auto position = clock.takeStepJustPassed (kickCursor, sampleOffset, toleranceSamples))
        addKick (pattern, lengths, sampleOffset, *position);

    for (int layer = 0; layer < numLayers; ++layer)
        if (const auto position = clock.takeStepJustPassed (bassCursors[(size_t) layer], sampleOffset, toleranceSamples))
            addBass (layer, pattern, lengths, sampleOffset, *position);
}

void StepScheduler::addKick (const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position)
{
    const auto barInRow = lengths.barInRow (0, position.bar);
    const auto step = barInRow < 0 ? -1 : barInRow * Pattern::kickStepsPerBar + position.stepInBar;
    lastKickStep = step;

    if (step >= 0 && pattern.isKickOn (step))
        add ({ offset, StepEvent::Type::kick, step });
}

void StepScheduler::addBass (int layer, const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position)
{
    const auto barInRow = lengths.barInRow (1 + layer, position.bar);
    const auto tick = barInRow < 0 ? -1 : barInRow * Pattern::ticksPerBar + position.stepInBar;
    lastBassSteps[(size_t) layer] = tick;

    if (tick >= 0 && pattern.note (layer, tick).on.load())
        add ({ offset, StepEvent::Type::bass, tick, layer });
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
