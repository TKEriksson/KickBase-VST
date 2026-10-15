// Anpassning: sök syntinställningar som ger samma mätvärden som referensen.
// Inkluderas sist i Analyzer.cpp, så att mätfunktionerna där kan användas.

namespace
{
    constexpr int fitHarmonics = 12;
    constexpr double fitRate = 44100.0; // kandidaterna renderas här och decimeras som referensen

    // Läser partiet, hittar slagen och flyttar dem till kickens ansats (som analyzeFile).
    bool prepareSection (const juce::File& file, double start, double end, Signal& x, Beats& beats, Section& section)
    {
        double fileSeconds = 0.0;
        x = loadMono (file, start, end, fileSeconds);
        if (x.size() < 1000)
            return false;
        beats = findBeats (x);
        if (beats.onsets.size() < 8)
            return false;
        section = { 0, (int) beats.onsets.size() - 1 };
        alignBeats (x, beats.onsets, section.firstBeat, section.numBeats);
        moveToKickStart (x, beats.onsets, section.firstBeat, section.numBeats);
        std::printf ("  %.1f BPM, %d slag\n", beats.bpm, section.numBeats);
        return true;
    }

    // Decimerar en rendering (44,1 kHz) till analysfrekvensen med samma filter som loadMono.
    Signal decimate (const std::vector<float>& input)
    {
        const auto factor = (int) std::round (fitRate / analysisRate);
        const auto antiAlias = juce::dsp::IIR::Coefficients<float>::makeLowPass (fitRate, (float) (analysisRate * 0.42));
        juce::dsp::IIR::Filter<float> a (antiAlias), b (antiAlias);
        Signal output;
        output.reserve (input.size() / (size_t) factor + 1);
        for (size_t i = 0; i < input.size(); ++i)
        {
            const auto filtered = b.processSample (a.processSample (input[i]));
            if (i % (size_t) factor == 0)
                output.push_back (filtered);
        }
        return output;
    }

    //==============================================================================
    // Basen

    struct BassMeasure
    {
        std::vector<float> early, late;   // övertoner 1..12, dB relativt grundtonen
        std::vector<float> envelope;      // volym var 5:e ms (0..80 ms), dB relativt tonens topp
    };

    BassMeasure measureNote (const Signal& raw, int start, int stepLength, double f0)
    {
        BassMeasure m;
        const auto ms = [] (double v) { return (int) (v * 0.001 * analysisRate); };
        auto profile = [&] (int from, int to)
        {
            std::vector<float> result;
            const auto levels = harmonicProfile (raw, start + from, std::min (to, stepLength) - from, f0, fitHarmonics);
            for (auto level : levels)
                result.push_back (toDb (level) - toDb (levels.empty() ? 1.0f : levels[0]));
            return result;
        };
        m.early = profile (ms (8), ms (40));
        m.late = profile (ms (40), ms (80));

        Signal window (raw.begin() + start, raw.begin() + std::min ((int) raw.size(), start + stepLength));
        const auto envelope = envelopeOf (lowpass (window, 1000.0));
        auto peak = 1.0e-9f;
        for (auto v : envelope)
            peak = std::max (peak, v);
        for (int t = 0; t <= 80; t += 5)
        {
            const auto i = ms (t);
            m.envelope.push_back (i < (int) envelope.size() ? toDb (envelope[(size_t) i]) - toDb (peak) : -60.0f);
        }
        return m;
    }

    float bassError (const BassMeasure& candidate, const BassMeasure& target)
    {
        double error = 0.0;
        auto harmonics = [&] (const std::vector<float>& c, const std::vector<float>& t)
        {
            for (size_t k = 1; k < std::min (c.size(), t.size()); ++k)
            {
                const auto weight = k < 6 ? 1.0 : 0.5;
                // Under -30 dB kan referensens överton vara maskerad: straffa bara om kandidaten är starkare.
                const auto diff = t[k] < -30.0f ? std::max (0.0f, c[k] - t[k]) : c[k] - t[k];
                error += weight * diff * diff;
            }
        };
        harmonics (candidate.early, target.early);
        harmonics (candidate.late, target.late);
        for (size_t i = 0; i < std::min (candidate.envelope.size(), target.envelope.size()); ++i)
        {
            const auto diff = std::max (-40.0f, candidate.envelope[i]) - std::max (-40.0f, target.envelope[i]);
            error += 0.5 * diff * diff;
        }
        return (float) error;
    }

