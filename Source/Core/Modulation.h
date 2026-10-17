#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "NodeEnvelope.h"
#include "Pattern.h"

// Modulationskällor och vart de modulerar.
//
// Som i Massive och Serum: en källa dras till valfri ratt (eller sätts i matrisen), och en koppling (route) säger hur mycket den
// rör ratten. Moduleringen läggs på i parameterns normaliserade skala (0..1), så att +20 % betyder 20 % av rattens vandring.
// Den räknas en gång per ljudblock.
//
// Källor (numren används i sparade projekt): 0-2 LFO 1-3, 3-6 makro 1-4 (rattar man vrider själv), sedan fyra per instrument:
// ENV 2, ENV 3, VELOCITY (senaste tonens) och RANDOM (ett slumptal per ton).
namespace Modulation
{
    constexpr int numLfos = 3;
    constexpr int numMacros = 4;
    constexpr int instrumentSources = 4;
    constexpr int firstMacro = numLfos;
    constexpr int firstInstrumentSource = numLfos + numMacros;
    constexpr int numSources = firstInstrumentSource + maxInstruments * instrumentSources;
    constexpr int maxRoutes = 64;

    inline bool isLfo (int source)             { return source >= 0 && source < numLfos; }
    inline bool isMacro (int source)           { return source >= firstMacro && source < firstInstrumentSource; }
    inline bool isInstrumentSource (int source) { return source >= firstInstrumentSource && source < numSources; }
    inline int instrumentSource (int layer, int kind) { return firstInstrumentSource + layer * instrumentSources + kind; }
    inline int layerOfSource (int source)      { return (source - firstInstrumentSource) / instrumentSources; }
    inline int kindOfSource (int source)       { return (source - firstInstrumentSource) % instrumentSources; }
    juce::String macroID (int macro);          // "macro1" ...
    juce::String kindName (int kind);          // "ENV 2", "ENV 3", "VELOCITY", "RANDOM"

    juce::StringArray shapeNames();
    juce::StringArray rateNames();
    double rateInQuarters (int index);

    juce::String paramID (int lfo, const char* suffix); // "lfo1Shape" ...
    juce::String sourceName (int source);               // "LFO 1", "MACRO 2", "INSTR 3 ENV 2" ...

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

    // Anropas en gång per block med positionen i fjärdedelar (från TransportClock) och tempot (för LFO:er som går i Hz).
    void update (double ppq, double bpm = 120.0);

    // LFO:ernas egna kurvor (formen Custom): x = fas 0..1, y = 0..1 (uppåt = positivt). Ägs av processorn.
    void setLfoCurves (const std::array<NodeEnvelope, Modulation::numLfos>* curves) { lfoCurves = curves; }

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

    // Instrumentens källor, skrivs av ljudtråden före update(): ENV 2, ENV 3, velocity och slump.
    void setInstrumentSources (int layer, float env2, float env3, float velocity, float random)
    {
        auto& s = layerSources[(size_t) layer];
        s = { env2, env3, velocity, random };
    }

    // Källans värde just nu (för gränssnittets visning): LFO -1..1, övriga 0..1.
    float sourceValue (int source) const { return juce::isPositiveAndBelow (source, Modulation::numSources) ? sourceDisplay[(size_t) source].load() : 0.0f; }

private:
    struct Lfo
    {
        std::atomic<float>* on;
        std::atomic<float>* shape;
        std::atomic<float>* rate;
        std::atomic<float>* mode;  // 0 = tempo-synkad (rate), 1 = fri hastighet (hz)
        std::atomic<float>* hz;
    };
    const std::array<NodeEnvelope, Modulation::numLfos>* lfoCurves = nullptr;

    const ModRoutes& routes;
    std::array<Lfo, Modulation::numLfos> lfos;
    std::array<std::atomic<float>*, Modulation::numMacros> macros {};
    std::array<std::array<float, Modulation::instrumentSources>, maxInstruments> layerSources {};
    std::array<std::atomic<float>, Modulation::numSources> sourceDisplay {};
    std::vector<juce::RangedAudioParameter*> parameters;
    std::vector<const std::atomic<float>*> raws;
    std::vector<float> offsets;                          // ljudtråden
    std::unique_ptr<std::atomic<float>[]> displayOffsets; // kopia för gränssnittet
    std::vector<int> touched;                            // index som fick modulering förra blocket
    bool anyActive = false;
};
