#include "Wavetable.h"
#include <juce_dsp/juce_dsp.h>

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr int analysisOrder = 13; // formen samplas i 8192 punkter innan den band-begränsas
}

Wavetable::Wavetable (juce::String nameIn, const Shape& shape) : name (std::move (nameIn))
{
    data.assign ((size_t) numLevels * numFrames * (frameSize + 1), 0.0f);

    const int analysisSize = 1 << analysisOrder;
    juce::dsp::FFT analysis (analysisOrder);
    juce::dsp::FFT synthesis (10); // frameSize = 1024
    static_assert (frameSize == 1024);

    std::vector<float> big ((size_t) analysisSize * 2), small ((size_t) frameSize * 2);

    for (int f = 0; f < numFrames; ++f)
    {
        const auto position = (double) f / (numFrames - 1);

        // Spektrum av formen, utan DC.
        for (int i = 0; i < analysisSize; ++i)
            big[(size_t) i] = (float) shape (position, (double) i / analysisSize);
        std::fill (big.begin() + analysisSize, big.end(), 0.0f);
        analysis.performRealOnlyForwardTransform (big.data(), true);

        auto peak = 0.0f;
        for (int level = 0; level < numLevels; ++level)
        {
            // Bara övertoner upp till nivåns gräns; resten nollas.
            const auto maxHarmonic = (frameSize / 2) >> level;
            std::fill (small.begin(), small.end(), 0.0f);
            for (int h = 1; h <= maxHarmonic && h < frameSize / 2; ++h)
            {
                small[(size_t) (2 * h)] = big[(size_t) (2 * h)];
                small[(size_t) (2 * h + 1)] = big[(size_t) (2 * h + 1)];
            }
            synthesis.performRealOnlyInverseTransform (small.data());

            if (level == 0)
                for (int i = 0; i < frameSize; ++i)
                    peak = std::max (peak, std::abs (small[(size_t) i]));

            // Samma skala på alla nivåer (toppen på den fulla vågformen blir 1).
            const auto scale = peak > 0.0f ? 1.0f / peak : 0.0f;
            auto* out = data.data() + ((size_t) level * numFrames + (size_t) f) * (frameSize + 1);
            for (int i = 0; i < frameSize; ++i)
                out[i] = small[(size_t) i] * scale;
            out[frameSize] = out[0];
        }
    }
}

float Wavetable::sample (float position, double phase, double maxHarmonics) const
{
    // Nivån vars övertoner ryms: 512 >> nivå <= maxHarmonics.
    auto level = 0;
    while (level < numLevels - 1 && ((frameSize / 2) >> level) > maxHarmonics)
        ++level;

    const auto framePosition = juce::jlimit (0.0f, 1.0f, position) * (numFrames - 1);
    const auto frame0 = std::min (numFrames - 2, (int) framePosition);
    const auto frameMix = framePosition - (float) frame0;

    const auto index = phase * frameSize;
    const auto i0 = juce::jlimit (0, frameSize - 1, (int) index);
    const auto fraction = (float) (index - i0);

    auto read = [&] (int frame)
    {
        const auto* d = frameData (level, frame);
        return d[i0] + (d[i0 + 1] - d[i0]) * fraction;
    };

    const auto a = read (frame0);
    if (frameMix <= 0.0f)
        return a;
    return a + (read (frame0 + 1) - a) * frameMix;
}

namespace
{
    double sine (double p)     { return std::sin (twoPi * p); }
    double triangle (double p) { return 1.0 - 4.0 * std::abs (std::fmod (p + 0.25, 1.0) - 0.5); }
    double saw (double p)      { return 2.0 * std::fmod (p + 0.5, 1.0) - 1.0; }
    double square (double p)   { return p < 0.5 ? 1.0 : -1.0; }

    // Vokalformanter (Hz) för A, E, I, O, U.
    constexpr double formants[5][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                      { 450, 800, 2830 }, { 325, 700, 2530 } };

