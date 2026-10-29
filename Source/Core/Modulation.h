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
// Den räknas var 32:a sample (PluginBase::processInControlChunks), oavsett DAW:ens buffertstorlek.
//
// Källor (numren används i sparade projekt): 0-2 LFO 1-3, 3-6 makro 1-4 (rattar man vrider själv), sedan fyra per instrument:
// ENV 2, ENV 3, VELOCITY (senaste tonens) och RANDOM (ett slumptal per ton). Sedan 47-53: LFO 4-10. Sist 54-73: två per instrument,
// AMP ENV (ENV 1, volymen) och FILT ENV (filter-envelopens form), i ett eget block så att inget äldre nummer flyttas.
namespace Modulation
{
    // Kontrolltakten: moduleringen (och det som räknas per fönster, t.ex. auto-filtrets svep) uppdateras på ett rutnät av fönster om
    // 32 samples räknat från prepareToPlay, oavsett hur DAW:en delar blocken (PluginBase::processInControlChunks).
    constexpr int controlInterval = 32;
    constexpr int numLfos = 3;
    constexpr int numMacros = 4;
    constexpr int instrumentSources = 4;
    constexpr int firstMacro = numLfos;
    constexpr int firstInstrumentSource = numLfos + numMacros;
    // LFO 4-10 (maxLfos) ligger sist bland källorna (47-53), så att sparade projekt med makron och instrumentkällor på sina gamla nummer läses som förut.
    constexpr int maxLfos = 10;
    constexpr int firstExtraLfoSource = firstInstrumentSource + maxInstruments * instrumentSources;
    constexpr int firstEnvSource = firstExtraLfoSource + (maxLfos - numLfos);
    constexpr int envSources = 2;                                    // AMP ENV, FILT ENV
    constexpr int instrumentKinds = instrumentSources + envSources;  // kind 0-3 i första blocket, 4-5 i blocket från 54
    // CODE 1-8 (74-81): LFO/ENV som Live Code lägger på (.lfo/.env). Sparas aldrig; kopplingarna från dem rensas vid save, load och stopp.
    constexpr int firstCodeSource = firstEnvSource + maxInstruments * envSources;
    constexpr int numCodeSources = 8;
    constexpr int numSources = firstCodeSource + numCodeSources;
    constexpr int maxRoutes = 64;
    constexpr int maxCodeRoutes = 32; // så många av de 64 kopplingarna får koden äga (resten är kvar åt handritade)

    inline int lfoSource (int lfo)             { return lfo < numLfos ? lfo : firstExtraLfoSource + (lfo - numLfos); }
    inline int lfoIndexOfSource (int source)   { return source >= 0 && source < numLfos ? source : source >= firstExtraLfoSource && source < firstEnvSource ? numLfos + (source - firstExtraLfoSource) : -1; }
    inline bool isLfoSource (int source)       { return lfoIndexOfSource (source) >= 0; }
    inline bool isLfo (int source)             { return source >= 0 && source < numLfos; } // bara de tre första (källa 0-2); använd isLfoSource för alla
    inline bool isMacro (int source)           { return source >= firstMacro && source < firstInstrumentSource; }
    inline bool isEnvSource (int source)       { return source >= firstEnvSource && source < firstCodeSource; } // AMP ENV / FILT ENV
    inline bool isCodeSource (int source)      { return source >= firstCodeSource && source < numSources; }
    inline bool isInstrumentSource (int source) { return (source >= firstInstrumentSource && source < firstExtraLfoSource) || isEnvSource (source); }
    inline int instrumentSource (int layer, int kind)
    {
        return kind < instrumentSources ? firstInstrumentSource + layer * instrumentSources + kind : firstEnvSource + layer * envSources + (kind - instrumentSources);
    }
    inline int layerOfSource (int source)      { return isEnvSource (source) ? (source - firstEnvSource) / envSources : (source - firstInstrumentSource) / instrumentSources; }
    inline int kindOfSource (int source)       { return isEnvSource (source) ? instrumentSources + (source - firstEnvSource) % envSources : (source - firstInstrumentSource) % instrumentSources; }
    juce::String macroID (int macro);          // "macro1" ...
    juce::String kindName (int kind);          // "ENV 2", "ENV 3", "VELOCITY", "RANDOM", "AMP ENV", "FILT ENV"

