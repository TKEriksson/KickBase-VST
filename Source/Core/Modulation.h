#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

// LFO:er och vart de modulerar.
//
// Som i Massive: en LFO dras till valfri ratt, och en koppling (route) säger hur mycket den
// rör ratten. Moduleringen läggs på i parameterns normaliserade skala (0..1), så att +20 %
// betyder 20 % av rattens vandring. Den räknas en gång per ljudblock.
namespace Modulation
{
    constexpr int numLfos = 3;
    constexpr int maxRoutes = 24;

    juce::StringArray shapeNames();
    juce::StringArray rateNames();
    double rateInQuarters (int index);

    juce::String paramID (int lfo, const char* suffix); // "lfo1Shape" ...
    juce::String sourceName (int lfo);                  // "LFO 1" ...

    // Kan en LFO dras till den här parametern? (Rattar för ljudet: kick, bas, effekter, ducking.)
    bool isModulatable (const juce::RangedAudioParameter& parameter);

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    // LFO:ns värde -1..1 för en form och fas (0..1). `cycle` används för random.
    float shapeValue (int shape, double phase, juce::int64 cycle);
}

// Kopplingarna från LFO:er till parametrar. Gränssnittet skriver, ljudtråden läser; varje fält
// är atomiskt och en koppling är aktiv först när `source` sätts (sist), så att ljudtråden
// aldrig ser en halvfärdig koppling som aktiv.
class ModRoutes
{
public:
    struct Route
    {
        std::atomic<int> source { -1 };   // LFO-index, -1 = ledig
        std::atomic<int> target { -1 };   // parameterns index i processorns parameterlista
        std::atomic<float> amount { 0.0f };
    };

    // Lägger till (eller hittar) en koppling. Returnerar dess plats, eller -1 om alla är upptagna.
    int add (int source, int target, float amount);
    void remove (int index);
    void clear();
    int find (int source, int target) const;

    // Sparas med parameterns ID (inte index), så att projekt klarar ändrade parameterlistor.
    static const juce::Identifier treeType;
    juce::ValueTree toValueTree (const juce::Array<juce::AudioProcessorParameter*>& parameters) const;
    void fromValueTree (const juce::ValueTree& tree, const juce::Array<juce::AudioProcessorParameter*>& parameters);

    std::array<Route, Modulation::maxRoutes> routes;
    std::atomic<int> version { 0 }; // ökas vid ändring, så att gränssnittet ritar om
};

// Räknar ut LFO:erna och moduleringen för ett block.
class Modulator
{
public:
    Modulator (juce::AudioProcessorValueTreeState& apvts, const ModRoutes& routes);

    // Anropas en gång per block med positionen i fjärdedelar (från TransportClock).
    void update (double ppq);

    // Parameterns värde med modulering, i parameterns egen enhet. `index` är parameterns
    // plats i processorns parameterlista; `value` det omodulerade värdet.
    float apply (int index, float value) const;

    // Samma sak för den som läser parametrarnas råa värden (t.ex. effekterna).
    float applyRaw (const std::atomic<float>* raw, float value) const;

    int indexOf (const juce::String& paramID) const;

    // För gränssnittet: varje LFO:s fas och värde, och hur mycket varje parameter moduleras nu
    // (normaliserat, summan av alla kopplingar).
    std::array<std::atomic<float>, Modulation::numLfos> phase {}, value {};
    float currentOffset (int index) const;

private:
    struct Lfo
    {
        std::atomic<float>* on;
        std::atomic<float>* shape;
        std::atomic<float>* rate;
    };

    const ModRoutes& routes;
    std::array<Lfo, Modulation::numLfos> lfos;
    std::vector<juce::RangedAudioParameter*> parameters;
    std::vector<const std::atomic<float>*> raws;
    std::vector<float> offsets;                          // ljudtråden
    std::unique_ptr<std::atomic<float>[]> displayOffsets; // kopia för gränssnittet
    std::vector<int> touched;                            // index som fick modulering förra blocket
    bool anyActive = false;
};
