#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include "NodeEnvelope.h"

// Inställningarna för kicken. Tider i millisekunder.
//
// Pluginet formar kicken med två envelopes av fria punkter (pitch och volym, som i Kick 3).
// Utan dem (nullptr) används den enkla modellen nedan (svep + glide, attack/hold/decay), som
// analysverktygen söker i.
struct KickParams
{
    float tailHz = 44.0f;       // svansens frekvens = tonarten

    const NodeEnvelope* pitchEnv = nullptr; // halvtoner över svansen över tid
    const NodeEnvelope* ampEnv = nullptr;   // volym 0..1 över tid; kicken slutar vid sista punkten
    float pitchAmount = 1.0f;   // skalar pitch-envelopen
    float lengthScale = 1.0f;   // sträcker båda envelopernas tid
    float startPhase = 0.0f;    // kroppens startfas, 0..1
    float harmonics = 0.0f;     // övertoner på kroppen, 0..1
    float harmonicsDecayMs = 60.0f;
    float harmonicsTilt = 1.0f; // hur fort övertonerna avtar uppåt (högre = mörkare)
    int clipMode = 0;           // drive: 0 mjuk, 1 hård, 2 rör (osymmetrisk)
    float highCutHz = 20000.0f; // lågpass på kicken (tämjer klicket)

    // Den enkla modellen:
    float pitchSemis = 42.0f;   // hur högt över svansen svepet börjar
    float pitchTimeMs = 45.0f;  // hur lång tid svepet tar
    float pitchCurve = 3.0f;    // 1 = rak linje, högre = snabbare fall i början
    float glideSemis = 0.0f;    // andra, långsamma svepet: börjar så här högt över svansen
    float glideTimeMs = 80.0f;  // och glider ner till svansen på så här lång tid
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

    // Den senaste kickens ljud spelas av gränssnittet för att rita vågformen (se render).
    static juce::AudioBuffer<float> render (const KickParams& p, const ClickSample* click, double sampleRate);

    // Envelopernas form, utan tillstånd. Används även av gränssnittet för att rita kurvorna.
    static float amplitudeAt (const KickParams& p, double seconds);
    static float pitchEnvelopeAt (const KickParams& p, double seconds);
    static float glideEnvelopeAt (const KickParams& p, double seconds);
    static float pitchSemitonesAt (const KickParams& p, double seconds); // båda svepen, halvtoner över svansen
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
    juce::dsp::StateVariableTPTFilter<float> highCut;
    juce::Random random;
};
