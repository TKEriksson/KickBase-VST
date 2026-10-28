#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "SampleData.h"
#include "MultiSample.h"
#include "Wavetable.h"
#include "FilterUnit.h"

enum class InstrumentFilterType { lowpass24, lowpass12, bandpass, highpass };

// Hur fasen förvrängs innan wavetablen läses (Serums "warp"). AM är ingen fasförvrängning utan amplitudmodulerar vågen.
// RING (B): OSC 1 multipliceras med OSC 2, OSC 3 eller båda (OscParams::ringSource); sist i enumen så gamla projekt är orörda.
enum class WarpMode { off, sync, bend, pwm, mirror, fm, flip, quantize, am, ring };

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
    int ringSource = 0;       // bara OSC 1 med warp RING (B): 0 = OSC 2, 1 = OSC 3, 2 = OSC 2 x OSC 3
    float phase = 0.0f;       // startfas 0..1
    float randomPhase = 0.0f; // 0 = samma fas varje ton (tight), 1 = slumpad
    float pan = 0.0f;         // -1 (vänster) ... 1 (höger)
    float stereo = 0.5f;      // hur långt unison-stämmorna sprids i stereobilden, 0..1
    bool on = true;           // av = oscillatorn hörs inte (och OSC B modulerar inte OSC A)
    bool morph = true;        // på = positionen tonar över mellan vågformerna; av = hoppar mellan tabellens steg
};

// En extra envelope (ENV 2 och ENV 3) som inte styr ljudet själv utan används som modulationskälla (dras till ratter eller sätts i matrisen).
struct ModEnvParams
{
    float attackMs = 5.0f, decayMs = 400.0f, sustain = 0.0f, releaseMs = 250.0f;
    bool sync = false;                                   // tiderna är notvärden (Modulation::syncTimeInQuarters) som följer tempot, i stället för ms
    int attackNote = 0, decayNote = 8, releaseNote = 7;  // index i Modulation::syncTimeNames()
    bool on = true;                                      // fas L: av = källan ger 0 (ON/OFF i ENV-fliken)

    // Samma envelope med tiderna i ms: oförändrad om sync är av, annars räknad från tempot (fjärdedelar x 60000 / bpm, aldrig under ms-rattarnas minimum).
    ModEnvParams withTempo (double bpm) const;
};

struct InstrumentParams
{
    float rootHz = 65.4f;        // tonartens grundton i basens oktav
    OscParams oscA, oscB, oscC;
    float warpEnv = 0.0f;        // extra warp på OSC A i början av varje ton (följer filter-envelopen)
    float sub = 0.3f;            // sinus, 0..1
    int subOctave = 0;           // 0 = samma oktav som grundtonen, -1 = en oktav ner
    float noise = 0.0f;          // vitt brus före filtret, 0..1
    InstrumentFilterType filterType = InstrumentFilterType::lowpass24;
    int filterModel = 0;         // 0 = Kickbase (SVF, filterType gäller), 1 = TB-303 (filterType ignoreras), 2 = Ladder, 3 = Formant, 4 = Comb (FilterModels.h)
    float filterP1 = 0.0f, filterP2 = 1.0f; // modellens två ratter (0..1), se ModelFilter
    // Filter 2 (fas D3): av som standard; sitter direkt efter Filter 1 (före drive), utan egen envelope och keytrack.
    bool filter2On = false;
    int filter2Model = 0, filter2Type = 3; // modell som filterModel; typ som filterType (3 = High)
    float filter2CutoffHz = 120.0f, filter2Resonance = 0.2f, filter2P1 = 0.0f, filter2P2 = 1.0f;
    int filterRouting = 0;       // 0 = Serial (Filter 1 -> Filter 2, MIX = Filter 2 torr/våt), 1 = Parallel (båda får samma insignal, (1-MIX) F1 + MIX F2)
    float filterMix = 1.0f;
    float keyTrack = 0.0f;       // 0..1: hur mycket cutoff följer tonhöjden (1 = helt, från C2)
    float cutoffHz = 300.0f;
    float resonance = 0.3f;      // 0..1
    float envAmount = 4.0f;      // hur många oktaver filtret öppnas av envelopen
    float envDecayMs = 120.0f;
    float envAttackMs = 1.0f;    // hur snabbt filtret öppnas (1 ms = som innan inställningen fanns)
    float velToEnv = 0.5f;       // hur mycket velocity påverkar filter-envelopen, 0..1
    float attackMs = 1.0f;
    float decayMs = 200.0f;
    float sustain = 0.6f;
    float releaseMs = 15.0f;
    float glideMs = 60.0f;
    float drive = 0.2f;          // 0..1
    int driveMode = 0;           // 0 mjuk, 1 hård, 2 vikning, 3 asymmetrisk, 4 krossad
    float level = 0.7f;

