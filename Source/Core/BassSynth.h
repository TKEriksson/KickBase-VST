#pragma once

#include <juce_dsp/juce_dsp.h>

enum class BassWave { saw, square, triangle, sine };

// Inställningarna för bassynten. Tider i millisekunder.
struct BassParams
{
    float rootHz = 65.4f;        // tonartens grundton i basens oktav
    BassWave wave = BassWave::saw;
    float sub = 0.3f;            // sinus en oktav ner, 0..1
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
    // källan (oscillator + filter + drive) och amplituden (envelope * velocity * level).
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

private:
    enum class Stage { idle, attack, decay, release };

    float oscillator (double phaseIn, double increment) const;
    static double coefficientFor (double milliseconds, double sampleRate);

    BassParams params;
    double sampleRate = 44100.0;

    double phase = 0.0, subPhase = 0.0;
    double logFrequency = 0.0, targetLogFrequency = 0.0; // log2(Hz), så att glide låter jämnt
    bool gliding = false;

    Stage stage = Stage::idle;
    float ampLevel = 0.0f;
    int gateRemaining = 0;
    bool slideHeld = false;  // nuvarande ton har slide och väntar på nästa steg
    float velocity = 1.0f;
    double noteTime = 0.0;   // sekunder sedan senaste anslag (för filter-envelopen)

    juce::dsp::StateVariableTPTFilter<float> filter1, filter2;
};
