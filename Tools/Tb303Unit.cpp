// `tool tb303unit`: mäter Tb303Filter (Source/Core/Tb303Filter.h) som enhet innan den kopplas in i synten (fas D1): passbandet för upp- och
// nedsamplingen utan TeeBee, aliasering nära Nyquist, fördröjning och gruppfördröjning vid 1 kHz, och CPU per sampling mot SVF-paret.

#include "../Source/Core/Tb303Filter.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace
{
    constexpr int dftSize = 4096;

    // Beloppet och fasen vid en exakt DFT-punkt (perioden dftSize/bin samplingar) över de sista dftSize samplingarna.
    void dftBin (const std::vector<double>& signal, int bin, double& magnitude, double& phase)
    {
        auto re = 0.0, im = 0.0;
        const auto start = signal.size() - (size_t) dftSize;
        for (int n = 0; n < dftSize; ++n)
        {
            const auto angle = -2.0 * juce::MathConstants<double>::pi * (double) bin * (double) n / (double) dftSize;
            re += signal[start + (size_t) n] * std::cos (angle);
            im += signal[start + (size_t) n] * std::sin (angle);
        }
        magnitude = 2.0 * std::sqrt (re * re + im * im) / (double) dftSize;
        phase = std::atan2 (im, re);
    }

    std::vector<double> throughChain (int bin, double fs)
    {
        Tb303Filter chain;
        chain.prepare (fs);
        std::vector<double> out (dftSize * 3);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = (double) chain.processWithoutFilter ((float) (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * (double) bin * (double) i / (double) dftSize)));
        return out;
    }

    std::vector<double> reference (int bin)
    {
        std::vector<double> in (dftSize * 3);
        for (size_t i = 0; i < in.size(); ++i)
            in[i] = 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * (double) bin * (double) i / (double) dftSize);
        return in;
    }

    double nowSeconds() { return (double) juce::Time::getHighResolutionTicks() / (double) juce::Time::getHighResolutionTicksPerSecond(); }
}

int tb303Unit()
{
    const auto fs = 44100.0;
    auto ok = true;

    std::printf ("Tb303Filter: översamplingen ensam (upp + ner utan TeeBee) vid %.0f Hz\n", fs);
    std::printf ("%10s %10s\n", "frekvens", "förstärkning dB");
    auto worstPassband = 0.0;
    for (const auto bin : { 40, 93, 186, 372, 745, 1115, 1672, 1700 })
    {
        const auto frequency = fs * bin / dftSize;
        double inMagnitude, inPhase, outMagnitude, outPhase;
        dftBin (reference (bin), bin, inMagnitude, inPhase);
        dftBin (throughChain (bin, fs), bin, outMagnitude, outPhase);
        const auto gain = 20.0 * std::log10 (outMagnitude / inMagnitude);
        if (frequency <= 18000.0)
            worstPassband = std::max (worstPassband, std::abs (gain));
        std::printf ("%10.0f %10.3f\n", frequency, gain);
    }
    std::printf ("största avvikelse upp till 18 kHz: %.3f dB (kravet: 0,5 dB)\n", worstPassband);
    ok = ok && worstPassband <= 0.5;

    // Fördröjning och gruppfördröjning vid 1 kHz (bin 93 = 1001 Hz): fasfördröjning = -fas / vinkelfrekvens, gruppfördröjning = -dfas/dvinkelfrekvens.
    {
        double m0, ph[3];
        const int bins[] { 92, 93, 94 };
        for (int i = 0; i < 3; ++i)
        {
            double inM, inP, outP;
            dftBin (reference (bins[i]), bins[i], inM, inP);
            dftBin (throughChain (bins[i], fs), bins[i], m0, outP);
            auto difference = outP - inP;
            while (difference > juce::MathConstants<double>::pi) difference -= juce::MathConstants<double>::twoPi;
            while (difference < -juce::MathConstants<double>::pi) difference += juce::MathConstants<double>::twoPi;
            ph[i] = difference;
        }
        const auto omega = juce::MathConstants<double>::twoPi * 93.0 / dftSize;
        const auto domega = juce::MathConstants<double>::twoPi / dftSize;
        std::printf ("fördröjning vid 1 kHz: fas %.3f samplingar, grupp %.3f samplingar (%.3f ms vid %.0f Hz)\n", -ph[1] / omega, -(ph[2] - ph[0]) / (2.0 * domega),
                     1000.0 * -(ph[2] - ph[0]) / (2.0 * domega) / fs, fs);
    }

    // Aliasering: en sinus vid 0,45 fs (bin 1843) genom upp + ner. Allt utom grundtonen är spår av bilder och vikning (dB under grundtonen).
    {
        const auto bin = 1843;
        const auto out = throughChain (bin, fs);
        double fundamental, p;
        dftBin (out, bin, fundamental, p);
        auto worstSpur = 0.0;
        auto worstBin = 0;
        for (int b = 1; b < dftSize / 2; ++b)
            if (b != bin)
            {
                double m, ph;
                dftBin (out, b, m, ph);
                if (m > worstSpur)
                {
                    worstSpur = m;
                    worstBin = b;
                }
            }
        const auto spurDb = 20.0 * std::log10 (std::max (1e-12, worstSpur / fundamental));
        std::printf ("aliasering: sinus vid %.0f Hz, största spår %.1f dB under grundtonen (bin %d)\n", fs * bin / dftSize, -spurDb, worstBin);
    }

    // CPU: mitt och sida (två instanser) mot SVF-paret som synten använder i dag (LP 24: filter1 + filter2 för mitt och sida = fyra SVF), 4 miljoner samplingar.
    {
        const auto count = 4000000;
        auto sink = 0.0f;
        const auto run = [&] (const char* name, auto&& body)
        {
            const auto start = nowSeconds();
            for (int i = 0; i < count; ++i)
                sink += body (i);
            const auto ns = (nowSeconds() - start) * 1.0e9 / count;
            std::printf ("%-52s %7.1f ns per sampling (mitt + sida)\n", name, ns);
            return ns;
        };
        Tb303Filter mid, side;
        mid.prepare (fs);
        side.prepare (fs);
        mid.setParameters (1200.0, 60.0);
        side.setParameters (1200.0, 60.0);
        const auto tbStatic = run ("TB-303 (oförändrad cutoff)", [&] (int i) { const auto x = (float) std::sin (0.01 * i); return mid.process (x) + side.process (0.5f * x); });
        const auto tbSweep = run ("TB-303 (cutoff ändras varje sampling, som envelopen)", [&] (int i)
        {
            const auto cutoff = 300.0 + 3000.0 * (0.5 + 0.5 * std::sin (0.0005 * i));
            mid.setParameters (cutoff, 60.0);
            side.setParameters (cutoff, 60.0);
            const auto x = (float) std::sin (0.01 * i);
            return mid.process (x) + side.process (0.5f * x);
        });
        juce::dsp::StateVariableTPTFilter<float> f1, f2, s1, s2;
        for (auto* f : { &f1, &f2, &s1, &s2 })
        {
            f->prepare ({ fs, 512, 1 });
            f->setType (juce::dsp::StateVariableTPTFilterType::lowpass);
            f->setResonance (0.7f);
        }
        const auto svfStatic = run ("SVF-paret LP 24 (oförändrad cutoff)", [&] (int i)
        {
            const auto x = (float) std::sin (0.01 * i);
            return f2.processSample (0, f1.processSample (0, x)) + s2.processSample (0, s1.processSample (0, 0.5f * x));
        });
        const auto svfSweep = run ("SVF-paret LP 24 (cutoff ändras varje sampling)", [&] (int i)
        {
            const auto cutoff = (float) (300.0 + 3000.0 * (0.5 + 0.5 * std::sin (0.0005 * i)));
            for (auto* f : { &f1, &f2, &s1, &s2 })
                f->setCutoffFrequency (cutoff);
            const auto x = (float) std::sin (0.01 * i);
            return f2.processSample (0, f1.processSample (0, x)) + s2.processSample (0, s1.processSample (0, 0.5f * x));
        });
        std::printf ("kvot TB-303 / SVF: %.2f (oförändrad cutoff), %.2f (varje sampling)   [%g]\n", tbStatic / svfStatic, tbSweep / svfSweep, (double) sink);
    }

    std::printf (ok ? "Tb303Filter: mätningen klar, passbandet inom kravet\n" : "Tb303Filter: FEL, passbandet utanför kravet\n");
    return ok ? 0 : 1;
}

