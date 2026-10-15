// Anpassning av hela takten: kick + bas tillsammans, jämfört med referensens medelslag.
// Inkluderas efter AnalyzerFit.inl (använder decimate, refine, harmonicProfile m.m.).
//
// Basgången har ofta olika toner i olika slag (t.ex. D# F F | E E E | F F F | E E E), så
// varje position i takten medelvärdesbildas för sig, och kandidaten spelar de uppmätta tonerna.

namespace
{
    struct BeatPattern
    {
        int stepsPerBeat = 4;
        std::vector<int> steps; // basens steg i slaget (0 = kicken)
    };

    BeatPattern patternFromName (const juce::String& name)
    {
        if (name == "galloping") return { 4, { 2, 3 } };
        if (name == "triplet")   return { 3, { 1, 2 } };
        if (name == "offbeat")   return { 4, { 2 } };
        return { 4, { 1, 2, 3 } }; // rolling
    }

    constexpr double frameSeconds = 0.005;
    constexpr int barBeats = 4;

    struct BarLayout
    {
        int beatLength = 0;                    // i analysfrekvensen
        int stepLength = 0;
        std::vector<std::vector<int>> notes;   // [position][steg i mönstret], halvtoner över grundtonen
        std::vector<std::vector<bool>> sure;   // tonhöjden säkert uppmätt (övertonerna mäts bara där)
        double rootHz = 0.0;
    };

    struct BeatFeatures
    {
        std::vector<float> low, mid, high; // volym var 5:e ms, dB relativt basbandets topp
        std::vector<float> pitch;          // kickens svep per slag, halvtoner över svansen (NaN = osäkert)
        std::vector<float> timbre;         // basens övertoner 1..12 (dB relativt grundtonen)
    };

    std::vector<float> bandEnvelope (const Signal& envelope, float reference)
    {
        std::vector<float> result;
        const auto hop = std::max (1, (int) (frameSeconds * analysisRate));
        for (int i = 0; i < (int) envelope.size(); i += hop)
            result.push_back (toDb (envelope[(size_t) i]) - toDb (reference));
        return result;
    }

    int ms (double v) { return (int) (v * 0.001 * analysisRate); }

    BeatFeatures measureBar (const Signal& bar, const BarLayout& layout, const BeatPattern& pattern, double tailHz)
    {
        BeatFeatures f;
        const auto low = envelopeOf (lowpass (bar, 150.0));
        auto reference = 1.0e-9f;
        for (auto v : low)
            reference = std::max (reference, v);

        f.low = bandEnvelope (low, reference);
        f.mid = bandEnvelope (envelopeOf (bandpass (bar, 150.0, 600.0)), reference);
        f.high = bandEnvelope (envelopeOf (bandpass (bar, 600.0, 2000.0)), reference);

        // Kickens svep i varje slag, som i fitKick.
        const auto swept = lowpass (bar, 800.0);
        const auto frequency = instantaneousFrequency (swept);
        const auto sweep = envelopeOf (swept);
        for (int b = 0; b < barBeats; ++b)
        {
            const auto beatStart = b * layout.beatLength;
            auto sweepPeak = 1.0e-9f;
            for (int i = 0; i < layout.stepLength; ++i)
                sweepPeak = std::max (sweepPeak, sweep[(size_t) (beatStart + i)]);
            for (auto t : pitchTimes)
            {
                const auto i = (size_t) (beatStart + ms (t));
                const auto hz = frequency[i];
                const auto audible = toDb (sweep[i]) > toDb (sweepPeak) - 20.0f;
                f.pitch.push_back (audible && hz > tailHz * 0.85 && hz < 700.0 ? (float) (12.0 * std::log2 (hz / tailHz)) : NAN);
            }
        }

        // Basens övertoner 8-40 ms in i varje ton med säker tonhöjd, vid tonens egen frekvens.
        std::vector<double> sum (fitHarmonics, 0.0);
        int count = 0;
        for (int b = 0; b < barBeats; ++b)
        {
            for (size_t s = 0; s < pattern.steps.size(); ++s)
            {
                if (! layout.sure[(size_t) b][s])
                    continue;
                const auto hz = layout.rootHz * std::pow (2.0, layout.notes[(size_t) b][s] / 12.0);
                const auto start = b * layout.beatLength + pattern.steps[s] * layout.stepLength + ms (8);
                const auto levels = harmonicProfile (bar, start, ms (32), hz, fitHarmonics);
                if (levels.empty() || levels[0] <= 0.0f)
                    continue;
                for (size_t k = 0; k < levels.size() && k < sum.size(); ++k)
                    sum[k] += toDb (levels[k]) - toDb (levels[0]);
                ++count;
            }
        }
        for (auto v : sum)
            f.timbre.push_back (count > 0 ? (float) (v / count) : 0.0f);
        return f;
    }