    ModEnvParams env2, env3;     // modulationsenveloper
    // fas L: ON/OFF och Sync för volym- och filter-envelopen (ENV-fliken). Sync-tiderna är index i Modulation::syncTimeNames().
    bool env1On = true, filterEnvOn = true;
    bool env1Sync = false, filterEnvSync = false;
    // fas S: FILTER av/på (hela Filter 1+2) och per källa (OSC 1, 2, 3, SAMPLE, NOISE); av = går förbi båda filtren. SAMPLE av som standard.
    bool filterOn = true;
    bool filterSrc[5] { true, true, true, false, true };
    int env1AttackNote = 0, env1DecayNote = 5, env1ReleaseNote = 1, filterAttackNote = 0, filterDecayNote = 4;
    int subWave = 0;             // 0 sinus, 1 triangel, 2 fyrkant, 3 såg
    float noiseTone = 1.0f;      // 0 mörkt brus ... 1 vitt brus
    int noiseType = 0;           // fas S: 0 White (förut), 1 Pink, 2 Brown, 3 Vinyl (frö-styrt)
    int warpEnvSource = 0;       // fas S: 0 Filter Env (förut), 1 Direct (egen decay), 2 ENV 2, 3 ENV 3
    float warpEnvDecayMs = 30.0f;
    bool invertPolarity = false;  // fas PH: vänder signalen (-1)
    float phaseDeg = 0.0f;        // fas PH: startfas i grader för OSC 1-3 och sub vid varje ny ton (läggs till OSC:ns egen PHASE)
    float pitchEnvSemis = 0.0f;  // tonhöjdsenvelope: så många halvtoner över (eller under) tonen i början av varje ton
    float pitchEnvDecayMs = 60.0f;

    // fas S2: FM-källa (2 operatorer, sinus): modulatorn (ratio x frekvensen) fasmodulerar carriern. Av som standard = ljudet oförändrat.
    bool fmOn = false;
    float fmLevel = 0.0f;        // 0..1
    float fmRatio = 1.0f;        // modulatorns frekvens / carrierns, 0.25..8
    float fmIndex = 0.3f;        // 0..1 (kurva index² till ca 0..8 rad)
    float fmEnvAmount = 0.0f;    // 0..1: hur mycket indexet följer en exp-envelope (0 = konstant)
    float fmEnvMs = 60.0f;       // envelopens decay (-60 dB), ms
    float fmSemis = 0.0f;        // carrierns (och modulatorns) transponering, halvtoner
    bool fmFilter = true;        // på = FM-källan går genom filtren (som OSC 1-3), av = förbi dem

    // Samplingen som tredje ljudkälla (lagrets egen inladdade sampling, eller nullptr): spelas före filtret.
    const ClickSample* sampleData = nullptr;
    float sampleLevel = 0.0f;    // 0..1
    float sampleRootHz = 130.8f; // samplingens egen tonhöjd; med KEY spelas den så att den följer noten
    bool sampleLoop = false;
    // Multisampling (sampleMulti): zonen väljs i noteOn ur sampleBank efter ton och anslag; spelas alltid med KEY mot zonens rotton, utan skivor.
    // Rösten sparar bara zonindex och slår upp zonen mot den bank som gäller just nu, så ett bankbyte lämnar ingen hängande pekare.
    const MultiSampleBank* sampleBank = nullptr;
    bool sampleMulti = false;
    bool sampleOn = false;       // samplingen hörs bara när den är på
    bool sampleKeyTrack = false; // av = spelas med sin egen tonhöjd och hastighet
    float sampleStart = 0.0f;    // 0..1: var i samplingen varje ton börjar
    bool slicerReverse = false;  // alla skivor spelas baklänges
    float allpassFreq = 200.0f;  // 4 allpass i serie efter filtren (mitten och sida var för sig); AMOUNT 0 = av
    float allpassAmount = 0.0f;  // 0..1: blandning torr -> allpass (1 = ren fasrotation)
    float samplePan = 0.0f, noisePan = 0.0f, fmPan = 0.0f; // INSTR-MIX: -1..1 (0 = mitten, ljudet som förut)
    float subPhaseDeg = 0.0f, fmPhaseDeg = 0.0f; // startfas per källa (grader), läggs till PH-fasen; 0 = som förut
    float aWidth = 1.0f, bWidth = 1.0f, cWidth = 1.0f; // OSC 1-3: 0 = mono, 1 = som förut, 2 = dubbel sida
    float sampleWidth = 1.0f, fmWidth = 1.0f;          // 1 = mono (som förut), upp till 2 = okorrelerat
    float noiseWidth = 1.0f;     // 1 = mono (som förut), upp till 2 = okorrelerat brus i L och R
    // 5b: tid per motor (OSC 1, OSC 2, OSC 3, SAMPLE, NOISE, SUB, FM): POS DELAY (ms, eller SYNC = notvärde 1..7 från tempot), LENGTH (0 = hela tonen), CURVE -1..1. Alla 0 = som förut.
    std::array<float, 7> delayMs {}, lengthMs {}, curve {};
    std::array<int, 7> delaySync {};
    InstrumentParams() { oscB.level = 0.0f; oscC.level = 0.0f; oscC.on = false; }

