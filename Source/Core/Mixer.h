#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Effects.h"
#include "Pattern.h"

// Mixern: varje ljudkälla (kicken, instrumenten och de fem effektkanalernas retur) har en
// gain-fader, panorering, en mätare och två brytare: om sidechainen ducker källan, och om den går
// genom maximizern (master) eller förbi den.
class Mixer
{
public:
    static constexpr int numLegacySources = 1 + maxInstruments + numLegacyEffectChannels; // källorna som fanns före FX12 (registreras först i layouten)
    static constexpr int numSources = 1 + maxInstruments + numEffectChannels; // kick, instrument 1-10, FX1-FX12
    static constexpr int kickSource = 0, firstBassSource = 1, firstFxSource = 1 + maxInstruments;
    static constexpr float minGainDb = -60.0f, maxGainDb = 12.0f;

    static juce::String sourceName (int source);                         // "KICK", "INSTR 1", "FX1"
    static juce::String paramID (int source, const juce::String& field); // field: Gain, Pan, Duck, Max
    static bool hasDuck (int source) { return source != kickSource; }   // kicken ducker inte sig själv

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
    // SOUNDSTAGE-val per källa ("Stage" = med i stereobreddningen). Läggs sist i layouten.
    static void addStageParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
    // Källor efter de gamla (FX12): fader, pan, duck, max och stage. Anropas SIST i layouten så att inga gamla index flyttas.
    static void addExtraSourceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    explicit Mixer (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate);

    // Läser parametrarna. Anropas en gång per block.
    void beginBlock();

    bool inStage (int source) const { return stage[(size_t) source] == nullptr || stage[(size_t) source]->load() >= 0.5f; }
    bool duckEnabled (int source) const { return duck[(size_t) source] == nullptr || duck[(size_t) source]->load() >= 0.5f; }

    // Lägger källan (mono om `right` är nullptr) på maximizerns buss eller förbi den, efter gain
    // och panorering, och uppdaterar mätaren. `level` är en extra faktor (t.ex. den torra andelen).
    // Mätaren visar hela källan efter fadern (före `level`). Anges sendLeft/sendRight får de källan efter
    // fader och panorering (utan `level`), så att det som skickas till en effektkanal följer fadern också.
    void mix (int source, const float* left, const float* right, int numSamples, float level,
              juce::AudioBuffer<float>& maxBus, juce::AudioBuffer<float>& bypassBus,
              float* sendLeft = nullptr, float* sendRight = nullptr, juce::AudioBuffer<float>* skipBus = nullptr, float levelFrom = -1.0f);
    // skipBus: källor med SOUNDSTAGE-valet av (och Max på) läggs här i stället för på maxBus.
    // levelFrom >= 0: `level` glider linjärt från det värdet över blocket (ändrad torr andel utan klick); annars konstant.

    // Parameterlås (pianorullen): panoreringen -1..1 som ersätter pan-parametern från nu tills nästa anrop; utanför -1..1 = inget lås.
    // Bara ljudtråden. Går via samma 20 ms-ramp som panoreringen, så det klickar inte.
    void setPanLock (int source, float pan);

    // Högsta topp (dB) sedan förra anropet, per kanal (0 vänster, 1 höger). Anropas av gränssnittet.
    float takePeakDb (int source, int channel) const;

private:
    static void addSourceRange (juce::AudioProcessorValueTreeState::ParameterLayout& layout, int firstSource, int endSource);

    struct Strip
    {
        juce::SmoothedValue<float> gain, panLeft, panRight;
        float lastDb = std::numeric_limits<float>::quiet_NaN(), lastGain = 0.0f; // senast omräknade gain-värdet
        float panLock = 2.0f; // utanför -1..1 = inget lås
    };
    static void panGains (float pan, float& left, float& right)
    {
        // Balans: mitten ger oförändrad nivå, åt ena hållet tystnar den andra sidan.
        left = pan > 0.0f ? 1.0f - pan : 1.0f;
        right = pan < 0.0f ? 1.0f + pan : 1.0f;
    }

    std::array<std::atomic<float>*, numSources> gainParam {}, panParam {}, duck {}, max {}, stage {};
    std::array<Strip, numSources> strips;
    mutable std::array<std::array<std::atomic<float>, 2>, numSources> peaks {};
    double sampleRate = 44100.0;
    bool fresh = true;
};