    struct BassCandidate
    {
        BassParams params;
        float gate = 0.6f; // tonens längd, andel av steget
    };

    BassMeasure renderBass (const BassCandidate& c, double f0, double stepSeconds)
    {
        BassSynth synth;
        synth.prepare (fitRate);
        auto p = c.params;
        p.rootHz = (float) f0;
        p.level = 1.0f;
        synth.setParams (p);

        const auto length = (int) (stepSeconds * fitRate);
        synth.noteOn (0, 0.8f, std::max (1, (int) (c.gate * length)), false);
        std::vector<float> audio ((size_t) length);
        for (auto& s : audio)
            s = synth.nextSample();

        const auto decimated = decimate (audio);
        return measureNote (decimated, 0, (int) decimated.size(), f0);
    }

    void printBass (const BassCandidate& c)
    {
        const auto& p = c.params;
        std::printf ("  osc A: %s pos %.2f  |  osc B: %s pos %.2f  level %.2f  semi %.0f  fine %.1f ct\n",
                     Wavetables::get (p.oscA.table).getName().toRawUTF8(), p.oscA.position,
                     Wavetables::get (p.oscB.table).getName().toRawUTF8(), p.oscB.position, p.oscB.level, p.oscB.semis, p.oscB.fineCents);
        std::printf ("  warp FM %.2f  warp env %.2f  sub %.2f (oct %d)  noise %.2f\n", p.oscA.warpAmount, p.warpEnv, p.sub, p.subOctave, p.noise);
        std::printf ("  filter: %s  cutoff %.0f Hz  reso %.2f  env %.2f oct  envDecay %.0f ms  drive %.2f\n",
                     p.filterType == BassFilterType::lowpass24 ? "LP24" : "LP12", p.cutoffHz, p.resonance, p.envAmount, p.envDecayMs, p.drive);
        std::printf ("  amp: attack %.1f  decay %.0f  sustain %.2f  release %.1f ms  gate %.2f av steget\n",
                     p.attackMs, p.decayMs, p.sustain, p.releaseMs, c.gate);
    }

    void printMeasure (const char* label, const BassMeasure& m)
    {
        std::printf ("  %-10s tidigt:", label);
        for (auto v : m.early) std::printf ("%4.0f", v);
        std::printf ("  | sent:");
        for (auto v : m.late) std::printf ("%4.0f", v);
        std::printf ("  | env:");
        for (size_t i = 0; i < m.envelope.size(); i += 2) std::printf ("%4.0f", m.envelope[i]);
        std::puts ("");
    }

    // Finjustering: prova att öka och minska varje parameter, med allt mindre steg.
    struct FitKnob
    {
        float* value;
        float lo, hi;
        bool logScale;
    };

    template <typename Evaluate>
    void refine (std::vector<FitKnob> knobs, float& bestError, Evaluate&& evaluate)
    {
        for (float scale = 0.25f; scale > 0.01f; scale *= 0.5f)
        {
            for (int pass = 0; pass < 2; ++pass)
            {
                for (auto& knob : knobs)
                {
                    for (const auto direction : { 1.0f, -1.0f })
                    {
                        const auto original = *knob.value;
                        *knob.value = knob.logScale ? juce::jlimit (knob.lo, knob.hi, original * std::exp (direction * scale * 2.0f))
                                                    : juce::jlimit (knob.lo, knob.hi, original + direction * scale * (knob.hi - knob.lo));
                        const auto e = evaluate();
                        if (e < bestError)
                            bestError = e;
                        else
                            *knob.value = original;
                    }
                }
            }
        }
    }
}

