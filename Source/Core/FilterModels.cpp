#include "FilterModels.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    // tanh som rationell approximation (Padé): exakt 1 från x = 3, kontinuerlig, ingen std::tanh på ljudtråden (långsam). Deterministisk.
    inline double fastTanh (double x)
    {
        if (x >= 3.0)
            return 1.0;
        if (x <= -3.0)
            return -1.0;
        return x * (27.0 + x * x) / (27.0 + 9.0 * x * x);
    }

    // Vokalernas formanter: basröst, a e i o u, fem formanter var (frekvens Hz, nivå dB, bandbredd Hz).
    // Källa: Csound Manual, Appendix D "Formant Values", bass voice, Table D.6 - D.10 (https://csound.com/docs/manual/MiscFormants.html).
    // Uppmätta värden (fakta), ingen kod är kopierad.
    struct Vowel { double freq[5], db[5], bw[5]; };
    constexpr Vowel vowels[5] {
        { { 600, 1040, 2250, 2450, 2750 }, { 0, -7, -9, -9, -20 },    { 60, 70, 110, 120, 130 } }, // a
        { { 400, 1620, 2400, 2800, 3100 }, { 0, -12, -9, -12, -18 },  { 40, 80, 100, 120, 120 } }, // e
        { { 250, 1750, 2600, 3050, 3340 }, { 0, -30, -16, -22, -28 }, { 60, 90, 100, 120, 120 } }, // i
        { { 400, 750, 2400, 2600, 2900 },  { 0, -11, -21, -20, -40 }, { 40, 80, 100, 120, 120 } }, // o
        { { 350, 600, 2400, 2675, 2950 },  { 0, -20, -32, -28, -36 }, { 40, 80, 100, 120, 120 } }, // u
    };
}

struct ModelFilter::Impl
{
    double sampleRate = 44100.0;
    int appliedModel = 0;

    //============================================================================ Ladder (2x)
    Oversampler2x oversampler;
    std::array<double, 4> ladderState {};   // de fyra TPT-integratorernas tillstånd
    double ladderG = 0.0, ladderK = 0.0, ladderDrive = 1.0, ladderDriveGain = 1.0, ladderMakeup = 1.0;
    double lastLadderCutoff = -1.0;
    int ladderMode = 0;

    // Vadim Zavalishin, "The Art of VA Filter Design" (Native Instruments, rev. 2.1.2), kapitlet om transistorstegen: fyra TPT-lågpass i serie, återkopplingen
    // u = x - k y4 löses i sluten form (y4 = G^4 u + summan av de fyra stegens tillstånd), ingen enkelsamplings-fördröjning.
    // En sampling på 2x-takten. y4 = fyra steg; lägena blandar stegens utgångar (som JUCE:s LadderFilter: LPF24 = y4, LPF12 = y2, BPF12 = y1 - y2, HPF12 = u - 2 y1 + y2).
    double ladderStep (double x)
    {
        const auto G = ladderG;
        const auto oneMinusG = 1.0 - G;
        const auto c1 = oneMinusG * ladderState[0], c2 = oneMinusG * ladderState[1], c3 = oneMinusG * ladderState[2], c4 = oneMinusG * ladderState[3];
        // y4 = G^4 u + (G^3 c1 + G^2 c2 + G c3 + c4); återkopplingen u = x - k y4 löses utan fördröjning.
        const auto G2 = G * G, G4 = G2 * G2;
        const auto feedback = G2 * G * c1 + G2 * c2 + G * c3 + c4;
        const auto uLinear = (x - ladderK * feedback) / (1.0 + ladderK * G4);
        const auto u = fastTanh (ladderDrive * uLinear) * ladderDriveGain; // mättnaden; resonansen över 4 självsvänger och begränsas här

        const auto y1 = G * u + c1;
        const auto y2 = G * y1 + c2;
        const auto y3 = G * y2 + c3;
        const auto y4 = G * y3 + c4;
        // TPT: s = y + v med v = G (x - s) = y - (1 - G) s ... uppdateringen s' = 2 y - s.
        ladderState[0] = 2.0 * y1 - ladderState[0];
        ladderState[1] = 2.0 * y2 - ladderState[1];
        ladderState[2] = 2.0 * y3 - ladderState[2];
        ladderState[3] = 2.0 * y4 - ladderState[3];

        double out;
        switch (ladderMode)
        {
            case 1:  out = y2; break;                      // LP 12
            case 2:  out = 2.0 * (y1 - y2); break;         // Band (12 dB, toppen 1)
            case 3:  out = u - 2.0 * y1 + y2; break;       // High (12 dB)
            default: out = y4; break;                      // LP 24
        }
        return out * ladderMakeup;
    }

