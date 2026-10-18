#include "Wavetable.h"
#include "Settings.h"
#include <juce_dsp/juce_dsp.h>
#include <mutex>

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
}

//==============================================================================
Wavetable::Wavetable (juce::String nameIn, const Shape& shape, int analysisOrder) : name (std::move (nameIn))
{
    data.assign ((size_t) numLevels * numFrames * (frameSize + 1), 0.0f);

    const int analysisSize = 1 << analysisOrder;
    juce::dsp::FFT analysis (analysisOrder);
    static_assert (frameSize == 1024);

    std::vector<float> big ((size_t) analysisSize * 2), spectrum ((size_t) frameSize * 2);

    for (int f = 0; f < numFrames; ++f)
    {
        const auto position = (double) f / (numFrames - 1);

        // Spektrum av formen, utan DC.
        for (int i = 0; i < analysisSize; ++i)
            big[(size_t) i] = (float) shape (position, (double) i / analysisSize);
        std::fill (big.begin() + analysisSize, big.end(), 0.0f);
        analysis.performRealOnlyForwardTransform (big.data(), true);

        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        for (int h = 1; h < frameSize / 2; ++h)
        {
            spectrum[(size_t) (2 * h)] = big[(size_t) (2 * h)];
            spectrum[(size_t) (2 * h + 1)] = big[(size_t) (2 * h + 1)];
        }
        storeFrame (f, spectrum);
    }
}

Wavetable::Wavetable (juce::String nameIn, const Additive& additive, int harmonicCount) : name (std::move (nameIn))
{
    data.assign ((size_t) numLevels * numFrames * (frameSize + 1), 0.0f);
    harmonicCount = juce::jlimit (1, maxHarmonics, harmonicCount);

    std::vector<double> amp ((size_t) maxHarmonics), phase ((size_t) maxHarmonics);
    std::vector<float> spectrum ((size_t) frameSize * 2);

    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (amp.begin(), amp.end(), 0.0);
        std::fill (phase.begin(), phase.end(), 0.0);
        additive ((double) f / (numFrames - 1), amp.data(), phase.data(), harmonicCount);

        // Överton k med amplitud A och fas p (A * sin(2 pi k t + p)) är bin k = A * (sin p, -cos p) (skalan spelar ingen roll: varje frame
        // normaliseras till toppvärdet 1).
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        for (int k = 1; k <= harmonicCount; ++k)
        {
            spectrum[(size_t) (2 * k)] = (float) (amp[(size_t) (k - 1)] * std::sin (phase[(size_t) (k - 1)]));
            spectrum[(size_t) (2 * k + 1)] = (float) (-amp[(size_t) (k - 1)] * std::cos (phase[(size_t) (k - 1)]));
        }
        storeFrame (f, spectrum);
    }
}

