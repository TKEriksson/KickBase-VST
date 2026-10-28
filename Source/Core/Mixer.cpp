#include "Mixer.h"

using namespace juce;

namespace
{
    String formatDb (float v)
    {
        return v <= Mixer::minGainDb + 0.05f ? String ("-inf") : (v > 0.0f ? "+" : "") + String (v, 1) + " dB";
    }
}

String Mixer::sourceName (int source)
{
    if (source == kickSource)
        return "KICK";
    if (source < firstFxSource)
        return "INSTR " + String (source - firstBassSource + 1);
    return Effects::channelLabel (source - firstFxSource);
}

String Mixer::paramID (int source, const String& field)
{
    String name = source == kickSource ? "Kick"
                : source < firstFxSource ? "Bass" + String (source - firstBassSource + 1)
                                         : "Fx" + String::charToString ((juce_wchar) ('A' + (source - firstFxSource)));
    return "mix" + name + field;
}

void Mixer::addParameters (AudioProcessorValueTreeState::ParameterLayout& layout)
{
    for (int s = 0; s < numSources; ++s)
    {
        const auto title = "Mixer " + (s == kickSource ? String ("Kick") : s < firstFxSource ? "Instr " + String (s - firstBassSource + 1) : sourceName (s)) + " ";

        NormalisableRange<float> gainRange (minGainDb, maxGainDb, 0.1f);
        gainRange.setSkewForCentre (-12.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramID (s, "Gain"), 1 }, title + "Gain", gainRange, 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatDb (v); })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramID (s, "Pan"), 1 }, title + "Pan",
                                                           NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
                                                               const auto p = roundToInt (std::abs (v) * 100.0f);
                                                               return p == 0 ? String ("C") : String (p) + (v < 0.0f ? " L" : " R"); })));
        if (hasDuck (s))
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (s, "Duck"), 1 }, title + "Sidechain", true));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (s, "Max"), 1 }, title + "Maximizer", true));
    }
}

void Mixer::addStageParameters (AudioProcessorValueTreeState::ParameterLayout& layout)
{
    for (int s = 0; s < numSources; ++s)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (s, "Stage"), 1 }, "Soundstage " + sourceName (s), true));
}

Mixer::Mixer (AudioProcessorValueTreeState& apvts)
{
    for (int s = 0; s < numSources; ++s)
    {
        gainParam[(size_t) s] = apvts.getRawParameterValue (paramID (s, "Gain"));
        panParam[(size_t) s]  = apvts.getRawParameterValue (paramID (s, "Pan"));
        duck[(size_t) s]      = hasDuck (s) ? apvts.getRawParameterValue (paramID (s, "Duck")) : nullptr;
        max[(size_t) s]       = apvts.getRawParameterValue (paramID (s, "Max"));
        stage[(size_t) s]     = apvts.getRawParameterValue (paramID (s, "Stage"));
        for (auto& peak : peaks[(size_t) s])
            peak = 0.0f;
    }
}

void Mixer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& strip : strips)
    {
        strip.gain.reset (sampleRate, 0.02);
        strip.panLeft.reset (sampleRate, 0.02);
        strip.panRight.reset (sampleRate, 0.02);
    }
    fresh = true;
}

void Mixer::beginBlock()
{
    for (int s = 0; s < numSources; ++s)
    {
        auto& strip = strips[(size_t) s];
        const auto db = gainParam[(size_t) s]->load();
        if (db != strip.lastDb)
        {
            strip.lastDb = db;
            strip.lastGain = db <= minGainDb + 0.05f ? 0.0f : Decibels::decibelsToGain (db);
        }
        const auto gain = strip.lastGain;
        const auto pan = jlimit (-1.0f, 1.0f, panParam[(size_t) s]->load());
        // Balans: mitten ger oförändrad nivå, åt ena hållet tystnar den andra sidan.
        const auto left = pan > 0.0f ? 1.0f - pan : 1.0f, right = pan < 0.0f ? 1.0f + pan : 1.0f;

        if (fresh)
        {
            strip.gain.setCurrentAndTargetValue (gain);
            strip.panLeft.setCurrentAndTargetValue (left);
            strip.panRight.setCurrentAndTargetValue (right);
        }
        else
        {
            strip.gain.setTargetValue (gain);
            strip.panLeft.setTargetValue (left);
            strip.panRight.setTargetValue (right);
        }
    }
    fresh = false;
}

void Mixer::mix (int source, const float* left, const float* right, int numSamples, float level,
                 AudioBuffer<float>& maxBus, AudioBuffer<float>& bypassBus, float* sendLeft, float* sendRight, AudioBuffer<float>* skipBus)
{
    auto& strip = strips[(size_t) source];
    const auto toMax = max[(size_t) source]->load() >= 0.5f;
    auto& bus = ! toMax ? bypassBus : (skipBus != nullptr && stage[(size_t) source]->load() < 0.5f ? *skipBus : maxBus);
    auto* outLeft = bus.getWritePointer (0);
    auto* outRight = bus.getWritePointer (1);
    float peakLeft = 0.0f, peakRight = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = strip.gain.getNextValue();
        const auto l = left[i] * gain * strip.panLeft.getNextValue();
        const auto r = (right != nullptr ? right[i] : left[i]) * gain * strip.panRight.getNextValue();
        outLeft[i] += l * level;
        outRight[i] += r * level;
        if (sendLeft != nullptr)
            sendLeft[i] = l;
        if (sendRight != nullptr)
            sendRight[i] = r;
        peakLeft = std::max (peakLeft, std::abs (l));
        peakRight = std::max (peakRight, std::abs (r));
    }

    auto& peak = peaks[(size_t) source];
    if (peakLeft > peak[0].load())  peak[0].store (peakLeft);
    if (peakRight > peak[1].load()) peak[1].store (peakRight);
}

float Mixer::takePeakDb (int source, int channel) const
{
    return Decibels::gainToDecibels (peaks[(size_t) source][(size_t) channel].exchange (0.0f), -100.0f);
}