    float beatError (const BeatFeatures& c, const BeatFeatures& t)
    {
        double error = 0.0;
        auto curve = [&] (const std::vector<float>& a, const std::vector<float>& b, double over, double under)
        {
            for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
            {
                const auto diff = std::max (-40.0f, a[i]) - std::max (-40.0f, b[i]);
                error += (diff > 0.0f ? over : under) * diff * diff;
            }
        };
        // Referensen har fler instrument i mellan- och diskantbanden: där är tystare mer okej än starkare.
        curve (c.low, t.low, 1.0, 1.0);
        curve (c.mid, t.mid, 0.7, 0.4);
        curve (c.high, t.high, 0.5, 0.1);

        for (size_t i = 0; i < std::min (c.pitch.size(), t.pitch.size()); ++i)
        {
            if (std::isnan (t.pitch[i]))
                continue;
            const auto candidate = std::isnan (c.pitch[i]) ? 0.0f : c.pitch[i];
            const auto diff = juce::jlimit (-8.0f, 8.0f, candidate - t.pitch[i]);
            error += 4.0 * diff * diff;
        }

        // Klangen är ett medel över takten; vikten motsvarar fyra slags kurvor.
        for (size_t k = 1; k < std::min (c.timbre.size(), t.timbre.size()); ++k)
        {
            const auto diff = t.timbre[k] < -30.0f ? std::max (0.0f, c.timbre[k] - t.timbre[k]) : c.timbre[k] - t.timbre[k];
            error += (k < 6 ? 8.0 : 4.0) * diff * diff;
        }
        return (float) error;
    }

    struct BeatCandidate
    {
        KickParams kick;
        BassParams bass;
        float bassLevel = 0.7f;  // relativt kicken
        float duck = 0.8f;
        float gate = 0.7f;       // basens tonlängd, andel av steget
        float mixDrive = 0.0f;   // mjuk klippning av summan (som en master)
    };

    // Renderar fem slag (det sista slaget i takten som inledning, sedan takten) och returnerar
    // takten i analysfrekvensen.
    Signal renderBar (const BeatCandidate& c, const BarLayout& layout, const BeatPattern& pattern, double tailHz)
    {
        KickEngine kick;
        BassSynth bass;
        Ducker ducker;
        kick.prepare (fitRate);
        bass.prepare (fitRate);
        ducker.prepare (fitRate);

        auto kickParams = c.kick;
        kickParams.tailHz = (float) tailHz;
        kickParams.level = 1.0f;
        kick.setParams (kickParams);
        auto bassParams = c.bass;
        bassParams.rootHz = (float) layout.rootHz;
        bassParams.level = 1.0f;
        bass.setParams (bassParams);

        const auto factor = (int) std::round (fitRate / analysisRate);
        const auto beatSamples = layout.beatLength * factor;
        const auto stepSamples = beatSamples / pattern.stepsPerBeat;
        std::vector<float> audio ((size_t) ((barBeats + 1) * beatSamples));
        const auto gain = 1.0f + c.mixDrive * 9.0f;
        const auto gateSamples = std::max (1, (int) (c.gate * stepSamples));

        for (int i = 0; i < (int) audio.size(); ++i)
        {
            const auto inBeat = i % beatSamples;
            const auto position = (i / beatSamples + barBeats - 1) % barBeats;
            if (inBeat == 0)
                kick.trigger (1.0f);
            for (size_t s = 0; s < pattern.steps.size(); ++s)
                if (inBeat == pattern.steps[s] * stepSamples)
                    bass.noteOn (layout.notes[(size_t) position][s], 0.8f, gateSamples, false);

            const auto k = kick.nextSample();
            const auto duckGain = ducker.next (kick.getEnvelope(), c.duck);
            auto out = k + bass.nextSample() * duckGain * c.bassLevel;
            if (c.mixDrive > 0.0f)
                out = std::tanh (gain * out) / std::tanh (gain);
            audio[(size_t) i] = out;
        }

        const auto decimated = decimate (audio);
        const auto from = (size_t) layout.beatLength;
        const auto to = std::min (decimated.size(), (size_t) ((barBeats + 1) * layout.beatLength));
        return Signal (decimated.begin() + (std::ptrdiff_t) from, decimated.begin() + (std::ptrdiff_t) to);
    }

