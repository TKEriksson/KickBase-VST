// `tool filtermodels`: mäter filtermodellerna Ladder, Formant och Comb (Source/Core/FilterModels.h, fas D2) som enheter: frekvenssvar mot en oberoende
// referens, självsvängning, NaN och extrema värden, återställning och CPU per sampling mot SVF-paret.

#include "../Source/Core/FilterModels.h"
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double fs = 48000.0;
    constexpr double pi = juce::MathConstants<double>::pi;

    // Förstärkningen för en sinus (exakt antal perioder i mätfönstret): effektivvärdet efter inställningen delat med insignalens.
    template <typename Process>
    double sineGain (Process&& process, double frequency, double amplitude = 0.05)
    {
        const auto settle = (int) (fs * 0.25), measure = (int) std::round (fs / frequency) * std::max (1, (int) (frequency / 50.0));
        auto in = 0.0, out = 0.0;
        for (int n = 0; n < settle + measure; ++n)
        {
            const auto x = (float) (amplitude * std::sin (2.0 * pi * frequency * (double) n / fs));
            const auto y = process (x);
            if (n >= settle)
            {
                in += (double) x * x;
                out += (double) y * y;
            }
        }
        return std::sqrt (out / std::max (1.0e-30, in));
    }

    double dB (double gain) { return 20.0 * std::log10 (std::max (1.0e-12, gain)); }
}

