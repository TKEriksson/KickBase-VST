#pragma once

#include <algorithm>

// Gate för FX-versionen: öppnas av basstegen och formar ljudet från en annan synth.
class StepGate
{
public:
    void prepare (double newSampleRate) { sampleRate = newSampleRate; level = 0.0f; remaining = 0; }

    void setTimes (float attackMs, float releaseMs)
    {
        attackStep = 1.0f / (float) (std::max (0.05f, attackMs) * 0.001 * sampleRate);
        releaseStep = 1.0f / (float) (std::max (0.05f, releaseMs) * 0.001 * sampleRate);
    }

    void open (float velocity, int gateSamples)
    {
        target = velocity;
        remaining = gateSamples;
    }

    // `passThrough` håller gaten helt öppen, t.ex. när DAW:en står still.
    float next (bool passThrough)
    {
        const auto goal = passThrough ? 1.0f : (remaining > 0 ? target : 0.0f);

        if (remaining > 0)
            --remaining;

        level = goal > level ? std::min (goal, level + attackStep)
                             : std::max (goal, level - releaseStep);
        return level;
    }

private:
    double sampleRate = 44100.0;
    float level = 0.0f, target = 1.0f;
    float attackStep = 0.01f, releaseStep = 0.01f;
    int remaining = 0;
};