    void printFeatures (const char* label, const BeatFeatures& f)
    {
        auto every = [] (const std::vector<float>& v, int n)
        {
            for (size_t i = 0; i < v.size(); i += (size_t) n)
                std::printf ("%4.0f", v[i]);
        };
        std::printf ("  %-9s bas<150 var 20ms:", label);
        every (f.low, 4);
        std::printf ("\n  %-9s 150-600 var 20ms:", "");
        every (f.mid, 4);
        std::printf ("\n  %-9s overtoner:", "");
        every (f.timbre, 1);
        std::printf ("  | svep slag 1:");
        for (size_t i = 0; i < f.pitch.size() / barBeats; ++i)
        {
            if (std::isnan (f.pitch[i]))
                std::printf ("   .");
            else
                std::printf ("%4.0f", f.pitch[i]);
        }
        std::puts ("");
    }

    double midiOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }

    // Ett medelslag per position i takten, efter varandra. rotation = vilket slag i partiet
    // som räknas som takten första (för att jämföra med en rendering som börjar på taktstrecket).
    Signal averageBar (const Signal& x, const Beats& beats, const Section& section, int beatLength, int rotation = 0)
    {
        Signal bar ((size_t) (barBeats * beatLength), 0.0f);
        for (int position = 0; position < barBeats; ++position)
        {
            int count = 0;
            for (int b = section.firstBeat + (position + rotation) % barBeats; b < section.firstBeat + section.numBeats; b += barBeats)
            {
                const auto onset = (int) std::round (beats.onsets[(size_t) b]);
                if (onset < 0 || onset + beatLength > (int) x.size())
                    continue; // första slaget kan ha flyttats före filens början
                for (int i = 0; i < beatLength; ++i)
                    bar[(size_t) (position * beatLength + i)] += x[(size_t) (onset + i)];
                ++count;
            }
            for (int i = 0; i < beatLength; ++i)
                bar[(size_t) (position * beatLength + i)] /= (float) std::max (1, count);
        }
        return bar;
    }

    // Mäter basens toner per steg och kickens svans i en takt av medelslag.
    bool analyseBar (const Signal& bar, const BeatPattern& pattern, BarLayout& layout, double& tailHz)
    {
        // Basens toner per steg. Säkra steg (YIN-säkerhet minst 0,5) mäter klangen; osäkra får en
        // gissning (säkerhet ned till 0,3, under 160 Hz), annars samma steg i ett annat slag.
        // Grundtonen = den vanligaste tonen (vid lika: den lägsta).
        const auto bassBand = bandpass (bar, 35.0, 350.0);
        std::vector<std::vector<double>> hz ((size_t) barBeats, std::vector<double> (pattern.steps.size(), 0.0));
        layout.sure.assign ((size_t) barBeats, std::vector<bool> (pattern.steps.size(), false));
        std::map<int, int> noteCount;
        std::puts ("  tonhojd per steg (Hz/sakerhet):");
        for (int b = 0; b < barBeats; ++b)
        {
            std::printf ("   slag %d:", b + 1);
            for (size_t s = 0; s < pattern.steps.size(); ++s)
            {
                float confidence = 0.0f;
                const auto stepStart = b * layout.beatLength + pattern.steps[s] * layout.stepLength;
                auto f = detectPitch (bassBand, stepStart + ms (6), layout.stepLength - ms (12), confidence);
                layout.sure[(size_t) b][s] = f > 0.0;
                if (f <= 0.0)
                    f = detectPitch (bassBand, stepStart + ms (6), layout.stepLength - ms (12), confidence, 30.0, 160.0, 0.3f);
                std::printf ("  %.1f/%.2f%s", f, confidence, layout.sure[(size_t) b][s] ? "" : "?");
                if (! layout.sure[(size_t) b][s])
                {
                    // Andra åsikt: den halvton (33-110 Hz) vars sex första övertoner har mest energi.
                    auto bestScore = 0.0;
                    auto bestHz = 0.0;
                    for (int note = 24; note <= 45; ++note)
                    {
                        const auto candidate = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                        const auto levels = harmonicProfile (bar, stepStart + ms (8), layout.stepLength - ms (16), candidate, 6);
                        auto score = 0.0;
                        for (size_t k = 0; k < levels.size(); ++k)
                            score += levels[k] / (double) (k + 1);
                        if (score > bestScore)
                        {
                            bestScore = score;
                            bestHz = candidate;
                        }
                    }
                    std::printf (" (overtoner: %s)", noteName (bestHz).toRawUTF8());
                }
                if (f > 0.0)
                {
                    hz[(size_t) b][s] = f;
                    ++noteCount[(int) std::round (midiOf (f))];
                }
            }
            std::puts ("");
        }
        if (noteCount.empty())
        {
            std::puts ("  kunde inte mata basens toner");
            return false;
        }
        auto rootNote = noteCount.begin()->first;
        for (const auto& [note, count] : noteCount)
            if (count > noteCount[rootNote])
                rootNote = note;

        std::vector<double> rootHzs;
        for (const auto& row : hz)
            for (auto f : row)
                if (f > 0.0 && (int) std::round (midiOf (f)) == rootNote)
                    rootHzs.push_back (f);
        std::sort (rootHzs.begin(), rootHzs.end());
        layout.rootHz = rootHzs[rootHzs.size() / 2];

        layout.notes.assign ((size_t) barBeats, std::vector<int> (pattern.steps.size(), 0));
        for (size_t s = 0; s < pattern.steps.size(); ++s)
        {
            int fallback = 0;
            for (int b = 0; b < barBeats; ++b)
                if (hz[(size_t) b][s] > 0.0)
                    fallback = (int) std::round (midiOf (hz[(size_t) b][s]) - midiOf (layout.rootHz));
            for (int b = 0; b < barBeats; ++b)
            {
                const auto f = hz[(size_t) b][s];
                layout.notes[(size_t) b][s] = f > 0.0 ? (int) std::round (midiOf (f) - midiOf (layout.rootHz)) : fallback;
            }
        }

        std::printf ("  grundton %.1f Hz (%s). Toner per slag:", layout.rootHz, noteName (layout.rootHz).toRawUTF8());
        for (int b = 0; b < barBeats; ++b)
        {
            std::printf (" |");
            for (size_t s = 0; s < pattern.steps.size(); ++s)
                std::printf (" %+d%s", layout.notes[(size_t) b][s], layout.sure[(size_t) b][s] ? "" : "?");
        }
        std::puts ("");

        // Kickens svans: momentan frekvens från 25 ms in tills strax före första bassteget, i alla slag.
        const auto frequency = instantaneousFrequency (lowpass (bar, 800.0));
        std::vector<float> tail;
        for (int b = 0; b < barBeats; ++b)
            for (int i = ms (25); i < pattern.steps.front() * layout.stepLength - ms (5); ++i)
            {
                const auto f = frequency[(size_t) (b * layout.beatLength + i)];
                if (f > 25.0f && f < 120.0f)
                    tail.push_back (f);
            }
        std::sort (tail.begin(), tail.end());
        tailHz = tail.empty() ? layout.rootHz : (double) tail[tail.size() / 2];
        std::printf ("  kickens svans %.1f Hz (%s)\n", tailHz, noteName (tailHz).toRawUTF8());
        return true;
    }
}

