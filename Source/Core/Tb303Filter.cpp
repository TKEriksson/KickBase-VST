#include "Tb303Filter.h"
#include <juce_dsp/juce_dsp.h> // före Open303:s headers (de definierar makron)
#include "../ThirdParty/open303/rosic_TeeBeeFilter.h"
#include <array>
#include <cmath>

struct Tb303Filter::Impl
{
    // Samma värden som JUCE:s egna standard för upp- och nedsampling (juce_Oversampling.cpp): övergång 0,1 av 2x-takten, spärr -70 dB.
    static constexpr double transitionWidth = 0.1, stopbandDb = -70.0;
    static constexpr int maxStages = 16;

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
    static const HalfBand& halfBand() { static const HalfBand instance; return instance; }

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

    rosic::TeeBeeFilter filter;
    std::array<double, maxStages> alpha {};
    std::array<double, maxStages> upState {}, downState {};
    double delayDown = 0.0;
    int directStages = 0, delayedStages = 0;
    double lastCutoff = -1.0, lastResonance = -1.0;

    Impl()
    {
        const auto& design = halfBand();
        directStages = design.directStages;
        delayedStages = design.delayedStages;
        alpha = design.alpha;
    }
};

Tb303Filter::Tb303Filter() : impl (std::make_unique<Impl>()) {}
Tb303Filter::~Tb303Filter() = default;
Tb303Filter::Tb303Filter (Tb303Filter&&) noexcept = default;
Tb303Filter& Tb303Filter::operator= (Tb303Filter&&) noexcept = default;

void Tb303Filter::prepare (double baseSampleRate)
{
    impl->filter.setSampleRate (2.0 * baseSampleRate); // (räknar om koefficienterna)
    impl->filter.setMode (rosic::TeeBeeFilter::TB_303);
    impl->lastCutoff = impl->lastResonance = -1.0;
    reset();
}

void Tb303Filter::reset()
{
    impl->filter.reset();
    impl->upState.fill (0.0);
    impl->downState.fill (0.0);
    impl->delayDown = 0.0;
}

void Tb303Filter::setParameters (double cutoffHz, double resonancePercent)
{
    if (cutoffHz == impl->lastCutoff && resonancePercent == impl->lastResonance)
        return;
    impl->lastCutoff = cutoffHz;
    impl->lastResonance = resonancePercent;
    impl->filter.setCutoff (cutoffHz, false);
    impl->filter.setResonance (resonancePercent, true);
}

float Tb303Filter::process (float inputSample)
{
    auto& s = *impl;
    const auto x = (double) inputSample;

    // Upp (som processSamplesUp): den direkta vägen ger den jämna utsamplingen, den fördröjda vägen den udda.
    const auto even = Impl::chain (s.alpha.data(), s.upState.data(), 0, s.directStages, x);
    const auto odd = Impl::chain (s.alpha.data(), s.upState.data(), s.directStages, s.directStages + s.delayedStages, x);

    // Filtret på 2x.
    const auto a = s.filter.getSample (even);
    const auto b = s.filter.getSample (odd);

    // Ner (som processSamplesDown): (fördröjd väg från förra paret + direkt väg) / 2.
    const auto directOut = Impl::chain (s.alpha.data(), s.downState.data(), 0, s.directStages, a);
    const auto delayedOut = Impl::chain (s.alpha.data(), s.downState.data(), s.directStages, s.directStages + s.delayedStages, b);
    const auto result = 0.5 * (s.delayDown + directOut);
    s.delayDown = delayedOut;

    // (Denormala tal hanteras av ScopedNoDenormals i processBlock, så JUCE:s snapToZero behövs inte.)
    return (float) result;
}

float Tb303Filter::processWithoutFilter (float inputSample)
{
    auto& s = *impl;
    const auto x = (double) inputSample;
    const auto even = Impl::chain (s.alpha.data(), s.upState.data(), 0, s.directStages, x);
    const auto odd = Impl::chain (s.alpha.data(), s.upState.data(), s.directStages, s.directStages + s.delayedStages, x);
    const auto directOut = Impl::chain (s.alpha.data(), s.downState.data(), 0, s.directStages, even);
    const auto delayedOut = Impl::chain (s.alpha.data(), s.downState.data(), s.directStages, s.directStages + s.delayedStages, odd);
    const auto result = 0.5 * (s.delayDown + directOut);
    s.delayDown = delayedOut;
    return (float) result;
}
