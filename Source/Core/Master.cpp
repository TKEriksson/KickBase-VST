#include "Master.h"

namespace
{
    constexpr int numExtraDelays = 4; // kickutgången och de tre baslagrens utgångar
}

MasterSection::MasterSection (juce::AudioProcessorValueTreeState& apvts)
    : on (apvts.getRawParameterValue ("masterOn")),
      driveDb (apvts.getRawParameterValue ("masterDrive")),
      clip (apvts.getRawParameterValue ("masterClip")),
      ceilingDb (apvts.getRawParameterValue ("masterCeiling")),
      releaseMs (apvts.getRawParameterValue ("masterRelease"))
{
    jassert (on != nullptr && driveDb != nullptr && clip != nullptr && ceilingDb != nullptr && releaseMs != nullptr);
}

void MasterSection::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    auto db = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "masterOn", 1 }, "Master On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "masterDrive", 1 }, "Master Drive",
                                                       NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "masterClip", 1 }, "Master Clip",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "masterCeiling", 1 }, "Master Ceiling",
                                                       NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    NormalisableRange<float> release (5.0f, 500.0f, 0.1f);
    release.setSkewForCentre (80.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "masterRelease", 1 }, "Master Release", release, 60.0f,
                                                       AudioParameterFloatAttributes().withStringFromValueFunction (
                                                           [] (float v, int) { return String (roundToInt (v)) + " ms"; })));
}

void MasterSection::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;
    channels = std::max (1, numChannels);
    lookahead = std::max (2, (int) std::round (lookaheadSeconds * sampleRate));

    dcCoefficient = (float) (1.0 - juce::MathConstants<double>::twoPi * 5.0 / sampleRate);
    delayLines.assign ((size_t) channels, std::vector<float> ((size_t) lookahead, 0.0f));
    minWindow.assign ((size_t) lookahead, 1.0f);
    boxWindow.assign ((size_t) lookahead, 1.0f);
    extraDelays.assign (numExtraDelays, std::vector<std::vector<float>> (2, std::vector<float> ((size_t) lookahead, 0.0f)));
    extraIndex.assign (numExtraDelays, 0);
    reset();
}

void MasterSection::reset()
{
    dcInput.assign ((size_t) channels, 0.0f);
    dcOutput.assign ((size_t) channels, 0.0f);
    for (auto& line : delayLines)
        std::fill (line.begin(), line.end(), 0.0f);
    std::fill (minWindow.begin(), minWindow.end(), 1.0f);
    std::fill (boxWindow.begin(), boxWindow.end(), 1.0f);
    boxSum = (double) lookahead;
    released = 1.0f;
    writeIndex = 0;
}

void MasterSection::process (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const auto enabled = on->load() >= 0.5f;
    const auto driveGain = juce::Decibels::decibelsToGain (driveDb->load());
    const auto clipAmount = clip->load();
    const auto ceiling = juce::Decibels::decibelsToGain (ceilingDb->load());
    const auto releaseCoefficient = (float) (1.0 - std::exp (-1.0 / (std::max (1.0f, releaseMs->load()) * 0.001 * sampleRate)));
    const auto numChannels = std::min (channels, buffer.getNumChannels());
    const auto window = lookahead;

    auto peak = 0.0f, lowestGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. DC-filter, drive och soft clip (tanh, blandat med det rena ljudet efter clip-mängd).
        auto inputPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto x = buffer.getSample (ch, i);
            if (enabled)
            {
                const auto blocked = x - dcInput[(size_t) ch] + dcCoefficient * dcOutput[(size_t) ch];
                dcInput[(size_t) ch] = x;
                dcOutput[(size_t) ch] = blocked;
                x = blocked * driveGain;
                x += (std::tanh (x) - x) * clipAmount;
            }
            buffer.setSample (ch, i, x);
            inputPeak = std::max (inputPeak, std::abs (x));
        }

        // 2. Vilken förstärkning toppen kräver, med release: sjunker direkt, stiger långsamt.
        const auto target = enabled && inputPeak > ceiling ? ceiling / inputPeak : 1.0f;
        released = std::min (target, released + (1.0f - released) * releaseCoefficient);

        // 3. Minsta värdet över fönstret, och sedan medelvärdet över fönstret: förstärkningen
        //    hinner glida ner under lookahead-tiden och är som lägst när toppen når utgången.
        minWindow[(size_t) writeIndex] = released;
        auto windowMin = 1.0f;
        for (const auto value : minWindow)
            windowMin = std::min (windowMin, value);

        boxSum += windowMin - boxWindow[(size_t) writeIndex];
        boxWindow[(size_t) writeIndex] = windowMin;
        const auto gain = (float) (boxSum / window);

        // 4. Fördröj ljudet, så att förstärkningen ligger före toppen.
        const auto readIndex = (writeIndex + 1) % window; // äldsta = window - 1 samples bakåt
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& line = delayLines[(size_t) ch];
            line[(size_t) writeIndex] = buffer.getSample (ch, i);
            auto y = line[(size_t) readIndex] * (enabled ? gain : 1.0f);
            if (enabled)
                y = juce::jlimit (-ceiling, ceiling, y); // sista skydd mot avrundningsfel
            buffer.setSample (ch, i, y);
            peak = std::max (peak, std::abs (y));
        }

        lowestGain = std::min (lowestGain, enabled ? gain : 1.0f);
        writeIndex = readIndex;
    }

    // Mätarna: toppvärde och mest förstärkningsminskning i blocket (med långsamt fall).
    const auto peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    outputPeakDb = std::max (peakDb, outputPeakDb.load() - 1.5f);
    const auto reduction = juce::Decibels::gainToDecibels (lowestGain, -60.0f);
    gainReductionDb = std::min (reduction, gainReductionDb.load() + 1.0f);
}

void MasterSection::delayOnly (juce::AudioBuffer<float>& buffer, int numSamples, int delaySlot)
{
    if (! juce::isPositiveAndBelow (delaySlot, numExtraDelays))
        return;

    auto& lines = extraDelays[(size_t) delaySlot];
    auto& index = extraIndex[(size_t) delaySlot];
    const auto window = lookahead;
    const auto numChannels = std::min (2, buffer.getNumChannels());

    for (int i = 0; i < numSamples; ++i)
    {
        const auto readIndex = (index + 1) % window;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& line = lines[(size_t) ch];
            line[(size_t) index] = buffer.getSample (ch, i);
            buffer.setSample (ch, i, line[(size_t) readIndex]);
        }
        index = readIndex;
    }
}
