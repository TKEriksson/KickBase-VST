// `tool tb303measure`: mäter hur TeeBeeFilter (Open303, läget TB_303) beter sig vid 1x, 2x och 4x översampling mot en 8x-referens, så att vi
// vet hur mycket översampling filtret behöver i Kickbase (fas D1 i docs/content-plan.md). Open303 själv kör filtret på 4x takten.

#include "../Source/ThirdParty/open303/rosic_TeeBeeFilter.h"
#include "../Source/Core/Tb303Filter.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace
{
    // Halverar samplingsfrekvensen: linjärfas FIR (Kaiser, ca 90 dB) med nollfördröjning (utsignalen ligger på samma plats som insignalen).
    std::vector<double> halve (const std::vector<double>& input)
    {
        constexpr int order = 80; // jämn: gruppfördröjning 40
        static const std::vector<double> taps = []
        {
            const auto bessel = [] (double x) { auto sum = 1.0, term = 1.0; for (int k = 1; k < 60; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; } return sum; };
            const auto beta = 0.1102 * (90.0 - 8.7);
            std::vector<double> t (order + 1);
            auto total = 0.0;
            for (int n = 0; n <= order; ++n)
            {
                const auto m = (double) n - 0.5 * order;
                const auto cutoff = 0.25; // normaliserad mot indatans takt: halva utsignalens Nyquist 0,25 (mitten i övergången 0,2-0.3 av indatats takt)
                const auto sinc = std::abs (m) < 1e-12 ? 2.0 * cutoff : std::sin (2.0 * juce::MathConstants<double>::pi * cutoff * m) / (juce::MathConstants<double>::pi * m);
                const auto r = 2.0 * n / order - 1.0;
                t[(size_t) n] = sinc * bessel (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel (beta);
                total += t[(size_t) n];
            }
            for (auto& v : t)
                v /= total;
            return t;
        }();
        const auto length = (int) input.size();
        std::vector<double> out ((size_t) length / 2);
        for (int j = 0; j < (int) out.size(); ++j)
        {
            const auto centre = 2 * j;
            auto sum = 0.0;
            for (int k = 0; k <= order; ++k)
            {
                const auto index = centre + order / 2 - k;
                if (index >= 0 && index < length)
                    sum += taps[(size_t) k] * input[(size_t) index];
            }
            out[(size_t) j] = sum;
        }
        return out;
    }

    std::vector<double> decimateBy (std::vector<double> signal, int factor)
    {
        for (; factor > 1; factor /= 2)
            signal = halve (signal);
        return signal;
    }

    std::vector<double> runFilter (const std::vector<double>& input, double rate, double cutoff, double resonancePercent)
    {
        rosic::TeeBeeFilter filter;
        filter.setSampleRate (rate);
        filter.setMode (rosic::TeeBeeFilter::TB_303);
        filter.setCutoff (cutoff, false);
        filter.setResonance (resonancePercent, true);
        filter.reset();
        std::vector<double> out (input.size());
        for (size_t i = 0; i < input.size(); ++i)
            out[i] = filter.getSample (input[i]);
        return out;
    }

    double rms (const std::vector<double>& v, size_t from)
    {
        auto sum = 0.0;
        for (size_t i = from; i < v.size(); ++i)
            sum += v[i] * v[i];
        return std::sqrt (sum / (double) std::max<size_t> (1, v.size() - from));
    }
}

// Beloppet vid grundtonens övertoner (perioden är exakt 400 samplingar, fönstret 20 perioder, så varje överton ligger på en exakt DFT-punkt).
// Filtret i TB_303-läget är linjärt, så skillnaden mellan olika översampling är en skillnad i frekvenssvar (och en liten tidsfördröjning), inte distorsion.
static std::vector<double> harmonicMagnitudes (const std::vector<double>& x, size_t start, int count)
{
    constexpr int period = 400, periods = 20;
    std::vector<double> magnitudes ((size_t) count);
    for (int k = 1; k <= count; ++k)
    {
        auto re = 0.0, im = 0.0;
        for (int n = 0; n < period * periods; ++n)
        {
            const auto angle = -2.0 * juce::MathConstants<double>::pi * (double) k * (double) n / (double) period;
            const auto v = x[start + (size_t) n];
            re += v * std::cos (angle);
            im += v * std::sin (angle);
        }
        magnitudes[(size_t) k - 1] = 2.0 * std::sqrt (re * re + im * im) / (double) (period * periods);
    }
    return magnitudes;
}

int tb303Measure()
{
    std::printf ("TeeBeeFilter TB_303: frekvenssvaret (beloppet vid såg-övertonerna upp till 0,35 fs) vid 1x, 2x (ideal) och Kickbase egen kedja mot 4x (Open303 kör filtret på 4x).\n");
    std::printf ("Största och medelavvikelse i dB (bara övertoner som är minst -50 dB under referensens starkaste). Lägre = bättre.\n");
    std::printf ("%8s %8s %5s | %-15s | %-15s | %-15s | %s\n", "fs", "cutoff", "res%", "1x max / medel", "2x ideal max/medel", "vår kedja max/medel", "ref topp");
    auto allFinite = true;
    for (const auto fs : { 44100.0, 48000.0, 96000.0 })
    {
        const auto period = 400; // grundtonen = fs / 400 (110,25 Hz vid 44,1 kHz)
        const auto n8 = (size_t) (period * 8 * 60); // 60 perioder vid 8x
        std::vector<double> saw (n8);
        for (size_t i = 0; i < n8; ++i)
            saw[i] = 0.5 * (2.0 * std::fmod ((double) i / (double) (period * 8), 1.0) - 1.0);
        const auto count = (int) (0.35 * period);
        const auto start = (size_t) period * 30; // väl efter insvängningen
        for (const auto cutoff : { 300.0, 1000.0, 3000.0, 8000.0, 16000.0 })
            for (const auto resonance : { 0.0, 60.0, 100.0 })
            {
                std::vector<double> mags[4];
                const int factors[] { 1, 2, 4, 8 };
                for (int f = 0; f < 4; ++f)
                {
                    const auto factor = factors[f];
                    if (f == 3) // fjärde kolumnen: Kickbase egen kedja (Tb303Filter: JUCE:s halvbands-IIR, TeeBee på 2x) i stället för 8x
                    {
                        Tb303Filter ours;
                        ours.prepare (fs);
                        ours.setParameters (cutoff, resonance);
                        std::vector<double> result;
                        for (const auto v : decimateBy (saw, 8))
                            result.push_back ((double) ours.process ((float) v));
                        for (const auto v : result)
                            allFinite = allFinite && std::isfinite (v);
                        mags[3] = harmonicMagnitudes (result, start, count);
                        continue;
                    }
                    const auto filtered = runFilter (decimateBy (saw, 8 / factor), fs * factor, cutoff, resonance);
                    for (const auto v : filtered)
                        allFinite = allFinite && std::isfinite (v);
                    mags[f] = harmonicMagnitudes (decimateBy (filtered, factor), start, count);
                }
                auto top = 0.0;
                for (const auto m : mags[2])
                    top = std::max (top, m);
                double maxError[3], meanError[3];
                const int compare[] { 0, 1, 3 }; // 1x, 2x och 8x mot 4x (Open303:s egen takt)
                for (int f = 0; f < 3; ++f)
                {
                    auto worst = 0.0, sum = 0.0;
                    auto used = 0;
                    for (size_t h = 0; h < mags[2].size(); ++h)
                        if (mags[2][h] > top * 0.00316) // minst -50 dB under toppen
                        {
                            const auto error = std::abs (20.0 * std::log10 (std::max (1e-12, mags[compare[f]][h]) / mags[2][h]));
                            worst = std::max (worst, error);
                            sum += error;
                            ++used;
                        }
                    maxError[f] = worst;
                    meanError[f] = used > 0 ? sum / used : 0.0;
                }
                std::printf ("%8.0f %8.0f %5.0f | %6.2f / %6.2f | %6.2f / %6.2f | %6.2f / %6.2f | %.3f\n", fs, cutoff, resonance, maxError[0], meanError[0], maxError[1], meanError[1],
                             maxError[2], meanError[2], top);
            }
    }
    std::printf (allFinite ? "alla värden ändliga\n" : "FEL: icke-ändliga värden\n");
    return allFinite ? 0 : 1;
}

// ===========================================================================================================================================
// `tool tb303test` (fas D1): TB-303-modellen som helhet. Svaret mot Open303:s egen takt (4x), stabilitet vid extrema inställningar, att allpass-kedjorna
// och TeeBee klingar ut utan CPU-toppar (denormaler) och modellbyte mitt i toner med resonans 100 %.
#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"
#include <chrono>

namespace
{
    // Skillnaden mot 4x i dB vid sågens övertoner (som tb303measure), för Kickbase egen kedja.
    void oursVersus4x (double fs, double cutoff, double resonance, double& maxError, double& meanError)
    {
        const auto period = 400;
        const auto n8 = (size_t) (period * 8 * 60);
        std::vector<double> saw (n8);
        for (size_t i = 0; i < n8; ++i)
            saw[i] = 0.5 * (2.0 * std::fmod ((double) i / (double) (period * 8), 1.0) - 1.0);
        const auto count = (int) (0.35 * period);
        const auto start = (size_t) period * 30;

        const auto reference = harmonicMagnitudes (decimateBy (runFilter (decimateBy (saw, 2), fs * 4, cutoff, resonance), 4), start, count);
        Tb303Filter ours;
        ours.prepare (fs);
        ours.setParameters (cutoff, resonance);
        std::vector<double> result;
        for (const auto v : decimateBy (saw, 8))
            result.push_back ((double) ours.process ((float) v));
        const auto mine = harmonicMagnitudes (result, start, count);

        auto top = 0.0;
        for (const auto m : reference)
            top = std::max (top, m);
        maxError = meanError = 0.0;
        auto used = 0;
        for (size_t h = 0; h < reference.size(); ++h)
            if (reference[h] > top * 0.00316)
            {
                const auto error = std::abs (20.0 * std::log10 (std::max (1e-12, mine[h]) / reference[h]));
                maxError = std::max (maxError, error);
                meanError += error;
                ++used;
            }
        meanError = used > 0 ? meanError / used : 0.0;
    }
}

int tb303Test()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // 1. Svaret mot 4x-referensen (Open303:s egen takt): inom de gränser som mättes (med marginal).
    struct Case { double fs, cutoff, resonance, maxLimit; };
    for (const auto& c : { Case { 44100.0, 300.0, 0.0, 1.2 }, Case { 44100.0, 1000.0, 60.0, 0.8 }, Case { 44100.0, 3000.0, 100.0, 0.5 }, Case { 44100.0, 8000.0, 100.0, 1.6 },
                           Case { 44100.0, 16000.0, 60.0, 3.3 }, Case { 48000.0, 1000.0, 100.0, 1.6 }, Case { 96000.0, 1000.0, 100.0, 1.6 } })
    {
        double maxError, meanError;
        oursVersus4x (c.fs, c.cutoff, c.resonance, maxError, meanError);
        check (maxError <= c.maxLimit && meanError <= c.maxLimit * 0.7, juce::String (c.fs, 0) + " Hz, cutoff " + juce::String (c.cutoff, 0) + ", resonans " + juce::String (c.resonance, 0)
                                                                        + " %: avvikelse mot 4x max " + juce::String (maxError, 2) + " dB, medel " + juce::String (meanError, 2) + " dB (gräns " + juce::String (c.maxLimit, 2) + ")");
    }

    // 2. Stabilitet: extrema inställningar, sågtand + stötar, alla samplingsfrekvenser: ändligt och begränsat.
    {
        auto finite = true;
        auto biggest = 0.0f;
        for (const auto fs : { 44100.0, 48000.0, 96000.0 })
            for (const auto cutoff : { 20.0, 200.0, 5000.0, 20000.0 })
                for (const auto resonance : { 0.0, 50.0, 100.0 })
                {
                    Tb303Filter filter;
                    filter.prepare (fs);
                    filter.setParameters (cutoff, resonance);
                    for (int i = 0; i < (int) fs * 3; ++i)
                    {
                        const auto phase = std::fmod (55.0 * (double) i / fs, 1.0);
                        auto x = (float) (0.5 * (2.0 * phase - 1.0));
                        if (i % 20000 == 0)
                            x = 1.0f; // stötar
                        if (i > (int) fs * 2)
                            x = 0.0f;
                        if (i % 4000 == 0) // cutoff som ändras under tiden, som filter-envelopen och automation
                            filter.setParameters (cutoff * (0.5 + 0.5 * std::sin (0.001 * i)) + 20.0, resonance);
                        const auto y = filter.process (x);
                        finite = finite && std::isfinite (y);
                        biggest = std::max (biggest, std::abs (y));
                    }
                }
        check (finite && biggest < 100.0f, "extrema inställningar (cutoff 20-20 000 Hz, resonans 0-100 %, 44,1-96 kHz): alla värden ändliga, största " + juce::String (biggest, 2));
    }

    // 3. Utklingning utan CPU-toppar: en stöt med resonans 100 %, sedan 10 s tystnad. Sista sekunden får inte ta märkbart mer tid än första
    //    (annars har allpass-kedjorna eller TeeBee gått in i denormaler). Körs med samma denormalskydd som processBlock (ScopedNoDenormals).
    {
        juce::ScopedNoDenormals noDenormals;
        Tb303Filter filter;
        filter.prepare (48000.0);
        filter.setParameters (800.0, 100.0);
        std::vector<double> perSecond;
        auto sink = 0.0f;
        for (int second = 0; second < 10; ++second)
        {
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 48000; ++i)
                sink += filter.process (second == 0 && i == 0 ? 1.0f : 0.0f);
            perSecond.push_back (std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count());
        }
        const auto first = perSecond.front(), last = perSecond.back();
        check (last <= first * 1.5 + 0.002, "10 s utklingning med resonans 100 %: första sekunden " + juce::String (first * 1000.0, 2) + " ms, sista " + juce::String (last * 1000.0, 2) + " ms (" + juce::String (sink) + ")");
    }

    // 4. Modellbyte mitt i toner med resonans 100 % (via parametrarna, som automation eller ett preset): inga icke-ändliga värden och ingen topp över 0 dBFS.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        Scenarios::makeHeavy (*processor, 3);
        auto& p = *processor;
        for (int layer = 0; layer < 3; ++layer)
            Scenarios::setInstrument (p, layer, InstrumentField::resonance, 1.0f);
        p.setPlayConfigDetails (0, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        auto finite = true;
        auto peak = 0.0f;
        for (int block = 0; block < 1200; ++block)
        {
            const auto model = (block / 3) % 2 == 0 ? 1.0f : 0.0f; // byte var tredje block, alltså mitt i tonerna
            for (int layer = 0; layer < 3; ++layer)
                Scenarios::setInstrument (p, layer, InstrumentField::filterModel, model);
            midi.clear();
            if (block % 80 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 0);
            if (block % 80 == 40)
                midi.addEvent (juce::MidiMessage::noteOff (1, 52), 0);
            buffer.clear();
            p.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                {
                    const auto v = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    peak = std::max (peak, std::abs (v));
                }
        }
        check (finite && peak <= 1.0f, "modellbyte var tredje block med resonans 100 %: alla värden ändliga, topp " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS (högst 0)");
    }

    std::printf (failed ? "TB-303: FEL\n" : "TB-303-modellen gick igenom\n");
    return failed ? 1 : 0;
}
