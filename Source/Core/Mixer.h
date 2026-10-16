#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Effects.h"

// Mixern: varje ljudkälla (kicken, de tre baslagren och de fem effektkanalernas retur) har en
// gain-fader, panorering, en mätare och två brytare: om sidechainen ducker källan, och om den går
// genom maximizern (master) eller förbi den.
class Mixer
{
public:
    static constexpr int numSources = 4 + numEffectChannels; // kick, bas 1-3, FX A-E
    static constexpr int kickSource = 0, firstBassSource = 1, firstFxSource = 4;
    static constexpr float minGainDb = -60.0f, maxGainDb = 12.0f;

    static juce::String sourceName (int source);                         // "KICK", "BASS 1", "FX A"
    static juce::String paramID (int source, const juce::String& field); // field: Gain, Pan, Duck, Max
    static bool hasDuck (int source) { return source != kickSource; }   // kicken ducker inte sig själv

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    explicit Mixer (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate);

    // Läser parametrarna. Anropas en gång per block.
    void beginBlock();

    bool duckEnabled (int source) const { return duck[(size_t) source] == nullptr || duck[(size_t) source]->load() >= 0.5f; }

    // Lägger källan (mono om `right` är nullptr) på maximizerns buss eller förbi den, efter gain
    // och panorering, och uppdaterar mätaren. `level` är en extra faktor (t.ex. den torra andelen).
    void mix (int source, const float* left, const float* right, int numSamples, float level,
              juce::AudioBuffer<float>& maxBus, juce::AudioBuffer<float>& bypassBus);

    // Högsta topp (dB) sedan förra anropet, per kanal (0 vänster, 1 höger). Anropas av gränssnittet.
    float takePeakDb (int source, int channel) const;

private:
    struct Strip
    {
        juce::SmoothedValue<float> gain, panLeft, panRight;
    };

    std::array<std::atomic<float>*, numSources> gainParam {}, panParam {}, duck {}, max {};
    std::array<Strip, numSources> strips;
    mutable std::array<std::array<std::atomic<float>, 2>, numSources> peaks {};
    double sampleRate = 44100.0;
    bool fresh = true;
};
