#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

// Inställningarna för kicken. Tider i millisekunder.
struct KickParams
{
    float tailHz = 44.0f;       // svansens frekvens = tonarten
    float pitchSemis = 42.0f;   // hur högt över svansen svepet börjar
    float pitchTimeMs = 45.0f;  // hur lång tid svepet tar
    float pitchCurve = 3.0f;    // 1 = rak linje, högre = snabbare fall i början
    float attackMs = 0.3f;
    float holdMs = 15.0f;
    float decayMs = 140.0f;
    float decayCurve = 2.0f;    // 1 = rak linje, högre = kortare svans
    float clickLevel = 0.3f;
    float clickLengthMs = 6.0f;
    float clickToneHz = 5000.0f;
    float drive = 0.2f;         // 0..1
    float level = 0.8f;
};

// En inladdad sampling som spelas som klick (de första millisekunderna av kicken).
// Ägs av processorn; ljudtråden läser bara.
struct ClickSample
{
    juce::AudioBuffer<float> audio; // mono, omsamplad till aktuell samplingsfrekvens
    juce::String name;
};

// Syntetiserad kick: sinus med pitch-envelope, amplitud-envelope, klick och drive.
class KickEngine
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const KickParams& newParams) { params = newParams; }
    void setClickSample (const ClickSample* sample) { clickSample = sample; }

    void trigger (float velocity);
    float nextSample();

    // Nuvarande amplitud-envelope (0..1). Basen duckas efter den.
    float getEnvelope() const { return envelope; }

    // Envelopernas form, utan tillstånd. Används även av gränssnittet för att rita kurvorna.
    static float amplitudeAt (const KickParams& p, double seconds);
    static float pitchEnvelopeAt (const KickParams& p, double seconds);
    static double lengthSeconds (const KickParams& p);

private:
    struct Voice
    {
        bool active = false;
        double time = 0.0;       // sekunder sedan anslaget
        double phase = 0.0;      // 0..1
        float velocity = 1.0f;
        float fade = 1.0f;       // < 1 när rösten tonas ut för att ge plats åt en ny
        bool fadingOut = false;
        int sampleIndex = 0;     // position i klicksamplingen
        juce::dsp::StateVariableTPTFilter<float> clickFilter;
    };

    float renderVoice (Voice& voice);

    KickParams params;
    const ClickSample* clickSample = nullptr;
    std::array<Voice, 2> voices;
    size_t currentVoice = 0;
    double sampleRate = 44100.0;
    float fadeStep = 0.0f;
    float envelope = 0.0f;
    juce::Random random;
};
