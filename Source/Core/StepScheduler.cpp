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
        clock.forEachStep (instrumentCursors[(size_t) layer], startSample, endSample, [&] (int offset, StepPosition position)
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
        if (const auto position = clock.takeStepJustPassed (instrumentCursors[(size_t) layer], sampleOffset, toleranceSamples))
            addBass (layer, pattern, lengths, sampleOffset, *position);
}

int StepScheduler::windowTick (int rowTicks, StepPosition position) const
{
    rowTicks = std::max (1, rowTicks);
    const auto elapsed = (position.bar - window.bar0) * (juce::int64) Pattern::ticksPerBar + position.stepInBar;
    const auto loopEnd = std::min (window.loopEnd, rowTicks);
    if (window.loopStart >= 0 && loopEnd > window.loopStart)
    {
        const auto length = (juce::int64) (loopEnd - window.loopStart);
        return window.loopStart + (int) (((elapsed % length) + length) % length);
    }
    return (int) ((((juce::int64) window.from + elapsed) % rowTicks + rowTicks) % rowTicks);
}

void StepScheduler::addKick (const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position)
{
    const auto barInRow = lengths.barInRow (0, position.bar);
    auto step = barInRow < 0 ? -1 : barInRow * Pattern::ticksPerBar + position.stepInBar;
    if (window.row == Pattern::kickLayer || window.allRows)
        step = windowTick (lengths.barsOf (0) * Pattern::ticksPerBar, position);
    lastKickStep = step;

    if (step >= 0 && pattern.note (Pattern::kickLayer, step).on.load())
        add ({ offset, StepEvent::Type::kick, step });
}

void StepScheduler::addBass (int layer, const Pattern& pattern, const PatternLengths& lengths, int offset, StepPosition position)
{
    const auto barInRow = lengths.barInRow (1 + layer, position.bar);
    auto tick = barInRow < 0 ? -1 : barInRow * Pattern::ticksPerBar + position.stepInBar;
    if (window.row == layer || window.allRows)
        tick = windowTick (lengths.barsOf (1 + layer) * Pattern::ticksPerBar, position);
    lastInstrumentSteps[(size_t) layer] = tick;

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