    std::vector<std::unique_ptr<Wavetable>> build()
    {
        std::vector<std::unique_ptr<Wavetable>> tables;
        auto add = [&tables] (const char* name, Wavetable::Shape shape) { tables.push_back (std::make_unique<Wavetable> (name, shape)); };

        // Sinus -> triangel -> såg -> fyrkant.
        add ("Basic", [] (double pos, double p)
        {
            const auto s = pos * 3.0;
            const auto segment = std::min (2, (int) s);
            const auto mix = s - segment;
            auto shape = [p] (int i) { return i == 0 ? sine (p) : i == 1 ? triangle (p) : i == 2 ? saw (p) : square (p); };
            return shape (segment) * (1.0 - mix) + shape (segment + 1) * mix;
        });

        // Pulsbredd 50 % -> 4 %: tunnare och nasalare.
        add ("PWM", [] (double pos, double p)
        {
            const auto width = 0.5 - 0.46 * pos;
            return (p < width ? 1.0 : -1.0) - (2.0 * width - 1.0);
        });

        // Hård sync: sågtanden startar om 1-8 gånger per period (skrikigt, resonant).
        add ("Sync", [] (double pos, double p)
        {
            const auto ratio = 1.0 + 7.0 * pos;
            return (2.0 * std::fmod (p * ratio, 1.0) - 1.0) * (1.0 - 0.3 * p);
        });

        // Övertoner läggs till en i taget: sinus -> sågtand med 32 övertoner.
        add ("Harmonics", [] (double pos, double p)
        {
            const auto count = 1.0 + pos * 31.0;
            auto y = 0.0;
            for (int k = 1; k <= (int) std::ceil (count); ++k)
            {
                const auto weight = std::min (1.0, count - (k - 1));
                y += weight * std::sin (twoPi * k * p) / k;
            }
            return y;
        });

        // FM med samma frekvens: metalliskt, mer övertoner ju längre in.
        add ("FM", [] (double pos, double p) { return std::sin (twoPi * p + pos * 6.0 * std::sin (twoPi * p)); });

        // FM med tredje övertonen: morrande.
        add ("Growl", [] (double pos, double p) { return std::sin (twoPi * p + pos * 4.0 * std::sin (twoPi * 3.0 * p)); });

        // Wavefolder: sinusen viks över sig själv, udda övertoner växer.
        add ("Fold", [] (double pos, double p) { return std::sin ((1.0 + 7.0 * pos) * std::sin (twoPi * p) * juce::MathConstants<double>::halfPi); });

        // Vokaler A -> E -> I -> O -> U (formanter räknade för en ton på 110 Hz).
        add ("Formant", [] (double pos, double p)
        {
            const auto v = pos * 4.0;
            const auto v0 = std::min (3, (int) v);
            const auto mix = v - v0;
            auto y = 0.0;
            for (int k = 1; k <= 48; ++k)
            {
                const auto hz = 110.0 * k;
                auto gain = 0.03;
                for (int i = 0; i < 3; ++i)
                {
                    const auto f = formants[v0][i] * (1.0 - mix) + formants[v0 + 1][i] * mix;
                    gain += std::exp (-std::pow ((hz - f) / 160.0, 2.0)) / (1.0 + i);
                }
                y += gain * std::sin (twoPi * k * p);
            }
            return y;
        });

        // Sågtand med allt färre steg: digitalt och krispigt.
        add ("Digital", [] (double pos, double p)
        {
            const auto steps = 32.0 - 28.0 * pos;
            return std::floor (saw (p) * steps) / steps;
        });

        // Fasförvrängning (som Casio CZ): cosinus som blir sågtandslik.
        add ("Phase Dist", [] (double pos, double p)
        {
            const auto d = 0.5 - 0.48 * pos;
            const auto warped = p < d ? 0.5 * p / d : 0.5 + 0.5 * (p - d) / (1.0 - d);
            return -std::cos (twoPi * warped);
        });

        // Klassiska enkla vågformer (som 3x Osc): en ren form, POS gör ingenting.
        add ("Classic Sine",     [] (double, double p) { return sine (p); });
        add ("Classic Triangle", [] (double, double p) { return triangle (p); });
        add ("Classic Saw",      [] (double, double p) { return saw (p); });
        add ("Classic Square",   [] (double, double p) { return square (p); });

        return tables;
    }

