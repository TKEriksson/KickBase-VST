#pragma once

#include <cmath>

// Trycker undan basen när kicken låter, efter kickens envelope. Förstärkningen glider mot
// målet på ungefär en millisekund, så att en snabb kickattack inte ger ett knäpp i basen.
class Ducker
{
public:
    void prepare (double sampleRate)
    {
        coefficient = (float) (1.0 - std::exp (-1.0 / (0.001 * sampleRate)));
        gain = 1.0f;
    }

    float next (float kickEnvelope, float amount)
    {
        gain += (1.0f - amount * kickEnvelope - gain) * coefficient;
        return gain;
    }

private:
    float coefficient = 0.02f;
    float gain = 1.0f;
};
