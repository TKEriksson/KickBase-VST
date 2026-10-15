#pragma once

#include <juce_dsp/juce_dsp.h>

enum class BassFilterType { lowpass24, lowpass12, bandpass, highpass };

// Inställningarna för bassynten. Tider i millisekunder.
//
// Oscillatorernas form är steglös: 0 = sinus, 1 = triangel, 2 = sågtand, 3 = fyrkant, och
// värden emellan blandar de två närmaste (t.ex. 2,5 = hälften sågtand, hälften fyrkant).
struct BassParams
{
    float rootHz = 65.4f;        // tonartens grundton i basens oktav
    float shape = 2.0f;          // oscillator 1: form 0..3
    float pulseWidth = 0.5f;     // fyrkantens pulsbredd, 0,05..0,95
    float sub = 0.3f;            // sinus, 0..1
    int subOctave = -1;          // 0 = samma oktav som grundtonen, -1 = en oktav ner
    float noise = 0.0f;          // vitt brus före filtret, 0..1
    float osc2Shape = 2.0f;      // oscillator 2: form 0..3
    float osc2PulseWidth = 0.5f;
    float osc2Level = 0.0f;      // andra oscillatorn, 0..1
    float osc2Semis = 0.0f;      // halvtoner från första oscillatorn
    float osc2Detune = 0.0f;     // cent, för tjockare ljud
    float fmAmount = 0.0f;       // oscillator 2 frekvensmodulerar oscillator 1, 0..1
    float fmEnvAmount = 0.0f;    // extra FM i början av varje ton (följer filter-envelopen), 0..1
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
};

// Monofonisk bassynt som spelas steg för steg av sequencern.
class BassSynth
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const BassParams& newParams) { params = newParams; }

    // Spelar en ton `semitones` från grundtonen i `gateSamples` samples. Med slide hålls tonen
    // kvar in i nästa steg, som då glider dit i stället för att slå an på nytt.
    void noteOn (int semitones, float velocity, int gateSamples, bool slide);
    void release();

    // Basen i två delar, så att effekter kan läggas före eller efter gaten:
    // källan (oscillatorer + filter + drive) och amplituden (envelope * velocity * level).
    struct Output
    {
        float source = 0.0f;
        float amplitude = 0.0f;
    };

    Output next();
    float nextSample() { const auto o = next(); return o.source * o.amplitude; }
    bool isActive() const { return stage != Stage::idle; }

    // Filter-envelopens form (0..1) efter `seconds`. Används även av gränssnittet.
    static float filterEnvelopeAt (const BassParams& p, double seconds);

    // En oscillator med steglös form (0..3) vid fas t (0..1), med polyBLEP mot vikning.
    static float oscillator (float shape, float pulseWidth, double t, double dt);

private:
    enum class Stage { idle, attack, decay, release };

    static double coefficientFor (double milliseconds, double sampleRate);

    BassParams params;
    double sampleRate = 44100.0;

    double phase = 0.0, subPhase = 0.0, phase2 = 0.0;
    double logFrequency = 0.0, targetLogFrequency = 0.0; // log2(Hz), så att glide låter jämnt
    bool gliding = false;

    Stage stage = Stage::idle;
    float ampLevel = 0.0f;
    int gateRemaining = 0;
    bool slideHeld = false;  // nuvarande ton har slide och väntar på nästa steg
    float velocity = 1.0f;
    double noteTime = 0.0;   // sekunder sedan senaste anslag (för filter-envelopen)

    juce::dsp::StateVariableTPTFilter<float> filter1, filter2;
    juce::Random random;
};
