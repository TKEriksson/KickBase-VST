#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

// LFO:er och vart de modulerar.
//
// Varje LFO har två mål med var sin bipolär mängd. Moduleringen läggs på i parameterns
// normaliserade skala (0..1), så att t.ex. +20 % på cutoff betyder 20 % av rattens vandring.
// Den räknas en gång per ljudblock.
namespace Modulation
{
    constexpr int numLfos = 2;
    constexpr int routesPerLfo = 2;

    // Mål. Ordningen är den som visas i listorna; lägg nya sist.
    enum class Destination
    {
        none,
        bassCutoff, bassResonance, bassEnvAmount, bassDrive, bassLevel, bassTune,
        kickPitch, kickDecay,
        filterCutoff, filterReso, distDrive, echoMix, echoFeedback, reverbMix, phaserMix, chorusMix,
        duckAmount,
        count
    };

    juce::StringArray destinationNames();
    const char* destinationParamID (Destination destination); // nullptr för none
    juce::StringArray shapeNames();
    juce::StringArray rateNames();
    double rateInQuarters (int index);

    juce::String paramID (int lfo, const char* suffix); // "lfo1Shape" ...

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    // LFO:ns värde -1..1 för en form och fas (0..1). `cycle` används för random.
    float shapeValue (int shape, double phase, juce::int64 cycle);
}

// Räknar ut LFO:erna och moduleringen för ett block.
class Modulator
{
public:
    explicit Modulator (juce::AudioProcessorValueTreeState& apvts);

    // Anropas en gång per block med positionen i fjärdedelar (från TransportClock).
    void update (double ppq);

    // Parameterns värde med modulering, i parameterns egen enhet. `fallback` är det
    // omodulerade värdet som redan lästs (returneras om inget modulerar målet).
    float apply (Modulation::Destination destination, float fallback) const;
    bool isModulated (Modulation::Destination destination) const;

    // För gränssnittet: varje LFO:s fas och värde.
    std::array<std::atomic<float>, Modulation::numLfos> phase {}, value {};

private:
    struct Lfo
    {
        std::atomic<float>* on;
        std::atomic<float>* shape;
        std::atomic<float>* rate;
        std::array<std::atomic<float>*, Modulation::routesPerLfo> destination, amount;
    };

    std::array<Lfo, Modulation::numLfos> lfos;
    std::array<juce::RangedAudioParameter*, (size_t) Modulation::Destination::count> targets {};
    std::array<float, (size_t) Modulation::Destination::count> offsets {};
};