int fitBeat (const juce::File& file, double start, double end, const juce::String& patternName)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0); // utskriften syns direkt, även om körningen avbryts
    std::printf ("Anpassar kick + bas mot %s (%s)\n", file.getFileName().toRawUTF8(), patternName.toRawUTF8());
    Signal x;
    Beats beats;
    Section section;
    if (! prepareSection (file, start, end, x, beats, section))
        return 1;

    const auto pattern = patternFromName (patternName);
    BarLayout layout;
    layout.beatLength = (int) std::round (beats.samplesPerBeat);
    layout.stepLength = layout.beatLength / pattern.stepsPerBeat;
    const auto bar = averageBar (x, beats, section, layout.beatLength);
    double tailHz = 0.0;
    if (! analyseBar (bar, pattern, layout, tailHz))
        return 1;
    const auto target = measureBar (bar, layout, pattern, tailHz);

    std::mt19937 rng (777);
    auto uniform = [&] (float lo, float hi) { return std::uniform_real_distribution<float> (lo, hi) (rng); };
    auto logUniform = [&] (float lo, float hi) { return std::exp (uniform (std::log (lo), std::log (hi))); };
    auto chance = [&] (float p) { return uniform (0.0f, 1.0f) < p; };
    // Inte +7: med FM ger kvinten halvtalsövertoner (en oktav ner) som klangmätningen inte ser.
    const float semitoneChoices[] { 0.0f, 12.0f, 12.0f, 19.0f, 24.0f };

    auto randomCandidate = [&]
    {
        BeatCandidate c;
        auto& k = c.kick;
        // Rimliga kickar: svepmätningen är brusig, så sökningen ska inte kunna hitta på konstiga
        // lösningar (t.ex. klicket som kropp).
        k.pitchSemis = uniform (12.0f, 48.0f);
        k.pitchTimeMs = logUniform (3.0f, 40.0f);
        k.pitchCurve = logUniform (1.0f, 5.0f);
        k.glideSemis = chance (0.4f) ? 0.0f : uniform (0.0f, 8.0f);
        k.glideTimeMs = logUniform (20.0f, 200.0f);
        k.attackMs = uniform (0.0f, 1.0f);
        k.holdMs = uniform (0.0f, 60.0f);
        k.decayMs = logUniform (60.0f, 350.0f);
        k.decayCurve = logUniform (1.0f, 3.5f);
        k.clickLevel = uniform (0.0f, 0.5f);
        k.clickLengthMs = uniform (1.0f, 6.0f);
        k.clickToneHz = logUniform (2000.0f, 12000.0f);
        k.drive = uniform (0.0f, 0.8f);

        auto& b = c.bass;
        b.shape = uniform (0.0f, 3.0f);
        b.pulseWidth = uniform (0.1f, 0.9f);
        b.sub = uniform (0.0f, 0.8f);
        b.subOctave = 0;
        b.noise = chance (0.7f) ? 0.0f : uniform (0.0f, 0.1f);
        b.osc2Shape = uniform (0.0f, 3.0f);
        b.osc2PulseWidth = uniform (0.1f, 0.9f);
        b.osc2Level = chance (0.3f) ? 0.0f : uniform (0.0f, 0.8f);
        b.osc2Semis = semitoneChoices[std::uniform_int_distribution<int> (0, 4) (rng)];
        b.osc2Detune = uniform (0.0f, 15.0f);
        b.fmAmount = chance (0.4f) ? 0.0f : uniform (0.0f, 0.6f);
        b.fmEnvAmount = chance (0.4f) ? 0.0f : uniform (0.0f, 0.6f);
        b.filterType = chance (0.5f) ? BassFilterType::lowpass24 : BassFilterType::lowpass12;
        b.cutoffHz = logUniform (40.0f, 2500.0f);
        b.resonance = uniform (0.0f, 0.7f);
        b.envAmount = uniform (0.0f, 6.0f);
        b.envDecayMs = logUniform (15.0f, 400.0f);
        b.velToEnv = 0.0f;
        b.drive = uniform (0.0f, 0.8f);
        b.attackMs = logUniform (0.3f, 20.0f);
        b.decayMs = logUniform (20.0f, 500.0f);
        b.sustain = uniform (0.0f, 1.0f);
        b.releaseMs = logUniform (2.0f, 60.0f);

        c.bassLevel = logUniform (0.2f, 2.0f);
        c.duck = uniform (0.0f, 1.0f);
        c.gate = uniform (0.3f, 1.0f);
        c.mixDrive = chance (0.5f) ? 0.0f : uniform (0.0f, 0.6f);
        return c;
    };

    auto evaluate = [&] (const BeatCandidate& c) { return beatError (measureBar (renderBar (c, layout, pattern, tailHz), layout, pattern, tailHz), target); };

    // De fem bästa slumpkandidaterna finjusteras var för sig (felytan har många lokala minimum).
    constexpr int numRandom = 4000, numRestarts = 5;
    std::vector<std::pair<float, BeatCandidate>> top;
    for (int i = 0; i < numRandom; ++i)
    {
        const auto c = randomCandidate();
        const auto e = evaluate (c);
        if ((int) top.size() < numRestarts || e < top.back().first)
        {
            top.emplace_back (e, c);
            std::sort (top.begin(), top.end(), [] (const auto& p, const auto& q) { return p.first < q.first; });
            if ((int) top.size() > numRestarts)
                top.pop_back();
        }
    }
    std::printf ("  basta efter %d slumpforsok: fel %.0f\n", numRandom, top.front().first);

    BeatCandidate best;
    auto bestError = std::numeric_limits<float>::max();
    for (auto& [startError, candidate] : top)
    {
        auto error = startError;
        auto& k = candidate.kick;
        auto& b = candidate.bass;
        refine ({ { &k.pitchSemis, 12.0f, 60.0f, false }, { &k.pitchTimeMs, 2.0f, 60.0f, true }, { &k.pitchCurve, 0.8f, 6.0f, true },
                  { &k.glideSemis, 0.0f, 12.0f, false }, { &k.glideTimeMs, 10.0f, 300.0f, true }, { &k.attackMs, 0.0f, 2.0f, false },
                  { &k.holdMs, 0.0f, 120.0f, false }, { &k.decayMs, 40.0f, 600.0f, true }, { &k.decayCurve, 0.8f, 5.0f, true },
                  { &k.clickLevel, 0.0f, 0.8f, false }, { &k.clickLengthMs, 1.0f, 10.0f, false }, { &k.clickToneHz, 1500.0f, 16000.0f, true },
                  { &k.drive, 0.0f, 1.0f, false },
                  { &b.shape, 0.0f, 3.0f, false }, { &b.pulseWidth, 0.05f, 0.95f, false }, { &b.sub, 0.0f, 1.0f, false },
                  { &b.noise, 0.0f, 0.15f, false }, { &b.osc2Shape, 0.0f, 3.0f, false }, { &b.osc2PulseWidth, 0.05f, 0.95f, false },
                  { &b.osc2Level, 0.0f, 1.0f, false }, { &b.osc2Detune, 0.0f, 50.0f, false }, { &b.fmAmount, 0.0f, 1.0f, false },
                  { &b.fmEnvAmount, 0.0f, 1.0f, false }, { &b.cutoffHz, 30.0f, 8000.0f, true }, { &b.resonance, 0.0f, 1.0f, false },
                  { &b.envAmount, 0.0f, 8.0f, false }, { &b.envDecayMs, 5.0f, 1000.0f, true }, { &b.drive, 0.0f, 1.0f, false },
                  { &b.attackMs, 0.05f, 50.0f, true }, { &b.decayMs, 5.0f, 2000.0f, true }, { &b.sustain, 0.0f, 1.0f, false },
                  { &b.releaseMs, 1.0f, 300.0f, true },
                  { &candidate.bassLevel, 0.05f, 3.0f, true }, { &candidate.duck, 0.0f, 1.0f, false },
                  { &candidate.gate, 0.1f, 1.0f, false }, { &candidate.mixDrive, 0.0f, 1.0f, false } },
                error, [&] { return evaluate (candidate); });
        std::printf ("  finjustering: fel %.0f -> %.0f\n", startError, error);
        if (error < bestError)
        {
            bestError = error;
            best = candidate;
        }
    }
    const auto& k = best.kick;
    const auto& b = best.bass;

    std::printf ("  efter finjustering: fel %.0f\n\nBASTA INSTALLNINGAR\n", bestError);
    std::printf ("  KICK: svans %.1f Hz  pitch %.1f st  p.time %.1f ms  p.curve %.2f  glide %.1f st  g.time %.0f ms\n",
                 tailHz, k.pitchSemis, k.pitchTimeMs, k.pitchCurve, k.glideSemis, k.glideTimeMs);
    std::printf ("        attack %.2f  hold %.1f  decay %.0f ms  d.curve %.2f  klick %.2f (%.1f ms, %.0f Hz)  drive %.2f\n",
                 k.attackMs, k.holdMs, k.decayMs, k.decayCurve, k.clickLevel, k.clickLengthMs, k.clickToneHz, k.drive);
    BassCandidate bassOnly { b, best.gate };
    std::puts ("  BAS:");
    printBass (bassOnly);
    std::printf ("  MIX: basniva %.2f (rel. kick)  duck %.2f  summa-drive %.2f\n", best.bassLevel, best.duck, best.mixDrive);

    // Samma sak i pluginets parametrar. Kicken följer tonarten: svansen ligger på tonen mellan A#0 och A1.
    const auto rootMidi = (int) std::round (midiOf (layout.rootHz));
    auto kickTail = 24 + ((rootMidi % 12) + 12) % 12;
    if (kickTail > 33)
        kickTail -= 12;
    const auto kickLevel = std::min (0.8f, 1.0f / std::max (best.bassLevel, 1.0e-3f));
    std::printf ("\nI PLUGINET (spela %s i pianorullen):\n", noteName (layout.rootHz * 4.0).toRawUTF8());
    std::printf ("  kickTune %.2f  kickLevel %.2f  bassLevel %.2f  duckAmount %.2f  bassTune %.2f\n",
                 midiOf (tailHz) - kickTail, kickLevel, std::min (1.0f, best.bassLevel * kickLevel), best.duck,
                 midiOf (layout.rootHz) - rootMidi);
    std::printf ("  gate %.2f av steget: bassGate %.2f (stegens gate 0.70)%s\n", best.gate, std::min (1.0f, best.gate / 0.7f),
                 best.gate > 0.7f ? ", eller stegens gate = gaten och bassGate 1" : "");
    if (best.mixDrive > 0.0f)
        std::printf ("  masterDrive %.1f dB  masterClip 1.00\n", juce::Decibels::gainToDecibels ((1.0f + best.mixDrive * 9.0f) / kickLevel));

    printFeatures ("RESULTAT", measureBar (renderBar (best, layout, pattern, tailHz), layout, pattern, tailHz));
    printFeatures ("REFERENS", target);
    return 0;
}