int filterModelTest()
{
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    //========================================================================== Ladder mot den analoga förebilden (bilinjär, förvriden)
    {
        auto analytic = [] (double f, double cutoff, int poles)
        {
            const auto x = std::tan (pi * f / (2.0 * fs)) / std::tan (pi * cutoff / (2.0 * fs)); // (stegets egen takt är 2x)
            return std::pow (1.0 / std::sqrt (1.0 + x * x), poles);
        };
        auto worst = 0.0;
        for (const auto f : { 100.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 })
        {
            ModelFilter filter;
            filter.prepare (fs);
            filter.setParameters (ModelFilter::ladder, 1000.0, 0.0, 0.0, 1.0, 0);
            const auto measured = sineGain ([&] (float x) { return filter.process (x); }, f);
            worst = std::max (worst, std::abs (dB (measured) - dB (analytic (f, 1000.0, 4))));
        }
        check (worst < 0.3, "Ladder LP 24 (cutoff 1 kHz): frekvenssvaret följer 4-poligt lågpass, störst avvikelse " + juce::String (worst, 3) + " dB (gräns 0,3)");

        ModelFilter lp12, hp12;
        lp12.prepare (fs);
        hp12.prepare (fs);
        lp12.setParameters (ModelFilter::ladder, 1000.0, 0.0, 0.0, 1.0, 1);
        hp12.setParameters (ModelFilter::ladder, 1000.0, 0.0, 0.0, 1.0, 3);
        const auto lp12Error = std::abs (dB (sineGain ([&] (float x) { return lp12.process (x); }, 2000.0)) - dB (analytic (2000.0, 1000.0, 2)));
        const auto x = std::tan (pi * 500.0 / (2.0 * fs)) / std::tan (pi * 1000.0 / (2.0 * fs));
        const auto hp12Error = std::abs (dB (sineGain ([&] (float s) { return hp12.process (s); }, 500.0)) - dB (x * x / (1.0 + x * x)));
        check (lp12Error < 0.3 && hp12Error < 0.3, "Ladder LP 12 och High följer sina förebilder (" + juce::String (lp12Error, 3) + " och " + juce::String (hp12Error, 3) + " dB)");

        // JUCE:s egen LadderFilter som sanity (dess cutoff går genom en 50 ms SmoothedValue, se FilterModels.h): samma sorts 24 dB/oktav-fall.
        juce::dsp::LadderFilter<float> juceLadder;
        juceLadder.prepare ({ fs, 512u, 1u });
        juceLadder.setMode (juce::dsp::LadderFilterMode::LPF24);
        juceLadder.setCutoffFrequencyHz (1000.0f);
        juceLadder.setResonance (0.0f);
        juceLadder.setDrive (1.0f);
        auto juceGain = [&] (double f)
        {
            juceLadder.reset();
            return sineGain ([&] (float s) { juce::AudioBuffer<float> b (1, 1); b.setSample (0, 0, s); juce::dsp::AudioBlock<float> block (b); juceLadder.process (juce::dsp::ProcessContextReplacing<float> (block)); return b.getSample (0, 0); }, f);
        };
        ModelFilter ours;
        ours.prepare (fs);
        ours.setParameters (ModelFilter::ladder, 1000.0, 0.0, 0.0, 1.0, 0);
        const auto slopeJuce = dB (juceGain (4000.0)) - dB (juceGain (2000.0));
        const auto slopeOurs = dB (sineGain ([&] (float s) { return ours.process (s); }, 4000.0)) - dB (sineGain ([&] (float s) { return ours.process (s); }, 2000.0));
        std::printf("  Ladder mot JUCE:s LadderFilter: lutning 2 -> 4 kHz %.1f dB (JUCE) och %.1f dB (vår)\n", slopeJuce, slopeOurs);
        check (std::abs (slopeJuce - slopeOurs) < 4.0 && slopeOurs < -18.0, "Ladder: samma 24 dB/oktav-lutning som JUCE:s LadderFilter (inom 4 dB)");

        // Resonans 100 %: självsvängning vid cutoff efter en impuls (nollgenomgångar), begränsad av mättnaden.
        ModelFilter ring;
        ring.prepare (fs);
        ring.setParameters (ModelFilter::ladder, 1000.0, 1.0, 0.0, 1.0, 0);
        std::vector<float> out ((size_t) (fs * 1.0));
        for (size_t n = 0; n < out.size(); ++n)
            out[n] = ring.process (n == 0 ? 0.5f : 0.0f);
        auto crossings = 0;
        for (size_t n = (size_t) (fs * 0.5); n + 1 < out.size(); ++n)
            if (out[n] < 0.0f && out[n + 1] >= 0.0f)
                ++crossings;
        auto peak = 0.0f;
        for (auto v : out)
            peak = std::max (peak, std::abs (v));
        const auto hz = (double) crossings / 0.5; // uppåtgående nollgenomgångar under 0,5 s
        check (std::abs (hz - 1000.0) < 40.0 && peak > 0.05f && peak < 4.0f, "Ladder resonans 100 %: självsvänger vid cutoff (" + juce::String (hz, 0) + " Hz, topp " + juce::String (peak, 2) + ")");
    }

    //========================================================================== Formant mot oberoende biquads (RBJ, bandpass med toppen 0 dB)
    {
        struct V { double f[5], db[5], bw[5]; };
        const V a { { 600, 1040, 2250, 2450, 2750 }, { 0, -7, -9, -9, -20 }, { 60, 70, 110, 120, 130 } };
        auto referenceMagnitude = [&] (double frequency, double bwScale)
        {
            std::complex<double> sum = 0.0;
            const auto z = std::polar (1.0, -2.0 * pi * frequency / fs);
            for (int i = 0; i < 5; ++i)
            {
                const auto q = a.f[i] / (a.bw[i] * bwScale);
                const auto w0 = 2.0 * pi * a.f[i] / fs, alpha = std::sin (w0) / (2.0 * q);
                const auto b0 = alpha, b2 = -alpha, a0 = 1.0 + alpha, a1 = -2.0 * std::cos (w0), a2 = 1.0 - alpha;
                const auto h = (b0 + b2 * z * z) / (a0 + a1 * z + a2 * z * z);
                sum += std::pow (10.0, a.db[i] / 20.0) * h;
            }
            return std::abs (sum);
        };
        auto worst = 0.0;
        for (const auto resonance : { 0.0, 0.5 })
            for (const auto f : { 200.0, 500.0, 600.0, 900.0, 1040.0, 1500.0, 2250.0, 2600.0, 3500.0, 6000.0 })
            {
                ModelFilter filter;
                filter.prepare (fs);
                filter.setParameters (ModelFilter::formant, 1000.0, resonance, 0.0, 1.0, 0);
                const auto measured = sineGain ([&] (float x) { return filter.process (x); }, f, 0.2);
                worst = std::max (worst, std::abs (dB (measured) - dB (referenceMagnitude (f, 2.0 * std::pow (0.05, resonance)))));
            }
        check (worst < 0.2, "Formant (vokalen A): frekvenssvaret följer fem parallella biquads, störst avvikelse " + juce::String (worst, 3) + " dB (gräns 0,2)");

        // Nivån: toppen över hela frekvensområdet för alla vokaler (MORPH) och två RESO får inte passera +6 dB.
        auto highest = 0.0;
        for (const auto morph : { 0.0, 0.125, 0.25, 0.5, 0.75, 0.875, 1.0 })
            for (const auto resonance : { 0.2, 0.9 })
                for (double f = 60.0; f < 12000.0; f *= 1.07)
                {
                    ModelFilter filter;
                    filter.prepare (fs);
                    filter.setParameters (ModelFilter::formant, 1000.0, resonance, morph, 1.0, 0);
                    highest = std::max (highest, sineGain ([&] (float x) { return filter.process (x); }, f, 0.2));
                }
        check (dB (highest) <= 6.0, "Formant: toppen över alla vokaler och frekvenser är " + juce::String (dB (highest), 2) + " dB (gräns +6 dB)");

        ModelFilter dry;
        dry.prepare (fs);
        dry.setParameters (ModelFilter::formant, 1000.0, 0.5, 0.3, 0.0, 0);
        auto identical = true;
        for (int n = 0; n < 200; ++n)
        {
            const auto x = (float) std::sin (0.1 * n);
            identical = identical && dry.process (x) == x;
        }
        check (identical, "Formant BLEND 0 släpper igenom insignalen oförändrad");
    }

    //========================================================================== Comb mot en rak implementation (heltalsfördröjning)
    {
        constexpr int delaySamples = 100;
        const auto fb = 0.7;
        ModelFilter comb;
        comb.prepare (fs);
        comb.setParameters (ModelFilter::comb, fs / delaySamples, fb / 0.97, 0.0, 1.0, 0); // reso så att återkopplingen blir 0,7
        std::vector<double> y (4000, 0.0);
        juce::Random random (7);
        auto worst = 0.0;
        for (size_t n = 0; n < y.size(); ++n)
        {
            const auto x = (double) random.nextFloat() * 2.0 - 1.0;
            y[n] = x + (n >= (size_t) delaySamples ? fb * y[n - (size_t) delaySamples] : 0.0);
            const auto expected = y[n] * std::sqrt (1.0 - fb * fb);
            const auto got = (double) comb.process ((float) x);
            worst = std::max (worst, std::abs (got - expected));
        }
        check (worst < 1.0e-5, "Comb (heltalsfördröjning 100): samma utsignal som en rak implementation, störst skillnad " + juce::String (worst, 8));

        // Kamfrekvensen följer cutoff (bråkdelsfördröjning): toppar vid 440 Hz och dalar halvvägs.
        ModelFilter tuned;
        tuned.prepare (fs);
        tuned.setParameters (ModelFilter::comb, 440.0, 0.9, 0.0, 1.0, 0);
        const auto peakGain = dB (sineGain ([&] (float x) { return tuned.process (x); }, 440.0));
        ModelFilter valley;
        valley.prepare (fs);
        valley.setParameters (ModelFilter::comb, 440.0, 0.9, 0.0, 1.0, 0);
        const auto valleyGain = dB (sineGain ([&] (float x) { return valley.process (x); }, 660.0));
        check (peakGain - valleyGain > 12.0, "Comb: toppen vid cutoff (" + juce::String (peakGain, 1) + " dB) och dalen vid 1,5 x cutoff (" + juce::String (valleyGain, 1) + " dB)");

        // DAMPING dämpar de höga övertonerna i slingan.
        ModelFilter bright, dark;
        bright.prepare (fs);
        dark.prepare (fs);
        bright.setParameters (ModelFilter::comb, 1000.0, 0.9, 0.0, 1.0, 0);
        dark.setParameters (ModelFilter::comb, 1000.0, 0.9, 1.0, 1.0, 0);
        const auto highBright = sineGain ([&] (float x) { return bright.process (x); }, 8000.0), highDark = sineGain ([&] (float x) { return dark.process (x); }, 8000.0);
        check (highDark < highBright * 0.7, "Comb DAMPING lägre vid 8 kHz (" + juce::String (dB (highDark), 1) + " mot " + juce::String (dB (highBright), 1) + " dB)");
        std::printf ("  Comb: fördröjningslinjen är %d samplingar (double) per kanal vid 48 kHz, %d vid 192 kHz\n", ModelFilter::combMaxDelaySamples (48000.0), ModelFilter::combMaxDelaySamples (192000.0));
    }

    //========================================================================== NaN, extrema värden och återställning
    {
        auto bad = 0;
        auto biggest = 0.0f;
        juce::Random random (11);
        for (const auto model : { ModelFilter::ladder, ModelFilter::formant, ModelFilter::comb })
            for (const auto cutoff : { 20.0, 200.0, 1000.0, 9000.0, 21600.0 })
                for (const auto resonance : { 0.0, 0.5, 1.0 })
                    for (const auto p1 : { 0.0, 1.0 })
                        for (const auto p2 : { 0.0, 1.0 })
                            for (const auto type : { 0, 1, 2, 3 })
                            {
                                ModelFilter filter;
                                filter.prepare (fs);
                                for (int n = 0; n < 6000; ++n)
                                {
                                    // Cutoff ändras var 7:e sampling (som en envelope) och insignalen är brus med en impuls av full nivå.
                                    if (n % 7 == 0)
                                        filter.setParameters (model, cutoff * (1.0 + 0.5 * std::sin (0.01 * n)), resonance, p1, p2, type);
                                    const auto y = filter.process (n == 100 ? 1.0f : random.nextFloat() * 2.0f - 1.0f);
                                    if (! std::isfinite (y))
                                        ++bad;
                                    biggest = std::max (biggest, std::abs (y));
                                }
                            }
        check (bad == 0 && biggest < 60.0f, "alla modeller med extrema värden: " + juce::String (bad) + " icke-ändliga, största värde " + juce::String (biggest, 1));

        auto silent = true;
        for (const auto model : { ModelFilter::ladder, ModelFilter::formant, ModelFilter::comb })
        {
            ModelFilter filter;
            filter.prepare (fs);
            filter.setParameters (model, 800.0, 0.8, 0.5, 1.0, 0);
            for (int n = 0; n < 3000; ++n)
                filter.process (random.nextFloat() * 2.0f - 1.0f);
            filter.reset();
            filter.setParameters (model, 800.0, 0.8, 0.5, 1.0, 0);
            for (int n = 0; n < 500; ++n)
                silent = silent && filter.process (0.0f) == 0.0f;
        }
        check (silent, "reset() nollställer alla tre modeller (tystnad in ger tystnad ut)");
    }

    //========================================================================== CPU per sampling (mitt + sida), cutoff ändras varje sampling (som filter-envelopen)
    {
        auto timeIt = [] (ModelFilter::Model model, bool changing)
        {
            ModelFilter mid, side;
            mid.prepare (fs);
            side.prepare (fs);
            juce::Random random (3);
            constexpr int count = 1 << 20;
            auto sink = 0.0f;
            const auto start = std::chrono::steady_clock::now();
            for (int n = 0; n < count; ++n)
            {
                const auto cutoff = changing ? 300.0 + 3000.0 * (0.5 + 0.5 * std::sin (0.002 * n)) : 1500.0;
                mid.setParameters (model, cutoff, 0.4, 0.3, 0.8, 0);
                side.setParameters (model, cutoff, 0.4, 0.3, 0.8, 0);
                sink += mid.process (random.nextFloat() - 0.5f) + side.process (random.nextFloat() - 0.5f);
            }
            const auto ns = std::chrono::duration<double, std::nano> (std::chrono::steady_clock::now() - start).count() / count;
            return ns + (sink == 12345.678f ? 1.0 : 0.0);
        };
        const auto names = std::array<const char*, 3> { "Ladder", "Formant", "Comb" };
        const auto models = std::array<ModelFilter::Model, 3> { ModelFilter::ladder, ModelFilter::formant, ModelFilter::comb };
        for (size_t i = 0; i < 3; ++i)
            std::printf ("  %-8s %6.1f ns per sampling (mitt + sida, oförändrad cutoff), %6.1f ns (cutoff ändras varje sampling)\n", names[i], timeIt (models[i], false), timeIt (models[i], true));
    }

    std::puts (failed ? "Filtermodellerna: FEL" : "Filtermodellerna gick igenom");
    return failed ? 1 : 0;
}