    //============================================================================ Formant
    struct Band
    {
        double a1 = 0.0, a2 = 0.0, a3 = 0.0, gain = 0.0;   // koefficienter (TPT-SVF) och nivå inklusive 1/Q (toppen 0 dB)
        double ic1 = 0.0, ic2 = 0.0;
    };
    std::array<Band, 5> bands;
    double formantShift = 1.0, formantBwScale = 1.0, formantMorph = 0.0, formantBlend = 1.0;
    int formantCounter = 0; // absolut: räknar en gång per sampling och nollställs bara av reset() (aldrig per DAW-block), så ljudet beror inte på blockstorleken
    static constexpr int formantUpdateInterval = 8; // koefficienterna räknas om var 8:e sampling (fem tan per omräkning)
    bool formantDirty = true;

    // Vokalen (MORPH) och bandbredden (RESO) ger de grundfrekvenser, nivåer och bandbredder som cutoff sedan förskjuter: de dyra delarna (exp, log, pow) görs bara
    // när MORPH eller RESO ändras, och bara tan (en per band) när cutoff ändras (filter-envelopen).
    std::array<double, 5> baseFrequency {}, baseAmplitude {}, baseBandwidth {};
    bool formantBaseDirty = true;

    void formantRecompute()
    {
        if (formantBaseDirty)
        {
            const auto position = juce::jlimit (0.0, 1.0, formantMorph) * 4.0;
            const auto index = std::min (3, (int) position);
            const auto t = position - index;
            const auto& v0 = vowels[index];
            const auto& v1 = vowels[index + 1];
            for (size_t i = 0; i < 5; ++i)
            {
                baseFrequency[i] = std::exp (std::log (v0.freq[i]) * (1.0 - t) + std::log (v1.freq[i]) * t);
                baseAmplitude[i] = std::pow (10.0, (v0.db[i] * (1.0 - t) + v1.db[i] * t) / 20.0);
                baseBandwidth[i] = std::max (10.0, (v0.bw[i] * (1.0 - t) + v1.bw[i] * t) * formantBwScale);
            }
            formantBaseDirty = false;
        }
        for (size_t i = 0; i < 5; ++i)
        {
            const auto frequency = std::min (0.45 * sampleRate, baseFrequency[i] * formantShift);
            const auto g = std::tan (juce::MathConstants<double>::pi * frequency / sampleRate);
            const auto k = baseBandwidth[i] / frequency; // 1/Q
            auto& b = bands[i];
            b.a1 = 1.0 / (1.0 + g * (g + k));
            b.a2 = g * b.a1;
            b.a3 = g * b.a2;
            b.gain = baseAmplitude[i] * k * formantNormalisation;
        }
    }
    static constexpr double formantNormalisation = 1.0;

    double formantStep (double x)
    {
        if (formantDirty && formantCounter == 0) // ändrade värden slår igenom vid nästa omräkningstillfälle (var 8:e sampling)
        {
            formantRecompute();
            formantDirty = false;
        }
        formantCounter = (formantCounter + 1) % formantUpdateInterval;

        auto wet = 0.0;
        for (auto& b : bands)
        {
            const auto v3 = x - b.ic2;
            const auto v1 = b.a1 * b.ic1 + b.a2 * v3;
            const auto v2 = b.ic2 + b.a2 * b.ic1 + b.a3 * v3;
            b.ic1 = 2.0 * v1 - b.ic1;
            b.ic2 = 2.0 * v2 - b.ic2;
            wet += b.gain * v1;
        }
        return x * (1.0 - formantBlend) + wet * formantBlend;
    }

    //============================================================================ Comb
    juce::dsp::DelayLine<double, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay { 4 };
    double combDelaySamples = 100.0, combFeedback = 0.0, combDampCoefficient = 1.0, combBlend = 1.0, combLowpass = 0.0;

    double combStep (double x)
    {
        // Slingans fördröjning är D = fördröjningslinjen + en sampling (återkopplingen tas från förra samplingens lågpass).
        const auto y = x + combFeedback * combLowpass;
        delay.pushSample (0, y);
        const auto delayed = delay.popSample (0, combDelaySamples);
        combLowpass += (delayed - combLowpass) * combDampCoefficient;
        const auto wet = y * std::sqrt (std::max (0.0, 1.0 - combFeedback * combFeedback)); // bevarar nivån för brus
        return x * (1.0 - combBlend) + wet * combBlend;
    }
};