void Wavetable::storeFrame (int f, std::vector<float>& full)
{
    juce::dsp::FFT synthesis (10); // frameSize = 1024
    std::vector<float> small ((size_t) frameSize * 2);

    auto peak = 0.0f;
    for (int level = 0; level < numLevels; ++level)
    {
        // Bara övertoner upp till nivåns gräns; resten nollas.
        const auto maxHarmonic = (frameSize / 2) >> level;
        std::fill (small.begin(), small.end(), 0.0f);
        for (int h = 1; h <= maxHarmonic && h < frameSize / 2; ++h)
        {
            small[(size_t) (2 * h)] = full[(size_t) (2 * h)];
            small[(size_t) (2 * h + 1)] = full[(size_t) (2 * h + 1)];
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

int Wavetable::stepFrame (float position) const
{
    if (steps <= 1)
        return 0;
    const auto step = (int) std::lround (juce::jlimit (0.0f, 1.0f, position) * (float) (steps - 1));
    return std::min (numFrames - 1, (int) std::lround ((double) step * (numFrames - 1) / (steps - 1)));
}

float Wavetable::sample (float position, double phase, double maxHarmonicsNow, bool morph) const
{
    // Nivån vars övertoner ryms: 512 >> nivå <= maxHarmonics.
    auto level = 0;
    while (level < numLevels - 1 && ((frameSize / 2) >> level) > maxHarmonicsNow)
        ++level;

    const auto index = phase * frameSize;
    const auto i0 = juce::jlimit (0, frameSize - 1, (int) index);
    const auto fraction = (float) (index - i0);

    auto read = [&] (int frame)
    {
        const auto* d = frameData (level, frame);
        return d[i0] + (d[i0 + 1] - d[i0]) * fraction;
    };

    if (! morph)
        return read (stepFrame (position));

    const auto framePosition = juce::jlimit (0.0f, 1.0f, position) * (numFrames - 1);
    const auto frame0 = std::min (numFrames - 2, (int) framePosition);
    const auto frameMix = framePosition - (float) frame0;

    const auto a = read (frame0);
    if (frameMix <= 0.0f)
        return a;
    return a + (read (frame0 + 1) - a) * frameMix;
}

//==============================================================================
namespace
{
    double sine (double p)     { return std::sin (twoPi * p); }
    double triangle (double p) { return 1.0 - 4.0 * std::abs (std::fmod (p + 0.25, 1.0) - 0.5); }
    double saw (double p)      { return 2.0 * std::fmod (p + 0.5, 1.0) - 1.0; }
    double square (double p)   { return p < 0.5 ? 1.0 : -1.0; }
    double frac (double p)     { return p - std::floor (p); }

    // Vokalformanter (Hz) för A, E, I, O, U.
    constexpr double formants[5][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                      { 450, 800, 2830 }, { 325, 700, 2530 } };

    // Pseudoslump som alltid ger samma tal för samma frö, så att tabellerna blir likadana varje gång.
    double random01 (unsigned seed, unsigned index)
    {
        auto x = seed * 2654435761u + index * 40503u + 12345u;
        x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
        return (double) (x & 0xffffff) / (double) 0x1000000;
    }

    double smoothstep (double t) { return t * t * (3.0 - 2.0 * t); }

    struct Entry
    {
        juce::String name, category;
        Wavetable::Shape shape;
        Wavetable::Additive additive;
        int order = 13;     // samplad form: analysens storlek
        int count = 128;    // additiv: antal övertoner
        std::atomic<const Wavetable*> ready { nullptr };
        std::atomic<bool> requested { false };
        std::unique_ptr<Wavetable> owned;
    };

    std::mutex& buildMutex() { static std::mutex m; return m; }

    void build (Entry& e)
    {
        std::lock_guard<std::mutex> lock (buildMutex());
        if (e.ready.load() != nullptr)
            return;
        e.owned = e.additive ? std::make_unique<Wavetable> (e.name, e.additive, e.count) : std::make_unique<Wavetable> (e.name, e.shape, e.order);
        if (e.name == "Basic")
            e.owned->setSteps (4); // sinus, triangel, såg, fyrkant: Morph av hoppar mellan dem
        e.ready.store (e.owned.get());
    }

    std::vector<std::unique_ptr<Entry>> makeSpecs()
    {
        std::vector<std::unique_ptr<Entry>> list;
        auto shaped = [&list] (const char* name, const char* category, Wavetable::Shape shape, int order = 13)
        {
            auto e = std::make_unique<Entry>();
            e->name = name; e->category = category; e->shape = std::move (shape); e->order = order;
            list.push_back (std::move (e));
        };
        auto additive = [&list] (const juce::String& name, const char* category, Wavetable::Additive fn, int count = 96)
        {
            auto e = std::make_unique<Entry>();
            e->name = name; e->category = category; e->additive = std::move (fn); e->count = count;
            list.push_back (std::move (e));
        };

        //======================================================================== de 14 ursprungliga (platserna får inte ändras)
        // Sinus -> triangel -> såg -> fyrkant.
        shaped ("Basic", "Basic", [] (double pos, double p)
        {
            const auto s = pos * 3.0;
            const auto segment = std::min (2, (int) s);
            const auto mix = s - segment;
            auto shape = [p] (int i) { return i == 0 ? sine (p) : i == 1 ? triangle (p) : i == 2 ? saw (p) : square (p); };
            return shape (segment) * (1.0 - mix) + shape (segment + 1) * mix;
        });

        // Pulsbredd 50 % -> 4 %: tunnare och nasalare.
        shaped ("PWM", "Basic", [] (double pos, double p)
        {
            const auto width = 0.5 - 0.46 * pos;
            return (p < width ? 1.0 : -1.0) - (2.0 * width - 1.0);
        });

        // Hård sync: sågtanden startar om 1-8 gånger per period (skrikigt, resonant).
        shaped ("Sync", "Basic", [] (double pos, double p)
        {
            const auto ratio = 1.0 + 7.0 * pos;
            return (2.0 * std::fmod (p * ratio, 1.0) - 1.0) * (1.0 - 0.3 * p);
        });

        // Övertoner läggs till en i taget: sinus -> sågtand med 32 övertoner.
        shaped ("Harmonics", "Basic", [] (double pos, double p)
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
        shaped ("FM", "Basic", [] (double pos, double p) { return std::sin (twoPi * p + pos * 6.0 * std::sin (twoPi * p)); });

        // FM med tredje övertonen: morrande.
        shaped ("Growl", "Basic", [] (double pos, double p) { return std::sin (twoPi * p + pos * 4.0 * std::sin (twoPi * 3.0 * p)); });

        // Wavefolder: sinusen viks över sig själv, udda övertoner växer.
        shaped ("Fold", "Basic", [] (double pos, double p) { return std::sin ((1.0 + 7.0 * pos) * std::sin (twoPi * p) * juce::MathConstants<double>::halfPi); });

        // Vokaler A -> E -> I -> O -> U (formanter räknade för en ton på 110 Hz).
        shaped ("Formant", "Basic", [] (double pos, double p)
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
        }, 11);

        // Sågtand med allt färre steg: digitalt och krispigt.
        shaped ("Digital", "Basic", [] (double pos, double p)
        {
            const auto steps = 32.0 - 28.0 * pos;
            return std::floor (saw (p) * steps) / steps;
        });

        // Fasförvrängning (som Casio CZ): cosinus som blir sågtandslik.
        shaped ("Phase Dist", "Basic", [] (double pos, double p)
        {
            const auto d = 0.5 - 0.48 * pos;
            const auto warped = p < d ? 0.5 * p / d : 0.5 + 0.5 * (p - d) / (1.0 - d);
            return -std::cos (twoPi * warped);
        });

        // Klassiska enkla vågformer (som 3x Osc): en ren form, POS gör ingenting.
        shaped ("Classic Sine",     "Classic", [] (double, double p) { return sine (p); });
        shaped ("Classic Triangle", "Classic", [] (double, double p) { return triangle (p); });
        shaped ("Classic Saw",      "Classic", [] (double, double p) { return saw (p); });
        shaped ("Classic Square",   "Classic", [] (double, double p) { return square (p); });

        //======================================================================== nya: Analog
        additive ("Saw Soft", "Analog", [] (double pos, double* a, double* ph, int n)
        {
            const auto alpha = 2.2 - 1.4 * pos; // mjuk -> ljus
            for (int k = 1; k <= n; ++k) { a[k - 1] = std::pow ((double) k, -alpha); ph[k - 1] = 0.0; }
        });
        additive ("Saw Bright", "Analog", [] (double pos, double* a, double* ph, int n)
        {
            const auto alpha = 1.0 - 0.7 * pos; // ljus -> mycket ljus
            for (int k = 1; k <= n; ++k) { a[k - 1] = std::pow ((double) k, -alpha); ph[k - 1] = 0.0; }
        }, 160);
        additive ("Square Soft", "Analog", [] (double pos, double* a, double* ph, int n)
        {
            const auto alpha = 2.4 - 1.5 * pos;
            for (int k = 1; k <= n; k += 2) { a[k - 1] = std::pow ((double) k, -alpha); ph[k - 1] = 0.0; }
        });
        additive ("Triangle Bright", "Analog", [] (double pos, double* a, double* ph, int n)
        {
            const auto alpha = 2.0 - 1.1 * pos;
            for (int k = 1; k <= n; k += 2) { a[k - 1] = std::pow ((double) k, -alpha); ph[k - 1] = (k % 4 == 3) ? juce::MathConstants<double>::pi : 0.0; }
        });
        shaped ("Pulse Deep", "Analog", [] (double pos, double p) { const auto w = 0.5 - 0.48 * pos; return (p < w ? 1.0 : -1.0) - (2.0 * w - 1.0); });
        shaped ("Saw Comb", "Analog", [] (double pos, double p) { return saw (p) - saw (p + 0.04 + 0.46 * pos); });
        shaped ("Saw Octave", "Analog", [] (double pos, double p) { return (saw (p) + pos * saw (2.0 * p)) / (1.0 + pos); });
        shaped ("Sine Saw", "Analog", [] (double pos, double p) { return sine (p) * (1.0 - pos) + saw (p) * pos; });
        shaped ("Sine Square", "Analog", [] (double pos, double p) { return sine (p) * (1.0 - pos) + square (p) * pos; });
        shaped ("Triangle Saw", "Analog", [] (double pos, double p) { return triangle (p) * (1.0 - pos) + saw (p) * pos; });
        shaped ("Square Saw", "Analog", [] (double pos, double p) { return square (p) * (1.0 - pos) + saw (p) * pos; });
        shaped ("Triangle Fold", "Analog", [] (double pos, double p) { return std::sin ((1.0 + 5.0 * pos) * triangle (p) * juce::MathConstants<double>::halfPi); }, 12);
        shaped ("Saw Fold", "Analog", [] (double pos, double p) { return std::sin ((1.0 + 4.0 * pos) * saw (p) * juce::MathConstants<double>::halfPi); });
        shaped ("Ramp Bend", "Analog", [] (double pos, double p) { return saw (std::pow (p, 1.0 + 3.0 * pos)); });

        //======================================================================== Filter: en sågtand eller fyrkant genom ett resonant filter som sveper
        auto filterSweep = [&additive] (const char* name, bool oddOnly, int type, double q)
        {
            additive (name, "Filter", [oddOnly, type, q] (double pos, double* a, double* ph, int n)
            {
                const auto cutoff = 1.0 + pos * 28.0; // filtrets gräns i överton
                for (int k = 1; k <= n; ++k)
                {
                    if (oddOnly && k % 2 == 0)
                        continue;
                    const auto x = k / cutoff;
                    const auto denom = std::sqrt ((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q));
                    const auto response = type == 0 ? 1.0 / denom                      // lågpass
                                        : type == 1 ? (x / q) / denom                  // bandpass
                                        : type == 2 ? x * x / denom                    // högpass
                                                    : std::abs (1.0 - x * x) / denom;  // notch
                    a[k - 1] = response / k;
                    ph[k - 1] = 0.0;
                }
            }, 128);
        };
        filterSweep ("LP Res Saw", false, 0, 3.0);
        filterSweep ("LP Res Saw Hard", false, 0, 10.0);
        filterSweep ("LP Res Square", true, 0, 5.0);
        filterSweep ("BP Res Saw", false, 1, 4.0);
        filterSweep ("HP Res Saw", false, 2, 3.0);
        filterSweep ("Notch Saw", false, 3, 2.5);
        additive ("Comb Sweep", "Filter", [] (double pos, double* a, double* ph, int n)
        {
            for (int k = 1; k <= n; ++k) { a[k - 1] = (0.55 + 0.45 * std::cos (twoPi * k * pos * 0.25)) / k; ph[k - 1] = 0.0; }
        });

        //======================================================================== FM: bärvåg c och modulator m
        const int fmPairs[][2] { { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 1, 7 }, { 1, 8 }, { 1, 11 }, { 2, 3 }, { 2, 5 }, { 2, 7 },
                                 { 3, 2 }, { 3, 4 }, { 3, 5 }, { 5, 2 } };
        for (const auto& pair : fmPairs)
        {
            const auto c = pair[0], m = pair[1];
            const auto maxIndex = m <= 3 ? 7.0 : 5.0;
            shaped (("FM " + juce::String (c) + ":" + juce::String (m)).toRawUTF8(), "FM",
                    [c, m, maxIndex] (double pos, double p) { return std::sin (twoPi * c * p + pos * maxIndex * std::sin (twoPi * m * p)); }, 12);
        }
        for (const int m : { 1, 2, 3 })
            shaped (("PM Saw " + juce::String (m)).toRawUTF8(), "FM", [m] (double pos, double p) { return saw (p + pos * 0.5 * std::sin (twoPi * m * p)); });
        for (const int m : { 2, 3 })
            shaped (("PM Square " + juce::String (m)).toRawUTF8(), "FM", [m] (double pos, double p) { return square (p + pos * 0.4 * std::sin (twoPi * m * p)); });

        //======================================================================== Digital
        shaped ("Stair Sine", "Digital", [] (double pos, double p) { const auto levels = 64.0 - 60.0 * pos; return std::floor (sine (p) * levels) / levels; });
        shaped ("Stair Triangle", "Digital", [] (double pos, double p) { const auto levels = 48.0 - 44.0 * pos; return std::floor (triangle (p) * levels) / levels; });
        for (const int r : { 2, 3, 5 })
            shaped (("Ring Saw " + juce::String (r)).toRawUTF8(), "Digital", [r] (double pos, double p) { return saw (p) * ((1.0 - pos) + pos * sine (r * p)); });
        additive ("Odd Spectrum", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto limit = 2 + (int) (pos * 80.0);
            for (int k = 1; k <= std::min (n, limit); k += 2) { a[k - 1] = 1.0 / k; ph[k - 1] = 0.0; }
        });
        additive ("Even Spectrum", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto limit = 2 + (int) (pos * 80.0);
            a[0] = 1.0;
            for (int k = 2; k <= std::min (n, limit); k += 2) { a[k - 1] = 1.0 / k; ph[k - 1] = 0.0; }
        });
        additive ("Prime Spectrum", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto limit = 2 + (int) (pos * 90.0);
            for (int k = 2; k <= std::min (n, limit); ++k)
            {
                auto prime = true;
                for (int d = 2; d * d <= k; ++d)
                    if (k % d == 0) { prime = false; break; }
                if (prime) { a[k - 1] = 1.0 / std::sqrt ((double) k); ph[k - 1] = 0.0; }
            }
            a[0] = 1.0;
        });
        additive ("Third Spectrum", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto limit = 3 + (int) (pos * 90.0);
            a[0] = 1.0;
            for (int k = 3; k <= std::min (n, limit); k += 3) { a[k - 1] = 1.2 / k; ph[k - 1] = 0.0; }
        });
        additive ("Comb Gap", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto gap = 2 + (int) (pos * 6.0);
            for (int k = 1; k <= n; ++k) { a[k - 1] = (k % gap == 0) ? 0.0 : 1.0 / k; ph[k - 1] = 0.0; }
        });
        additive ("Tilt Bright", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto alpha = 1.6 - 1.9 * pos; // mörk -> mycket ljus
            for (int k = 1; k <= n; ++k) { a[k - 1] = std::pow ((double) k, -alpha); ph[k - 1] = 0.0; }
        }, 64);
        additive ("Peak Harmonic", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto centre = 1.0 + pos * 30.0;
            for (int k = 1; k <= n; ++k) { a[k - 1] = std::exp (-std::pow ((k - centre) / 1.6, 2.0)) + 0.25 / k; ph[k - 1] = 0.0; }
        });
        additive ("Two Peaks", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            const auto first = 1.0 + pos * 14.0, second = 3.0 + pos * 26.0;
            for (int k = 1; k <= n; ++k)
            {
                a[k - 1] = std::exp (-std::pow ((k - first) / 1.4, 2.0)) + 0.8 * std::exp (-std::pow ((k - second) / 1.8, 2.0)) + 0.2 / k;
                ph[k - 1] = 0.0;
            }
        });
        additive ("Octaves", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            auto octave = 0;
            for (int k = 1; k <= n; k *= 2, ++octave) { a[k - 1] = std::pow (1.0 - 0.8 * pos, (double) octave) ; ph[k - 1] = 0.0; }
        });
        additive ("Fifths", "Digital", [] (double pos, double* a, double* ph, int n)
        {
            a[0] = 1.0;
            auto step = 0;
            for (int k = 3; k <= n; k *= 2, ++step) { a[k - 1] = 0.3 + 0.7 * pos; ph[k - 1] = 0.0; }
            for (int k = 2; k <= n; k *= 2) { a[k - 1] = 0.5; ph[k - 1] = 0.0; }
        });
        for (const int m : { 2, 3, 4, 6, 8 })
            shaped (("Pulse Train " + juce::String (m)).toRawUTF8(), "Digital", [m] (double pos, double p) { return frac (p * m) < 0.5 - 0.46 * pos ? 1.0 : -1.0; });

        //======================================================================== Random: slumpade spektrum som glider mellan varandra
        for (int index = 1; index <= 8; ++index)
        {
            const auto seed = (unsigned) index * 9176u;
            const auto alpha = 0.7 + 0.12 * (index % 5);
            additive ("Random " + juce::String (index), "Random", [seed, alpha] (double pos, double* a, double* ph, int n)
            {
                constexpr int controls = 5;
                const auto scaled = pos * (controls - 1);
                const auto c0 = std::min (controls - 2, (int) scaled);
                const auto mix = smoothstep (scaled - c0);
                for (int k = 1; k <= n; ++k)
                {
                    auto value = [&] (int control) { return random01 (seed + (unsigned) control * 31u, (unsigned) k) * std::pow ((double) k, -alpha); };
                    a[k - 1] = value (c0) * (1.0 - mix) + value (c0 + 1) * mix;
                    ph[k - 1] = twoPi * (random01 (seed, (unsigned) k + 500u) * (1.0 - mix) + random01 (seed + 31u, (unsigned) k + 500u) * mix) * 0.0 + twoPi * random01 (seed, (unsigned) k + 900u);
                }
            }, 64);
        }

        //======================================================================== Vocal: vokaler genom formanter
        auto vowels = [&additive] (const char* name, std::vector<int> sequence, double baseHz, double bandwidth, int count)
        {
            additive (name, "Vocal", [sequence, baseHz, bandwidth] (double pos, double* a, double* ph, int n)
            {
                const auto steps = (double) (sequence.size() - 1);
                const auto v = pos * steps;
                const auto v0 = std::min ((int) sequence.size() - 2, (int) v);
                const auto mix = v - v0;
                for (int k = 1; k <= n; ++k)
                {
                    const auto hz = baseHz * k;
                    auto gain = 0.03;
                    for (int i = 0; i < 3; ++i)
                    {
                        const auto f = formants[sequence[(size_t) v0]][i] * (1.0 - mix) + formants[sequence[(size_t) v0 + 1]][i] * mix;
                        gain += std::exp (-std::pow ((hz - f) / bandwidth, 2.0)) / (1.0 + i);
                    }
                    a[k - 1] = gain;
                    ph[k - 1] = 0.0;
                }
            }, count);
        };
        vowels ("Vowels UOAEI", { 4, 3, 0, 1, 2 }, 110.0, 160.0, 80);
        vowels ("Vowels AO Low", { 0, 3, 0 }, 55.0, 150.0, 160);
        vowels ("Vowels EI Mid", { 1, 2, 1 }, 82.0, 170.0, 110);
        vowels ("Vowels Choir", { 0, 1, 2, 3, 4 }, 130.0, 240.0, 64);
        vowels ("Vowels Robot", { 0, 2, 4, 2 }, 65.0, 90.0, 140);
        vowels ("Vowels Deep 41", { 0, 3, 4, 3 }, 41.0, 140.0, 220);

        //======================================================================== Organ: drawbars
        auto organ = [&additive] (const char* name, std::vector<double> full)
        {
            additive (name, "Organ", [full] (double pos, double* a, double* ph, int n)
            {
                // Tonerna dras in en efter en: positionen bestämmer hur många av registren som är ute.
                const int harmonics[] { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                for (size_t i = 0; i < full.size() && i < std::size (harmonics); ++i)
                {
                    const auto k = harmonics[i];
                    if (k > n) continue;
                    const auto amount = juce::jlimit (0.0, 1.0, pos * (double) full.size() * 0.9 - (double) i * 0.6 + (i == 0 ? 1.0 : 0.0));
                    a[k - 1] = full[i] * (i == 0 ? 1.0 : amount);
                    ph[k - 1] = 0.0;
                }
            }, 24);
        };
        organ ("Organ Full", { 1.0, 0.9, 0.8, 0.8, 0.5, 0.4, 0.3, 0.2, 0.2 });
        organ ("Organ Jazz", { 1.0, 0.2, 0.9, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0 });
        organ ("Organ Church", { 0.8, 1.0, 0.6, 0.9, 0.4, 0.5, 0.2, 0.3, 0.1 });
        organ ("Organ Reed", { 1.0, 0.5, 1.0, 0.4, 0.9, 0.3, 0.6, 0.2, 0.4 });

        //======================================================================== Psy: ljud som passar en rullande psybas
        additive ("Psy Roll", "Psy", [] (double pos, double* a, double* ph, int n)
        {
            const auto centre = 2.0 + pos * 5.0;
            for (int k = 1; k <= n; ++k) { a[k - 1] = std::pow ((double) k, -1.15) * (1.0 + 2.5 * std::exp (-std::pow ((k - centre) / 1.2, 2.0))); ph[k - 1] = 0.0; }
        }, 96);
        additive ("Psy Sub Punch", "Psy", [] (double pos, double* a, double* ph, int n)
        {
            a[0] = 1.0; a[1] = 0.15;
            if (n >= 3) a[2] = 0.15 + 0.85 * pos;
            if (n >= 5) a[4] = 0.5 * pos;
            if (n >= 7) a[6] = 0.3 * pos * pos;
            juce::ignoreUnused (ph);
        }, 12);
        shaped ("Psy Growl A", "Psy", [] (double pos, double p) { return std::sin (twoPi * p + pos * 5.0 * std::sin (twoPi * 2.0 * p) + pos * 0.8 * std::sin (twoPi * 5.0 * p)); }, 12);
        shaped ("Psy Growl B", "Psy", [] (double pos, double p) { return std::sin ((1.0 + 5.0 * pos) * std::sin (twoPi * p + pos * 3.0 * std::sin (twoPi * 3.0 * p)) * juce::MathConstants<double>::halfPi); }, 12);
        vowels ("Psy Growl C", { 0, 3, 0, 4 }, 41.0, 120.0, 220);
        additive ("Psy Acid", "Psy", [] (double pos, double* a, double* ph, int n)
        {
            const auto cutoff = 1.0 + pos * 20.0;
            constexpr auto q = 9.0;
            for (int k = 1; k <= n; ++k)
            {
                const auto x = k / cutoff;
                a[k - 1] = 1.0 / std::sqrt ((1.0 - x * x) * (1.0 - x * x) + (x / q) * (x / q)) / k;
                ph[k - 1] = 0.0;
            }
        }, 128);
        shaped ("Psy Zap", "Psy", [] (double pos, double p) { return std::sin ((1.0 + 2.0 * pos) * saw (frac (p * (1.0 + 7.0 * pos))) * juce::MathConstants<double>::halfPi); });
        shaped ("Psy Hoover", "Psy", [] (double pos, double p) { return (saw (p) + saw (p + 0.31 * pos) + saw (p + 0.67 * pos)) / 3.0; });
        shaped ("Psy Pulse Stack", "Psy", [] (double pos, double p) { return (square (p) + (frac (p) < 0.25 + 0.2 * pos ? 1.0 : -1.0)) * 0.5; });
        shaped ("Psy Metal", "Psy", [] (double pos, double p) { return std::sin ((1.0 + 3.0 * pos) * std::sin (twoPi * p + pos * 4.0 * std::sin (twoPi * 3.0 * p)) * juce::MathConstants<double>::halfPi); }, 12);
        shaped ("Psy Thump", "Psy", [] (double pos, double p) { return std::sin (twoPi * std::pow (p, 1.0 + 2.0 * pos)); });

        //======================================================================== Evolve och Grit: slumpade ljud som utvecklas
        for (int index = 1; index <= 12; ++index)
        {
            const auto seed = (unsigned) index * 7723u + 17u;
            const auto alpha = 0.9 + 0.08 * (index % 6);
            additive ("Evolve " + juce::String (index), "Evolve", [seed, alpha] (double pos, double* a, double* ph, int n)
            {
                constexpr int controls = 6;
                const auto scaled = pos * (controls - 1);
                const auto c0 = std::min (controls - 2, (int) scaled);
                const auto mix = smoothstep (scaled - c0);
                for (int k = 1; k <= n; ++k)
                {
                    auto amplitude = [&] (int control)
                    {
                        const auto r = random01 (seed + (unsigned) control * 53u, (unsigned) k);
                        return (r < 0.35 ? 0.1 : 1.0) * r * std::pow ((double) k, -alpha);
                    };
                    a[k - 1] = amplitude (c0) * (1.0 - mix) + amplitude (c0 + 1) * mix;
                    ph[k - 1] = twoPi * random01 (seed, (unsigned) k + 7000u);
                }
            }, 48);
        }
        for (int index = 1; index <= 4; ++index)
        {
            const auto seed = (unsigned) index * 4421u + 3u;
            additive ("Grit " + juce::String (index), "Grit", [seed, index] (double pos, double* a, double* ph, int n)
            {
                for (int k = 1; k <= n; ++k)
                {
                    a[k - 1] = (0.35 + 0.65 * random01 (seed, (unsigned) k + (unsigned) (pos * 4.0) * 97u)) * std::pow ((double) k, -0.15 * index);
                    ph[k - 1] = twoPi * random01 (seed + 5u, (unsigned) k);
                }
            }, 56);
        }

        return list;
    }

    std::vector<std::unique_ptr<Entry>>& registry()
    {
        static auto specs = []
        {
            auto list = makeSpecs();
            build (*list.front()); // "Basic" finns alltid: den spelas när en tabell inte är klar
            return list;
        }();
        return specs;
    }

    constexpr int legacyUserIndex = 14;

    int registryIndex (int choice)
    {
        if (choice < legacyUserIndex)
            return choice;
        if (choice == legacyUserIndex)
            return -1;
        return std::min ((int) registry().size() - 1, choice - 1);
    }
}

