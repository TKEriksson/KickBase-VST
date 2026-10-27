#include "TransportClock.h"

void TransportClock::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    ppqPerSample = bpm / 60.0 / sampleRate;
    blockStartPpq = 0.0;
    blockSize = 0;
    ++generation;
}

void TransportClock::beginBlock (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position, int numSamples)
{
    // Där den egna klockan skulle vara nu om den fick fortsätta från förra blocket.
    const auto continuedPpq = hold ? blockStartPpq : blockStartPpq + blockSize * ppqPerSample;
    blockSize = numSamples;

    const auto hostPpq = position.hasValue() ? position->getPpqPosition() : juce::nullopt;
    hostPlaying = position.hasValue() && position->getIsPlaying();
    followingHost = ! forceFreeRun && hostPlaying && hostPpq.hasValue();

    if (position.hasValue())
    {
        if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0)
            bpm = *hostBpm;

        if (const auto sig = position->getTimeSignature(); sig.hasValue() && sig->numerator > 0 && sig->denominator > 0)
            barLengthPpq = sig->numerator * 4.0 / sig->denominator;
    }

    // Egen klocka: Kickbases eget tempo (om satt) och hållning gäller bara här. Spelar värden styr den (en VST3 kan inte ändra värdens tempo).
    if (! followingHost && freeRunBpm > 0.0)
        bpm = freeRunBpm;

    ppqPerSample = bpm / 60.0 / sampleRate;
    if (hold && ! followingHost)
        ppqPerSample = 1.0e-9; // klockan står still (hjulet scrubbar); inga steg passeras

    if (followingHost)
    {
        blockStartPpq = *hostPpq;

        if (const auto hostBarStart = position->getPpqPositionOfLastBarStart(); hostBarStart.hasValue())
            barStartPpq = *hostBarStart;
        else
            barStartPpq = std::floor (blockStartPpq / barLengthPpq + epsilon) * barLengthPpq;
    }
    else
    {
        blockStartPpq = continuedPpq;
        barStartPpq = std::floor (blockStartPpq / barLengthPpq + epsilon) * barLengthPpq;
    }

    barIndex = (juce::int64) std::llround (barStartPpq / barLengthPpq);
}

void TransportClock::restartFreeRunAt (int sampleOffset)
{
    if (followingHost)
        return;

    blockStartPpq = -sampleOffset * ppqPerSample;
    barStartPpq = 0.0;
    barIndex = 0;
    ++generation;
}

void TransportClock::seekFreeRunToBar (juce::int64 bar)
{
    if (followingHost)
        return;

    blockStartPpq = (double) bar * barLengthPpq;
    barStartPpq = blockStartPpq;
    barIndex = bar;
    ++generation;
}

std::optional<StepPosition> TransportClock::takeStepJustPassed (StepCursor& cursor, int sampleOffset, int toleranceSamples) const
{
    const auto spq = cursor.stepsPerQuarter;
    const auto ppq = getPpqAt (sampleOffset);

    syncCursor (cursor, ppq);

    const auto step = (juce::int64) std::floor ((ppq - barStartPpq) * spq + epsilon);
    const auto stepPpq = barStartPpq + (double) step / spq;

    if (stepPpq <= cursor.lastFiredPpq + epsilon || (ppq - stepPpq) / ppqPerSample > toleranceSamples)
        return std::nullopt;

    cursor.lastFiredPpq = stepPpq;
    return positionOf (step, spq);
}

void TransportClock::syncCursor (StepCursor& cursor, double fromPpq) const
{
    // Efter en omstart, eller om DAW:en hoppat bakåt (loop, spolning), börjar markören om.
    if (cursor.generation != generation || fromPpq < cursor.lastEndPpq - 1.0e-6)
    {
        cursor.lastFiredPpq = -std::numeric_limits<double>::infinity();
        cursor.lastEndPpq = -std::numeric_limits<double>::infinity();
        cursor.generation = generation;
    }
}

StepPosition TransportClock::positionOf (juce::int64 stepFromBarStart, double stepsPerQuarter) const
{
    const auto stepsPerBar = std::max ((juce::int64) 1, (juce::int64) std::floor (barLengthPpq * stepsPerQuarter + epsilon));

    // Golvdivision, så att steg före taktens början hamnar i föregående takt.
    auto barOffset = stepFromBarStart / stepsPerBar;
    if (stepFromBarStart % stepsPerBar < 0)
        --barOffset;

    return { (int) (stepFromBarStart - barOffset * stepsPerBar), barIndex + barOffset };
}
