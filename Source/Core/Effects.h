#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Basens effekter. Varje effekt finns en gång, med egna parametrar; ordningen och om den
// ligger före eller efter gaten väljs av användaren.
enum class EffectType { filter, distortion, eq, chorus, phaser, echo, reverb };
constexpr int numEffects = 7;

namespace Effects
{
    juce::String name (EffectType type);         // "FILTER" ...
    juce::String parameterPrefix (EffectType type); // "fxFilter" ... (alla effektens parameter-ID börjar så)
    bool isEffectParameter (const juce::String& paramID);

    // Tempo-synkade tider för echo, i fjärdedelar.
    juce::StringArray echoTimeNames();
    double echoTimeInQuarters (int index);

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
}

// Effekternas ordning. Gränssnittet skriver, ljudtråden läser (atomiskt, inga lås).
struct EffectOrder
{
    EffectOrder() { reset(); }

    void reset()
    {
        for (int i = 0; i < numEffects; ++i)
            slots[(size_t) i] = i;
    }

    EffectType at (int slot) const { return (EffectType) slots[(size_t) slot].load(); }

    // Flyttar effekten på plats `from` till plats `to`.
    void move (int from, int to);

    juce::String toString() const;
    void fromString (const juce::String& text);

    std::array<std::atomic<int>, numEffects> slots;
};

class EffectChain
{
public:
    explicit EffectChain (juce::AudioProcessorValueTreeState& apvts);
    ~EffectChain();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Läser parametrarna. Anropas en gång per block, före process.
    void update (double bpm);

    // Kör de påslagna effekterna som ligger före (postGate = false) eller efter gaten,
    // i ordningen `order`. `buffer` är stereo.
    void process (juce::AudioBuffer<float>& buffer, int numSamples, bool postGate, const EffectOrder& order);

    // Finns det någon påslagen effekt på den sidan av gaten?
    bool hasActive (bool postGate) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