    juce::StringArray shapeNames();
    juce::StringArray rateNames();
    double rateInQuarters (int index);

    // Tiderna för ENV 2 och ENV 3 i läget Sync: 0, 1/64 ... 4 bars (T = triol, D = punkterad). Längden i fjärdedelar (4 = en takt).
    juce::StringArray syncTimeNames();
    double syncTimeInQuarters (int index);

    juce::String paramID (int lfo, const char* suffix); // "lfo1Shape" ... (lfo = index 0..9)
    juce::String sourceName (int source);               // "LFO 1", "MACRO 2", "INSTR 3 ENV 2", "LFO 4" (källa 47) ...

    // Kan en LFO dras till den här parametern? (Rattar för ljudet: kick, bas, effekter, ducking.)
    bool isModulatable (const juce::RangedAudioParameter& parameter);

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    // LFO 4-10 (lfo4On ... lfo10SnapRange). Anropas sist i instrumentets layout (nya parametrar sist); effekterna har bara LFO 1-3.
    void addExtraLfoParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

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

    // Anropas en gång per bit om 32 samples med positionen i fjärdedelar (från TransportClock) och tempot (för LFO:er som går i Hz).
    void update (double ppq, double bpm = 120.0);

    // LFO:ernas egna kurvor (formen Custom): x = fas 0..1, y = 0..1 (uppåt = positivt). Ägs av processorn.
    void setLfoCurves (const std::array<NodeEnvelope, Modulation::maxLfos>* curves) { lfoCurves = curves; }

    // Parameterns värde med modulering, i parameterns egen enhet. `index` är parameterns
    // plats i processorns parameterlista; `value` det omodulerade värdet.
    float apply (int index, float value) const;

    // Samma sak för den som läser parametrarnas råa värden (t.ex. effekterna).
    float applyRaw (const std::atomic<float>* raw, float value) const;

    int indexOf (const juce::String& paramID) const;

    // Moduleras något just nu (någon koppling med en påslagen källa, från senaste update)?
    bool isActive() const { return anyActive; }
    const std::vector<int>& touchedParameters() const { return touched; } // parametrarna (processorns index) som moduleras nu

    // För gränssnittet: varje LFO:s fas och värde, och hur mycket varje parameter moduleras nu
    // (normaliserat, summan av alla kopplingar).
    std::array<std::atomic<float>, Modulation::maxLfos> phase {}, value {};
    // SNAP (bara Hz-läget): närmaste notvärde (index i rateNames, -1 = Sync-läge) och om hastigheten är låst mot det. Skrivs av ljudtråden.
    std::array<std::atomic<int>, Modulation::maxLfos> snapNote {};
    std::array<std::atomic<bool>, Modulation::maxLfos> snapLocked {};
    float currentOffset (int index) const;

    // Vilka LFO:er som räknas (bit i = LFO i+1). LFO 1-3 alltid i början; LFO 4-10 läggs till och tas bort av PluginBase::setLfoActive.
    // Inaktiva räknas inte alls (0 CPU). Gränssnittstrådarna får bara sätta/nollställa bitar atomärt, aldrig röra ljudtrådens fält.
    std::atomic<uint32_t> activeMask { 7u };

    // Instrumentens källor, skrivs av ljudtråden före update(): ENV 2, ENV 3, velocity och slump, och AMP ENV / FILT ENV
    // (0 när ingen koppling använder dem, se usesEnvSources).
    void setInstrumentSources (int layer, float env2, float env3, float velocity, float random, float ampEnv = 0.0f, float filterEnv = 0.0f)
    {
        auto& s = layerSources[(size_t) layer];
        s = { env2, env3, velocity, random, ampEnv, filterEnv };
    }

