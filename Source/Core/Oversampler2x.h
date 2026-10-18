#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Översampling 2x per sampling med JUCE:s halvbands-IIR (polyfas, kaskad av allpass), delad av filtermodellerna som behöver den (TB-303, Ladder).
//
// Synten spelar sample-exakt och kan inte köra ett block före filtret, så upp- och nedsamplingen är skriven per sampling: koefficienterna kommer ur
// FilterDesign::designIIRLowpassHalfBandPolyphaseAllpassMethod (juce_FilterDesign.cpp:621-701, samma konstruktion som dsp::Oversampling använder) och
// kedjorna är skrivna efter Oversampling2TimesPolyphaseIIR::processSamplesUp och processSamplesDown (juce_Oversampling.cpp:322-376 och 378-434).
// Allt är double, ingen allokering efter konstruktionen, tillståndet nollställs av reset().
//
// Utbrutet ur Tb303Filter (fas D1) för fas D2; ljudet i TB-303-modellen är bit för bit detsamma (rendersigtb303).
class Oversampler2x
{
public:
    // Samma värden som JUCE:s egna standard för upp- och nedsampling (juce_Oversampling.cpp): övergång 0,1 av 2x-takten, spärr -70 dB.
    static constexpr double transitionWidth = 0.1, stopbandDb = -70.0;
    static constexpr int maxStages = 16;

    Oversampler2x()
    {
        const auto& design = halfBand();
        directStages = design.directStages;
        delayedStages = design.delayedStages;
        alpha = design.alpha;
    }

    void reset()
    {
        upState.fill (0.0);
        downState.fill (0.0);
        delayDown = 0.0;
    }

    // Skickar `input` upp till 2x, kör `atDoubleRate(double)` på de två samplingarna (jämn, sedan udda) och sampla ner igen.
    template <typename Fn>
    double process (double input, Fn&& atDoubleRate)
    {
        // Upp (som processSamplesUp): den direkta vägen ger den jämna utsamplingen, den fördröjda vägen den udda.
        const auto even = chain (alpha.data(), upState.data(), 0, directStages, input);
        const auto odd = chain (alpha.data(), upState.data(), directStages, directStages + delayedStages, input);

        const auto a = atDoubleRate (even);
        const auto b = atDoubleRate (odd);

        // Ner (som processSamplesDown): (fördröjd väg från förra paret + direkt väg) / 2.
        const auto directOut = chain (alpha.data(), downState.data(), 0, directStages, a);
        const auto delayedOut = chain (alpha.data(), downState.data(), directStages, directStages + delayedStages, b);
        const auto result = 0.5 * (delayDown + directOut);
        delayDown = delayedOut;
        return result;
    }

private:
    struct HalfBand
    {
        int stages = 0, directStages = 0, delayedStages = 0;
        std::array<double, maxStages> alpha {};

        HalfBand()
        {
            const auto structure = juce::dsp::FilterDesign<double>::designIIRLowpassHalfBandPolyphaseAllpassMethod (transitionWidth, stopbandDb);
            // JUCE:s ordning (juce_Oversampling.cpp:290-301): den direkta vägens koefficienter (ai[0], ai[2], ...) och sedan den fördröjda vägens (ai[1], ai[3], ...).
            const auto& ai = structure.alpha;
            const auto count = ai.size();
            jassert (count <= maxStages);
            for (int i = 0; i < count; i += 2)
                alpha[(size_t) stages++] = ai[i];
            directStages = stages;
            for (int i = 1; i < count; i += 2)
                alpha[(size_t) stages++] = ai[i];
            delayedStages = stages - directStages;
        }
    };
    static const HalfBand& halfBand() { static const HalfBand instance; return instance; } // (skapas första gången, aldrig på ljudtråden: prepare() i filtren)

    // Allpass-kedjan: stegen first..last-1 i alpha (direktvägen först, sedan den fördröjda vägen, som JUCE bygger coefficientsUp).
    static double chain (const double* coefficients, double* state, int first, int last, double input)
    {
        for (int n = first; n < last; ++n)
        {
            const auto a = coefficients[n];
            const auto output = a * input + state[n];
            state[n] = input - a * output;
            input = output;
        }
        return input;
    }

    std::array<double, maxStages> alpha {};
    std::array<double, maxStages> upState {}, downState {};
    double delayDown = 0.0;
    int directStages = 0, delayedStages = 0;
};
