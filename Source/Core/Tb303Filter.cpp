#include "Tb303Filter.h"
#include <juce_dsp/juce_dsp.h> // före Open303:s headers (de definierar makron)
#include "Oversampler2x.h"
#include "../ThirdParty/open303/rosic_TeeBeeFilter.h"
#include <array>
#include <cmath>

struct Tb303Filter::Impl
{
    rosic::TeeBeeFilter filter;
    Oversampler2x oversampler; // delad med filtermodellerna i FilterModels.cpp (fas D2)
    double lastCutoff = -1.0, lastResonance = -1.0;
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
    impl->oversampler.reset();
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
    // (Denormala tal hanteras av ScopedNoDenormals i processBlock, så JUCE:s snapToZero behövs inte.)
    return (float) s.oversampler.process ((double) inputSample, [&s] (double v) { return s.filter.getSample (v); });
}

float Tb303Filter::processWithoutFilter (float inputSample)
{
    auto& s = *impl;
    return (float) s.oversampler.process ((double) inputSample, [] (double v) { return v; });
}