    // Live Code-källor (CODE 1-8). Meddelandetråden skriver inställningarna, ljudtråden räknar. kind: 0 av, 1 LFO, 2 ENV.
    // LFO: rate (Hz) och shape (0-5 som shapeValue). ENV: attack/decay i ms, startar när instrumentet `layer` får en ny ton (AD, ingen sustain).
    // När en plats stängs tonas den ut på ~20 ms; `idle` blir sant när den är helt tyst och platsen kan återanvändas.
    struct CodeSlot
    {
        std::atomic<int> kind { 0 };
        std::atomic<float> rate { 1.0f };
        std::atomic<int> shape { 0 };
        std::atomic<float> attackMs { 10.0f }, decayMs { 200.0f };
        std::atomic<int> layer { 0 };
        std::atomic<bool> idle { true };
    };
    std::array<CodeSlot, Modulation::numCodeSources> codeSlots;

    // Antal toner instrumentet startat (för ENV-startsignalen). Skrivs av ljudtråden före update().
    void setInstrumentNoteCount (int layer, juce::uint64 count) { noteCounts[(size_t) layer] = count; }
    void setSampleRate (double sampleRate) { codeDt = (double) Modulation::controlInterval / std::max (1.0, sampleRate); }

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
        std::atomic<float>* snap;      // SNAP i Hz-läget: på/av
        std::atomic<float>* snapRange; // ± procent av notvärdets hastighet
    };
    const std::array<NodeEnvelope, Modulation::maxLfos>* lfoCurves = nullptr;
    // Fas-kontinuitet vid ändrad hastighet (se update): offset i perioder, senaste perioder per fjärdedel och senaste positionen.
    std::array<double, Modulation::maxLfos> phaseOffset {}, lastCyclesPerQuarter {}, lastPpq {};

    struct CodeRuntime
    {
        bool wasOn = false, envActive = false;
        double phase = 0.0, envMs = 0.0;
        juce::int64 cycle = 0;
        juce::uint64 lastCount = 0;
        float fade = 0.0f, lastRaw = 0.0f;
    };
    std::array<CodeRuntime, Modulation::numCodeSources> codeRuntime {};
    std::array<juce::uint64, maxInstruments> noteCounts {};
    double codeDt = (double) Modulation::controlInterval / 44100.0;

    const ModRoutes& routes;
    std::array<Lfo, Modulation::maxLfos> lfos {};
    std::array<std::atomic<float>*, Modulation::numMacros> macros {};
    std::array<std::array<float, Modulation::instrumentKinds>, maxInstruments> layerSources {};
    std::array<std::atomic<float>, Modulation::numSources> sourceDisplay {};
    std::vector<juce::RangedAudioParameter*> parameters;
    std::vector<const std::atomic<float>*> raws;
    // MAX TIME: de fem tidsparametrarna moduleras över knobbens synliga område (legacyMax * MAX/1 s), inte över hela APVTS-intervallet,
    // så att moduleringsdjupet i gamla projekt är oförändrat vid standard (1 s).
    struct TimeMod { int index = -1; float min = 0.0f, centre = 1.0f, legacyMax = 1.0f; const std::atomic<float>* maxRaw = nullptr; const std::atomic<float>* ownRaw = nullptr; int group = 0; };
    std::vector<TimeMod> timeMods;
    std::vector<int> timeModSlot;                        // per parameterindex: plats i timeMods eller -1
    std::vector<float> offsets;                          // ljudtråden
    std::unique_ptr<std::atomic<float>[]> displayOffsets; // kopia för gränssnittet
    std::vector<int> touched;                            // index som fick modulering förra blocket
    bool anyActive = false;
};