int fitBass (const juce::File& file, double start, double end, int stepsPerBeat)
{
    std::printf ("Anpassar basen mot %s\n", file.getFileName().toRawUTF8());
    Signal x;
    Beats beats;
    Section section;
    if (! prepareSection (file, start, end, x, beats, section))
        return 1;

    // Mät referensen: medel av alla steg med säker tonhöjd, per position i takten.
    const auto bassBand = bandpass (x, 35.0, 350.0);
    const auto beatLength = (int) beats.samplesPerBeat;
    const auto stepLength = (int) (beats.samplesPerBeat / stepsPerBeat);
    BassMeasure target;
    std::vector<double> f0s;
    int used = 0;

    for (int position = 0; position < 4; ++position)
    {
        Signal raw ((size_t) beatLength, 0.0f), band ((size_t) beatLength, 0.0f);
        int count = 0;
        for (int b = section.firstBeat + position; b < section.firstBeat + section.numBeats; b += 4, ++count)
        {
            const auto onset = (int) std::round (beats.onsets[(size_t) b]);
            for (int i = 0; i < beatLength && onset + i < (int) x.size(); ++i)
            {
                raw[(size_t) i] += x[(size_t) (onset + i)];
                band[(size_t) i] += bassBand[(size_t) (onset + i)];
            }
        }
        if (count == 0)
            continue;

        // I sextondelsmönster hörs kickens svans ofta in i andra sextondelen (samma frekvens som
        // basen), så bara steg 3-4 mäts där. Trioler ligger längre från kicken: båda mäts.
        for (int step = stepsPerBeat == 4 ? 2 : 1; step < stepsPerBeat; ++step)
        {
            const auto stepStart = step * stepLength;
            float confidence = 0.0f;
            const auto f0 = detectPitch (band, stepStart + (int) (0.006 * analysisRate), stepLength - (int) (0.012 * analysisRate), confidence);
            if (f0 <= 0.0 || confidence < 0.7f)
                continue;

            const auto m = measureNote (raw, stepStart, stepLength, f0);
            if (used == 0)
                target = m;
            else
            {
                for (size_t k = 0; k < std::min (target.early.size(), m.early.size()); ++k) target.early[k] += m.early[k];
                for (size_t k = 0; k < std::min (target.late.size(), m.late.size()); ++k) target.late[k] += m.late[k];
                for (size_t k = 0; k < std::min (target.envelope.size(), m.envelope.size()); ++k) target.envelope[k] += m.envelope[k];
            }
            f0s.push_back (f0);
            ++used;
        }
    }

    if (used == 0)
    {
        std::puts ("  hittade inga steg med saker tonhojd");
        return 1;
    }
    for (auto& v : target.early) v /= (float) used;
    for (auto& v : target.late) v /= (float) used;
    for (auto& v : target.envelope) v /= (float) used;
    std::sort (f0s.begin(), f0s.end());
    const auto f0 = f0s[f0s.size() / 2];
    const auto stepSeconds = stepLength / analysisRate;
    std::printf ("  %d steg, grundton %.1f Hz (%s), steg %.0f ms\n", used, f0, noteName (f0).toRawUTF8(), stepSeconds * 1000.0);
    printMeasure ("REFERENS", target);

    // Sökning: slumpmässiga kandidater, sedan finjustering av den bästa en parameter i taget.
    std::mt19937 rng (12345);
    auto uniform = [&] (float lo, float hi) { return std::uniform_real_distribution<float> (lo, hi) (rng); };
    auto logUniform = [&] (float lo, float hi) { return std::exp (uniform (std::log (lo), std::log (hi))); };
    const float semitoneChoices[] { 0.0f, 7.0f, 12.0f, 19.0f, 24.0f };

    auto randomCandidate = [&]
    {
        BassCandidate c;
        auto& p = c.params;
        p.oscA.position = uniform (0.0f, 1.0f);
        p.sub = uniform (0.0f, 0.8f);
        p.subOctave = 0; // grundtonen mäts här; en sub en oktav under syns inte i mätningen
        p.noise = uniform (0.0f, 1.0f) < 0.7f ? 0.0f : uniform (0.0f, 0.2f);
        p.oscB.position = uniform (0.0f, 1.0f);
        p.oscB.level = uniform (0.0f, 1.0f) < 0.3f ? 0.0f : uniform (0.0f, 0.8f);
        p.oscB.semis = semitoneChoices[std::uniform_int_distribution<int> (0, 4) (rng)];
        p.oscB.fineCents = uniform (0.0f, 15.0f);
        p.oscA.warp = WarpMode::fm;
        p.oscA.warpAmount = uniform (0.0f, 1.0f) < 0.4f ? 0.0f : uniform (0.0f, 0.6f);
        p.warpEnv = uniform (0.0f, 1.0f) < 0.4f ? 0.0f : uniform (0.0f, 0.6f);
        p.filterType = uniform (0.0f, 1.0f) < 0.5f ? BassFilterType::lowpass24 : BassFilterType::lowpass12;
        p.cutoffHz = logUniform (40.0f, 2500.0f);
        p.resonance = uniform (0.0f, 0.7f);
        p.envAmount = uniform (0.0f, 6.0f);
        p.envDecayMs = logUniform (15.0f, 400.0f);
        p.velToEnv = 0.0f;
        p.drive = uniform (0.0f, 0.8f);
        p.attackMs = logUniform (0.3f, 20.0f);
        p.decayMs = logUniform (20.0f, 500.0f);
        p.sustain = uniform (0.0f, 1.0f);
        p.releaseMs = logUniform (2.0f, 60.0f);
        c.gate = uniform (0.3f, 1.0f);
        return c;
    };

    BassCandidate best = randomCandidate();
    auto evaluateBest = [&] { return bassError (renderBass (best, f0, stepSeconds), target); };
    auto bestError = evaluateBest();
    constexpr int numRandom = 3000;
    for (int i = 0; i < numRandom; ++i)
    {
        const auto c = randomCandidate();
        const auto e = bassError (renderBass (c, f0, stepSeconds), target);
        if (e < bestError)
        {
            bestError = e;
            best = c;
        }
    }
    std::printf ("  basta efter %d slumpforsok: fel %.0f\n", numRandom, bestError);

    auto& p = best.params;
    refine ({ { &p.oscA.position, 0.0f, 1.0f, false }, { &p.sub, 0.0f, 1.0f, false },
              { &p.noise, 0.0f, 0.5f, false }, { &p.oscB.position, 0.0f, 1.0f, false },
              { &p.oscB.level, 0.0f, 1.0f, false }, { &p.oscB.fineCents, 0.0f, 50.0f, false }, { &p.oscA.warpAmount, 0.0f, 1.0f, false },
              { &p.warpEnv, 0.0f, 1.0f, false }, { &p.cutoffHz, 30.0f, 8000.0f, true }, { &p.resonance, 0.0f, 1.0f, false },
              { &p.envAmount, 0.0f, 8.0f, false }, { &p.envDecayMs, 5.0f, 1000.0f, true }, { &p.drive, 0.0f, 1.0f, false },
              { &p.attackMs, 0.05f, 50.0f, true }, { &p.decayMs, 5.0f, 2000.0f, true }, { &p.sustain, 0.0f, 1.0f, false },
              { &p.releaseMs, 1.0f, 300.0f, true }, { &best.gate, 0.1f, 1.0f, false } },
            bestError, evaluateBest);

    std::printf ("  efter finjustering: fel %.0f\n\nBASTA INSTALLNINGAR:\n", bestError);
    printBass (best);
    printMeasure ("RESULTAT", renderBass (best, f0, stepSeconds));
    printMeasure ("REFERENS", target);
    return 0;
}

