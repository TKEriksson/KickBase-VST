#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "SampleData.h"
#include "Wavetable.h"

enum class BassFilterType { lowpass24, lowpass12, bandpass, highpass };

// Hur fasen förvrängs innan wavetablen läses (Serums "warp").
enum class WarpMode { off, sync, bend, pwm, mirror, fm };

// En wavetable-oscillator.
struct OscParams
{
    int table = 0;            // Wavetables::get; Wavetables::userIndex() = den egna inladdade tabellen (user)
    const Wavetable* user = nullptr; // oscillatorns egna tabell (från en WAV-fil), eller nullptr
    float position = 2.0f / 3.0f; // 0..1 genom tabellen (Basic: 0 sinus, 1/3 triangel, 2/3 såg, 1 fyrkant)
    float level = 1.0f;       // 0..1
    float semis = 0.0f;       // halvtoner
    float fineCents = 0.0f;
    int unison = 1;           // 1..7 stämmor
    float detuneCents = 12.0f;// sidostämmornas avstånd
    float blend = 0.5f;       // sidostämmornas nivå
    WarpMode warp = WarpMode::off;
    float warpAmount = 0.0f;  // 0..1
    float phase = 0.0f;       // startfas 0..1
    float randomPhase = 0.0f; // 0 = samma fas varje ton (tight), 1 = slumpad
    float pan = 0.0f;         // -1 (vänster) ... 1 (höger)
    float stereo = 0.5f;      // hur långt unison-stämmorna sprids i stereobilden, 0..1
    bool on = true;           // av = oscillatorn hörs inte (och OSC B modulerar inte OSC A)
};

struct BassParams
{
    float rootHz = 65.4f;        // tonartens grundton i basens oktav
    OscParams oscA, oscB, oscC;
    float warpEnv = 0.0f;        // extra warp på OSC A i början av varje ton (följer filter-envelopen)
    float sub = 0.3f;            // sinus, 0..1
    int subOctave = 0;           // 0 = samma oktav som grundtonen, -1 = en oktav ner
    float noise = 0.0f;          // vitt brus före filtret, 0..1
    BassFilterType filterType = BassFilterType::lowpass24;
    float keyTrack = 0.0f;       // 0..1: hur mycket cutoff följer tonhöjden (1 = helt, från C2)
    float cutoffHz = 300.0f;
    float resonance = 0.3f;      // 0..1
    float envAmount = 4.0f;      // hur många oktaver filtret öppnas av envelopen
    float envDecayMs = 120.0f;
    float velToEnv = 0.5f;       // hur mycket velocity påverkar filter-envelopen, 0..1
    float attackMs = 1.0f;
    float decayMs = 200.0f;
    float sustain = 0.6f;
    float releaseMs = 15.0f;
    float glideMs = 60.0f;
    float drive = 0.2f;          // 0..1
    float level = 0.7f;

    // Samplingen som tredje ljudkälla (lagrets egen inladdade sampling, eller nullptr): spelas före filtret.
    const ClickSample* sampleData = nullptr;
    float sampleLevel = 0.0f;    // 0..1
    float sampleRootHz = 130.8f; // samplingens egen tonhöjd; med KEY spelas den så att den följer noten
    bool sampleLoop = false;
    bool sampleOn = false;       // samplingen hörs bara när den är på
    bool sampleKeyTrack = false; // av = spelas med sin egen tonhöjd och hastighet
    float sampleStart = 0.0f;    // 0..1: var i samplingen varje ton börjar

    BassParams() { oscB.level = 0.0f; oscC.level = 0.0f; oscC.on = false; }
};

// Monofonisk bassynt som spelas steg för steg av sequencern.
class BassSynth
{
public:
    static constexpr int maxUnison = 7;

    void prepare (double sampleRate);

    // Slumpen (brus, slumpfas) är seedad, så att samma preset renderar bit-identiskt varje gång.
    void setSeed (juce::int64 seed) { random.setSeed (seed); }
    void reset();
    void setParams (const BassParams& newParams) { params = newParams; }

    // Spelar en ton `semitones` från grundtonen i `gateSamples` samples.
    void noteOn (int semitones, float velocity, int gateSamples);

    // En glide-ton (som en "slide note" i FL Studio): ingen ny ton slås an, utan den som klingar glider till
    // `semitones` på `samples` samples och blir kvar där tills den slutar. Ger false om ingenting klingar.
    bool glideTo (int semitones, int samples);
    void release();

    // Basen i två delar, så att effekter kan läggas före eller efter gaten:
    // källan (oscillatorer + filter + drive) och amplituden (envelope * velocity * level).
    struct Output
    {
        float left = 0.0f, right = 0.0f; // stereo (oscillatorernas pan och unison-spridning)
        float amplitude = 0.0f;
    };

    Output next();
    float nextSample() { const auto o = next(); return 0.5f * (o.left + o.right) * o.amplitude; }
    bool isActive() const { return stage != Stage::idle; }

    // Var i samplingen den spelar (0..1), eller -1 när ingen sampling klingar. För gränssnittets markör.
    float getSampleFraction() const
    {
        if (stage == Stage::idle || ! params.sampleOn || params.sampleData == nullptr || params.sampleLevel <= 0.0f)
            return -1.0f;
        const auto length = (double) params.sampleData->audio.getNumSamples();
        if (length < 2.0 || (samplePosition >= length && ! params.sampleLoop))
            return -1.0f;
        return (float) (std::fmod (samplePosition, length) / length);
    }

    // Filter-envelopens form (0..1) efter `seconds`. Används även av gränssnittet.
    static float filterEnvelopeAt (const BassParams& p, double seconds);

    // Fasen efter warp (för gränssnittets bild och för oscillatorn). `fmInput` -1..1.
    static double warpPhase (WarpMode mode, float amount, double phase, float fmInput);

    struct Stereo
    {
        float left = 0.0f, right = 0.0f;
        float mid() const { return 0.5f * (left + right); }
    };

    // En wavetable-oscillator med unison. next() ger signalen utan nivå (OSC B kan modulera A
    // även när den själv är tyst).
    struct Oscillator
    {
        std::array<double, maxUnison> phases {};
        void retrigger (const OscParams& p, juce::Random& random);
        Stereo next (const OscParams& p, double frequency, double sampleRate, float warpAmount, float fmInput);
    };

private:
    enum class Stage { idle, attack, decay, release };

    static double coefficientFor (double milliseconds, double sampleRate);

    BassParams params;
    double sampleRate = 44100.0;

    Oscillator oscA, oscB, oscC;
    double subPhase = 0.0;
    double samplePosition = 0.0; // i samplingens samples
    double logFrequency = 0.0, targetLogFrequency = 0.0; // log2(Hz), så att glide låter jämnt
    int glideSamplesLeft = 0;
    double glideStep = 0.0;  // ändring i log-frekvens per sample under ett glide

    Stage stage = Stage::idle;
    float ampLevel = 0.0f;
    int gateRemaining = 0;
    float velocity = 1.0f;
    double noteTime = 0.0;   // sekunder sedan senaste anslag (för filter-envelopen)

    // Filtren för mitten (filter1, filter2) och sidan: samma inställning, så att det blir stereo utan att
    // ljudet låter olika i vänster och höger.
    juce::dsp::StateVariableTPTFilter<float> filter1, filter2, sideFilter1, sideFilter2;
    juce::Random random { 1 };
};
