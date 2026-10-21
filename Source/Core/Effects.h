#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "Modulation.h"
#include "SignalTap.h"

// Effektkanaler, som i FL Studios mixer: tre kanaler (A, B, C) med fem platser var. Varje plats
// har en effekttyp (eller Off) och sex rattar vars betydelse beror på typen. Samma effekt kan
// ligga i flera platser och kanaler; platserna körs i ordning 1, 2, 3 ...
//
// Varje baslager väljer en kanal och hur mycket av dess ljud som går genom den. Kanalen har
// egen nivå och egen duckning under kicken.
//
// Rattarna är parametrar 0..1 (så att parameteruppsättningen är fast, som VST3 kräver);
// Effects::knobs(typ) säger vad de betyder och hur de räknas om till riktiga enheter.
enum class EffectType { off, filter, distortion, eq, chorus, phaser, echo, reverb,
                        compressor, gate, comb, ringMod, bitcrusher, autoFilter, maximizer, ducker, disperser,
                        widener, flanger, transient, dynamicEq, count }; // FX2 (sist i listan: projekt sparar typens index)
constexpr int numEffectTypes = (int) EffectType::count; // inklusive Off
constexpr int numEffectChannels = 11; // FX1 ... FX11
constexpr int slotsPerChannel = 5;
constexpr int knobsPerSlot = 10; // P1-P8 är de ursprungliga; P9-P10 tillkom i FX1 (gamla projekt får dem satta till neutralt av migrateLegacyKnobs)

// Ljudet från kicken och instrumenten som effekten SIDECHAIN lyssnar på (sida för sida, per block): 0 = kicken, 1-10 = instrumenten.
// Pekarna gäller bara under det pågående blocket. Instrumentet fyller i dem; tomma pekare betyder att källan inte spelar.
struct SourceSignals
{
    static constexpr int count = 11;
    std::array<const float*, count> left {}, right {}; // right = nullptr för mono (kicken)
};

namespace Effects
{
    juce::StringArray typeNames();                // "Off", "Filter", ... (som i typlistan)
    juce::String name (EffectType type);          // "FILTER" ...
    juce::String shortName (EffectType type);     // "FILT" ... (platsknapparna)
    juce::String channelName (int channel);       // bokstaven i parameter-ID:n: "A" ... "K" (FX1 = A; de fem första är de ursprungliga)
    juce::String channelLabel (int channel);      // namnet i gränssnittet: "FX1" ... "FX11"

    // Parameter-ID: "fxA2Type", "fxA2P1" ... "fxA2P6" (platserna räknas från 1 i ID:t) och
    // kanalens "fxALevel", "fxADuck". Kanalerna 1-11 heter A-K i ID:n (sparade projekt pekar på de gamla A-E).
    juce::String slotParamID (int channel, int slot, const juce::String& suffix);
    juce::String knobParamID (int channel, int slot, int knob);
    // Gamla projekt (utan egenskapen fxKnobsV) saknar rattar som tillkommit sedan dess: sätter dem till sitt standardvärde (neutralt) i state-trädet.
    void migrateLegacyKnobs (juce::ValueTree& state);
    constexpr int knobsVersion = 2;
    // Rattar som tillkommit sedan FX1: sant och standardvärdet (0..1) om ratt k i typen är en sådan (saknas den i en fil ska den få detta, inte 0.5).
    bool newKnobDefault (EffectType type, int k, float& normalised);
    juce::String channelParamID (int channel, const juce::String& suffix);
    bool isEffectParameter (const juce::String& paramID);

    // En ratt i en effekttyp.
    struct Knob
    {
        enum class Unit { plain, percent, hz, ms, db, ratio, bits, times, octaves };

        juce::String name;
        float min = 0.0f, max = 1.0f;
        float centre = 0.0f;           // skev skala med detta värde i mitten (0 = linjär)
        float defaultValue = 0.0f;
        Unit unit = Unit::plain;
        juce::StringArray choices;     // inte tom = val (värdet är indexet)

        bool isChoice() const { return ! choices.isEmpty(); }
        float fromNormalised (float normalised) const;
        float toNormalised (float value) const;
        juce::String text (float value) const;
    };

    const std::vector<Knob>& knobs (EffectType type);

    // Tempo-synkade tider (echo, gate, auto filter), i fjärdedelar.
    juce::StringArray syncNames();
    double syncInQuarters (int index);

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    // Kanalens EQ (högpass, fyra band, lågpass) och kompressor. Alltid med, men neutrala som
    // standard (inga gain, högpass av, ratio 1:1), och då körs ingen bearbetning alls.
    constexpr int eqBands = 4; // lågt hyllfilter, två klockor, högt hyllfilter

    struct EqSettings
    {
        float hp = 20.0f, lp = 20000.0f;                  // Hz; av vid ytterlägena
        float freq[eqBands] { 100.0f, 400.0f, 2500.0f, 8000.0f };
        float gain[eqBands] { 0.0f, 0.0f, 0.0f, 0.0f };   // dB
        float q[eqBands] { 0.7f, 1.0f, 1.0f, 0.7f };      // bara klockorna använder Q