namespace Wavetables
{
    int builtInCount() { return (int) registry().size(); }
    int userIndex() { return legacyUserIndex; }
    int listSize() { return builtInCount() + 1; }
    int classicIndex (int shape) { return 10 + juce::jlimit (0, 3, shape); }

    juce::StringArray names()
    {
        juce::StringArray result;
        for (int choice = 0; choice < listSize(); ++choice)
            result.add (choice == legacyUserIndex ? juce::String ("User WAV") : registry()[(size_t) registryIndex (choice)]->name);
        return result;
    }

    juce::String categoryOf (int choice)
    {
        const auto index = registryIndex (choice);
        return index < 0 ? juce::String ("User") : registry()[(size_t) index]->category;
    }

    const Wavetable& get (int choice)
    {
        auto& entries = registry();
        const auto index = registryIndex (choice);
        if (index >= 0)
        {
            auto& entry = *entries[(size_t) index];
            if (const auto* ready = entry.ready.load())
                return *ready;
            entry.requested = true; // bakgrundstråden bygger den; under tiden spelas Basic
        }
        return *entries.front()->ready.load();
    }

    bool isReady (int choice)
    {
        const auto index = registryIndex (choice);
        return index < 0 || registry()[(size_t) index]->ready.load() != nullptr;
    }