//==============================================================================
// Kicken

namespace
{
    struct KickMeasure
    {
        std::vector<float> envelope; // kroppen (under 250 Hz), dB relativt toppen, vid kickTimes
        std::vector<float> pitch;    // halvtoner över svansen vid pitchTimes (NaN = osäkert)
        float click = -60.0f;        // diskantens topp de första 6 ms, dB relativt kroppen
    };

    const int kickTimes[] { 2, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
    const double pitchTimes[] { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 10.0, 13.0, 16.0, 20.0, 25.0, 30.0, 40.0 };

    KickMeasure measureKick (const Signal& beat, double tailHz, int region)
    {
        KickMeasure m;
        const auto body = envelopeOf (lowpass (beat, 250.0));
        auto peak = 1.0e-9f;
        for (int i = 0; i < std::min (region, (int) body.size()); ++i)
            peak = std::max (peak, body[(size_t) i]);
        for (auto t : kickTimes)
        {
            const auto i = (int) (t * 0.001 * analysisRate);
            m.envelope.push_back (i < (int) body.size() ? toDb (body[(size_t) i]) - toDb (peak) : -60.0f);
        }

        const auto frequency = instantaneousFrequency (lowpass (beat, 800.0));
        const auto sweep = envelopeOf (lowpass (beat, 800.0));
        auto sweepPeak = 1.0e-9f;
        for (int i = 0; i < std::min (region, (int) sweep.size()); ++i)
            sweepPeak = std::max (sweepPeak, sweep[(size_t) i]);
        for (auto t : pitchTimes)
        {
            const auto i = (int) (t * 0.001 * analysisRate);
            const auto hz = i < (int) frequency.size() ? frequency[(size_t) i] : 0.0f;
            const auto audible = i < (int) sweep.size() && toDb (sweep[(size_t) i]) > toDb (sweepPeak) - 20.0f;
            // Orimliga värden (under svansen eller över 700 Hz) betyder att mätningen störs.
            m.pitch.push_back (audible && hz > tailHz * 0.85 && hz < 700.0 ? (float) (12.0 * std::log2 (hz / tailHz)) : NAN);
        }

        const auto high = envelopeOf (highpass (beat, 2000.0));
        auto clickPeak = 1.0e-9f;
        for (int i = 0; i < (int) (0.006 * analysisRate) && i < (int) high.size(); ++i)
            clickPeak = std::max (clickPeak, high[(size_t) i]);
        m.click = toDb (clickPeak) - toDb (peak);
        return m;
    }

    float kickError (const KickMeasure& c, const KickMeasure& t)
    {
        double error = 0.0;
        for (size_t i = 0; i < std::min (c.envelope.size(), t.envelope.size()); ++i)
        {
            const auto diff = std::max (-40.0f, c.envelope[i]) - std::max (-40.0f, t.envelope[i]);
            error += diff * diff;
        }
        for (size_t i = 0; i < std::min (c.pitch.size(), t.pitch.size()); ++i)
        {
            if (std::isnan (t.pitch[i]))
                continue;
            const auto candidate = std::isnan (c.pitch[i]) ? 0.0f : c.pitch[i];
            const auto diff = juce::jlimit (-8.0f, 8.0f, candidate - t.pitch[i]);
            error += 4.0 * diff * diff;
        }
        const auto clickDiff = std::max (-40.0f, c.click) - std::max (-40.0f, t.click);
        error += 0.3 * clickDiff * clickDiff;
        return (float) error;
    }

    KickMeasure renderKick (const KickParams& params, int region)
    {
        KickEngine kick;
        kick.prepare (fitRate);
        auto p = params;
        p.level = 1.0f;
        kick.setParams (p);
        kick.trigger (1.0f);
        std::vector<float> audio ((size_t) (0.15 * fitRate));
        for (auto& s : audio)
            s = kick.nextSample();
        return measureKick (decimate (audio), p.tailHz, region);
    }

    void printKickMeasure (const char* label, const KickMeasure& m)
    {
        std::printf ("  %-9s env:", label);
        for (auto v : m.envelope) std::printf ("%4.0f", v);
        std::printf ("  | pitch:");
        for (auto v : m.pitch)
        {
            if (std::isnan (v))
                std::printf ("   .");
            else
                std::printf ("%4.0f", v);
        }
        std::printf ("  | klick %.0f\n", m.click);
    }
}

int fitKick (const juce::File& file, double start, double end)
{
    std::printf ("Anpassar kicken mot %s\n", file.getFileName().toRawUTF8());
    Signal x;
    Beats beats;
    Section section;
    if (! prepareSection (file, start, end, x, beats, section))
        return 1;

    const auto average = averageBeats (x, beats.onsets, section.firstBeat, section.numBeats, (int) beats.samplesPerBeat);
    const auto region = (int) (beats.samplesPerBeat / 4.0);

    // Svansens frekvens som i analysen: median av momentan frekvens från 25 ms tills kicken fallit 12 dB.
    const auto frequency = instantaneousFrequency (lowpass (average, 800.0));
    const auto body = envelopeOf (lowpass (average, 250.0));
    auto peak = 0.0f;
    int peakAt = 0;
    for (int i = 0; i < region; ++i)
        if (body[(size_t) i] > peak) { peak = body[(size_t) i]; peakAt = i; }
    auto audibleEnd = region;
    for (int i = peakAt; i < region; ++i)
        if (toDb (body[(size_t) i]) < toDb (peak) - 12.0f) { audibleEnd = i; break; }
    std::vector<float> tail;
    for (int i = (int) (0.025 * analysisRate); i < audibleEnd; ++i)
        if (frequency[(size_t) i] > 25.0f && frequency[(size_t) i] < 120.0f)
            tail.push_back (frequency[(size_t) i]);
    if (tail.empty())
    {
        std::puts ("  kunde inte mata svansen");
        return 1;
    }
    std::sort (tail.begin(), tail.end());
    const auto tailHz = (double) tail[tail.size() / 2];
    std::printf ("  svans %.1f Hz (%s)\n", tailHz, noteName (tailHz).toRawUTF8());

    const auto target = measureKick (average, tailHz, region);
    printKickMeasure ("REFERENS", target);

    std::mt19937 rng (54321);
    auto uniform = [&] (float lo, float hi) { return std::uniform_real_distribution<float> (lo, hi) (rng); };
    auto logUniform = [&] (float lo, float hi) { return std::exp (uniform (std::log (lo), std::log (hi))); };

    auto randomKick = [&]
    {
        KickParams p;
        p.tailHz = (float) tailHz;
        p.pitchSemis = uniform (0.0f, 48.0f);
        p.pitchTimeMs = logUniform (2.0f, 120.0f);
        p.pitchCurve = logUniform (0.3f, 8.0f);
        p.glideSemis = uniform (0.0f, 1.0f) < 0.4f ? 0.0f : uniform (0.0f, 12.0f);
        p.glideTimeMs = logUniform (10.0f, 300.0f);
        p.attackMs = uniform (0.0f, 2.0f);
        p.holdMs = uniform (0.0f, 60.0f);
        p.decayMs = logUniform (40.0f, 400.0f);
        p.decayCurve = logUniform (0.5f, 4.0f);
        p.clickLevel = uniform (0.0f, 0.6f);
        p.clickLengthMs = uniform (0.5f, 10.0f);
        p.clickToneHz = logUniform (1000.0f, 12000.0f);
        p.drive = uniform (0.0f, 0.8f);
        return p;
    };

    auto best = randomKick();
    auto evaluateBest = [&] { return kickError (renderKick (best, region), target); };
    auto bestError = evaluateBest();
    constexpr int numRandom = 3000;
    for (int i = 0; i < numRandom; ++i)
    {
        const auto p = randomKick();
        const auto e = kickError (renderKick (p, region), target);
        if (e < bestError) { bestError = e; best = p; }
    }
    std::printf ("  basta efter %d slumpforsok: fel %.0f\n", numRandom, bestError);

    refine ({ { &best.pitchSemis, 0.0f, 72.0f, false }, { &best.pitchTimeMs, 1.0f, 300.0f, true }, { &best.pitchCurve, 0.2f, 8.0f, true },
              { &best.glideSemis, 0.0f, 24.0f, false }, { &best.glideTimeMs, 5.0f, 400.0f, true }, { &best.attackMs, 0.0f, 10.0f, false },
              { &best.holdMs, 0.0f, 200.0f, false }, { &best.decayMs, 10.0f, 1500.0f, true }, { &best.decayCurve, 0.2f, 8.0f, true },
              { &best.clickLevel, 0.0f, 1.0f, false }, { &best.clickLengthMs, 1.0f, 30.0f, false }, { &best.clickToneHz, 500.0f, 16000.0f, true },
              { &best.drive, 0.0f, 1.0f, false } },
            bestError, evaluateBest);

    std::printf ("  efter finjustering: fel %.0f\n\nBASTA INSTALLNINGAR:\n", bestError);
    std::printf ("  svans %.1f Hz  pitch %.1f st  p.time %.1f ms  p.curve %.2f  glide %.1f st  g.time %.0f ms\n",
                 best.tailHz, best.pitchSemis, best.pitchTimeMs, best.pitchCurve, best.glideSemis, best.glideTimeMs);
    std::printf ("  attack %.2f  hold %.1f  decay %.0f ms  d.curve %.2f  klick %.2f (%.1f ms, %.0f Hz)  drive %.2f\n",
                 best.attackMs, best.holdMs, best.decayMs, best.decayCurve, best.clickLevel, best.clickLengthMs, best.clickToneHz, best.drive);
    printKickMeasure ("RESULTAT", renderKick (best, region));
    printKickMeasure ("REFERENS", target);
    return 0;
}