        bool isNeutral() const
        {
            for (auto g : gain)
                if (std::abs (g) > 0.01f)
                    return false;
            return hp <= 21.0f && lp >= 19500.0f;
        }
    };

    EqSettings readEq (const juce::AudioProcessorValueTreeState& apvts, int channel);

    // Samma sak för ljudtråden: pekarna till kanalens EQ-värden slås upp en gång (readEq bygger parameter-ID:n som text, vilket allokerar).
    struct EqRefs
    {
        EqRefs (const juce::AudioProcessorValueTreeState& apvts, int channel);
        EqSettings read() const;

    private:
        std::array<const std::atomic<float>*, 2 + eqBands * 3> values {}; // hp, lp, sedan frekvens, gain och Q för varje band
    };

    // Filtrens koefficienter i ordningen högpass, band 1-4, lågpass (samma som ljudtråden använder).
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, eqBands + 2> eqCoefficients (double sampleRate, const EqSettings& settings);
    std::array<std::array<float, 6>, eqBands + 2> eqArrayCoefficients (double sampleRate, const EqSettings& settings); // samma, utan att allokera (ljudtråden)

    // Frekvenssvaret i dB vid en frekvens (för ritningen).
    float eqResponseDb (double sampleRate, const EqSettings& settings, float hz);

    // Sätter en plats till en effekttyp med typens standardrattar (med gest, som en användare).
    void setSlotType (juce::AudioProcessorValueTreeState& apvts, int channel, int slot, EffectType type);

    // Flyttar platsen `from` till `to` i kanalen (de mellan flyttas ett steg), med alla parametrar.
    void moveSlot (juce::AudioProcessorValueTreeState& apvts, int channel, int from, int to);

    EffectType slotType (const juce::AudioProcessorValueTreeState& apvts, int channel, int slot);
}

// En effektkanal: platserna i ordning.
class EffectChain
{
public:
    EffectChain (juce::AudioProcessorValueTreeState& apvts, int channel);
    ~EffectChain();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Läser parametrarna. Anropas en gång per block, före process. `ppq` är positionen i
    // fjärdedelar vid blockets början (för de tempo-synkade effekterna).
    void update (double bpm, double ppq, int controlPhase = 0);

    // LFO-modulering av effekternas rattar (kan vara nullptr).
    void setModulator (const Modulator* modulator);

    // Källorna som SIDECHAIN-effekten lyssnar på (kan vara nullptr).
    void setSources (const SourceSignals* sources);

    // Kör kanalens påslagna platser. `buffer` är stereo.
    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    // Finns det någon påslagen effekt i kanalen?
    bool hasActive() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// De tre effektkanalerna och routingen till dem. Lager (eller FX-versionens synth) skickar ljud
// till en kanal med `send`; `process` kör kanalernas effekter, duckar dem under kicken och lägger
// resultatet (kanalens nivå) på mixen.
class EffectRack
{
public:
    explicit EffectRack (juce::AudioProcessorValueTreeState& apvts);
    ~EffectRack();

    void prepare (double sampleRate, int maxBlockSize);
    void reset(); // tömmer effekternas, EQ:ns och kompressorns tillstånd (utan att allokera)
    void setModulator (const Modulator* modulator);

    // Ljudet från kicken och instrumenten som kanalernas SIDECHAIN-effekter lyssnar på. Sätts före process, i varje block.
    void setSources (const SourceSignals& sources);

    // Börja ett block: töm kanalernas ingångar och läs parametrarna.
    void beginBlock (int numSamples, double bpm, double ppq, int controlPhase = 0); // controlPhase: buffertens början i sitt 32-fönster

    // Har kanalen några påslagna effekter? (Annars skickas inget dit: ljudet går orört vidare.)
    bool isActive (int channel) const;

    // Lägger `amount` av ljudet på kanalens ingång.
    void send (int channel, const float* left, const float* right, int numSamples, float amount);

    // Vad gränssnittet ritar per kanal: ljudet in i EQ:n, in i kompressorn, ut ur den, och
    // kompressorns förstärkning (i båda kanalerna).
    SignalTap eqInputTap[numEffectChannels], compInputTap[numEffectChannels], compOutputTap[numEffectChannels],
              compGainTap[numEffectChannels];

    // Kör kanalerna och adderar dem till `mix` (stereo). `kickEnvelope` är kickens envelope per
    // sample (0..1) som kanalernas duckning följer.
    // Utan `mix` adderas inget; kanalens ljud hämtas då med output(). `duckEnabled` (en per kanal)
    // stänger av duckningen för kanaler där sidechainen är bortvald i mixern.
    void process (int numSamples, const float* kickEnvelope, juce::AudioBuffer<float>* mix = nullptr, const bool* duckEnabled = nullptr);

    // Kanalens färdiga ljud (stereo, efter effekter, EQ, kompressor, duckning och nivå) efter process.
    const juce::AudioBuffer<float>& output (int channel) const;
    bool isIdle (int channel) const; // kanalen hoppades över i det här blocket (inget skickat, ingen effekt): dess buffert är tom

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