int compareBeat (const juce::File& file, double start, double end, const juce::String& patternName, const juce::File& rendered)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    std::printf ("Jamfor %s med %s (%s)\n", rendered.getFileName().toRawUTF8(), file.getFileName().toRawUTF8(), patternName.toRawUTF8());
    Signal x, y;
    Beats beats, renderBeats;
    Section section, renderSection;
    if (! prepareSection (file, start, end, x, beats, section))
        return 1;
    const auto referenceRate = analysisRate;
    if (! prepareSection (rendered, 0.0, 100000.0, y, renderBeats, renderSection))
        return 1;
    if (std::abs (analysisRate - referenceRate) > 1.0)
    {
        // Analysfrekvensen är global (sätts av den senast lästa filen), så båda måste vara lika.
        std::printf ("  renderingen och referensen har olika samplingsfrekvens (%.0f / %.0f Hz analys)\n", analysisRate, referenceRate);
        return 1;
    }

    const auto pattern = patternFromName (patternName);
    BarLayout layout;
    layout.beatLength = (int) std::round (beats.samplesPerBeat);
    layout.stepLength = layout.beatLength / pattern.stepsPerBeat;
    const auto bar = averageBar (x, beats, section, layout.beatLength);
    double tailHz = 0.0;
    if (! analyseBar (bar, pattern, layout, tailHz))
        return 1;
    const auto target = measureBar (bar, layout, pattern, tailHz);

    // Renderingen börjar på taktstrecket, referensens parti kanske inte: välj den förskjutning
    // som passar bäst. Renderingens sista slag (svansen efter tonen) räknas inte.
    if (renderSection.numBeats > barBeats)
        --renderSection.numBeats;
    auto bestError = std::numeric_limits<float>::max();
    BeatFeatures best;
    int bestRotation = 0;
    for (int rotation = 0; rotation < barBeats; ++rotation)
    {
        const auto features = measureBar (averageBar (y, renderBeats, renderSection, layout.beatLength, rotation), layout, pattern, tailHz);
        const auto error = beatError (features, target);
        if (error < bestError)
        {
            bestError = error;
            best = features;
            bestRotation = rotation;
        }
    }
    std::printf ("  fel %.0f (renderingens slag %d som referensens forsta)\n", bestError, bestRotation + 1);
    printFeatures ("RENDER", best);
    printFeatures ("REFERENS", target);
    return 0;
}