    void ensure (int choice)
    {
        if (const auto index = registryIndex (choice); index >= 0)
            build (*registry()[(size_t) index]);
    }

    void ensureAll()
    {
        for (auto& entry : registry())
            build (*entry);
    }

    juce::File libraryFolder()
    {
        return Settings::wavetablesFolder();
    }

    std::unique_ptr<Wavetable> fromFrames (const juce::String& name, const std::vector<float>& frames, int steps)
    {
        constexpr int size = Wavetable::frameSize, numFrames = Wavetable::numFrames;
        if (frames.size() < (size_t) (size * numFrames))
            return nullptr;

        auto table = std::make_unique<Wavetable> (name, [&frames] (double position, double phase)
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
        table->setSteps (steps);
        return table;
    }

    std::unique_ptr<Wavetable> fromAudio (const std::vector<float>& mono, const juce::String& name, std::vector<float>& framesOut)
    {
        constexpr int size = Wavetable::frameSize, numFrames = Wavetable::numFrames, cycle = 2 * size;
        const auto length = (int) mono.size();
        if (length < 16)
            return nullptr;

        framesOut.assign ((size_t) (size * numFrames), 0.0f);
        auto steps = 1; // en enda cykel: Morph av har bara en vågform att spela

        if (length >= 2 * cycle)
        {
            // Cykler om 2048 samples (Serums format): frames väljs jämnt över filen och halveras till 1024.
            const auto cycles = length / cycle;
            steps = std::min (numFrames, cycles);
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

        return fromFrames (name, framesOut, steps);
    }
}

//==============================================================================
TableBuilder::TableBuilder() : juce::Thread ("Kickbase wavetable builder")
{
    startThread (juce::Thread::Priority::low);
}

TableBuilder::~TableBuilder()
{
    stopThread (4000);
}

void TableBuilder::run()
{
    while (! threadShouldExit())
    {
        auto built = false;
        for (auto& entry : registry())
        {
            if (threadShouldExit())
                return;
            if (entry->requested.load() && entry->ready.load() == nullptr)
            {
                build (*entry);
                built = true;
            }
        }
        if (! built)
            wait (40);
    }
}
