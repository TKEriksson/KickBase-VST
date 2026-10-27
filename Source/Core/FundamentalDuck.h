#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

// FUNDAMENTAL DUCK (ett instruments motor): ett smalt peaking-filter (RBJ, negativ förstärkning) på motorns grundton,
// som dämpas medan kicken klingar. Stereo, transponerad direkt form II. Av = full förbigång och nollat tillstånd,
// så ljudet är bit-identiskt med förut. Koefficienterna uppdateras bara när förstärkning eller frekvens har rört sig.
class FundamentalDuck
{
public:
    static constexpr float q = 1.4f;
    static constexpr float minGainDb = 0.05f;      // under detta räknas filtret som av

    void prepare (double newSampleRate) { sampleRate = newSampleRate; reset(); }

    void reset()
    {
        z1l = z2l = z1r = z2r = 0.0;
        currentGainDb = 0.0f;
        coefGainDb = 0.0f;
        coefHz = 0.0f;
        active = false;
    }

    // Dämpning i dB (>= 0) vid hz för de närmaste samplen. Returnerar true om filtret är i bruk.
    bool setTarget (float hz, float attenuationDb)
    {
        if (hz < 20.0f || attenuationDb < minGainDb)
        {
            if (active && currentGainDb > minGainDb)
            {
                // Släpp ut mjukt mot noll utan att byta frekvens.
                currentGainDb *= 0.9f;
                updateCoefficients (coefHz > 0.0f ? coefHz : hz, currentGainDb);
                return true;
            }
            reset();
            return false;
        }
        currentGainDb += (attenuationDb - currentGainDb) * 0.5f;
        const auto cents = coefHz > 0.0f ? std::abs (1200.0f * std::log2 (hz / coefHz)) : 1000.0f;
        if (! active || cents > 1.0f || std::abs (currentGainDb - coefGainDb) > 0.05f)
            updateCoefficients (hz, currentGainDb);
        active = true;
        return true;
    }

    bool isActive() const { return active; }
    float getAttenuationDb() const { return active ? coefGainDb : 0.0f; }

    void process (float& left, float& right)
    {
        if (! active)
            return;
        const auto xl = (double) left, xr = (double) right;
        const auto yl = b0 * xl + z1l;
        z1l = b1 * xl - a1 * yl + z2l;
        z2l = b2 * xl - a2 * yl;
        const auto yr = b0 * xr + z1r;
        z1r = b1 * xr - a1 * yr + z2r;
        z2r = b2 * xr - a2 * yr;
        left = (float) yl;
        right = (float) yr;
    }

private:
    void updateCoefficients (float hz, float attenuationDb)
    {
        const auto clampedHz = juce::jlimit (20.0, 0.45 * sampleRate, (double) hz);
        const auto amp = std::pow (10.0, -(double) attenuationDb / 40.0);
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * clampedHz / sampleRate;
        const auto alpha = std::sin (w0) / (2.0 * (double) q);
        const auto cosW = std::cos (w0);
        const auto a0 = 1.0 + alpha / amp;
        b0 = (1.0 + alpha * amp) / a0;
        b1 = (-2.0 * cosW) / a0;
        b2 = (1.0 - alpha * amp) / a0;
        a1 = (-2.0 * cosW) / a0;
        a2 = (1.0 - alpha / amp) / a0;
        coefHz = hz;
        coefGainDb = attenuationDb;
    }

    double sampleRate = 44100.0;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1l = 0.0, z2l = 0.0, z1r = 0.0, z2r = 0.0;
    float currentGainDb = 0.0f, coefGainDb = 0.0f, coefHz = 0.0f;
    bool active = false;
};
