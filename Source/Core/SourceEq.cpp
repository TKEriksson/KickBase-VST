#include "SourceEq.h"
#include "Params.h"

namespace
{
    juce::NormalisableRange<float> skewedRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> range (lo, hi, 0.1f);
        range.setSkewForCentre (centre);
        return range;
    }

    bool sameSettings (const Effects::EqSettings& a, const Effects::EqSettings& b)
    {
        if (a.hp != b.hp || a.lp != b.lp)
            return false;
        for (int i = 0; i < Effects::eqBands; ++i)
            if (a.freq[i] != b.freq[i] || a.gain[i] != b.gain[i] || a.q[i] != b.q[i])
                return false;
        return true;
    }
}

void SourceEq::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& prefix, const juce::String& title)
{
    using namespace juce;
    auto hzText = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { prefix + "EqHp", 1 }, title + " EQ High Pass", skewedRange (20.0f, 600.0f, 90.0f), 20.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 21.0f ? String ("Off") : String (roundToInt (v)) + " Hz"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { prefix + "EqLp", 1 }, title + " EQ Low Pass", skewedRange (1500.0f, 20000.0f, 6000.0f), 20000.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 19500.0f ? String ("Off") : (v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"); })));

    const float bandLow[] { 30.0f, 60.0f, 400.0f, 2000.0f }, bandHigh[] { 400.0f, 2000.0f, 8000.0f, 16000.0f };
    const float bandDefault[] { 100.0f, 400.0f, 2500.0f, 8000.0f }, bandCentre[] { 100.0f, 400.0f, 2000.0f, 6000.0f };
    for (int b = 0; b < Effects::eqBands; ++b)
    {
        const auto name = title + " EQ " + String (b + 1) + " ";
        const auto key = prefix + "Eq" + String (b + 1);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { key + "F", 1 }, name + "Freq", skewedRange (bandLow[b], bandHigh[b], bandCentre[b]), bandDefault[b],
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { key + "G", 1 }, name + "Gain", NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { key + "Q", 1 }, name + "Q", skewedRange (0.3f, 8.0f, 1.2f), b == 1 || b == 2 ? 1.0f : 0.7f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
    }
}

juce::String SourceEq::prefixFor (int source)
{
    if (source <= 0)
        return "kick";
    if (source > ParamID::numBassLayers)
        return "master";
    return "bass" + juce::String (source);
}

Effects::EqSettings SourceEq::read (const juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
{
    auto get = [&] (const juce::String& suffix, float fallback)
    {
        const auto* value = apvts.getRawParameterValue (prefix + suffix);
        return value != nullptr ? value->load() : fallback;
    };
    Effects::EqSettings s;
    s.hp = get ("EqHp", 20.0f);
    s.lp = get ("EqLp", 20000.0f);
    for (int b = 0; b < Effects::eqBands; ++b)
    {
        const auto key = "Eq" + juce::String (b + 1);
        s.freq[b] = get (key + "F", s.freq[b]);
        s.gain[b] = get (key + "G", 0.0f);
        s.q[b] = get (key + "Q", s.q[b]);
    }
    return s;
}

SourceEq::SourceEq (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix)
{
    values[0] = apvts.getRawParameterValue (prefix + "EqHp");
    values[1] = apvts.getRawParameterValue (prefix + "EqLp");
    for (int b = 0; b < Effects::eqBands; ++b)
    {
        const auto key = prefix + "Eq" + juce::String (b + 1);
        values[(size_t) (2 + b * 3)] = apvts.getRawParameterValue (key + "F");
        values[(size_t) (3 + b * 3)] = apvts.getRawParameterValue (key + "G");
        values[(size_t) (4 + b * 3)] = apvts.getRawParameterValue (key + "Q");
    }
}

void SourceEq::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    haveCoefficients = false;
    reset();
}

void SourceEq::reset()
{
    for (auto& channel : filters)
        for (auto& filter : channel)
            filter.reset();
}

Effects::EqSettings SourceEq::settings() const
{
    Effects::EqSettings s;
    if (values[0] == nullptr)
        return s;
    s.hp = values[0]->load();
    s.lp = values[1]->load();
    for (int b = 0; b < Effects::eqBands; ++b)
    {
        s.freq[b] = values[(size_t) (2 + b * 3)]->load();
        s.gain[b] = values[(size_t) (3 + b * 3)]->load();
        s.q[b] = values[(size_t) (4 + b * 3)]->load();
    }
    return s;
}

bool SourceEq::process (float* left, float* right, int numSamples)
{
    const auto s = settings();
    if (s.isNeutral())
    {
        wasNeutral = true;
        return false;
    }

    if (wasNeutral)
        reset(); // inget gammalt tillstånd från förra gången EQ:n var på
    if (! haveCoefficients || wasNeutral || ! sameSettings (s, last))
    {
        const auto coefficients = Effects::eqCoefficients (sampleRate, s);
        for (auto& channel : filters)
            for (size_t f = 0; f < numFilters; ++f)
                channel[f].coefficients = coefficients[f];
        last = s;
        haveCoefficients = true;
    }
    wasNeutral = false;

    for (int channel = 0; channel < (right != nullptr ? 2 : 1); ++channel)
    {
        auto* data = channel == 0 ? left : right;
        auto& chain = filters[(size_t) channel];
        for (int i = 0; i < numSamples; ++i)
        {
            auto x = data[i];
            for (auto& filter : chain)
                x = filter.processSample (x);
            data[i] = x;
        }
    }
    return true;
}