// `tool includecheck [rotmapp]`: håller isoleringen av Open303:s headers (de definierar globala makron och funktioner som ändrade ljudet i ett instrument när de
// läckte in via InstrumentSynth.h). Ger 1 om någon fil i Source/ eller Tools/ utom de tillåtna inkluderar en av dem.
int includeCheck (const juce::String& rootArgument)
{
    const auto root = rootArgument.isNotEmpty() ? juce::File::getCurrentWorkingDirectory().getChildFile (rootArgument) : juce::File::getCurrentWorkingDirectory();
    // Tillåtna: själva Open303-mappen, filtrets .cpp (pimpl) och Tools/Tb303Tests.cpp (en blad-fil i verktyget som jämför mot TeeBeeFilter ensam).
    const auto allowed = [] (const juce::String& relative)
    {
        return relative.startsWith ("Source/ThirdParty/open303/") || relative == "Source/Core/Tb303Filter.cpp" || relative == "Tools/Tb303Tests.cpp";
    };
    auto checked = 0, violations = 0;
    for (const auto* folder : { "Source", "Tools" })
        for (const auto& file : root.getChildFile (folder).findChildFiles (juce::File::findFiles, true, "*.h;*.cpp;*.inl"))
        {
            const auto relative = file.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            ++checked;
            juce::StringArray lines;
            file.readLines (lines);
            for (int i = 0; i < lines.size(); ++i)
            {
                const auto line = lines[i].trim();
                if (! line.startsWith ("#include"))
                    continue;
                if ((line.contains ("open303/") || line.contains ("rosic_") || line.contains ("GlobalFunctions.h") || line.contains ("GlobalDefinitions.h")) && ! allowed (relative))
                {
                    std::printf ("FEL: %s:%d inkluderar Open303: %s\n", relative.toRawUTF8(), i + 1, line.toRawUTF8());
                    ++violations;
                }
            }
        }
    if (checked == 0)
    {
        std::printf ("FEL: hittade inga filer under %s (kör från repots rot eller ange den)\n", root.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf (violations == 0 ? "Open303:s headers ligger bara där de ska (%d filer granskade)\n" : "FEL: %d bryt mot isoleringen\n", violations == 0 ? checked : violations);
    return violations == 0 ? 0 : 1;
}