    // Samma inställningar med alla envelope-tider i ms (Sync räknat från tempot) och, om applyOnOff, ENV 1/FILTER ENV av inbakade:
    // ENV 1 av = full nivå medan tonen hålls (1 ms attack, 5 ms release mot klick), FILTER ENV av = envAmount 0 (WARP ENV följer formen som förut).
    // Räknas alltid från notvärdena och ON-flaggorna, så den kan anropas flera gånger. Diagrammen anropar den utan applyOnOff (rutan tonas i stället).
    InstrumentParams withTempo (double bpm, bool applyOnOff = true) const;
};

// Monofonisk bassynt som spelas steg för steg av sequencern.
class InstrumentSynth
{
public:
    static constexpr int maxUnison = 7;

    void prepare (double sampleRate);

    // Slumpen (brus, slumpfas) är seedad, så att samma preset renderar bit-identiskt varje gång.
    void setSeed (juce::int64 seed) { random.setSeed (seed); }
    void reset();
    void setParams (const InstrumentParams& newParams) { rawParams = newParams; refreshEnvTimes(); }
    // Tempot (BPM) för ENV 2 och ENV 3 i läget Sync. Anropas varje bit om 32 samples; en tempoändring mitt i ett steg ger en kontinuerlig kurva.
    void setTempo (double newBpm) { if (newBpm != bpm) { bpm = newBpm; refreshEnvTimes(); } }

    // Spelar en ton `semitones` från grundtonen i `gateSamples` samples.
    // slice >= 0: en not i raden "Slice N" (0-baserat); spelar den skivan i ClickSample::slices, originalhastighet, ingen loop. -1: vanlig ton.
    void noteOn (int semitones, float velocity, int gateSamples, int slice = -1);

    // En glide-ton (som en "slide note" i FL Studio): ingen ny ton slås an, utan den som klingar glider till
    // `semitones` på `samples` samples och blir kvar där tills den slutar. Ger false om ingenting klingar.
    bool glideTo (int semitones, int samples);
    bool isSounding() const { return stage != Stage::idle; }
    int getTargetSemitones() const { return targetSemitones; } // tonen (eller glidemålet) rösten är på väg mot
    void release();
    // Tonar ut tonen på `milliseconds` oavsett RELEASE: när ett steg spelats strax innan en "ton av" som slutar mönstret. Nästa ton börjar som vanligt.
    void fadeOutQuickly (double milliseconds);

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
    // Rösten grundton i Hz just nu (glide och tonhöjdsmodulering inräknade); 0 före första tonen. Läses av FUNDAMENTAL DUCK.
    float getFrequencyHz() const { return (float) cachedFrequency; }
    bool isReleasing() const { return stage == Stage::release; }

