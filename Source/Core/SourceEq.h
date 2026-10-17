#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "Effects.h"

// En EQ för en ljudkälla (kicken, ett instrument eller mastern): högpass, fyra band (lågt hyllfilter, två klockor, högt hyllfilter) och lågpass,
// samma uppbyggnad som effektkanalernas EQ. Neutral som standard, och då körs ingen bearbetning alls. Redigeras i ANALYZER-fliken.
//
// Parameter-ID:n: prefix + "EqHp", "EqLp", "Eq1F", "Eq1G", "Eq1Q" ... "Eq4Q". Prefixet är "kick", "bass3" (instrument 3) eller "master", så att
// EQ:n följer med kickens, instrumentets och masterns presets.
class SourceEq
{
public:
    SourceEq (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& prefix, const juce::String& title);

    // Prefixet för en källa i ANALYZER-fliken: 0 = kicken, 1-10 = instrument, 11 = mastern.
    static juce::String prefixFor (int source);

    static Effects::EqSettings read (const juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);

    void prepare (double sampleRate);
    void reset();

    // Ljudtråden. `right` kan vara nullptr (mono). Returnerar true om något bearbetades.
    bool process (float* left, float* right, int numSamples);

    Effects::EqSettings settings() const;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    static constexpr int numFilters = Effects::eqBands + 2;

    std::array<std::atomic<float>*, 2 + Effects::eqBands * 3> values {};
    std::array<std::array<Filter, numFilters>, 2> filters;
    Effects::EqSettings last;
    double sampleRate = 44100.0;
    bool wasNeutral = true;
    bool haveCoefficients = false;
};
