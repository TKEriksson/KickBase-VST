#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include "NodeEnvelope.h"
#include "SampleData.h"

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
    int clipMode = 0;           // drive: 0 mjuk, 1 hård, 2 rör (osymmetrisk), 3 vikning
    float highCutHz = 20000.0f; // lågpass på kicken (tämjer klicket)

    // Fler lager och former (som andra kick-synthar): kroppens vågform, sub, FM, brusig topp, bitkrossning, lågt skär.
    int wave = 0;               // kroppens vågform: 0 sinus, 1 triangel, 2 mjuk fyrkant, 3 mjuk såg
    float sub = 0.0f;           // sinus en oktav under kroppen, 0..1
    float fm = 0.0f;            // FM på kroppen, 0..1: en andra sinus (fmRatio gånger frekvensen) modulerar fasen, avtar på fmDecayMs
    float fmRatio = 2.0f;
    float fmDecayMs = 40.0f;
    float noise = 0.0f;         // brusig topp (högpassat brus som avtar för sig), 0..1
    float noiseDecayMs = 60.0f;
    float noiseToneHz = 6000.0f;
    float crush = 0.0f;         // bitkrossning efter drive, 0..1 (16 bitar ... 4 bitar)
    float lowCutHz = 20.0f;     // högpass: tar bort sub-rumlet under kickens ton (20 = av)
    float velPitch = 0.0f;      // 0..1: lägre velocity ger ett mindre pitch-svep
    int sampleMode = 0;         // inladdad sampling: 0 klick (de första millisekunderna), 1 lager (hela, ovanpå kicken), 2 ersätter (bara sampling)
    float sampleLevel = 1.0f;   // nivå för sampling i läge lager/ersätt (klicket använder clickLevel)
    float sampleTuneSemis = 0.0f; // samplingens uppspelningshastighet i halvtoner

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

// Syntetiserad kick: sinus med pitch-envelope, amplitud-envelope, klick och drive.
class KickEngine
{
public:
    void prepare (double sampleRate);

    // Slumpen (klickens brus) är seedad, så att samma preset renderar bit-identiskt varje gång.
    void setSeed (juce::int64 seed) { random.setSeed (seed); }
    void reset();
    void setParams (const KickParams& newParams) { params = newParams; }
    void setClickSample (const ClickSample* sample) { clickSample = sample; }

    // `transposeSemitones`: flyttar hela kicken (svans och svep) så här många halvtoner, för kicknoter med tonhöjd.
    void trigger (float velocity, int transposeSemitones = 0);
    float nextSample();

    // Nuvarande amplitud-envelope (0..1). Basen duckas efter den.
    float getEnvelope() const { return envelope; }

    // Den senaste kickens ljud spelas av gränssnittet för att rita vågformen (se render).
    static juce::AudioBuffer<float> render (const KickParams& p, const ClickSample* click, double sampleRate);

    // Hur länge kicken klingar: envelopen, eller hela samplingen om den spelas som lager/ersättning.
    static double totalSeconds (const KickParams& p, const ClickSample* sample, double sampleRate);

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
        double subPhase = 0.0, fmPhase = 0.0;
        float pitchScale = 1.0f; // hur mycket av pitch-svepet som används (velocity)
        float sampleFollower = 0.0f; // samplingens nivå, för duckningen
        double samplePosition = 0.0; // i samplingens samples
        float velocity = 1.0f;
        float transpose = 1.0f;  // frekvensfaktor
        float fade = 1.0f;       // < 1 när rösten tonas ut för att ge plats åt en ny
        bool fadingOut = false;
        juce::dsp::StateVariableTPTFilter<float> clickFilter, noiseFilter;
    };

    float renderVoice (Voice& voice);

    KickParams params;
    const ClickSample* clickSample = nullptr;
    std::array<Voice, 2> voices;
    size_t currentVoice = 0;
    double sampleRate = 44100.0;
    float fadeStep = 0.0f;
    float envelope = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> highCut, lowCut;
    juce::Random random { 1 };
};