ModelFilter::ModelFilter() : impl (std::make_unique<Impl>()) {}
ModelFilter::~ModelFilter() = default;
ModelFilter::ModelFilter (ModelFilter&&) noexcept = default;
ModelFilter& ModelFilter::operator= (ModelFilter&&) noexcept = default;

void ModelFilter::prepare (double baseSampleRate)
{
    auto& s = *impl;
    s.sampleRate = baseSampleRate;
    s.delay.setMaximumDelayInSamples (combMaxDelaySamples (baseSampleRate));
    s.delay.prepare ({ baseSampleRate, 1u, 1u });
    s.lastLadderCutoff = -1.0;
    reset();
}

void ModelFilter::reset()
{
    auto& s = *impl;
    s.oversampler.reset();
    s.ladderState.fill (0.0);
    for (auto& b : s.bands)
        b.ic1 = b.ic2 = 0.0;
    s.formantCounter = 0;
    s.formantDirty = true;
    s.formantBaseDirty = true;
    s.delay.reset();
    s.combLowpass = 0.0;
}

void ModelFilter::setParameters (Model model, double cutoffHz, double resonance, double p1, double p2, int filterType)
{
    auto& s = *impl;
    s.appliedModel = (int) model;
    resonance = juce::jlimit (0.0, 1.0, resonance);
    p1 = juce::jlimit (0.0, 1.0, p1);
    p2 = juce::jlimit (0.0, 1.0, p2);

    switch (model)
    {
        case ladder:
        {
            if (cutoffHz != s.lastLadderCutoff)
            {
                s.lastLadderCutoff = cutoffHz;
                const auto g = std::tan (juce::MathConstants<double>::pi * juce::jlimit (20.0, 0.45 * s.sampleRate, cutoffHz) / (2.0 * s.sampleRate)); // 2x-takten
                s.ladderG = g / (1.0 + g);
            }
            s.ladderMode = juce::jlimit (0, 3, filterType);
            s.ladderK = 4.4 * resonance; // över 4: självsvängning, mättnaden begränsar amplituden
            s.ladderDrive = 1.0 + 5.0 * p1;
            s.ladderDriveGain = 1.0 / std::sqrt (s.ladderDrive); // mättnaden ger (nästan) samma nivå vid olika DRIVE
            const auto kEffective = std::min (s.ladderK, 4.0) * (1.0 - resonance * resonance * resonance * resonance);
            s.ladderMakeup = 1.0 + 0.5 * kEffective; // passbandet tappar nivå med resonansen
            break;
        }
        case formant:
        {
            const auto shift = juce::jlimit (0.25, 4.0, cutoffHz / 1000.0);
            const auto bwScale = 2.0 * std::pow (0.05, resonance); // RESO 0: dubbla bandbredden, 1: en tjugondel
            if (bwScale != s.formantBwScale || p1 != s.formantMorph)
            {
                s.formantBwScale = bwScale;
                s.formantMorph = p1;
                s.formantBaseDirty = true;
                s.formantDirty = true;
            }
            if (shift != s.formantShift)
            {
                s.formantShift = shift;
                s.formantDirty = true; // räknas om vid nästa omräkningstillfälle
            }
            s.formantBlend = p2;
            break;
        }
        case comb:
        {
            const auto delaySamples = juce::jlimit (3.0, (double) combMaxDelaySamples (s.sampleRate) - 4.0, s.sampleRate / std::max (20.0, cutoffHz));
            s.combDelaySamples = delaySamples - 1.0; // (återkopplingen tas från förra samplingens lågpass)
            s.delay.setDelay (s.combDelaySamples);
            s.combFeedback = 0.97 * resonance;
            s.combDampCoefficient = 1.0 - 0.95 * p1;
            s.combBlend = p2;
            break;
        }
    }
}

float ModelFilter::process (float input)
{
    auto& s = *impl;
    const auto x = (double) input;
    switch (s.appliedModel)
    {
        case ladder:  return (float) s.oversampler.process (x, [&s] (double v) { return s.ladderStep (v); });
        case formant: return (float) s.formantStep (x);
        case comb:    return (float) s.combStep (x);
        default:      return input;
    }
}