    // Modulationskällorna: 0 = ENV 2, 1 = ENV 3, 2 = senaste tonens velocity, 3 = ett slumptal per ton, 4 = AMP ENV (ENV 1),
    // 5 = FILT ENV (filter-envelopens form, utan AMOUNT) (alla 0..1). AMP ENV och FILT ENV ger 0 när rutan är av.
    float getModulationValue (int source) const
    {
        switch (source)
        {
            case 0:  return params.env2.on ? env2.level : 0.0f;
            case 1:  return params.env3.on ? env3.level : 0.0f;
            case 2:  return stage == Stage::idle ? 0.0f : velocity;
            case 4:  return params.env1On && stage != Stage::idle ? ampLevel : 0.0f;
            case 5:  return params.filterEnvOn && stage != Stage::idle ? filterEnvLevel : 0.0f;
            default: return randomValue;
        }
    }

    // Var i samplingen den spelar (0..1), eller -1 när ingen sampling klingar. För gränssnittets markör.
    float getSampleFraction() const
    {
        const auto* data = activeSample();
        if (stage == Stage::idle || ! params.sampleOn || data == nullptr || params.sampleLevel <= 0.0f)
            return -1.0f;
        const auto length = (double) data->audio.getNumSamples();
        if (length < 2.0 || (samplePosition >= length && ! params.sampleLoop))
            return -1.0f;
        return (float) (std::fmod (samplePosition, length) / length);
    }

    // Filter-envelopens form (0..1) efter `seconds`. Används även av gränssnittet.
    static float filterEnvelopeAt (const InstrumentParams& p, double seconds);

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

        // Tonhöjdskvoterna (halvtoner, finstämning, stämmornas spridning) räknas om bara när inställningarna ändras, inte varje sample.
        float lastSemis = 1.0e30f, lastFine = 1.0e30f, pitchRatio = 1.0f, lastDetune = 1.0e30f;
        int lastVoices = -1;
        std::array<double, maxUnison> detuneRatios {};
        void retrigger (const OscParams& p, juce::Random& random, double extraPhase = 0.0);
        Stereo next (const OscParams& p, double frequency, double sampleRate, float warpAmount, float fmInput);
    };

