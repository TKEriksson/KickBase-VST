#pragma once

#include <juce_core/juce_core.h>
#include <array>

// Ett andra ordningens IIR-filter (transponerad direktform II) som räknar exakt som juce::dsp::IIR::Filter<float>::processSample med
// koefficienter satta via IIR::Coefficients (samma normalisering och samma operationer i samma ordning, så ljudet är bit-identiskt),
// men utan JUCE:s kontroller och generella loop för varje sample. EQ:erna (källornas, effektkanalernas och EQ-effekten) kör många sådana
// filter i rad på varje sample: med 10 instrument och EQ överallt var JUCE:s filter den största enskilda kostnaden på ljudtråden.
struct Biquad
{
    // Koefficienterna i JUCE:s ordning (b0, b1, b2, a0, a1, a2), t.ex. från dsp::IIR::ArrayCoefficients<float>::makePeakFilter.
    void setCoefficients (const std::array<float, 6>& values)
    {
        // Som Coefficients::assignImpl: allt utom a0 delas med a0.
        const auto a0 = values[3];
        const auto a0Inv = ! juce::approximatelyEqual (a0, 0.0f) ? 1.0f / values[3] : 0.0f;
        c = { values[0] * a0Inv, values[1] * a0Inv, values[2] * a0Inv, values[4] * a0Inv, values[5] * a0Inv };
    }

    void setIdentity() { c = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f }; }
    void reset() { s0 = s1 = 0.0f; }

    float process (float sample)
    {
        // Samma uttryck som IIR::Filter::processSample för ordning 2.
        const auto output = (c[0] * sample) + s0;
        s0 = (c[1] * sample) - (c[3] * output) + s1;
        s1 = (c[2] * sample) - (c[4] * output);
        return output;
    }

private:
    std::array<float, 5> c { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // b0, b1, b2, a1, a2 (normaliserade)
    float s0 = 0.0f, s1 = 0.0f;
};