    const std::vector<std::unique_ptr<Wavetable>>& all()
    {
        static const auto tables = build();
        return tables;
    }
}

namespace Wavetables
{
    int count() { return (int) all().size(); }
    int userIndex() { return count(); }

    juce::StringArray names()
    {
        juce::StringArray result;
        for (const auto& table : all())
            result.add (table->getName());
        result.add ("User WAV");
        return result;
    }

    std::unique_ptr<Wavetable> fromFrames (const juce::String& name, const std::vector<float>& frames)
    {
        constexpr int size = Wavetable::frameSize, numFrames = Wavetable::numFrames;
        if (frames.size() < (size_t) (size * numFrames))
            return nullptr;

        return std::make_unique<Wavetable> (name, [&frames] (double position, double phase)
        {
            const auto framePosition = juce::jlimit (0.0, 1.0, position) * (numFrames - 1);
            const auto f0 = std::min (numFrames - 2, (int) framePosition);
            const auto mix = framePosition - f0;
            const auto index = phase * size;
            const auto i0 = juce::jlimit (0, size - 1, (int) index), i1 = (i0 + 1) % size;
            const auto fraction = index - i0;
            auto read = [&] (int frame)
            {
                const auto* d = frames.data() + (size_t) frame * size;
                return d[i0] + (d[i1] - d[i0]) * fraction;
            };
            return read (f0) * (1.0 - mix) + read (f0 + 1) * mix;
        });
    }

    std::unique_ptr<Wavetable> fromAudio (const std::vector<float>& mono, const juce::String& name, std::vector<float>& framesOut)
    {
        constexpr int size = Wavetable::frameSize, numFrames = Wavetable::numFrames, cycle = 2 * size;
        const auto length = (int) mono.size();
        if (length < 16)
            return nullptr;

        framesOut.assign ((size_t) (size * numFrames), 0.0f);

        if (length >= 2 * cycle)
        {
            // Cykler om 2048 samples (Serums format): frames väljs jämnt över filen och halveras till 1024.
            const auto cycles = length / cycle;
            for (int f = 0; f < numFrames; ++f)
            {
                const auto source = (int) std::lround ((double) f * (cycles - 1) / (numFrames - 1));
                for (int i = 0; i < size; ++i)
                {
                    const auto j = source * cycle + 2 * i;
                    framesOut[(size_t) (f * size + i)] = 0.5f * (mono[(size_t) j] + mono[(size_t) j + 1]);
                }
            }
        }
        else
        {
            // En enda cykel: hela filen sträcks till 1024 samples och används i alla frames.
            for (int i = 0; i < size; ++i)
            {
                const auto position = (double) i * length / size;
                const auto i0 = std::min (length - 1, (int) position), i1 = std::min (length - 1, i0 + 1);
                const auto fraction = (float) (position - i0);
                const auto value = mono[(size_t) i0] + (mono[(size_t) i1] - mono[(size_t) i0]) * fraction;
                for (int f = 0; f < numFrames; ++f)
                    framesOut[(size_t) (f * size + i)] = value;
            }
        }

        // Ta bort likspänning per frame och skala så att toppen i hela tabellen blir 1.
        auto peak = 0.0f;
        for (int f = 0; f < numFrames; ++f)
        {
            auto* frame = framesOut.data() + (size_t) f * size;
            auto mean = 0.0;
            for (int i = 0; i < size; ++i)
                mean += frame[i];
            mean /= size;
            for (int i = 0; i < size; ++i)
            {
                frame[i] -= (float) mean;
                peak = std::max (peak, std::abs (frame[i]));
            }
        }
        if (peak < 1.0e-6f)
            return nullptr;
        for (auto& v : framesOut)
            v /= peak;

        return fromFrames (name, framesOut);
    }

    const Wavetable& get (int index)
    {
        const auto& tables = all();
        return *tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, index)];
    }
}