private:
    enum class Stage { idle, attack, decay, release };

    static double coefficientFor (double milliseconds, double sampleRate);

    // Minns det senaste svaret från en dyr beräkning (exp, pow) så länge indata är oförändrad, vilket de allra flesta samples är: exakt samma
    // värden som förut, men utan att räkna om dem 48 000 gånger i sekunden.
    struct CoefficientMemo
    {
        double milliseconds = -1.0, rate = -1.0, value = 0.0;
        double get (double ms, double sampleRate)
        {
            if (ms != milliseconds || sampleRate != rate)
            {
                milliseconds = ms;
                rate = sampleRate;
                value = coefficientFor (ms, sampleRate);
            }
            return value;
        }
    };

    InstrumentParams params;     // rawParams.withTempo (bpm): det som spelas
    InstrumentParams rawParams;  // som de kom från setParams
    double sampleRate = 44100.0;

    Oscillator oscA, oscB, oscC;
    double subPhase = 0.0;
    double fmCarrierPhase = 0.0, fmModPhase = 0.0; // FM-källans faser (0..1)
    float fmEnvLevel = 0.0f;                        // 1 vid anslaget, faller mot 0 (FM ENV)
    double samplePosition = 0.0; // i samplingens samples
    bool sliceNote = false;      // tonen kom från en slice-rad (satt i noteOn)
    int multiZone = -1;          // zonen i params.sampleBank som tonen valde i noteOn (-1 = ingen)
    const ClickSample* activeSample() const noexcept; // samplingen rösten spelar: lagrets egen eller multizonen (nullptr om ingen)
    bool sliceVoice = false;     // tonen spelar en skiva (satt i noteOn)
    int sliceBegin = 0, sliceEnd = 0;
    double logFrequency = 0.0, targetLogFrequency = 0.0; // log2(Hz), så att glide låter jämnt
    int glideSamplesLeft = 0;
    int targetSemitones = 0;
    double glideStep = 0.0;  // ändring i log-frekvens per sample under ett glide

    struct ModEnv
    {
        enum class EnvStage { idle, attack, decay, release };
        EnvStage envStage = EnvStage::idle;
        float level = 0.0f;
        CoefficientMemo decayMemo, releaseMemo;
        void trigger() { envStage = EnvStage::attack; }
        void release() { if (envStage != EnvStage::idle) envStage = EnvStage::release; }
        void reset() { envStage = EnvStage::idle; level = 0.0f; }
        void next (const ModEnvParams& p, double rate);
    };
    ModEnv env2, env3;
    double bpm = 120.0;
    // 5b: POS DELAY per motor (sekunder), låst i noteOn (SYNC räknas då från tempot); resetPending = motorns fas nollställs när fördröjningen passerats.
    std::array<double, 7> delaySeconds {};
    std::array<bool, 7> resetPending {};
    float engineGate (int engine) const noexcept; // 0 före POS DELAY, sedan LENGTH/CURVE; exakt 1.0f när motorns tid är av
    ModEnvParams envTimes2, envTimes3; // ENV 2 och ENV 3 med tiderna i ms (ur params och tempot)
    void refreshEnvTimes();
    float pitchEnv = 0.0f;       // 1 vid anslaget, faller mot 0 (tonhöjdsenvelopen)
    float warpDirect = 0.0f;     // fas S: WARP ENV Direct: 1 vid anslaget, faller mot 0
    float noiseState = 0.0f;     // brusets lågpass (NOISE TONE)
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f, brownState = 0.0f, vinylHiss = 0.0f; // fas S: brustyper
    float randomValue = 0.0f;    // modulationskällan RANDOM: nytt för varje ton
    juce::Random modRandom { 99 };

    Stage stage = Stage::idle;
    float ampLevel = 0.0f;
    float filterEnvLevel = 0.0f; // senaste samplets filter-envelope (0..1), källan FILT ENV
    double forcedReleaseMs = 0.0; // > 0: release på så här många ms (fadeOutQuickly), annars RELEASE
    int gateRemaining = 0;
    float velocity = 1.0f;
    double noteTime = 0.0;   // sekunder sedan senaste anslag (för filter-envelopen)

    // Filtret för mitten och sidan: samma inställning, så att det blir stereo utan att ljudet låter olika i vänster och höger. Modellbyte nollställer filtret som tar över.
    FilterUnit filterMid, filterSide;
    FilterUnit filter2Mid, filter2Side; // Filter 2 (körs bara när det är på)
    bool filter2WasOn = false;           // av -> på nollställer Filter 2 (gammalt tillstånd ska inte höras)
    float allpassState[2][4] {};         // allpass: tillstånd för mitten (0) och sidan (1)
    bool allpassWasOn = false;
    float noiseDelay[64] {};             // noiseWidth: en fördröjd kopia av bruset ger den okorrelerade delen
    int noiseDelayPos = 0;
    float sampleDelay[64] {}, fmDelay[64] {}; // samma sak för SAMPLE och FM
    int widthDelayPos = 0;
    float lastKeyFrequency = -1.0f, lastKeyTrack = -1.0f, cachedKeyFollow = 1.0f;
    double lastLogFrequency = 1.0e30, lastPitchOffset = 1.0e30, cachedFrequency = 0.0;
    CoefficientMemo ampDecayMemo, ampReleaseMemo, pitchEnvMemo, warpDirectMemo, fmEnvMemo;

    // Drive: 0 mjuk (tanh), 1 hård (klippning), 2 vikning (wavefolder), 3 asymmetrisk (jämna övertoner), 4 krossad (bitar).
    // Kurvans konstanter (tanh av förstärkningen m.m.) räknas bara om när DRIVE eller läget ändras; själva uträkningen är densamma som förut.
    struct DriveShape
    {
        float drive = -1.0f;
        int mode = -1;
        float gain = 1.0f, bias = 0.0f, tanhGain = 1.0f, tanhBias = 0.0f, levels = 1.0f;

        void update (float newDrive, int newMode)
        {
            if (newDrive == drive && newMode == mode)
                return;
            drive = newDrive;
            mode = newMode;
            gain = 1.0f + drive * 9.0f;
            bias = 0.35f * drive;
            tanhGain = std::tanh (gain);
            tanhBias = std::tanh (bias);
            levels = std::exp2 (8.0f - 6.0f * drive);
        }

        float apply (float x) const
        {
            if (drive <= 0.0f)
                return x;
            switch (mode)
            {
                case 1:  return juce::jlimit (-1.0f, 1.0f, gain * x);
                case 2:  return std::sin (juce::MathConstants<float>::halfPi * 0.6f * gain * x);
                case 3:  return (std::tanh (gain * x + bias) - tanhBias) / std::max (0.2f, tanhGain);
                case 4:  return std::round (std::tanh (gain * 0.5f * x) * levels) / levels;
                default: return std::tanh (gain * x) / tanhGain;
            }
        }
    } driveShape;
    juce::Random random { 1 };
};
