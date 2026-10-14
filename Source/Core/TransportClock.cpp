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
    const auto continuedPpq = blockStartPpq + blockSize * ppqPerSample;
    blockSize = numSamples;

    if (position.hasValue())
    {
        if (const auto hostBpm = position->getBpm(); hostBpm.hasValue() && *hostBpm > 0.0)
            bpm = *hostBpm;

        if (const auto sig = position->getTimeSignature(); sig.hasValue() && sig->numerator > 0 && sig->denominator > 0)
            barLengthPpq = sig->numerator * 4.0 / sig->denominator;
    }

    ppqPerSample = bpm / 60.0 / sampleRate;

    const auto hostPpq = position.hasValue() ? position->getPpqPosition() : juce::nullopt;
    followingHost = position.hasValue() && position->getIsPlaying() && hostPpq.hasValue();

    if (followingHost)
    {
        blockStartPpq = *hostPpq;

        if (const auto hostBarStart = position->getPpqPositionOfLastBarStart(); hostBarStart.hasValue())
            barStartPpq = *hostBarStart;
        else
            barStartPpq = std::floor (blockStartPpq / barLengthPpq) * barLengthPpq;
    }
    else
    {
        blockStartPpq = continuedPpq;
        barStartPpq = std::floor (blockStartPpq / barLengthPpq) * barLengthPpq;
    }
}

void TransportClock::restartFreeRunAt (int sampleOffset)
{
    if (followingHost)
        return;

    blockStartPpq = -sampleOffset * ppqPerSample;
    barStartPpq = 0.0;
    ++generation;
}

std::optional<int> TransportClock::takeStepJustPassed (StepCursor& cursor, int sampleOffset, int toleranceSamples) const
{
    const auto spq = cursor.stepsPerQuarter;
    const auto ppq = getPpqAt (sampleOffset);

    syncCursor (cursor, ppq);

    const auto step = (juce::int64) std::floor ((ppq - barStartPpq) * spq + epsilon);
    const auto stepPpq = barStartPpq + (double) step / spq;

    if (stepPpq <= cursor.lastFiredPpq + epsilon || (ppq - stepPpq) / ppqPerSample > toleranceSamples)
        return std::nullopt;

    cursor.lastFiredPpq = stepPpq;
    return stepInBar (step, spq);
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

int TransportClock::stepInBar (juce::int64 stepFromBarStart, double stepsPerQuarter) const
{
    const auto stepsPerBar = std::max ((juce::int64) 1, (juce::int64) std::floor (barLengthPpq * stepsPerQuarter + epsilon));
    return (int) (((stepFromBarStart % stepsPerBar) + stepsPerBar) % stepsPerBar);
}
