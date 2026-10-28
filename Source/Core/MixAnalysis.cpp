#include "MixAnalysis.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <map>
#include <set>
#include "PluginBase.h"
#include "OfflineRender.h"
#include "AnalysisAbout.h"
#include "Settings.h"

namespace MixAnalysis
{
    namespace
    {
        constexpr float binNorm = 4.0f / (float) fftSize; // en sinus i full skala blir 0 dB
        // Trösklar som både findings (evaluate) och verktygsvägen (suggested_fixes) använder; ändra på ett ställe.
        constexpr int phaseMinActive = 4;               // aktiva 16-delar innan fasfyndet bedöms (golv)
        constexpr double phaseMinActiveShare = 0.15;    // ...och minst så stor andel av alla 16-delar, så en lång render inte blir känsligare än en kort
        constexpr float phaseActivePower = 1.0e-4f;     // en 16-del räknas först när kick och bas båda ligger över ungefär -40 dB (kicksvansen räknas inte som möte)
        constexpr float phaseCorrWarn = -0.3f, phaseCorrProblem = -0.6f;
        constexpr float phaseSumWarnDb = -3.0f, phaseSumProblemDb = -6.0f;
        constexpr float lowStereoInfo = 0.8f, lowStereoWarn = 0.5f, lowStereoProblem = 0.0f; // L/R-korrelation 20-120 Hz per takt: under 0,8 info, under 0,5 varning, under 0 problem
        constexpr double lowStereoMinShare = 0.25;      // ...och minst två takter eller så här stor andel av takterna
        constexpr double stereoSilentPower = 1.0e-5;    // bandeffekt per sampel under detta (ca -50 dB) räknas som tyst: korrelationen 1
        constexpr float limiterWarnDb = 6.0f, limiterProblemDb = 9.0f; // mäts på 95:e percentilen över ramarna, inte på enstaka maxblock
        constexpr double mudLowFloorPerFrame = 1.0e-4;  // medeleffekt 40-150 Hz i kick och bas tillsammans (-40 dB); under det bedöms inte lera
        // Kick mot bas i lågbandet, per 16-del (AN): kicken ska vinna där de möts, och svansen får inte äta basen.
        constexpr float kickLeadMarginDb = 3.0f;        // på slag: basen får ligga högst så här mycket under kicken innan kicken inte "vinner"
        constexpr float tailOverBassDb = 6.0f;          // mellan slag: kicksvansen så här mycket över basen = svansen tar basen
        constexpr double kickBassMinShare = 0.25;       // ...i minst två 16-delar och så här stor andel av de bedömda
        constexpr float kickBassActiveDb = -50.0f;      // en 16-del bedöms bara när både kick och bas ligger över detta (RMS)
        struct KickBassSummary
        {
            std::vector<int> leadBars, tailBars; // takter (1-baserade) där kicken inte vinner / där svansen tar basen
            int hitSlots = 0, tailSlots = 0, leadFails = 0, tailFails = 0;
            bool leadFound = false, tailFound = false;
        };

        KickBassSummary summariseKickBass (const PhaseMetrics& p)
        {
            KickBassSummary s;
            const auto slots = p.lowKickDb.size();
            if (p.lowBassDb.size() != slots || p.lowKickHit.size() != slots)
                return s;
            for (size_t i = 0; i < slots; ++i)
            {
                if (p.lowKickDb[i] < kickBassActiveDb || p.lowBassDb[i] < kickBassActiveDb)
                    continue;
                const auto diff = p.lowKickDb[i] - p.lowBassDb[i]; // positivt = kicken är starkare
                if (p.lowKickHit[i] != 0)
                {
                    ++s.hitSlots;
                    if (diff < kickLeadMarginDb)
                    {
                        ++s.leadFails;
                        s.leadBars.push_back ((int) i / 16 + 1);
                    }
                }
                else
                {
                    ++s.tailSlots;
                    if (diff > tailOverBassDb)
                    {
                        ++s.tailFails;
                        s.tailBars.push_back ((int) i / 16 + 1);
                    }
                }
            }
            s.leadBars.erase (std::unique (s.leadBars.begin(), s.leadBars.end()), s.leadBars.end());
            s.tailBars.erase (std::unique (s.tailBars.begin(), s.tailBars.end()), s.tailBars.end());
            s.leadFound = s.leadFails >= 2 && (double) s.leadFails >= kickBassMinShare * (double) s.hitSlots;
            s.tailFound = s.tailFails >= 2 && (double) s.tailFails >= kickBassMinShare * (double) s.tailSlots;
            return s;
        }

        // Fasfyndets underlag, gemensamt för findings och verktygsvägen: aktiva 16-delar och medelkorrelationen viktad med effekten (Σkb / Σ√(kk·bb)).
        struct PhaseSummary
        {
            int active = 0, minActive = phaseMinActive;
            float mean = 0.0f;
            std::vector<int> badBars;
            bool enough() const { return active >= minActive; }
        };

        PhaseSummary summarisePhase (const PhaseMetrics& p)
        {
            PhaseSummary s;
            const auto slots = p.lowCorrelation.size();
            const auto weighted = p.lowWeight.size() == slots;
            s.minActive = std::max (phaseMinActive, (int) std::ceil (phaseMinActiveShare * (double) slots));
            auto sumWeight = 0.0, sumWeighted = 0.0;
            for (size_t i = 0; i < slots; ++i)
            {
                if (p.lowActive[i] == 0 || (weighted && p.lowWeight[i] < phaseActivePower))
                    continue;
                const auto w = weighted ? (double) p.lowWeight[i] : 1.0;
                ++s.active;
                sumWeight += w;
                sumWeighted += w * p.lowCorrelation[i];
                if (p.lowCorrelation[i] < phaseCorrWarn)
                    s.badBars.push_back ((int) i / 16 + 1);
            }
            s.badBars.erase (std::unique (s.badBars.begin(), s.badBars.end()), s.badBars.end());
            s.mean = sumWeight > 0.0 ? (float) (sumWeighted / sumWeight) : 0.0f;
            return s;
        }

        // Basens stereobredd (band 0): lägsta nivån som tillräckligt många takter ligger under.
        struct StereoSummary
        {
            bool found = false;
            Severity severity = Severity::info;
            float limit = lowStereoInfo;
            std::vector<int> bars;
        };

        StereoSummary summariseStereo (const PhaseMetrics& p)
        {
            StereoSummary s;
            const auto total = p.stereoCorrelation.size();
            const auto enough = [total] (size_t n) { return n >= 2 || (double) n >= lowStereoMinShare * (double) total; };
            const struct { float limit; Severity severity; } levels[] = { { lowStereoProblem, Severity::problem }, { lowStereoWarn, Severity::warning }, { lowStereoInfo, Severity::info } };
            for (const auto& level : levels)
            {
                std::vector<int> bars;
                for (size_t bar = 0; bar < total; ++bar)
                    if (p.stereoCorrelation[bar][0] < level.limit)
                        bars.push_back ((int) bar + 1);
                if (! bars.empty() && enough (bars.size()))
                {
                    s.found = true;
                    s.severity = level.severity;
                    s.limit = level.limit;
                    s.bars = std::move (bars);
                    break;
                }
            }
            return s;
        }

        constexpr float lobePower = 1.5f;                 // en ren ton fördelas på ungefär tre steg (Hann): summan blir 1,5 gånger toppen

        juce::String noteName (float hz)
        {
            const auto note = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
            return juce::MidiMessage::getMidiNoteName (note, true, true, 5);
        }

        juce::String hzText (float hz) { return hz < 100.0f ? juce::String (hz, 1) + " Hz" : juce::String (juce::roundToInt (hz)) + " Hz"; }

        juce::String rangeText (float low, float high)
        {
            auto text = [] (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)); };
            return text (low) + "-" + text (high) + (high >= 1000.0f ? "" : " Hz");
        }

        juce::String barsText (std::vector<int> bars)
        {
            std::sort (bars.begin(), bars.end());
            bars.erase (std::unique (bars.begin(), bars.end()), bars.end());
            juce::String text;
            for (size_t i = 0; i < bars.size();)
            {
                size_t j = i;
                while (j + 1 < bars.size() && bars[j + 1] == bars[j] + 1)
                    ++j;
                if (text.isNotEmpty())
                    text << ", ";
                text << bars[i] << (j > i ? "-" + juce::String (bars[j]) : juce::String());
                i = j + 1;
            }
            return text;
        }

        // Bin -> band för en samplingsfrekvens.
        std::vector<int> binBands (double rate)
        {
            const auto& edges = bandEdges();
            std::vector<int> map ((size_t) fftSize / 2, -1);
            for (int b = 1; b < fftSize / 2; ++b)
            {
                const auto hz = (float) (b * rate / fftSize);
                for (int band = 0; band < numBands(); ++band)
                    if (hz >= edges[(size_t) band] && hz < edges[(size_t) band + 1])
                    {
                        map[(size_t) b] = band;
                        break;
                    }
            }
            return map;
        }

        // Frekvensen i en bit av en signal ur autokorrelationen (25-100 Hz): första toppen som är nästan lika hög som den högsta, så att
        // en multipel av perioden inte väljs. Tidsdomänen ger rätt värde även när svepet ännu rör sig lite, till skillnad från en topp i ett medelspektrum.
        float periodHz (const std::vector<float>& x, int from, int to, double rate)
        {
            const auto n = to - from;
            if (n < (int) (rate * 0.06))
                return 0.0f;
            std::vector<float> y ((size_t) n);
            auto mean = 0.0;
            for (int i = 0; i < n; ++i)
                mean += x[(size_t) (from + i)];
            mean /= n;
            auto energy = 0.0;
            for (int i = 0; i < n; ++i)
            {
                y[(size_t) i] = x[(size_t) (from + i)] - (float) mean;
                energy += (double) y[(size_t) i] * y[(size_t) i];
            }
            if (energy / n < 1.0e-8)
                return 0.0f;

            const auto minLag = (int) (rate / 100.0), maxLag = std::min ((int) (rate / 25.0), n / 2);
            if (maxLag - minLag < 4)
                return 0.0f;
            // Energierna i de två överlappande delarna ur kumulativa summor (förr räknades de om för varje fördröjning: tre gånger så mycket arbete).
            std::vector<double> cumulative ((size_t) n + 1, 0.0);
            for (int i = 0; i < n; ++i)
                cumulative[(size_t) i + 1] = cumulative[(size_t) i] + (double) y[(size_t) i] * y[(size_t) i];
            std::vector<double> r ((size_t) maxLag + 2, 0.0);
            for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
            {
                double dot = 0.0;
                for (int i = 0; i + lag < n; ++i)
                    dot += (double) y[(size_t) i] * y[(size_t) (i + lag)];
                const auto a = cumulative[(size_t) (n - lag)];                     // y[0 .. n-lag)
                const auto b = cumulative[(size_t) n] - cumulative[(size_t) lag]; // y[lag .. n)
                r[(size_t) lag] = dot / std::sqrt (a * b + 1.0e-20);
            }
            auto highest = 0.0;
            for (int lag = minLag; lag <= maxLag; ++lag)
                highest = std::max (highest, r[(size_t) lag]);
            if (highest < 0.5)
                return 0.0f;
            for (int lag = minLag; lag <= maxLag; ++lag)
                if (r[(size_t) lag] >= 0.9 * highest && r[(size_t) lag] >= r[(size_t) lag - 1] && r[(size_t) lag] >= r[(size_t) lag + 1])
                {
                    const auto a = r[(size_t) lag - 1], m = r[(size_t) lag], z = r[(size_t) lag + 1];
                    const auto denominator = a - 2.0 * m + z;
                    const auto delta = std::abs (denominator) > 1.0e-12 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - z) / denominator) : 0.0;
                    return (float) (rate / ((double) lag + delta));
                }
            return 0.0f;
        }

        // Grundtonen ur ett effektspektrum (de lägsta stegen): den lägsta toppen inom 10 dB från den starkaste mellan 28 och 400 Hz.
        float findFundamental (const float* power, double rate, float minDb)
        {
            const auto binHz = (float) (rate / fftSize);
            const auto first = std::max (2, (int) (28.0f / binHz)), last = std::min (lowBins - 2, (int) (400.0f / binHz));
            auto toDb = [&] (int b) { return 10.0f * std::log10 (power[b] + 1.0e-12f); };
            auto strongest = -200.0f;
            for (int b = first; b <= last; ++b)
                strongest = std::max (strongest, toDb (b));
            if (strongest < minDb)
                return 0.0f;
            for (int b = first; b <= last; ++b)
            {
                const auto level = toDb (b);
                if (level >= strongest - 10.0f && level >= toDb (b - 1) && level >= toDb (b + 1))
                {
                    const auto a = toDb (b - 1), c = toDb (b + 1);
                    const auto denominator = a - 2.0f * level + c;
                    const auto delta = std::abs (denominator) > 1.0e-3f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denominator) : 0.0f;
                    return ((float) b + delta) * binHz;
                }
            }
            return 0.0f;
        }
    }

    //==========================================================================================
    const std::vector<float>& bandEdges()
    {
        static const std::vector<float> edges { 20, 30, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000,
                                                2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500, 20000 };
        return edges;
    }

    int numBands() { return (int) bandEdges().size() - 1; }

    float& Capture::band (int source, int frame, int b) { return bandDb[((size_t) frame * numSources + (size_t) source) * (size_t) numBands() + (size_t) b]; }
    float Capture::band (int source, int frame, int b) const { return bandDb[((size_t) frame * numSources + (size_t) source) * (size_t) numBands() + (size_t) b]; }

    void Capture::resize (int frameCount)
    {
        frames = frameCount;
        bandDb.assign ((size_t) frameCount * numSources * (size_t) numBands(), -120.0f);
        lowPower.assign ((size_t) frameCount * numSources * lowBins, 0.0f);
        frameBar.assign ((size_t) frameCount, 0);
    }

    //==========================================================================================
    Report evaluate (const Capture& c, const Options& options, const Reference* reference)
    {
        Report r;
        r.sensitivity = juce::jlimit (0, 2, options.sensitivity);
        r.bars = c.bars;
        r.bpm = c.bpm;
        r.seconds = c.bars * 4.0 * 60.0 / std::max (1.0, c.bpm);
        r.referenceName = reference != nullptr ? reference->name : juce::String();
        if (c.frames < 6 || c.bars < 1)
        {
            r.error = "The loop was too short to analyse.";
            return r;
        }

        struct Sens { float margin, floorRange, relRange, minShare, maskMargin, kickRel; int minFrames; };
        static const Sens table[3] { { 3.0f, 35.0f, 18.0f, 0.7f, 10.0f, 12.0f, 5 },
                                     { 6.0f, 45.0f, 24.0f, 0.5f, 6.0f, 15.0f, 3 },
                                     { 9.0f, 55.0f, 30.0f, 0.35f, 4.0f, 18.0f, 2 } };
        const auto s = table[r.sensitivity];
        const auto nb = numBands(), frames = c.frames;
        const auto& edges = bandEdges();

        // Starkaste nivån i mixen (utan mastern) och varje källas egen starkaste.
        auto gmax = -120.0f;
        std::array<float, numSources> ownMax {};
        ownMax.fill (-120.0f);
        for (int f = 0; f < frames; ++f)
            for (int src = 0; src < numSources; ++src)
                if (c.present[(size_t) src])
                    for (int b = 0; b < nb; ++b)
                    {
                        const auto level = c.band (src, f, b);
                        ownMax[(size_t) src] = std::max (ownMax[(size_t) src], level);
                        if (src != masterSource)
                            gmax = std::max (gmax, level);
                    }
        if (gmax < -75.0f)
        {
            r.error = "Nothing plays in the loop. Check that the kick or an instrument is on and that the pattern has notes.";
            return r;
        }
        r.valid = true;
        const auto floorDb = std::max (gmax - s.floorRange, -90.0f);

        auto level = [&c] (int src, int f, int b) { return c.band (src, f, b); };
        auto active = [&] (int src, int f, int b)
        {
            const auto l = level (src, f, b);
            return l >= floorDb && l >= ownMax[(size_t) src] - s.relRange;
        };

        // Källornas grundtoner (medelvärde av de lägsta stegen över de ramar där källan spelar) och kickens svans.
        std::array<float, numSources> fundamental {};
        float kickTail = 0.0f;
        for (int src = 0; src < numSources; ++src)
        {
            if (! c.present[(size_t) src])
                continue;
            std::array<float, lowBins> sum {};
            auto used = 0;
            for (int f = 0; f < frames; ++f)
            {
                auto frameMax = -120.0f;
                for (int b = 0; b < nb; ++b)
                    frameMax = std::max (frameMax, level (src, f, b));
                if (frameMax < ownMax[(size_t) src] - 12.0f)
                    continue;
                const auto* low = &c.lowPower[((size_t) f * numSources + (size_t) src) * lowBins];
                for (int k = 0; k < lowBins; ++k)
                    sum[(size_t) k] += low[k];
                ++used;
            }
            if (used == 0)
                continue;
            for (auto& v : sum)
                v /= (float) used;
            fundamental[(size_t) src] = findFundamental (sum.data(), c.rate, std::max (floorDb - 12.0f, -90.0f));

            if (src == kickSource)
            {
                // Svansen: den starkaste toppen i kickens medelspektrum mellan 25 och 120 Hz.
                const auto binHz = (float) (c.rate / fftSize);
                auto best = 0.0f;
                auto bestBin = -1;
                for (int k = std::max (2, (int) (25.0f / binHz)); k <= std::min (lowBins - 2, (int) (120.0f / binHz)); ++k)
                    if (sum[(size_t) k] > best)
                    {
                        best = sum[(size_t) k];
                        bestBin = k;
                    }
                if (bestBin > 0)
                {
                    auto db = [&] (int k) { return 10.0f * std::log10 (sum[(size_t) k] + 1.0e-12f); };
                    const auto a = db (bestBin - 1), m = db (bestBin), z = db (bestBin + 1);
                    const auto denominator = a - 2.0f * m + z;
                    const auto delta = std::abs (denominator) > 1.0e-3f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - z) / denominator) : 0.0f;
                    kickTail = ((float) bestBin + delta) * binHz;
                }
            }
        }
        // Svansen mätt i tidsdomänen efter slagen är säkrare än toppen i medelspektrumet (som drar mot slagets svep och mot överton).
        if (c.kickTailMeasuredHz > 0.0f)
        {
            kickTail = c.kickTailMeasuredHz;
            r.kickTailFromTime = true;
        }
        r.kickTailHz = kickTail;
        r.kickTailDesignHz = c.kickTailDesignHz;

        for (int src = 0; src < numSources; ++src)
        {
            SourceInfo info;
            info.name = c.names[(size_t) src];
            info.present = c.present[(size_t) src];
            info.peakDb = ownMax[(size_t) src];
            if (! c.framePeakDb.empty())
            {
                auto powerSum = 0.0;
                for (int f = 0; f < frames; ++f)
                {
                    info.samplePeakDb = std::max (info.samplePeakDb, c.framePeakDb[(size_t) f * numSources + (size_t) src]);
                    powerSum += std::pow (10.0, 0.1 * c.frameRmsDb[(size_t) f * numSources + (size_t) src]);
                }
                info.rmsDb = frames > 0 ? (float) (10.0 * std::log10 (powerSum / frames + 1.0e-12)) : -120.0f;
            }
            info.fundamentalHz = src == kickSource ? kickTail : fundamental[(size_t) src];
            info.note = info.fundamentalHz > 0.0f ? noteName (info.fundamentalHz) : juce::String();
            r.sources.push_back (info);
        }

        // Hur mycket varje instrument duckas vid kickens slag, i medeltal.
        std::array<float, numInstruments> duck {};
        if (! c.duckDepthDb.empty())
        {
            for (const auto& onset : c.duckDepthDb)
                for (int i = 0; i < numInstruments; ++i)
                    duck[(size_t) i] += onset[(size_t) i];
            for (auto& d : duck)
                d /= (float) c.duckDepthDb.size();
        }

        auto severityFor = [] (float below, Severity cap)
        {
            const auto base = below < 12.0f ? Severity::problem : below < 24.0f ? Severity::warning : Severity::info;
            return (Severity) std::min ((int) base, (int) cap);
        };

        const auto bars = c.bars;
        struct Cell { int hit = 0, denomA = 0, denomB = 0; double weakSum = 0.0; };
        struct Group { int b0 = 0, b1 = 0; std::set<int> bars; double weakSum = 0.0; int hits = 0; };

        // Slår ihop intilliggande band som flaggats i ungefär samma takter till grupper.
        auto groupsOf = [&] (const std::vector<std::vector<Cell>>& cells, bool maskMode)
        {
            std::vector<Group> groups;
            Group current;
            auto open = false;
            auto gap = 0;
            auto close = [&] { if (open) groups.push_back (current); open = false; };
            for (int b = 0; b < nb; ++b)
            {
                std::set<int> flagged;
                double weakSum = 0.0;
                auto hits = 0;
                for (int bar = 0; bar < bars; ++bar)
                {
                    const auto& cell = cells[(size_t) b][(size_t) bar];
                    const auto denom = maskMode ? cell.denomA : std::min (cell.denomA, cell.denomB);
                    if (denom > 0 && cell.hit >= s.minFrames && (float) cell.hit >= s.minShare * (float) denom)
                    {
                        flagged.insert (bar + 1);
                        weakSum += cell.weakSum;
                        hits += cell.hit;
                    }
                }
                if (flagged.empty())
                {
                    if (++gap > 2) // tre tomma band i rad avslutar gruppen
                        close();
                    continue;
                }
                gap = 0;
                if (open)
                {
                    current.b1 = b;
                }
                else
                {
                    current = {};
                    current.b0 = current.b1 = b;
                    open = true;
                }
                current.bars.insert (flagged.begin(), flagged.end());
                current.weakSum += weakSum;
                current.hits += hits;
            }
            close();
            return groups;
        };

        // Hur två källors grundtoner förhåller sig: `away` = avstånd i halvtoner till närmaste "samma ton" (oktaver och unison räknas som samma).
        auto toneDistance = [&] (int a, int b) -> float
        {
            const auto fa = a == kickSource ? kickTail : fundamental[(size_t) a], fb = b == kickSource ? kickTail : fundamental[(size_t) b];
            if (fa <= 0.0f || fb <= 0.0f)
                return -1.0f;
            const auto semis = 12.0f * std::log2 (fa / fb);
            return std::abs (semis - 12.0f * std::round (semis / 12.0f));
        };

        auto tuningHint = [&] (int a, int b) -> juce::String
        {
            const auto away = toneDistance (a, b);
            if (away < 0.0f)
                return {};
            const auto fa = a == kickSource ? kickTail : fundamental[(size_t) a], fb = b == kickSource ? kickTail : fundamental[(size_t) b];
            const auto nameA = c.names[(size_t) a], nameB = c.names[(size_t) b];
            if (away >= 0.7f && away <= 1.4f)
                return " " + nameA + " plays " + noteName (fa) + " (" + hzText (fa) + ") and " + nameB + " " + noteName (fb) + " (" + hzText (fb)
                       + "): about " + juce::String (away, 1) + " semitones from the same note, which beats and sounds out of tune together. Retune one of them.";
            if (away < 0.5f)
                return " They are on the same note (" + noteName (fa) + " / " + noteName (fb) + ", octaves count), so they reinforce each other rather than clash.";
            return {};
        };

        auto duckHint = [&] (int instrument) -> juce::String
        {
            if (instrument < firstInstrument || instrument >= masterSource || c.duckDepthDb.empty())
                return {};
            const auto depth = duck[(size_t) (instrument - firstInstrument)];
            const auto name = c.names[(size_t) instrument];
            if (depth < 3.0f)
                return " " + name + " is barely ducked by the kick (" + juce::String (depth, 1) + " dB): raise its DUCK in the SIDECHAIN tab if it should get out of the way.";
            if (depth < 9.0f)
                return " " + name + " is ducked " + juce::String (depth, 1) + " dB at the kick; a deeper DUCK or a faster release in the SIDECHAIN tab gives the kick more room.";
            return " " + name + " is already ducked " + juce::String (depth, 1) + " dB, so what collides is mostly its tail after the kick: shorten the sidechain release or EQ it.";
        };

        // --- Krockar mellan två källor.
        for (int i = 0; i < masterSource; ++i)
        {
            for (int j = i + 1; j < masterSource; ++j)
            {
                if (! c.present[(size_t) i] || ! c.present[(size_t) j])
                    continue;

                std::vector<std::vector<Cell>> cells ((size_t) nb, std::vector<Cell> ((size_t) bars));
                for (int f = 0; f < frames; ++f)
                {
                    const auto bar = c.frameBar[(size_t) f];
                    if (bar < 0 || bar >= bars)
                        continue;
                    for (int b = 0; b < nb; ++b)
                    {
                        auto& cell = cells[(size_t) b][(size_t) bar];
                        const auto a = active (i, f, b), z = active (j, f, b);
                        cell.denomA += a ? 1 : 0;
                        cell.denomB += z ? 1 : 0;
                        if (a && z && std::abs (level (i, f, b) - level (j, f, b)) <= s.margin)
                        {
                            ++cell.hit;
                            cell.weakSum += std::min (level (i, f, b), level (j, f, b));
                        }
                    }
                }

                for (const auto& group : groupsOf (cells, false))
                {
                    const auto low = edges[(size_t) group.b0], high = edges[(size_t) group.b1 + 1];
                    const auto weakDb = (float) (group.weakSum / std::max (1, group.hits));
                    const auto below = gmax - weakDb;
                    const auto withKick = i == kickSource;
                    auto cap = Severity::problem;
                    if (withKick)
                        cap = low >= 250.0f ? Severity::info : Severity::warning;
                    else if (low >= 400.0f)
                        cap = Severity::info;

                    Finding f;
                    const auto away = toneDistance (i, j);
                    if (! withKick && away >= 0.0f && away < 0.5f)
                        cap = Severity::info; // samma ton (unison eller oktav): lager på lager med flit
                    f.severity = severityFor (below, cap);
                    if (away >= 0.7f && away <= 1.4f && (int) f.severity < (int) Severity::warning)
                        f.severity = Severity::warning; // nästan samma ton men inte riktigt: skorrar
                    f.category = "collision";
                    f.sources = { c.names[(size_t) i], c.names[(size_t) j] };
                    f.lowHz = low;
                    f.highHz = high;
                    f.share = (float) group.bars.size() / (float) bars;
                    f.bars.assign (group.bars.begin(), group.bars.end());
                    f.title = c.names[(size_t) i] + " and " + c.names[(size_t) j] + " collide at " + rangeText (low, high);
                    f.detail = "Both play at similar loudness (within " + juce::String (juce::roundToInt (s.margin)) + " dB) at the same time in "
                             + juce::String ((int) group.bars.size()) + " of " + juce::String (bars) + " bars (bars " + barsText (f.bars) + "). The weaker of them is about "
                             + juce::String (juce::roundToInt (below)) + " dB below the loudest part of the mix.";
                    if (withKick)
                        f.detail += " Kick and instrument may share frequencies when the sidechain gives the kick the hit and the instrument the gaps, so it is only a problem if the kick gets buried or the low end turns muddy." + duckHint (j) + tuningHint (i, j);
                    else
                        f.detail += tuningHint (i, j) + " Give them different registers (octave or tune), EQ one of them (ANALYZER > SPECTRUM > EQ), or duck one under the other (EFFECTS tab: a SIDECHAIN effect with one as SOURCE, and send the other 100 % to that FX channel).";
                    r.findings.push_back (std::move (f));
                }
            }
        }

        // --- En instrument som skymmer kicken där kicken har sin energi.
        if (c.present[(size_t) kickSource])
        {
            for (int j = firstInstrument; j < masterSource; ++j)
            {
                if (! c.present[(size_t) j])
                    continue;
                std::vector<std::vector<Cell>> cells ((size_t) nb, std::vector<Cell> ((size_t) bars));
                for (int f = 0; f < frames; ++f)
                {
                    const auto bar = c.frameBar[(size_t) f];
                    if (bar < 0 || bar >= bars)
                        continue;
                    for (int b = 0; b < nb; ++b)
                    {
                        const auto lk = level (kickSource, f, b), lj = level (j, f, b);
                        if (lk < std::max (floorDb, ownMax[(size_t) kickSource] - s.kickRel))
                            continue; // kicken spelar inte här (eller är inte viktig i det här bandet)
                        auto& cell = cells[(size_t) b][(size_t) bar];
                        ++cell.denomA;
                        if (lj >= floorDb && lj - lk > s.maskMargin)
                        {
                            ++cell.hit;
                            cell.weakSum += lk;
                        }
                    }
                }
                for (const auto& group : groupsOf (cells, true))
                {
                    const auto low = edges[(size_t) group.b0], high = edges[(size_t) group.b1 + 1];
                    const auto kickDb = (float) (group.weakSum / std::max (1, group.hits));
                    Finding f;
                    f.severity = (gmax - kickDb) < 25.0f ? Severity::problem : Severity::warning;
                    f.category = "masking";
                    f.sources = { c.names[(size_t) j], c.names[(size_t) kickSource] };
                    f.lowHz = low;
                    f.highHz = high;
                    f.share = (float) group.bars.size() / (float) bars;
                    f.bars.assign (group.bars.begin(), group.bars.end());
                    f.title = c.names[(size_t) j] + " covers the kick at " + rangeText (low, high);
                    f.detail = "When the kick plays, " + c.names[(size_t) j] + " is more than " + juce::String (juce::roundToInt (s.maskMargin)) + " dB louder than the kick here in "
                             + juce::String ((int) group.bars.size()) + " of " + juce::String (bars) + " bars (bars " + barsText (f.bars) + "), so the kick loses punch."
                             + duckHint (j) + tuningHint (j, kickSource) + " You can also lower " + c.names[(size_t) j] + " or cut it here with the EQ (ANALYZER > SPECTRUM > EQ).";
                    r.findings.push_back (std::move (f));
                }
            }
        }

        // --- Kickens svans: för låg eller för hög.
        if (c.present[(size_t) kickSource] && kickTail > 0.0f)
        {
            if (kickTail < 30.0f)
            {
                Finding f;
                f.severity = Severity::warning;
                f.category = "kick";
                f.sources = { c.names[(size_t) kickSource] };
                f.lowHz = f.highHz = kickTail;
                f.title = "The kick's tail is very low (" + hzText (kickTail) + ")";
                f.detail = "A tail near " + noteName (kickTail) + " is inaudible on many speakers and uses a lot of headroom. Tune the kick up (KICK tab, TUNE) or raise the key note so the tail lands around 40-55 Hz.";
                r.findings.push_back (std::move (f));
            }
            else if (kickTail > 62.0f)
            {
                Finding f;
                f.severity = Severity::warning;
                f.category = "kick";
                f.sources = { c.names[(size_t) kickSource] };
                f.lowHz = f.highHz = kickTail;
                f.title = "The kick's tail is high (" + hzText (kickTail) + ")";
                f.detail = "A tail near " + noteName (kickTail) + " sounds thin and sits in the bass's register, which makes the two collide. Tune the kick down (KICK tab, TUNE) towards 40-55 Hz, or move the bass.";
                r.findings.push_back (std::move (f));
            }
        }

        // --- Rumlande djupbas under 30 Hz (inte kicken: dess svans kan ligga där med flit).
        for (int src = firstInstrument; src < numSources; ++src)
        {
            if (! c.present[(size_t) src])
                continue;
            auto count = 0, activeFrames = 0;
            std::vector<int> barList;
            for (int f = 0; f < frames; ++f)
            {
                const auto bar = c.frameBar[(size_t) f];
                if (bar < 0 || bar >= bars)
                    continue;
                if (level (src, f, 0) >= floorDb)
                    ++activeFrames; // spelar något i basbandet
                if (level (src, f, 0) >= std::max (floorDb, ownMax[(size_t) src] - 6.0f))
                {
                    ++count;
                    barList.push_back (bar + 1);
                }
            }
            if (count >= 2 * s.minFrames && (float) count >= 0.3f * (float) std::max (1, activeFrames))
            {
                Finding f;
                f.severity = Severity::warning;
                f.category = "sub";
                f.sources = { c.names[(size_t) src] };
                f.lowHz = edges[0];
                f.highHz = edges[1];
                f.bars = barList;
                f.share = (float) std::set<int> (barList.begin(), barList.end()).size() / (float) bars;
                f.title = c.names[(size_t) src] + " has strong energy below 30 Hz";
                f.detail = "Energy that low is hard to hear, rumbles on large systems and eats headroom (bars " + barsText (barList) + "). "
                           + (src == masterSource ? "Check which source it comes from, and high-pass it (ANALYZER > SPECTRUM > EQ, HP about 30 Hz)."
                                                  : "Raise its octave or tune, or high-pass it with the EQ (ANALYZER > SPECTRUM > EQ, HP 30-40 Hz).");
                r.findings.push_back (std::move (f));
            }
        }

        // --- Mastern: limitern.
        r.limiterMaxDb = c.limiterMaxReductionDb;
        auto limiterLevelDb = c.limiterMaxReductionDb; // utan ramvärden (äldre capture) faller vi tillbaka på maxet
        if (! c.frameLimiterDb.empty())
        {
            auto sorted = c.frameLimiterDb;
            std::sort (sorted.begin(), sorted.end());
            limiterLevelDb = sorted[(size_t) std::max (0, (int) std::ceil (0.95 * (double) sorted.size()) - 1)];
        }
        if (limiterLevelDb >= limiterWarnDb)
        {
            Finding f;
            f.severity = limiterLevelDb >= limiterProblemDb ? Severity::problem : Severity::warning;
            f.category = "master";
            f.sources = { c.names[(size_t) masterSource] };
            f.title = "The master limiter works hard (" + juce::String (limiterLevelDb, 1) + " dB in the loudest 5 % of the time, up to " + juce::String (c.limiterMaxReductionDb, 1) + " dB)";
            f.detail = "Kick and instruments together are louder than the master's ceiling, so the limiter squashes the peaks (usually the kick). Lower levels in the MIXER tab or the loudest instruments instead of letting the limiter do it. A single peak does not trigger this; the 95th percentile over the frames decides.";
            r.findings.push_back (std::move (f));
        }

        // --- Fas och stereo (AN steg 2).
        if (const auto& ph = c.phase; ph.valid)
        {
            const auto summary = summarisePhase (ph);
            const auto activeCount = summary.active;
            const auto mean = summary.mean;
            const auto& badBars = summary.badBars;
            if (summary.enough() && (mean < phaseCorrWarn || ph.sumVsMaxDb < phaseSumWarnDb))
            {
                Finding f;
                f.severity = mean < phaseCorrProblem || ph.sumVsMaxDb < phaseSumProblemDb ? Severity::problem : Severity::warning;
                f.category = "phase";
                f.sources = { "Kick", "Instruments" };
                f.lowHz = 30.0f;
                f.highHz = 150.0f;
                f.bars = badBars;
                f.share = (float) activeCount / (float) std::max<size_t> (1, ph.lowCorrelation.size());
                f.title = "Kick and bass partly cancel each other at 30-150 Hz (correlation " + juce::String (mean, 2) + ")";
                f.detail = "The sum of kick and bass peaks " + juce::String (ph.sumVsMaxDb, 1) + " dB against the loudest one alone. Their waves run in opposite directions, so the low end gets thinner. "
                           "Try another phase or RAND off on the bass, shift the bass note a little in time, or change the kick's start phase. "
                           "Or read the bass wave with analyse_wavetable and shift its fundamental's phase, or remove the fundamental where it overlaps the kick, with edit_wavetable_harmonics.";
                r.findings.push_back (std::move (f));
            }
            if (activeCount >= 4 && ph.sumVsMaxDb > 3.0f)
            {
                Finding f;
                f.severity = ph.sumVsMaxDb > 6.0f ? Severity::warning : Severity::info;
                f.category = "phase";
                f.sources = { "Kick", "Instruments" };
                f.lowHz = 30.0f;
                f.highHz = 150.0f;
                f.share = (float) activeCount / (float) std::max<size_t> (1, ph.lowCorrelation.size());
                f.title = "Kick and bass add up " + juce::String (ph.sumVsMaxDb, 1) + " dB above the loudest one at 30-150 Hz";
                f.detail = "Where they play together the low end gets louder than either alone, which eats headroom and can distort the master. "
                           "Lower the bass velocity (or its level) by about " + juce::String (ph.sumVsMaxDb, 1) + " dB (a factor of " + juce::String (juce::Decibels::decibelsToGain (-ph.sumVsMaxDb), 2) + "), "
                           "or remove the bass fundamental where it overlaps the kick with edit_wavetable_harmonics.";
                r.findings.push_back (std::move (f));
            }
            const auto kb = summariseKickBass (ph);
            if (kb.leadFound)
            {
                Finding f;
                f.severity = Severity::warning;
                f.category = "kickbass";
                f.sources = { "Kick", "Instruments" };
                f.lowHz = 30.0f;
                f.highHz = 150.0f;
                f.bars = kb.leadBars;
                f.share = (float) kb.leadFails / (float) std::max (1, kb.hitSlots);
                f.title = "The bass is as loud as the kick where they meet (" + juce::String (kb.leadFails) + " of " + juce::String (kb.hitSlots) + " hits)";
                f.detail = "On the kick's hits the bass should be at least " + juce::String (kickLeadMarginDb, 0) + " dB under the kick at 30-150 Hz, so the kick wins the downbeat. Lower the bass level or deepen the sidechain depth on the bass, bars " + barsText (kb.leadBars) + ".";
                r.findings.push_back (std::move (f));
            }
            if (kb.tailFound)
            {
                Finding f;
                f.severity = Severity::info;
                f.category = "kickbass";
                f.sources = { "Kick", "Instruments" };
                f.lowHz = 30.0f;
                f.highHz = 150.0f;
                f.bars = kb.tailBars;
                f.share = (float) kb.tailFails / (float) std::max (1, kb.tailSlots);
                f.title = "The kick tail takes the bass (" + juce::String (kb.tailFails) + " of " + juce::String (kb.tailSlots) + " steps between hits)";
                f.detail = "Between hits the kick tail is more than " + juce::String (tailOverBassDb, 0) + " dB louder than the bass at 30-150 Hz, so the bass is hardly heard, bars " + barsText (kb.tailBars) + ". Shorten the kick's decay or release, or lift the bass between hits (shorter sidechain release).";
                r.findings.push_back (std::move (f));
            }
            if (ph.clickHits > 0)
            {
                Finding f;
                f.severity = Severity::warning;
                f.category = "phase";
                f.sources = { "Kick" };
                f.share = (float) ph.clickHits / (float) std::max<size_t> (1, ph.hits.size());
                f.title = "The kick may click when it is cut off (" + juce::String (ph.clickHits) + " of " + juce::String ((int) ph.hits.size()) + " hits)";
                f.detail = "The kick is not close to zero when the next hit or its end arrives (above -60 dB), which clicks. Shorten the decay or lengthen the release in AMP ENV.";
                r.findings.push_back (std::move (f));
            }
            if (ph.peakSpreadDb > 1.0f && ph.hits.size() >= 2)
            {
                Finding f;
                f.severity = Severity::info;
                f.category = "phase";
                f.sources = { "Kick" };
                f.title = "The kick's peak level varies " + juce::String (ph.peakSpreadDb, 1) + " dB between hits";
                f.detail = "Often a random phase or an LFO that hits the kick differently each time. Fixed phase gives equally hard hits.";
                r.findings.push_back (std::move (f));
            }
            const auto stereo = summariseStereo (ph);
            const auto& wideBars = stereo.bars;
            if (stereo.found)
            {
                Finding f;
                f.severity = stereo.severity;
                f.category = "stereo";
                f.sources = { c.names[(size_t) masterSource] };
                f.lowHz = 20.0f;
                f.highHz = 120.0f;
                f.bars = wideBars;
                f.share = (float) wideBars.size() / (float) std::max (1, bars);
                f.title = "The low end below 120 Hz is wide or out of phase between left and right";
                f.detail = "Left/right correlation is below " + juce::String (stereo.limit, 1) + " in bars " + barsText (wideBars) + ". Wide bass disappears in mono (clubs, phones). Keep bass and kick centred, for example with a lower STEREO/UNISON width.";
                r.findings.push_back (std::move (f));
            }
        }

        // --- Lärdomar från kick+bas-arbetet (plan-v2): RAND-fas på en bas, och för mycket runt 200 Hz.
        {
            auto lowShare = [&] (int src, float fromHz, float toHz)
            {
                auto part = 0.0, all = 0.0;
                for (int f = 0; f < frames; ++f)
                    for (int b = 0; b < nb; ++b)
                    {
                        const auto power = std::pow (10.0, level (src, f, b) / 10.0);
                        all += power;
                        if (edges[(size_t) b] >= fromHz && edges[(size_t) b + 1] <= toHz)
                            part += power;
                    }
                return all > 0.0 ? (float) (part / all) : 0.0f;
            };
            for (int i = 0; i < numInstruments; ++i)
            {
                const auto src = firstInstrument + i;
                if (! c.present[(size_t) src] || c.randomPhase[(size_t) i] < 0.1f || lowShare (src, 0.0f, 150.0f) < 0.5f)
                    continue;
                Finding f;
                f.severity = Severity::warning;
                f.category = "phase";
                f.sources = { c.names[(size_t) src] };
                f.lowHz = 30.0f;
                f.highHz = 150.0f;
                f.title = c.names[(size_t) src] + " is a bass with random phase (RAND " + juce::String (juce::roundToInt (c.randomPhase[(size_t) i] * 100.0f)) + "%)";
                f.detail = "Each note starts at a random point in the wave, so the bass meets the kick differently every time and sometimes cancels it. Set RAND to 0 on a bass (OSC > RAND) and use a fixed PHASE instead.";
                r.findings.push_back (std::move (f));
            }
            // Lera mäts på kicken och på de instrument som är en bas (minst hälften av energin under 150 Hz), inte på mastern: ett pad eller en lead runt 200 Hz är inte kick-och-bas-lera.
            std::vector<int> lowSources;
            if (c.present[(size_t) kickSource])
                lowSources.push_back (kickSource);
            for (int i = 0; i < numInstruments; ++i)
                if (c.present[(size_t) (firstInstrument + i)] && lowShare (firstInstrument + i, 0.0f, 150.0f) >= 0.5f)
                    lowSources.push_back (firstInstrument + i);
            if (! lowSources.empty())
            {
                auto bandPower = [&] (float fromHz, float toHz)
                {
                    auto sum = 0.0;
                    for (const auto src : lowSources)
                        for (int f = 0; f < frames; ++f)
                            for (int b = 0; b < nb; ++b)
                                if (edges[(size_t) b] >= fromHz && edges[(size_t) b + 1] <= toHz)
                                    sum += std::pow (10.0, level (src, f, b) / 10.0);
                    return sum;
                };
                const auto low = bandPower (40.0f, 150.0f), mud = bandPower (160.0f, 250.0f);
                if (low > mudLowFloorPerFrame * frames && mud > 0.6 * low)
                {
                    Finding f;
                    f.severity = Severity::warning;
                    f.category = "mud";
                    for (const auto src : lowSources)
                        f.sources.add (c.names[(size_t) src]);
                    f.lowHz = 160.0f;
                    f.highHz = 250.0f;
                    f.share = (float) std::min (1.0, mud / low);
                    f.title = "A lot of energy around 200 Hz in kick and bass (" + juce::String (10.0 * std::log10 (mud / low), 1) + " dB against 40-150 Hz)";
                    f.detail = "The range around 200 Hz makes the low end boxy and takes room from the kick's punch. Cut it a few dB with the EQ on the bass (ANALYZER > SPECTRUM > EQ, bell about 200 Hz) or high-pass the layers that do not need it.";
                    r.findings.push_back (std::move (f));
                }
            }
        }

        // --- Mot en referenslåt: balansen mellan band, med nivån anpassad över 120 Hz - 5 kHz.
        if (reference != nullptr && c.present[(size_t) masterSource])
        {
            const auto map = binBands (reference->rate);
            std::vector<double> refPower ((size_t) nb, 0.0);
            for (int b = 1; b < fftSize / 2; ++b)
                if (map[(size_t) b] >= 0)
                    refPower[(size_t) map[(size_t) b]] += (double) reference->power[(size_t) b] / lobePower;

            std::vector<double> masterPower ((size_t) nb, 0.0);
            auto used = 0;
            for (int f = 0; f < frames; ++f)
            {
                auto frameMax = -120.0f;
                for (int b = 0; b < nb; ++b)
                    frameMax = std::max (frameMax, level (masterSource, f, b));
                if (frameMax < ownMax[(size_t) masterSource] - 30.0f)
                    continue;
                for (int b = 0; b < nb; ++b)
                    masterPower[(size_t) b] += std::pow (10.0, level (masterSource, f, b) / 10.0);
                ++used;
            }
            if (used > 0)
            {
                std::vector<float> diff ((size_t) nb, 0.0f);
                auto offset = 0.0, offsetCount = 0.0;
                for (int b = 0; b < nb; ++b)
                {
                    const auto m = 10.0 * std::log10 (masterPower[(size_t) b] / used + 1.0e-12), q = 10.0 * std::log10 (refPower[(size_t) b] + 1.0e-12);
                    diff[(size_t) b] = (float) (m - q);
                    if (edges[(size_t) b] >= 120.0f && edges[(size_t) b + 1] <= 5400.0f)
                    {
                        offset += m - q;
                        offsetCount += 1.0;
                    }
                }
                offset /= std::max (1.0, offsetCount);
                const float threshold = r.sensitivity == 0 ? 6.0f : r.sensitivity == 1 ? 4.5f : 3.0f;
                for (int b = 1; b < nb && edges[(size_t) b + 1] <= 16000.0f;)
                {
                    const auto d = diff[(size_t) b] - (float) offset;
                    if (std::abs (d) < threshold)
                    {
                        ++b;
                        continue;
                    }
                    auto end = b;
                    auto sum = d;
                    auto count = 1;
                    while (end + 1 < nb && edges[(size_t) end + 2] <= 16000.0f)
                    {
                        const auto next = diff[(size_t) end + 1] - (float) offset;
                        if (std::abs (next) < threshold || (next > 0.0f) != (d > 0.0f))
                            break;
                        ++end;
                        sum += next;
                        ++count;
                    }
                    const auto average = sum / (float) count;
                    Finding f;
                    f.severity = std::abs (average) >= 8.0f ? Severity::warning : Severity::info;
                    f.category = "reference";
                    f.sources = { c.names[(size_t) masterSource], "Reference" };
                    f.lowHz = edges[(size_t) b];
                    f.highHz = edges[(size_t) end + 1];
                    f.title = "Master has " + juce::String (std::abs (average), 1) + " dB " + (average > 0.0f ? "more" : "less") + " at " + rangeText (f.lowHz, f.highHz) + " than the reference";
                    f.detail = "Compared with " + reference->name + " with the levels matched over 120 Hz-5 kHz. It is a hint, not an error: a different style or loop can differ on purpose. "
                               + (average > 0.0f ? "To match it, cut here with the EQ (ANALYZER > SPECTRUM > EQ) on the instrument that fills this range, or on the master."
                                                 : "To match it, boost here with the EQ (ANALYZER > SPECTRUM > EQ), or give an instrument more energy in this range.");
                    r.findings.push_back (std::move (f));
                    b = end + 1;
                }
            }
        }

        std::stable_sort (r.findings.begin(), r.findings.end(), [] (const Finding& a, const Finding& b)
        {
            if (a.severity != b.severity)
                return (int) a.severity > (int) b.severity;
            return a.share > b.share;
        });
        // Alla problem och varningar (högst tolv) och några få upplysningar.
        {
            std::vector<Finding> kept;
            auto infos = 0;
            for (auto& finding : r.findings)
            {
                if (finding.severity == Severity::info && ++infos > 4)
                    continue;
                if (kept.size() < 16)
                    kept.push_back (std::move (finding));
            }
            r.findings = std::move(kept);
        }
        return r;
    }

    //==========================================================================================
    PhaseDescription describePhase (const PhaseMetrics& p)
    {
        PhaseDescription d;
        if (! p.valid)
            return d;

        auto round2 = [] (float v) { return std::round (v * 100.0) / 100.0; };
        auto barArray = [] (const std::vector<int>& bars)
        {
            juce::Array<juce::var> a;
            for (auto b : bars)
                a.add (b);
            return a;
        };
        auto addFinding = [&] (const juce::String& id, const char* severity, const juce::String& what, double value, const std::vector<int>& bars)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", id);
            o->setProperty ("severity", juce::String (severity));
            o->setProperty ("what", what);
            o->setProperty ("value", value);
            o->setProperty ("bars", barArray (bars));
            d.findings.add (juce::var (o));
        };
        // Förslagen är alltid kandidater att jämföra (A/B): inget ändras utan att användaren godkänner.
        auto addFix = [&] (const juce::String& finding, const juce::String& action, const juce::String& target, const juce::String& why)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("finding", finding);
            o->setProperty ("action", action);
            o->setProperty ("target", target);
            o->setProperty ("why", why);
            o->setProperty ("candidate", true);
            o->setProperty ("needs_approval", true);
            d.fixes.add (juce::var (o));
        };

        const auto summary = summarisePhase (p);
        const auto mean = summary.mean;
        const auto& badBars = summary.badBars;

        if (summary.enough() && (mean < phaseCorrWarn || p.sumVsMaxDb < phaseSumWarnDb))
        {
            addFinding ("kick_bass_cancel", mean < phaseCorrProblem || p.sumVsMaxDb < phaseSumProblemDb ? "problem" : "warning",
                        "Kick and bass partly cancel at 30-150 Hz (mean correlation, value = correlation)", round2 (mean), badBars);
            addFix ("kick_bass_cancel", "invert_polarity", "bass layer (OSC phase +180 deg)", "Turns an opposing wave round; try it when the mean correlation is below -0.6.");
            addFix ("kick_bass_cancel", "fixed_phase", "bass OSC phase / RAND off", "A fixed start phase makes every note meet the kick the same way.");
            addFix ("kick_bass_cancel", "shift_bass_in_time", "bass notes (a few ms later)", "Moves the bass peak away from the kick's.");
            addFix ("kick_bass_cancel", "kick_start_phase", "kick start phase", "Alternative to changing the bass: change where the kick's wave starts.");
        }

        auto flips = 0;
        for (size_t i = 1; i < p.hits.size(); ++i)
            if (p.hits[i].polarity != 0 && p.hits[i - 1].polarity != 0 && p.hits[i].polarity != p.hits[i - 1].polarity)
                ++flips;
        if (flips > 0)
        {
            addFinding ("kick_polarity_varies", "warning", "The kick's first swing changes direction between hits (value = number of changes)", flips, {});
            addFix ("kick_polarity_varies", "fixed_phase", "kick start phase / RAND off", "A kick that starts in different directions meets the bass differently every hit.");
        }

        const auto kb = summariseKickBass (p);
        if (kb.leadFound)
        {
            addFinding ("kick_bass_lead", "warning", "The bass is within 3 dB of the kick on the hits (value = number of hits)", kb.leadFails, kb.leadBars);
            addFix ("kick_bass_lead", "lower_bass_level", "bass level / sidechain depth on the bass", "The kick should win the downbeat at 30-150 Hz.");
        }
        if (kb.tailFound)
        {
            addFinding ("kick_tail_takes_bass", "info", "The kick tail is over 6 dB above the bass between hits (value = number of steps)", kb.tailFails, kb.tailBars);
            addFix ("kick_tail_takes_bass", "shorten_decay", "kick AMP ENV decay / release, or sidechain release", "A long tail covers the bass between the hits.");
        }

        if (p.clickHits > 0)
        {
            addFinding ("kick_click", "warning", "The kick ends above -60 dB (value = number of hits)", p.clickHits, {});
            addFix ("kick_click", "shorten_decay", "kick AMP ENV decay / release", "The kick should be near zero before the next hit.");
        }

        if (p.peakSpreadDb > 1.0f && p.hits.size() >= 2)
        {
            addFinding ("kick_peak_spread", "info", "The kick's peak level varies between hits (value = dB spread)", round2 (p.peakSpreadDb), {});
            addFix ("kick_peak_spread", "fixed_phase", "kick start phase / LFO retrigger", "Equal start gives equally hard hits.");
        }

        const auto stereo = summariseStereo (p);
        const auto& wideBars = stereo.bars;
        if (stereo.found)
        {
            addFinding ("low_end_wide", stereo.severity == Severity::problem ? "problem" : stereo.severity == Severity::warning ? "warning" : "info",
                        "L/R correlation at 20-120 Hz is under " + juce::String (stereo.limit, 1) + " (value = number of bars)", (int) wideBars.size(), wideBars);
            addFix ("low_end_wide", "narrow_low_end", "bass STEREO/UNISON width, or a mono-maker below 120 Hz", "Wide bass disappears in mono.");
        }
        return d;
    }

    namespace
    {
        // lowband.* : kick/bas-korrelationen per 16-del, summan mot största toppen, slagens topp/klick och stereomåtten per takt.
        juce::var phaseVar (const PhaseMetrics& p)
        {
            auto round2 = [] (float v) { return std::round (v * 100.0) / 100.0; };
            auto* o = new juce::DynamicObject();
            juce::Array<juce::var> correlation, sumDiff, hits, stereo;
            for (size_t i = 0; i < p.lowCorrelation.size(); ++i)
            {
                correlation.add (p.lowActive[i] != 0 ? juce::var (round2 (p.lowCorrelation[i])) : juce::var()); // null = kick eller bas är tyst
                sumDiff.add (p.lowActive[i] != 0 ? juce::var (round2 (p.lowSumDiffDb[i])) : juce::var());
            }
            o->setProperty ("correlation", correlation);
            o->setProperty ("sum_diff_db", sumDiff);
            o->setProperty ("sum_vs_max_db", round2 (p.sumVsMaxDb));
            for (const auto& h : p.hits)
            {
                auto* e = new juce::DynamicObject();
                e->setProperty ("peak_db", round2 (h.peakDb));
                e->setProperty ("polarity", h.polarity);
                e->setProperty ("end_db", round2 (h.endDb));
                e->setProperty ("click", h.endDb > -60.0f);
                hits.add (juce::var (e));
            }
            o->setProperty ("kick_hits", hits);
            o->setProperty ("kick_peak_spread_db", round2 (p.peakSpreadDb));
            o->setProperty ("click_hits", p.clickHits);
            for (size_t bar = 0; bar < p.stereoCorrelation.size(); ++bar)
            {
                auto* e = new juce::DynamicObject();
                juce::Array<juce::var> corr, ms;
                for (int b = 0; b < PhaseMetrics::stereoBands; ++b)
                {
                    corr.add (round2 (p.stereoCorrelation[bar][(size_t) b]));
                    ms.add (round2 (p.midSideDb[bar][(size_t) b]));
                }
                e->setProperty ("bar", (int) bar + 1);
                e->setProperty ("lr_correlation", corr);
                e->setProperty ("mid_side_db", ms);
                stereo.add (juce::var (e));
            }
            o->setProperty ("stereo_bands_hz", juce::StringArray ({ "<120", "120-500", "500-4000", ">4000" }).joinIntoString (","));
            o->setProperty ("stereo_per_bar", stereo);
            o->setProperty ("band_hz", "30-150");
            const auto structured = describePhase (p);
            o->setProperty ("phase_findings", structured.findings);
            o->setProperty ("suggested_fixes", structured.fixes);
            return juce::var (o);
        }
    }

    juce::var Report::toVar() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("ok", valid);
        if (error.isNotEmpty())
            root->setProperty ("error", error);
        root->setProperty ("bars_analysed", bars);
        root->setProperty ("bpm", bpm);
        root->setProperty ("sensitivity", sensitivity == 0 ? "low" : sensitivity == 1 ? "normal" : "high");
        root->setProperty ("kick_tail_hz", kickTailHz);
        if (kickTailDesignHz > 0.0f)
            root->setProperty ("kick_tail_design_hz", kickTailDesignHz);
        root->setProperty ("kick_tail_measured_in", kickTailFromTime ? "time domain, 120-340 ms after each hit (median)" : "average spectrum");
        root->setProperty ("limiter_max_reduction_db", limiterMaxDb);
        if (referenceName.isNotEmpty())
            root->setProperty ("reference", referenceName);
        if (capture != nullptr && capture->phase.valid)
            root->setProperty ("lowband", phaseVar (capture->phase));

        juce::Array<juce::var> sourceList;
        for (const auto& source : sources)
        {
            if (! source.present)
                continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", source.name);
            o->setProperty ("peak_band_db", source.peakDb);
            o->setProperty ("sample_peak_db", source.samplePeakDb);
            o->setProperty ("rms_db", source.rmsDb);
            if (source.fundamentalHz > 0.0f)
            {
                o->setProperty ("fundamental_hz", source.fundamentalHz);
                o->setProperty ("note", source.note);
            }
            sourceList.add (juce::var (o));
        }
        root->setProperty ("sources", sourceList);

        if (! afterMaster.empty())
        {
            juce::Array<juce::var> shares;
            for (const auto& share : afterMaster)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("name", share.name);
                o->setProperty ("heard_rms_db", share.heardRmsDb);
                o->setProperty ("after_master_rms_db", share.masterRmsDb);
                o->setProperty ("heard_peak_db", share.heardPeakDb);
                o->setProperty ("after_master_peak_db", share.masterPeakDb);
                juce::Array<juce::var> heardBands, masterBands, edges;
                for (int b = 0; b < shareBands; ++b)
                {
                    heardBands.add (share.heardBandDb[(size_t) b]);
                    masterBands.add (share.masterBandDb[(size_t) b]);
                    edges.add (shareBandName (b));
                }
                o->setProperty ("bands_hz", edges);
                o->setProperty ("heard_band_rms_db", heardBands);
                o->setProperty ("after_master_band_rms_db", masterBands);
                shares.add (juce::var (o));
            }
            root->setProperty ("after_master", shares);
            root->setProperty ("after_master_note", "Per source: its own signal (after ducking and EQ, before the mixer fader, pan, effects and master) vs what it adds to the final mix after the master (master output minus master output with the source muted). Not additive when the limiter or compressor works.");
        }

        juce::Array<juce::var> list;
        for (const auto& finding : findings)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("severity", finding.severity == Severity::problem ? "problem" : finding.severity == Severity::warning ? "warning" : "info");
            o->setProperty ("category", finding.category);
            o->setProperty ("title", finding.title);
            o->setProperty ("detail", finding.detail);
            juce::Array<juce::var> names;
            for (const auto& name : finding.sources)
                names.add (name);
            o->setProperty ("sources", names);
            o->setProperty ("low_hz", finding.lowHz);
            o->setProperty ("high_hz", finding.highHz);
            juce::Array<juce::var> barList;
            for (auto bar : finding.bars)
                barList.add (bar);
            o->setProperty ("bars", barList);
            o->setProperty ("share_of_bars", finding.share);
            list.add (juce::var (o));
        }
        root->setProperty ("findings", list);
        return juce::var (root);
    }

    Comparison compareReports (const Report& before, const Report& after)
    {
        Comparison result;
        if (! before.valid || ! after.valid)
            return result;
        result.valid = true;

        for (const auto& a : after.sources)
            for (const auto& b : before.sources)
                if (a.present && b.present && a.name == b.name)
                {
                    Comparison::SourceDelta delta;
                    delta.name = a.name;
                    delta.rmsDb = a.rmsDb - b.rmsDb;
                    delta.peakDb = a.samplePeakDb - b.samplePeakDb;
                    delta.fundamentalBefore = b.fundamentalHz;
                    delta.fundamentalAfter = a.fundamentalHz;
                    result.sources.push_back (delta);
                    break;
                }

        const auto key = [] (const Finding& f) { return f.category + "|" + f.title; };
        std::set<juce::String> beforeKeys, afterKeys;
        for (const auto& f : before.findings) beforeKeys.insert (key (f));
        for (const auto& f : after.findings) afterKeys.insert (key (f));
        for (const auto& f : before.findings)
            if (afterKeys.count (key (f)) == 0)
                result.resolved.push_back (f.title);
        for (const auto& f : after.findings)
        {
            if (beforeKeys.count (key (f)) == 0)
                result.added.push_back (f.title);
            else
                ++result.unchanged;
        }
        result.limiterDb = after.limiterMaxDb - before.limiterMaxDb;
        result.kickTailBefore = before.kickTailHz;
        result.kickTailAfter = after.kickTailHz;
        return result;
    }

    juce::String Comparison::toText() const
    {
        if (! valid)
            return "No comparison: keep a report first (KEEP), then analyse again.";
        const auto signedDb = [] (float v) { return (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };
        juce::String text = "COMPARE (after minus kept)\n";
        for (const auto& s : sources)
        {
            text << s.name << ": level " << signedDb (s.rmsDb) << ", peak " << signedDb (s.peakDb);
            if (std::abs (s.fundamentalAfter - s.fundamentalBefore) > 0.5f)
                text << ", fundamental " << juce::String (s.fundamentalBefore, 1) << " -> " << juce::String (s.fundamentalAfter, 1) << " Hz";
            text << "\n";
        }
        if (std::abs (limiterDb) > 0.05f)
            text << "Limiter max: " << signedDb (limiterDb) << "\n";
        if (kickTailBefore > 0.0f || kickTailAfter > 0.0f)
            text << "Kick tail: " << juce::String (kickTailBefore, 1) << " -> " << juce::String (kickTailAfter, 1) << " Hz\n";
        text << "Findings: " << (int) resolved.size() << " gone, " << (int) added.size() << " new, " << unchanged << " the same\n";
        for (const auto& t : resolved) text << "  gone: " << t << "\n";
        for (const auto& t : added) text << "  new: " << t << "\n";
        return text;
    }
    juce::String Report::toText() const
    {
        if (! valid)
            return error;
        juce::String text;
        text << bars << " bars at " << juce::String (bpm, 0) << " BPM analysed";
        if (kickTailHz > 0.0f)
            text << "; kick tail " << hzText (kickTailHz) << " (" << noteName (kickTailHz) << ")";
        text << ".\n";
        for (const auto& finding : findings)
            text << (finding.severity == Severity::problem ? "[PROBLEM] " : finding.severity == Severity::warning ? "[WARNING] " : "[info] ") << finding.title << "\n    " << finding.detail << "\n";
        if (findings.empty())
            text << "Nothing to report at this sensitivity.\n";
        return text;
    }


    //==========================================================================================
    // Sparad analys: allt som mättes, för en AI eller för att analysera för hand.
    juce::String ExportOptions::toString() const
    {
        juce::String text;
        if (findings) text << "f";
        if (sources) text << "s";
        if (bars) text << "b";
        if (harmonics) text << "h";
        if (spectra) text << "p";
        if (frames) text << "r";
        if (kicks) text << "k";
        if (synth) text << "y";
        if (presets) text << "t";
        if (overview) text << "o";
        if (dynamics) text << "d";
        if (modulation) text << "m";
        if (about) text << "a";
        return text;
    }

    ExportOptions ExportOptions::fromString (const juce::String& text)
    {
        ExportOptions o;
        if (text.isEmpty())
            return o;
        o.findings = text.containsChar ('f');
        o.sources = text.containsChar ('s');
        o.bars = text.containsChar ('b');
        o.harmonics = text.containsChar ('h');
        o.spectra = text.containsChar ('p');
        o.frames = text.containsChar ('r');
        o.kicks = text.containsChar ('k');
        o.synth = text.containsChar ('y');
        o.presets = text.containsChar ('t');
        o.overview = text.containsChar ('o');
        o.dynamics = text.containsChar ('d');
        o.modulation = text.containsChar ('m');
        o.about = text.containsChar ('a');
        return o;
    }

    ExportOptions ExportOptions::fromNames (const juce::StringArray& names, bool allIfEmpty)
    {
        ExportOptions o;
        if (names.isEmpty() && allIfEmpty)
            return o;
        auto has = [&names] (std::initializer_list<const char*> aliases)
        {
            for (const auto* alias : aliases)
                for (const auto& name : names)
                    if (name.equalsIgnoreCase (alias))
                        return true;
            return false;
        };
        o.findings = has ({ "findings", "report" });
        o.sources = has ({ "sources", "overview_per_source" });
        o.bars = has ({ "bars", "bar_levels", "band_levels" });
        o.harmonics = has ({ "harmonics", "overtones" });
        o.spectra = has ({ "spectra", "spectrum", "spectra_12th_octave" });
        o.frames = has ({ "frames", "frame_levels", "spectrogram" });
        o.kicks = has ({ "kicks", "ducking" });
        o.synth = has ({ "synth", "settings", "state", "parameters" });
        o.presets = has ({ "presets" });
        o.overview = has ({ "overview", "structured", "settings_overview" });
        o.dynamics = has ({ "dynamics", "levels", "loudness" });
        o.modulation = has ({ "modulation", "lfo", "limiter" });
        o.about = has ({ "about", "readme", "explanation" });
        if (has ({ "all" }))
            return ExportOptions();
        return o;
    }

    std::vector<int> ExportFilter::parseBars (const juce::var& value)
    {
        std::vector<int> bars;
        auto add = [&bars] (int bar) { if (bar >= 1 && bar <= 256) bars.push_back (bar); };
        if (auto* list = value.getArray())
        {
            for (const auto& item : *list)
                add ((int) item);
        }
        else if (value.isString() || value.isInt())
        {
            for (const auto& part : juce::StringArray::fromTokens (value.toString(), ", ", {}))
            {
                const auto dash = part.indexOfChar ('-');
                if (dash > 0)
                {
                    for (int bar = part.substring (0, dash).getIntValue(); bar <= part.substring (dash + 1).getIntValue() && bar <= 256; ++bar)
                        add (bar);
                }
                else
                {
                    add (part.getIntValue());
                }
            }
        }
        return bars;
    }

    bool ExportFilter::wantsBar (int bar1Based) const
    {
        return bars.empty() || std::find (bars.begin(), bars.end(), bar1Based) != bars.end();
    }

    namespace
    {
        double round1 (double v) { return std::round (v * 10.0) / 10.0; }

        juce::Array<juce::var> numbers (const float* values, int count)
        {
            juce::Array<juce::var> list;
            for (int i = 0; i < count; ++i)
                list.add (round1 (values[i]));
            return list;
        }

        bool matchesFilter (const juce::String& name, const juce::StringArray& filter)
        {
            if (filter.isEmpty())
                return true;
            for (const auto& f : filter)
                if (name.containsIgnoreCase (f.trim()) || f.trim().equalsIgnoreCase ("all"))
                    return true;
            return false;
        }

        constexpr int octaveBins = 120; // 1/12-oktav från 20 Hz

        // Bandets mitt (geometriskt), och spektrets tyngdpunkt ur en rad bandnivåer (dB).
        double bandCentre (int b) { return std::sqrt ((double) bandEdges()[(size_t) b] * (double) bandEdges()[(size_t) b + 1]); }

        double centroidOfDb (const float* levels)
        {
            double sum = 0.0, weighted = 0.0;
            for (int b = 0; b < numBands(); ++b)
            {
                const auto p = std::pow (10.0, levels[b] / 10.0);
                sum += p;
                weighted += p * bandCentre (b);
            }
            return sum > 1.0e-12 ? weighted / sum : 0.0;
        }

        // Sex frekvensområden: energi (dB) ur en rad bandnivåer.
        const char* const groupNames[] { "sub_20_60", "bass_60_250", "low_mid_250_500", "mid_500_2k", "high_mid_2k_6k", "high_6k_20k" };
        int groupOf (double hz) { return hz < 60.0 ? 0 : hz < 250.0 ? 1 : hz < 500.0 ? 2 : hz < 2000.0 ? 3 : hz < 6000.0 ? 4 : 5; }

        juce::var groupsOfDb (const float* levels)
        {
            std::array<double, 6> sums {};
            for (int b = 0; b < numBands(); ++b)
                sums[(size_t) groupOf (bandCentre (b))] += std::pow (10.0, levels[b] / 10.0);
            auto* o = new juce::DynamicObject();
            for (size_t i = 0; i < sums.size(); ++i)
                o->setProperty (groupNames[i], round1 (10.0 * std::log10 (sums[i] + 1.0e-12)));
            return juce::var (o);
        }

        // Förklaringen (JSON i flera delar, se AnalysisAbout.h).
        juce::var aboutVar()
        {
            juce::String json;
            for (const auto* part : AnalysisAbout::parts)
                json << juce::String::fromUTF8 (part);
            auto parsed = juce::JSON::parse (json);
            return parsed.isVoid() ? juce::var ("(the explanation could not be read)") : parsed;
        }

        // Mönstret omräknat till loopens egen tid: för varje takt och varje rad de toner som börjar där (takt, taktslag, längd i taktslag).
        juce::var patternTimeline (const juce::var& state, int bars, const ExportFilter& filter)
        {
            const auto pattern = state["pattern"];
            const auto ticksPerBar = std::max (1, (int) pattern["ticks_per_bar"]);
            const auto loopBars = std::max (1, (int) pattern["bars"]);
            const auto* layers = pattern["layers (instruments 0-9 and 'kick')"].getArray();
            const auto parameters = state["parameters"];
            if (layers == nullptr)
                return {};

            struct Row { juce::String name; const juce::Array<juce::var>* notes = nullptr; int rowBars = 1; bool loops = true; };
            std::vector<Row> rows;
            for (const auto& layer : *layers)
            {
                Row row;
                const auto isKick = layer["layer"].isString();
                row.name = isKick ? juce::String ("Kick") : layer["name"].toString();
                row.notes = layer["notes [tick, length, pitch, velocity, glide]"].getArray();
                if (row.notes == nullptr || row.notes->isEmpty())
                    continue;
                const auto suffix = isKick ? juce::String ("Kick") : "Bass" + juce::String ((int) layer["layer"] + 1);
                const auto lengthText = parameters[juce::Identifier ("rowBars" + suffix)].toString();
                row.rowBars = layer.hasProperty ("bars") ? std::max (1, (int) layer["bars"]) // radens egen längd (R9)
                                                            : (lengthText.containsAnyOf ("0123456789") ? std::max (1, lengthText.getIntValue()) : loopBars);
                if (parameters.hasProperty (juce::Identifier ("rowLoop" + suffix)))
                    row.loops = (bool) parameters[juce::Identifier ("rowLoop" + suffix)];
                if (matchesFilter (row.name, filter.sources))
                    rows.push_back (row);
            }

            juce::Array<juce::var> timeline;
            for (int bar = 0; bar < bars; ++bar)
            {
                if (! filter.wantsBar (bar + 1))
                    continue;
                auto* entry = new juce::DynamicObject();
                entry->setProperty ("bar", bar + 1);
                auto* notesByRow = new juce::DynamicObject();
                for (const auto& row : rows)
                {
                    const auto barInRow = row.loops ? bar % row.rowBars : (bar < row.rowBars ? bar : -1);
                    if (barInRow < 0)
                        continue;
                    juce::Array<juce::var> list;
                    for (const auto& note : *row.notes)
                    {
                        const auto tick = (int) note[0];
                        if (tick / ticksPerBar != barInRow)
                            continue;
                        auto* n = new juce::DynamicObject();
                        n->setProperty ("beat", std::round ((1.0 + (tick % ticksPerBar) / (ticksPerBar / 4.0)) * 100.0) / 100.0);
                        n->setProperty ("length_beats", std::round (((int) note[1] / (ticksPerBar / 4.0)) * 100.0) / 100.0);
                        n->setProperty ("pitch_semitones", (int) note[2]);
                        n->setProperty ("velocity", note[3]);
                        if ((bool) note[4])
                            n->setProperty ("glide", true);
                        if (note.size() > 5)
                            n->setProperty ("chord_semitones", note[5]); // fas P: ackordets intervall från tonen
                        list.add (juce::var (n));
                    }
                    if (! list.isEmpty())
                        notesByRow->setProperty (row.name, list);
                }
                entry->setProperty ("notes", juce::var (notesByRow));
                timeline.add (juce::var (entry));
            }
            return timeline;
        }

        // Alla inställningar ordnade efter vad de hör till: kicken, varje instruments oscillatorer (A/B/C med wavetable), filter, envelopes, EQ
        // och sändningar, effektkanalerna med sina platser, mastern, mixern, sidechainen, moduleringen och mönstrets inställningar.
        juce::var buildOverview (const juce::var& state)
        {
            auto* parameters = state["parameters"].getDynamicObject();
            if (parameters == nullptr)
                return {};

            juce::StringArray instrumentNames;
            if (auto* layers = state["pattern"]["layers (instruments 0-9 and 'kick')"].getArray())
                for (const auto& layer : *layers)
                    if (! layer["layer"].isString())
                        instrumentNames.set ((int) layer["layer"], layer["name"].toString());

            auto* root = new juce::DynamicObject();
            auto* kick = new juce::DynamicObject();
            auto* kickEq = new juce::DynamicObject();
            auto* master = new juce::DynamicObject();
            auto* masterEq = new juce::DynamicObject();
            auto* mixer = new juce::DynamicObject();
            auto* sidechain = new juce::DynamicObject();
            auto* modulation = new juce::DynamicObject();
            auto* patternSettings = new juce::DynamicObject();
            auto* other = new juce::DynamicObject();

            struct Instrument
            {
                juce::DynamicObject::Ptr whole { new juce::DynamicObject() };
                juce::DynamicObject::Ptr oscillators { new juce::DynamicObject() }, oscA { new juce::DynamicObject() }, oscB { new juce::DynamicObject() }, oscC { new juce::DynamicObject() };
                juce::DynamicObject::Ptr pitch { new juce::DynamicObject() }, subNoise { new juce::DynamicObject() }, filter { new juce::DynamicObject() }, amp { new juce::DynamicObject() };
                juce::DynamicObject::Ptr drive { new juce::DynamicObject() }, gate { new juce::DynamicObject() }, sample { new juce::DynamicObject() };
                juce::DynamicObject::Ptr env2 { new juce::DynamicObject() }, env3 { new juce::DynamicObject() }, pitchEnv { new juce::DynamicObject() };
                juce::DynamicObject::Ptr eq { new juce::DynamicObject() }, routing { new juce::DynamicObject() }, rest { new juce::DynamicObject() };
                bool used = false;
            };
            std::array<Instrument, numInstruments> instruments;

            struct Channel
            {
                juce::DynamicObject::Ptr level { new juce::DynamicObject() }, eq { new juce::DynamicObject() }, comp { new juce::DynamicObject() };
                bool touched = false;
            };
            std::array<Channel, 11> channels;

            static const juce::StringArray oscFields { "Table", "Pos", "Level", "Semi", "Fine", "Unison", "Detune", "Blend", "Warp", "WarpAmount", "Phase", "Random", "Pan", "Stereo", "On" };

            for (const auto& property : parameters->getProperties())
            {
                const auto id = property.name.toString();
                const auto& value = property.value;

                if (id.startsWith ("bass") && id.length() > 5 && juce::CharacterFunctions::isDigit (id[4]))
                {
                    const auto digits = juce::CharacterFunctions::isDigit (id[5]) ? 2 : 1;
                    const auto index = id.substring (4, 4 + digits).getIntValue() - 1;
                    if (! juce::isPositiveAndBelow (index, numInstruments))
                        continue;
                    const auto field = id.substring (4 + digits);
                    auto& in = instruments[(size_t) index];
                    in.used = true;
                    const char oscLetter = field.isNotEmpty() ? (char) field[0] : 0;
                    const auto oscField = field.substring (1);
                    if ((oscLetter == 'A' || oscLetter == 'B' || oscLetter == 'C') && oscFields.contains (oscField))
                        (oscLetter == 'A' ? in.oscA : oscLetter == 'B' ? in.oscB : in.oscC)->setProperty (oscField, value);
                    else if (field.startsWith ("Eq"))
                        in.eq->setProperty (field.substring (2), value);
                    else if (field == "WarpEnv" || field == "Sub" || field == "SubOctave" || field == "SubWave" || field == "Noise" || field == "NoiseTone")
                        in.subNoise->setProperty (field, value);
                    else if (field == "FilterType" || field == "KeyTrack" || field == "Cutoff" || field == "Resonance" || field == "EnvAmount" || field == "EnvAttack" || field == "EnvDecay" || field == "VelToEnv")
                        in.filter->setProperty (field, value);
                    else if (field == "Attack" || field == "Decay" || field == "Sustain" || field == "Release" || field == "Glide")
                        in.amp->setProperty (field, value);
                    else if (field == "Drive" || field == "DriveMode")
                        in.drive->setProperty (field, value);
                    else if (field.startsWith ("Gate") && field != "Gate")
                        in.gate->setProperty (field.substring (4), value);
                    else if (field.startsWith ("Sample"))
                        in.sample->setProperty (field.substring (6), value);
                    else if (field.startsWith ("Env2"))
                        in.env2->setProperty (field.substring (4), value);
                    else if (field.startsWith ("Env3"))
                        in.env3->setProperty (field.substring (4), value);
                    else if (field.startsWith ("PitchEnv"))
                        in.pitchEnv->setProperty (field.substring (8), value);
                    else if (field == "Octave" || field == "Tune")
                        in.pitch->setProperty (field, value);
                    else if (field == "On" || field == "Level" || field == "Duck" || field == "Grid" || field == "FxChannel" || field == "FxAmount" || field == "Gate" || field == "Delay" || field == "Swing" || field == "Humanize")
                        in.routing->setProperty (field == "Gate" ? juce::String ("NoteLength") : field, value);
                    else
                        in.rest->setProperty (field, value);
                }
                else if (id.startsWith ("kickEq"))
                    kickEq->setProperty (id.substring (6), value);
                else if (id.startsWith ("kick"))
                    kick->setProperty (id.substring (4), value);
                else if (id.startsWith ("masterEq"))
                    masterEq->setProperty (id.substring (8), value);
                else if (id.startsWith ("master"))
                    master->setProperty (id.substring (6), value);
                else if (id.startsWith ("mix"))
                    mixer->setProperty (id.substring (3), value);
                else if (id.startsWith ("sc"))
                    sidechain->setProperty (id.substring (2), value);
                else if (id.startsWith ("lfo") || id.startsWith ("macro"))
                    modulation->setProperty (id, value);
                else if (id.startsWith ("fx") && id.length() > 3 && id[2] >= 'A' && id[2] <= 'A' + numEffectChannels - 1 && id != "fxKey")
                {
                    auto& channel = channels[(size_t) (id[2] - 'A')];
                    const auto rest = id.substring (3);
                    if (rest.length() >= 2 && juce::CharacterFunctions::isDigit (rest[0]))
                        continue; // effektplatserna beskrivs under "effects" nedan
                    if (rest == "Level" || rest == "Duck")
                        channel.level->setProperty (rest, value);
                    else if (rest.startsWith ("Eq"))
                        channel.eq->setProperty (rest.substring (2), value);
                    else if (rest.startsWith ("Comp"))
                        channel.comp->setProperty (rest.substring (4), value);
                    if (rest == "Level" && std::abs ((double) value - 1.0) > 1.0e-3)
                        channel.touched = true;
                    else if (rest == "Duck" && (double) value > 1.0e-3)
                        channel.touched = true;
                    else if ((rest.endsWith ("EqHp") && (double) value > 21.0) || (rest.endsWith ("EqLp") && (double) value < 19500.0) || (rest.startsWith ("Eq") && rest.endsWith ("G") && std::abs ((double) value) > 0.05))
                        channel.touched = true;
                    else if (rest == "CompRatio" && (double) value > 1.01)
                        channel.touched = true;
                }
                else if (id == "patternBars" || id == "noteEnd" || id.startsWith ("rowBars") || id.startsWith ("rowLoop") || id.startsWith ("rowGrid"))
                    patternSettings->setProperty (id, value);
                else
                    other->setProperty (id, value);
            }

            juce::Array<juce::var> instrumentList;
            for (int i = 0; i < numInstruments; ++i)
            {
                auto& in = instruments[(size_t) i];
                if (! in.used)
                    continue;
                in.oscillators->setProperty ("A", juce::var (in.oscA.get()));
                in.oscillators->setProperty ("B", juce::var (in.oscB.get()));
                in.oscillators->setProperty ("C", juce::var (in.oscC.get()));
                auto* entry = in.whole.get();
                entry->setProperty ("number", i + 1);
                entry->setProperty ("name", instrumentNames[i].isNotEmpty() ? instrumentNames[i] : "Instr " + juce::String (i + 1));
                entry->setProperty ("parameter_prefix", "bass" + juce::String (i + 1));
                entry->setProperty ("routing_and_levels", juce::var (in.routing.get()));
                entry->setProperty ("pitch", juce::var (in.pitch.get()));
                entry->setProperty ("oscillators", juce::var (in.oscillators.get()));
                entry->setProperty ("sub_and_noise", juce::var (in.subNoise.get()));
                entry->setProperty ("filter", juce::var (in.filter.get()));
                entry->setProperty ("amp_envelope", juce::var (in.amp.get()));
                entry->setProperty ("drive", juce::var (in.drive.get()));
                entry->setProperty ("gate_chopper", juce::var (in.gate.get()));
                entry->setProperty ("sample", juce::var (in.sample.get()));
                entry->setProperty ("modulation_envelope_2", juce::var (in.env2.get()));
                entry->setProperty ("modulation_envelope_3", juce::var (in.env3.get()));
                entry->setProperty ("pitch_envelope", juce::var (in.pitchEnv.get()));
                entry->setProperty ("eq", juce::var (in.eq.get()));
                if (in.rest->getProperties().size() > 0)
                    entry->setProperty ("other", juce::var (in.rest.get()));
                instrumentList.add (juce::var (entry));
            }

            // Effektkanalerna: nivå, duckning, EQ, kompressor och platserna (från state.effects).
            juce::Array<juce::var> channelList;
            for (int ch = 0; ch < 11; ++ch)
            {
                const auto label = "FX" + juce::String (ch + 1);
                juce::Array<juce::var> slots;
                if (auto* effects = state["effects"].getArray())
                    for (const auto& slot : *effects)
                        if (slot["channel"].toString() == label)
                            slots.add (slot);
                auto& channel = channels[(size_t) ch];
                if (slots.isEmpty() && ! channel.touched)
                    continue;
                auto* entry = new juce::DynamicObject();
                entry->setProperty ("channel", label);
                entry->setProperty ("parameter_prefix", "fx" + juce::String ("ABCDEFGHIJK").substring (ch, ch + 1));
                entry->setProperty ("level_and_duck", juce::var (channel.level.get()));
                entry->setProperty ("slots_in_order", slots);
                entry->setProperty ("eq", juce::var (channel.eq.get()));
                entry->setProperty ("compressor", juce::var (channel.comp.get()));
                channelList.add (juce::var (entry));
            }

            kick->setProperty ("note", "Parameter ids are 'kick' + the name here.");
            root->setProperty ("kick", juce::var (kick));
            root->setProperty ("kick_eq", juce::var (kickEq));
            root->setProperty ("instruments", instrumentList);
            root->setProperty ("effect_channels", channelList);
            root->setProperty ("master", juce::var (master));
            root->setProperty ("master_eq", juce::var (masterEq));
            root->setProperty ("mixer", juce::var (mixer));
            root->setProperty ("sidechain", juce::var (sidechain));
            root->setProperty ("modulation_sources", juce::var (modulation));
            root->setProperty ("modulation_routes", state["lfo_routes"]);
            root->setProperty ("pattern_settings", juce::var (patternSettings));
            root->setProperty ("kick_envelopes", juce::var ([&state] { auto* e = new juce::DynamicObject(); e->setProperty ("pitch", state["kick_pitch_envelope"]); e->setProperty ("amplitude", state["kick_amp_envelope"]); return e; }()));
            root->setProperty ("sidechain_curve", state["sidechain_curve"]);
            root->setProperty ("lfo_custom_curves", state["lfo_custom_curves"]);
            root->setProperty ("samples_loaded", state["samples_loaded"]);
            root->setProperty ("user_wavetables_loaded", state["user_wavetables_loaded"]);
            root->setProperty ("other", juce::var (other));
            return juce::var (root);
        }
    }

    juce::var Report::toExportVar (const ExportOptions& o, const ExportFilter& filter) const
    {
        auto* root = new juce::DynamicObject();
        if (! valid || capture == nullptr)
        {
            root->setProperty ("ok", false);
            root->setProperty ("error", valid ? juce::String ("The analysis data is not available.") : error);
            return juce::var (root);
        }

        const auto& c = *capture;
        const auto nb = numBands();
        const auto& edges = bandEdges();
        const auto binHz = c.rate / fftSize;
        const auto binsCount = fftSize / 2;

        // Bandområdet som tas med: de band som överlappar minHz-maxHz.
        auto b0 = 0, b1 = nb;
        while (b0 < nb && edges[(size_t) b0 + 1] <= filter.minHz)
            ++b0;
        while (b1 > b0 && edges[(size_t) b1 - 1] >= filter.maxHz)
            --b1;
        if (b1 <= b0)
        {
            b0 = 0;
            b1 = nb;
        }
        const auto bandCount = b1 - b0;
        const auto keepSource = [&] (int s) { return c.present[(size_t) s] && matchesFilter (c.names[(size_t) s], filter.sources); };

        if (o.about)
            root->setProperty ("about", aboutVar());

        // --- Mått och förklaringar
        auto* meta = new juce::DynamicObject();
        meta->setProperty ("format", "kickbase-analysis-2");
        {
            // Var tiden finns i just den här filen (det som saknas står uttryckligen): en AI som bara läser början ska ändå hitta den.
            juce::String time;
            time << "Time is measured from the start of the analysed loop; bar numbers start at 1 and beats at 1.0. ";
            time << (o.frames ? "frames: a SNAPSHOT of every source about every 45 ms (frames.seconds, frames.bar, frames.beat_in_bar) with the level in every frequency band, rms/peak/brightness, the ducking per instrument"
                                 + juce::String (o.modulation ? ", the limiter reduction and the value of every modulation source and every modulated parameter at that time. " : ". ")
                              : "frames: NOT included in this file (it is the big per-frame section; export again with frames to get a snapshot every 45 ms). ");
            time << (o.synth ? "pattern_timeline: every note of the pattern by bar and beat (which instrument, pitch, length), and synth: all settings and the notes. " : "pattern_timeline and synth (notes, all settings): NOT included in this file. ");
            time << (o.kicks ? "kick_hits: the time, bar and beat of every kick hit and how deep each instrument is ducked at it. " : "kick_hits: NOT included. ");
            time << (o.bars ? "per_bar: levels and harmonics per source and bar." : "per_bar: NOT included.");
            meta->setProperty ("where_is_time_and_snapshots", time);
        }
        meta->setProperty ("generated", juce::Time::getCurrentTime().toISO8601 (true));
        meta->setProperty ("bars_analysed", c.bars);
        meta->setProperty ("bpm", c.bpm);
        meta->setProperty ("beats_per_bar", 4);
        meta->setProperty ("sample_rate", c.rate);
        meta->setProperty ("fft_size", fftSize);
        meta->setProperty ("frame_window_ms", round1 (1000.0 * fftSize / c.rate));
        meta->setProperty ("frame_spacing_ms", "about 43-53");
        meta->setProperty ("frame_count", c.frames);
        meta->setProperty ("sensitivity", sensitivity == 0 ? "low" : sensitivity == 1 ? "normal" : "high");
        juce::Array<juce::var> edgeList;
        for (auto e : edges)
            edgeList.add (e);
        meta->setProperty ("band_edges_hz", edgeList);
        auto* bandsIncluded = new juce::DynamicObject();
        bandsIncluded->setProperty ("first_band_index", b0);
        bandsIncluded->setProperty ("last_band_index", b1 - 1);
        bandsIncluded->setProperty ("note", "Every band array in this file holds the bands first_band_index..last_band_index of band_edges_hz (all bands unless a frequency filter was used).");
        meta->setProperty ("bands_included", juce::var (bandsIncluded));
        auto* filters = new juce::DynamicObject();
        juce::Array<juce::var> filterSources, filterBars;
        for (const auto& name : filter.sources)
            filterSources.add (name);
        for (auto bar : filter.bars)
            filterBars.add (bar);
        filters->setProperty ("sources", filterSources);
        filters->setProperty ("bars", filterBars);
        filters->setProperty ("min_hz", filter.minHz);
        filters->setProperty ("max_hz", filter.maxHz > 1.0e8f ? 20000.0f : filter.maxHz);
        filters->setProperty ("frame_step", filter.frameStep);
        meta->setProperty ("filters_applied (empty = everything)", juce::var (filters));
        meta->setProperty ("units", "All levels are in dB where 0 dB is a full-scale sine wave. Band levels are the summed power of the FFT bins in the band (a pure tone shows about 0 dB). "
                                    "Source signals are mono mixes taken after ducking and EQ and before the effect sends; Master is the output after the limiter. "
                                    "Bar numbers start at 1 and are positions in the pattern loop; beat positions run 1.0 to 4.99.");
        juce::Array<juce::var> sourceNames;
        for (int s = 0; s < numSources; ++s)
            if (keepSource (s))
                sourceNames.add (c.names[(size_t) s]);
        meta->setProperty ("sources", sourceNames);
        if (referenceName.isNotEmpty())
            meta->setProperty ("reference", referenceName);
        root->setProperty ("ok", true);
        root->setProperty ("meta", juce::var (meta));

        // --- Fynden
        if (o.findings)
        {
            const auto base = toVar();
            root->setProperty ("findings", base["findings"]);
            root->setProperty ("kick_tail_hz", kickTailHz);
            if (kickTailDesignHz > 0.0f)
                root->setProperty ("kick_tail_design_hz", kickTailDesignHz);
            root->setProperty ("limiter_max_reduction_db", limiterMaxDb);
            if (! aiNotes.isVoid())
                root->setProperty ("ai_notes", aiNotes);
        }

        // Medelnivå (i effekt) och toppnivå för en källa över ramar [first, last).
        auto bandStats = [&] (int source, int firstFrame, int lastFrame, std::vector<float>& average, std::vector<float>& peak)
        {
            average.assign ((size_t) nb, -120.0f);
            peak.assign ((size_t) nb, -120.0f);
            const auto frames = std::max (1, lastFrame - firstFrame);
            for (int b = 0; b < nb; ++b)
            {
                double sum = 0.0;
                for (int f = firstFrame; f < lastFrame; ++f)
                {
                    const auto level = c.band (source, f, b);
                    sum += std::pow (10.0, level / 10.0);
                    peak[(size_t) b] = std::max (peak[(size_t) b], level);
                }
                average[(size_t) b] = (float) (10.0 * std::log10 (sum / frames + 1.0e-12));
            }
        };

        // --- Översikt per källa, hela loopen
        if (o.sources)
        {
            juce::Array<juce::var> list;
            for (int s = 0; s < numSources; ++s)
            {
                if (! keepSource (s))
                    continue;
                std::vector<float> average, peak;
                bandStats (s, 0, c.frames, average, peak);
                auto* entry = new juce::DynamicObject();
                entry->setProperty ("name", c.names[(size_t) s]);
                entry->setProperty ("peak_band_db", round1 (sources[(size_t) s].peakDb));
                entry->setProperty ("sample_peak_db", round1 (sources[(size_t) s].samplePeakDb));
                entry->setProperty ("rms_db", round1 (sources[(size_t) s].rmsDb));
                if (sources[(size_t) s].fundamentalHz > 0.0f)
                {
                    entry->setProperty ("fundamental_hz", round1 (sources[(size_t) s].fundamentalHz));
                    entry->setProperty ("note", sources[(size_t) s].note);
                }
                entry->setProperty ("average_band_db", numbers (average.data() + b0, bandCount));
                entry->setProperty ("max_band_db", numbers (peak.data() + b0, bandCount));
                if (o.dynamics)
                {
                    entry->setProperty ("centroid_hz", round1 (centroidOfDb (average.data())));
                    entry->setProperty ("energy_by_range_db", groupsOfDb (average.data()));
                }
                list.add (juce::var (entry));
            }
            root->setProperty ("sources", list);
        }

        // --- Takt för takt
        if (o.bars || o.harmonics || o.spectra || o.dynamics)
        {
            juce::Array<juce::var> list;
            for (int s = 0; s < numSources; ++s)
            {
                if (! keepSource (s))
                    continue;
                juce::Array<juce::var> barList;
                for (int bar = 0; bar < c.bars; ++bar)
                {
                    if (! filter.wantsBar (bar + 1))
                        continue;
                    auto firstFrame = -1, lastFrame = -1;
                    for (int f = 0; f < c.frames; ++f)
                        if (c.frameBar[(size_t) f] == bar)
                        {
                            if (firstFrame < 0)
                                firstFrame = f;
                            lastFrame = f + 1;
                        }
                    if (firstFrame < 0)
                        continue;

                    auto* entry = new juce::DynamicObject();
                    entry->setProperty ("bar", bar + 1);
                    entry->setProperty ("frames", lastFrame - firstFrame);
                    std::vector<float> average, peak;
                    bandStats (s, firstFrame, lastFrame, average, peak);
                    if (o.bars)
                    {
                        double total = 0.0;
                        for (int b = 0; b < nb; ++b)
                            total += std::pow (10.0, average[(size_t) b] / 10.0);
                        entry->setProperty ("loudness_db", round1 (10.0 * std::log10 (total + 1.0e-12)));
                        entry->setProperty ("average_band_db", numbers (average.data() + b0, bandCount));
                        entry->setProperty ("max_band_db", numbers (peak.data() + b0, bandCount));
                    }
                    if (o.dynamics)
                    {
                        auto topDb = -120.0f;
                        double rmsPower = 0.0;
                        for (int f = firstFrame; f < lastFrame; ++f)
                        {
                            topDb = std::max (topDb, c.framePeakDb[(size_t) f * numSources + (size_t) s]);
                            rmsPower += std::pow (10.0, c.frameRmsDb[(size_t) f * numSources + (size_t) s] / 10.0);
                        }
                        const auto rmsDb = 10.0 * std::log10 (rmsPower / std::max (1, lastFrame - firstFrame) + 1.0e-12);
                        entry->setProperty ("rms_db", round1 (rmsDb));
                        entry->setProperty ("peak_db", round1 (topDb));
                        entry->setProperty ("crest_db", round1 (topDb - rmsDb));
                        entry->setProperty ("centroid_hz", round1 (centroidOfDb (average.data())));
                        entry->setProperty ("energy_by_range_db", groupsOfDb (average.data()));
                    }

                    // Medelspektrumet för takten (effekt per steg).
                    const auto framesInBar = std::max (1, c.barFrameCount.empty() ? 1 : c.barFrameCount[(size_t) bar]);
                    const auto* power = c.barPower.empty() ? nullptr : &c.barPower[((size_t) bar * numSources + (size_t) s) * (size_t) binsCount];
                    if (power != nullptr && (o.harmonics || o.spectra))
                    {
                        std::vector<float> mean ((size_t) binsCount);
                        for (int k = 0; k < binsCount; ++k)
                            mean[(size_t) k] = power[k] / (float) framesInBar;

                        if (o.harmonics)
                        {
                            const auto f0 = findFundamental (mean.data(), c.rate, -75.0f);
                            if (f0 > 0.0f)
                            {
                                entry->setProperty ("fundamental_hz", round1 (f0));
                                entry->setProperty ("note", noteName (f0));
                                const auto fundamentalBin = juce::jlimit (1, binsCount - 2, (int) std::round (f0 / binHz));
                                const auto fundamentalDb = 10.0 * std::log10 (std::max ({ mean[(size_t) fundamentalBin - 1], mean[(size_t) fundamentalBin], mean[(size_t) fundamentalBin + 1] }) + 1.0e-12);
                                juce::Array<juce::var> harmonicList;
                                for (int n = 1; n <= 32; ++n)
                                {
                                    const auto hz = (double) f0 * n;
                                    if (hz > std::min (20000.0, c.rate * 0.48))
                                        break;
                                    if (hz < filter.minHz || hz > filter.maxHz)
                                        continue;
                                    const auto centre = (int) std::round (hz / binHz);
                                    const auto reach = std::max (1, (int) std::ceil (0.02 * hz / binHz));
                                    auto best = 0.0f;
                                    auto bestBin = centre;
                                    for (int k = std::max (1, centre - reach); k <= std::min (binsCount - 1, centre + reach); ++k)
                                        if (mean[(size_t) k] > best)
                                        {
                                            best = mean[(size_t) k];
                                            bestBin = k;
                                        }
                                    auto* h = new juce::DynamicObject();
                                    h->setProperty ("n", n);
                                    h->setProperty ("hz", round1 (bestBin * binHz));
                                    const auto db = 10.0 * std::log10 (best + 1.0e-12);
                                    h->setProperty ("db", round1 (db));
                                    h->setProperty ("db_vs_fundamental", round1 (db - fundamentalDb));
                                    harmonicList.add (juce::var (h));
                                }
                                entry->setProperty ("harmonics", harmonicList);
                            }
                        }

                        if (o.spectra)
                        {
                            std::vector<double> sums ((size_t) octaveBins, 0.0);
                            for (int k = 1; k < binsCount; ++k)
                            {
                                const auto hz = k * binHz;
                                if (hz < 20.0)
                                    continue;
                                const auto index = (int) std::floor (12.0 * std::log2 (hz / 20.0));
                                if (index >= 0 && index < octaveBins)
                                    sums[(size_t) index] += mean[(size_t) k];
                            }
                            juce::Array<juce::var> spectrum;
                            for (int i = 0; i < octaveBins; ++i)
                            {
                                const auto centreHz = 20.0 * std::pow (2.0, (i + 0.5) / 12.0);
                                if (centreHz >= filter.minHz && centreHz <= filter.maxHz)
                                    spectrum.add (round1 (10.0 * std::log10 (sums[(size_t) i] / lobePower + 1.0e-12)));
                            }
                            entry->setProperty ("spectrum_12th_octave_db", spectrum);
                        }
                    }
                    barList.add (juce::var (entry));
                }
                auto* sourceEntry = new juce::DynamicObject();
                sourceEntry->setProperty ("name", c.names[(size_t) s]);
                sourceEntry->setProperty ("bars", barList);
                list.add (juce::var (sourceEntry));
            }
            root->setProperty ("per_bar", list);
            if (o.spectra)
            {
                juce::Array<juce::var> centres;
                for (int i = 0; i < octaveBins; ++i)
                {
                    const auto centreHz = 20.0 * std::pow (2.0, (i + 0.5) / 12.0);
                    if (centreHz >= filter.minHz && centreHz <= filter.maxHz)
                        centres.add (round1 (centreHz));
                }
                root->setProperty ("spectrum_12th_octave_centre_hz", centres);
            }
        }

        // --- Kickens slag och duckningen
        if (o.kicks)
        {
            juce::Array<juce::var> list;
            const auto secondsPerBeat = 60.0 / std::max (1.0, c.bpm);
            for (size_t i = 0; i < c.kickOnsetSeconds.size(); ++i)
            {
                const auto t = std::max (0.0, c.kickOnsetSeconds[i]);
                auto bar = (int) std::floor (t / (4.0 * secondsPerBeat) + 0.01); // ett slag strax före taktstrecket hör till nästa takt
                if (! filter.wantsBar (bar + 1))
                    continue;
                auto beat = (t - bar * 4.0 * secondsPerBeat) / secondsPerBeat + 1.0;
                if (beat < 1.0)
                    beat = 1.0;
                auto* hit = new juce::DynamicObject();
                hit->setProperty ("bar", bar + 1);
                hit->setProperty ("beat", std::round (beat * 100.0) / 100.0);
                hit->setProperty ("time_s", std::round (t * 1000.0) / 1000.0);
                if (i < c.duckDepthDb.size())
                {
                    auto* ducking = new juce::DynamicObject();
                    for (int instrument = 0; instrument < numInstruments; ++instrument)
                        if (keepSource (firstInstrument + instrument))
                            ducking->setProperty (c.names[(size_t) (firstInstrument + instrument)], round1 (c.duckDepthDb[i][(size_t) instrument]));
                    hit->setProperty ("ducking_db", juce::var (ducking));
                }
                list.add (juce::var (hit));
            }
            root->setProperty ("kick_hits", list);
            if (c.phase.valid)
                root->setProperty ("lowband", phaseVar (c.phase));
        }

        // --- Presets, alla inställningar och mönstret i loopens tid
        const auto state = synthState["data"].isVoid() ? synthState : synthState["data"];
        if (! synthState.isVoid())
        {
            if (o.presets && ! state["presets"].isVoid())
                root->setProperty ("presets", state["presets"]);
            if (o.overview)
                root->setProperty ("synth_overview", buildOverview (state));
            if (o.synth)
            {
                root->setProperty ("synth", state);
                const auto timeline = patternTimeline (state, c.bars, filter);
                if (! timeline.isVoid())
                    root->setProperty ("pattern_timeline", timeline);
            }
        }

        return juce::var (root);
    }

    juce::String Report::toExportJson (const ExportOptions& o, const ExportFilter& filter) const
    {
        auto base = toExportVar (o, filter);
        auto json = juce::JSON::toString (base, true);
        if (! o.frames || capture == nullptr || ! (bool) base["ok"])
            return json;

        // Nivåerna för varje ram (spektrogrammet) skrivs för hand (en decimal): som vanlig JSON blir de flera gånger större.
        const auto& c = *capture;
        const auto nb = numBands();
        const auto& edges = bandEdges();
        auto b0 = 0, b1 = nb;
        while (b0 < nb && edges[(size_t) b0 + 1] <= filter.minHz)
            ++b0;
        while (b1 > b0 && edges[(size_t) b1 - 1] >= filter.maxHz)
            --b1;
        if (b1 <= b0)
        {
            b0 = 0;
            b1 = nb;
        }
        std::vector<int> frameList;
        {
            auto counter = 0;
            for (int f = 0; f < c.frames; ++f)
                if (filter.wantsBar (c.frameBar[(size_t) f] + 1) && (counter++ % std::max (1, filter.frameStep)) == 0)
                    frameList.push_back (f);
        }
        const auto keep = [&] (int s) { return c.present[(size_t) s] && matchesFilter (c.names[(size_t) s], filter.sources); };

        juce::String frames;
        frames.preallocateBytes ((size_t) frameList.size() * numSources * (size_t) (b1 - b0) * 7);
        frames << ",\n  \"frames\": {\n    \"note\": \"One column per frame (about every 43-53 ms) through the loop = a spectrogram. band_db[source][frame][band] uses the bands first_band_index..last_band_index of meta.band_edges_hz. seconds[frame] is the middle of the frame's 171 ms window, bar[frame] the bar it belongs to. "
               << "rms_db and peak_db are the source's level in the frame, centroid_hz the brightness (the power-weighted mean frequency). ducking_db[instrument][frame] is how many dB each instrument was ducked in the frame.\",\n";
        frames << "    \"seconds\": [";
        for (size_t i = 0; i < frameList.size(); ++i)
            frames << (i > 0 ? "," : "") << juce::String (c.frameSeconds[(size_t) frameList[i]], 3);
        frames << "],\n    \"bar\": [";
        for (size_t i = 0; i < frameList.size(); ++i)
            frames << (i > 0 ? "," : "") << (c.frameBar[(size_t) frameList[i]] + 1);
        frames << "],\n    \"beat_in_bar\": [";
        for (size_t i = 0; i < frameList.size(); ++i)
        {
            const auto secondsPerBeat = 60.0 / std::max (1.0, c.bpm);
            const auto t = c.frameSeconds[(size_t) frameList[i]];
            const auto bar = c.frameBar[(size_t) frameList[i]];
            frames << (i > 0 ? "," : "") << juce::String (1.0 + (t - bar * 4.0 * secondsPerBeat) / secondsPerBeat, 2);
        }
        frames << "],\n    \"band_db\": {\n";
        auto first = true;
        for (int s = 0; s < numSources; ++s)
        {
            if (! keep (s))
                continue;
            frames << (first ? "" : ",\n") << "      \"" << c.names[(size_t) s].replace ("\"", "'") << "\": [";
            first = false;
            for (size_t i = 0; i < frameList.size(); ++i)
            {
                frames << (i > 0 ? "," : "") << "[";
                for (int b = b0; b < b1; ++b)
                    frames << (b > b0 ? "," : "") << juce::String (c.band (s, frameList[i], b), 1);
                frames << "]";
            }
            frames << "]";
        }
        frames << "\n    }";

        if (o.dynamics)
        {
            auto writeSeries = [&] (const char* key, auto valueOf)
            {
                frames << ",\n    \"" << key << "\": {\n";
                auto firstSource = true;
                for (int s = 0; s < numSources; ++s)
                {
                    if (! keep (s))
                        continue;
                    frames << (firstSource ? "" : ",\n") << "      \"" << c.names[(size_t) s].replace ("\"", "'") << "\": [";
                    firstSource = false;
                    for (size_t i = 0; i < frameList.size(); ++i)
                        frames << (i > 0 ? "," : "") << juce::String (valueOf (s, frameList[i]), 1);
                    frames << "]";
                }
                frames << "\n    }";
            };
            writeSeries ("rms_db", [&c] (int s, int f) { return c.frameRmsDb[(size_t) f * numSources + (size_t) s]; });
            writeSeries ("peak_db", [&c] (int s, int f) { return c.framePeakDb[(size_t) f * numSources + (size_t) s]; });
            writeSeries ("centroid_hz", [&c] (int s, int f)
            {
                std::vector<float> row ((size_t) numBands());
                for (int b = 0; b < numBands(); ++b)
                    row[(size_t) b] = c.band (s, f, b);
                return (float) centroidOfDb (row.data());
            });
        }

        if (o.kicks && ! c.frameDuckDb.empty())
        {
            frames << ",\n    \"ducking_db\": {\n";
            auto firstInst = true;
            for (int i = 0; i < numInstruments; ++i)
            {
                if (! keep (firstInstrument + i))
                    continue;
                frames << (firstInst ? "" : ",\n") << "      \"" << c.names[(size_t) (firstInstrument + i)].replace ("\"", "'") << "\": [";
                firstInst = false;
                for (size_t k = 0; k < frameList.size(); ++k)
                    frames << (k > 0 ? "," : "") << juce::String (c.frameDuckDb[(size_t) frameList[k] * numInstruments + (size_t) i], 1);
                frames << "]";
            }
            frames << "\n    }";
        }
        if (o.modulation)
        {
            if (! c.frameLimiterDb.empty())
            {
                frames << ",\n    \"limiter_reduction_db\": [";
                for (size_t i = 0; i < frameList.size(); ++i)
                    frames << (i > 0 ? "," : "") << juce::String (c.frameLimiterDb[(size_t) frameList[i]], 1);
                frames << "]";
            }
            const auto sourceCount = (int) c.modSourceNames.size(), targetCount = (int) c.modTargetIds.size();
            if (sourceCount > 0)
            {
                frames << ",\n    \"modulation_sources\": {\n";
                for (int j = 0; j < sourceCount; ++j)
                {
                    frames << (j > 0 ? ",\n" : "") << "      \"" << c.modSourceNames[(size_t) j].replace ("\"", "'") << "\": [";
                    for (size_t i = 0; i < frameList.size(); ++i)
                        frames << (i > 0 ? "," : "") << juce::String (c.frameModSources[(size_t) frameList[i] * (size_t) sourceCount + (size_t) j], 3);
                    frames << "]";
                }
                frames << "\n    }";
            }
            if (targetCount > 0)
            {
                frames << ",\n    \"modulated_parameters\": {\n";
                for (int j = 0; j < targetCount; ++j)
                {
                    frames << (j > 0 ? ",\n" : "") << "      \"" << c.modTargetIds[(size_t) j] << "\": [";
                    for (size_t i = 0; i < frameList.size(); ++i)
                        frames << (i > 0 ? "," : "") << juce::String (c.frameModTargets[(size_t) frameList[i] * (size_t) targetCount + (size_t) j], 3);
                    frames << "]";
                }
                frames << "\n    }";
            }
        }
        frames << "\n  }\n}";

        const auto end = json.lastIndexOfChar ('}');
        return json.substring (0, end).trimEnd() + frames;
    }

    bool Report::saveExport (const juce::File& file, const ExportOptions& o, const ExportFilter& filter) const
    {
        file.getParentDirectory().createDirectory();
        return file.replaceWithText (toExportJson (o, filter), false, false, "\n");
    }

    //==========================================================================================
    std::shared_ptr<const Reference> loadReference (const juce::File& file, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            error = "Could not read " + file.getFileName() + " (WAV, AIFF, FLAC, OGG or MP3).";
            return nullptr;
        }

        const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120.0));
        if (length < fftSize * 2)
        {
            error = "The reference is too short (at least a few seconds).";
            return nullptr;
        }
        juce::AudioBuffer<float> audio ((int) std::max (1u, reader->numChannels), length);
        reader->read (&audio, 0, length, 0, true, true);
        std::vector<float> mono ((size_t) length, 0.0f);
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            for (int i = 0; i < length; ++i)
                mono[(size_t) i] += audio.getSample (ch, i) / (float) audio.getNumChannels();

        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
        std::array<float, fftSize * 2> data {};
        auto reference = std::make_shared<Reference>();
        reference->name = file.getFileNameWithoutExtension();
        reference->rate = reader->sampleRate;
        reference->power.assign ((size_t) fftSize / 2, 0.0f);

        auto used = 0;
        for (int start = 0; start + fftSize <= length; start += fftSize / 2)
        {
            double energy = 0.0;
            for (int i = 0; i < fftSize; ++i)
            {
                data[(size_t) i] = mono[(size_t) (start + i)];
                energy += (double) data[(size_t) i] * data[(size_t) i];
            }
            if (std::sqrt (energy / fftSize) < 0.003) // under ~-50 dBFS: paus, räknas inte
                continue;
            std::fill (data.begin() + fftSize, data.end(), 0.0f);
            window.multiplyWithWindowingTable (data.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform (data.data(), true);
            for (int b = 0; b < fftSize / 2; ++b)
            {
                const auto m = data[(size_t) b] * binNorm;
                reference->power[(size_t) b] += m * m;
            }
            ++used;
        }
        if (used == 0)
        {
            error = "The reference is silent.";
            return nullptr;
        }
        for (auto& p : reference->power)
            p /= (float) used;
        return reference;
    }

    //==========================================================================================
    namespace
    {
        // Bandpass (högpass lowHz, lågpass highHz; 0 = av), två biquads per gräns.
        std::vector<float> bandPass (const std::vector<float>& x, int n, double rate, double lowHz, double highHz)
        {
            using Filter = juce::dsp::IIR::Filter<float>;
            using Coeffs = juce::dsp::IIR::Coefficients<float>;
            std::vector<Filter> chain;
            for (int i = 0; i < 2 && lowHz > 0.0; ++i)
                chain.emplace_back (Coeffs::makeHighPass (rate, (float) lowHz));
            for (int i = 0; i < 2 && highHz > 0.0; ++i)
                chain.emplace_back (Coeffs::makeLowPass (rate, (float) highHz));
            std::vector<float> y ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                auto v = x[(size_t) i];
                for (auto& f : chain)
                    v = f.processSample (v);
                y[(size_t) i] = v;
            }
            return y;
        }

        float toDb (double ratio, double floorValue = 1.0e-6) { return (float) (20.0 * std::log10 (std::max (ratio, floorValue))); }
    }

    PhaseMetrics measurePhase (const PhaseInput& in)
    {
        PhaseMetrics m;
        const auto n = (int) std::min ({ in.kickL.size(), in.kickR.size(), in.bassL.size(), in.bassR.size(), in.masterL.size(), in.masterR.size() });
        if (n <= 0 || in.samplesPerBar <= 0.0 || in.bars <= 0 || in.start >= n)
            return m;
        m.valid = true;

        std::vector<float> kick ((size_t) n), bass ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            kick[(size_t) i] = 0.5f * (in.kickL[(size_t) i] + in.kickR[(size_t) i]);
            bass[(size_t) i] = 0.5f * (in.bassL[(size_t) i] + in.bassR[(size_t) i]);
        }
        const auto kickLow = bandPass (kick, n, in.rate, 30.0, 150.0), bassLow = bandPass (bass, n, in.rate, 30.0, 150.0);

        // Per 16-del: korrelation och summa/differens-energi.
        const auto per16 = in.samplesPerBar / 16.0;
        for (int i = 0; i < in.bars * 16; ++i)
        {
            const auto from = std::min (n, in.start + (int) std::round (i * per16)), to = std::min (n, in.start + (int) std::round ((i + 1) * per16));
            double kk = 0.0, bb = 0.0, kb = 0.0;
            for (int s = from; s < to; ++s)
            {
                const double k = kickLow[(size_t) s], b = bassLow[(size_t) s];
                kk += k * k;
                bb += b * b;
                kb += k * b;
            }
            const auto len = (double) std::max (1, to - from);
            const auto active = kk / len > 1.0e-6 && bb / len > 1.0e-6; // över ungefär -60 dB RMS
            m.lowActive.push_back (active ? 1 : 0);
            m.lowWeight.push_back ((float) (std::sqrt (kk * bb) / len));
            m.lowKickDb.push_back ((float) (10.0 * std::log10 (kk / len + 1.0e-12)));
            m.lowBassDb.push_back ((float) (10.0 * std::log10 (bb / len + 1.0e-12)));
            m.lowKickHit.push_back (std::any_of (in.onsets.begin(), in.onsets.end(), [from, to] (int o) { return o >= from && o < to; }) ? 1 : 0);
            m.lowCorrelation.push_back (active ? (float) juce::jlimit (-1.0, 1.0, kb / std::sqrt (kk * bb)) : 0.0f);
            m.lowSumDiffDb.push_back (active ? (float) (10.0 * std::log10 ((kk + bb + 2.0 * kb + 1.0e-12) / (kk + bb - 2.0 * kb + 1.0e-12))) : 0.0f);
        }

        // Summans topp mot den största enskilda toppen.
        {
            double peakSum = 0.0, peakKick = 0.0, peakBass = 0.0;
            for (int s = in.start; s < n; ++s)
            {
                const double k = kickLow[(size_t) s], b = bassLow[(size_t) s];
                peakSum = std::max (peakSum, std::abs (k + b));
                peakKick = std::max (peakKick, std::abs (k));
                peakBass = std::max (peakBass, std::abs (b));
            }
            const auto largest = std::max (peakKick, peakBass);
            m.sumVsMaxDb = largest > 1.0e-4 ? toDb (peakSum / largest) : 0.0f;
        }

        // Per slag: topp, första utslagets tecken och värdet där slaget slutar (klick om det inte är nära noll).
        {
            auto lowest = 1000.0f, highest = -1000.0f;
            for (size_t i = 0; i < in.onsets.size(); ++i)
            {
                const auto from = in.onsets[i];
                auto to = std::min (n, from + (int) (0.5 * in.rate));
                if (i + 1 < in.onsets.size())
                    to = std::min (to, in.onsets[i + 1]);
                if (from < 0 || to - from < 64)
                    continue;
                PhaseMetrics::Hit hit;
                float peak = 0.0f;
                for (int s = from; s < to; ++s)
                    peak = std::max (peak, std::abs (kick[(size_t) s]));
                hit.peakDb = toDb (peak, 1.0e-6);
                for (int s = from; s < to && hit.polarity == 0; ++s)
                    if (std::abs (kick[(size_t) s]) > 0.1f * peak)
                        hit.polarity = kick[(size_t) s] > 0.0f ? 1 : -1;
                hit.endDb = toDb (std::abs (kick[(size_t) to - 1]), 1.0e-6);
                if (hit.endDb > -60.0f)
                    ++m.clickHits;
                if (hit.peakDb > -60.0f)
                {
                    lowest = std::min (lowest, hit.peakDb);
                    highest = std::max (highest, hit.peakDb);
                }
                m.hits.push_back (hit);
            }
            m.peakSpreadDb = highest >= lowest ? highest - lowest : 0.0f;
        }

        // Vyns underlag: första slaget som hörs (högst en takts kvart, 0,5 s), vågform och spektrum.
        for (size_t i = 0; i < m.hits.size() && i < in.onsets.size(); ++i)
        {
            if (m.hits[i].peakDb <= -60.0f)
                continue;
            const auto from = in.onsets[i];
            auto to = std::min (n, from + std::min ((int) (in.samplesPerBar / 4.0), (int) (0.5 * in.rate)));
            if (i + 1 < in.onsets.size())
                to = std::min (to, in.onsets[i + 1]);
            const auto len = to - from;
            if (len < 256)
                break;
            m.scopeStep = 8;
            m.scopeRate = in.rate / m.scopeStep;
            for (int s = from; s + m.scopeStep <= to; s += m.scopeStep)
            {
                float k = 0.0f, b = 0.0f;
                for (int j = 0; j < m.scopeStep; ++j)
                {
                    k += kick[(size_t) (s + j)];
                    b += bass[(size_t) (s + j)];
                }
                m.scopeKick.push_back (k / (float) m.scopeStep);
                m.scopeBass.push_back (b / (float) m.scopeStep);
            }

            constexpr int order = 14, fftSize = 1 << order;
            juce::dsp::FFT fft (order);
            juce::dsp::WindowingFunction<float> window ((size_t) std::min (len, fftSize), juce::dsp::WindowingFunction<float>::hann);
            const auto used = std::min (len, fftSize);
            const auto binHz = (float) (in.rate / fftSize);
            const auto firstBin = (int) std::ceil (20.0f / binHz), lastBin = (int) std::floor (300.0f / binHz);
            m.spectrumHz0 = (float) firstBin * binHz;
            m.spectrumStepHz = binHz;
            std::vector<float> data ((size_t) fftSize * 2);
            auto spectrum = [&] (auto sample, std::vector<float>& out)
            {
                std::fill (data.begin(), data.end(), 0.0f);
                for (int s = 0; s < used; ++s)
                    data[(size_t) s] = sample (from + s);
                window.multiplyWithWindowingTable (data.data(), (size_t) used);
                fft.performFrequencyOnlyForwardTransform (data.data(), true);
                for (int bin = firstBin; bin <= lastBin; ++bin)
                    out.push_back (toDb ((double) data[(size_t) bin] * 4.0 / used));
            };
            spectrum ([&] (int s) { return kick[(size_t) s]; }, m.spectrumKick);
            spectrum ([&] (int s) { return bass[(size_t) s]; }, m.spectrumBass);
            spectrum ([&] (int s) { return kick[(size_t) s] + bass[(size_t) s]; }, m.spectrumSum);
            break;
        }

        // Stereo per takt i fyra band: L/R-korrelation och M/S.
        {
            const double edges[PhaseMetrics::stereoBands + 1] = { 20.0, 120.0, 500.0, 4000.0, 0.0 };
            std::vector<float> bandL[PhaseMetrics::stereoBands], bandR[PhaseMetrics::stereoBands];
            for (int b = 0; b < PhaseMetrics::stereoBands; ++b)
            {
                const auto lowHz = edges[b], highHz = b == PhaseMetrics::stereoBands - 1 ? 0.0 : edges[b + 1];
                bandL[b] = bandPass (in.masterL, n, in.rate, lowHz, highHz);
                bandR[b] = bandPass (in.masterR, n, in.rate, lowHz, highHz);
            }
            for (int bar = 0; bar < in.bars; ++bar)
            {
                const auto from = std::min (n, in.start + (int) std::round (bar * in.samplesPerBar)), to = std::min (n, in.start + (int) std::round ((bar + 1) * in.samplesPerBar));
                std::array<float, PhaseMetrics::stereoBands> corr {}, ms {};
                for (int b = 0; b < PhaseMetrics::stereoBands; ++b)
                {
                    double ll = 0.0, rr = 0.0, lr = 0.0;
                    for (int s = from; s < to; ++s)
                    {
                        const double l = bandL[b][(size_t) s], r = bandR[b][(size_t) s];
                        ll += l * l;
                        rr += r * r;
                        lr += l * r;
                    }
                    const auto len = (double) std::max (1, to - from);
                    const auto silent = ll / len < stereoSilentPower || rr / len < stereoSilentPower;
                    corr[(size_t) b] = silent ? 1.0f : (float) juce::jlimit (-1.0, 1.0, lr / std::sqrt (ll * rr));
                    const auto mid = 0.25 * (ll + rr + 2.0 * lr), side = 0.25 * (ll + rr - 2.0 * lr);
                    ms[(size_t) b] = (float) (10.0 * std::log10 ((mid + 1.0e-12) / (side + 1.0e-12)));
                }
                m.stereoCorrelation.push_back (corr);
                m.midSideDb.push_back (ms);
            }
        }
        return m;
    }

    //==========================================================================================
    std::unique_ptr<PluginBase> prepareCopy (PluginBase& source)
    {
        auto copy = source.cloneForOffline();
        juce::MemoryBlock state;
        source.getStateInformation (state);
        copy->setStateInformation (state.getData(), (int) state.getSize());
        copy->ensureTablesInUse();
        return copy;
    }

    void applyTap (PluginBase& copy, int tap)
    {
        if (tap != 1)
            return;

        auto set = [&copy] (const juce::String& id, float value)
        {
            if (auto* parameter = copy.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        for (int source = 0; source <= numInstruments; ++source)
        {
            const auto prefix = SourceEq::prefixFor (source);
            set (prefix + "EqHp", 20.0f);
            set (prefix + "EqLp", 20000.0f);
            for (int band = 1; band <= Effects::eqBands; ++band)
                set (prefix + "Eq" + juce::String (band) + "G", 0.0f);
            if (source > 0)
                set (prefix + "Duck", 0.0f);
        }
    }

    const char* shareBandName (int band)
    {
        static const char* names[shareBands] = { "20-60", "60-150", "150-500", "500-2k", "2k-20k" };
        return names[juce::jlimit (0, shareBands - 1, band)];
    }

    void muteSource (PluginBase& copy, int source)
    {
        const auto id = source == kickSource ? juce::String (ParamID::kickLevel) : ParamID::bass (source - firstInstrument, InstrumentField::level);
        if (auto* parameter = copy.apvts.getParameter (id))
            parameter->setValueNotifyingHost (0.0f);
    }

    namespace
    {
        constexpr double analysisRate = 48000.0;
        constexpr int analysisBlock = 512;
    }

    void renderMaster (PluginBase& copy, double bpm, int keyNote, int bars, const std::function<bool()>& cancelled,
                       std::vector<float>& left, std::vector<float>& right)
    {
        std::vector<float> tapL, tapR;
        OfflineRender::Settings settings;
        settings.sampleRate = analysisRate;
        settings.blockSize = analysisBlock;
        settings.bpm = bpm;
        settings.bars = bars;
        settings.notes = { keyNote >= 0 ? keyNote : 60 };
        settings.extraBars = 0;
        settings.shouldStop = cancelled;
        settings.onBlock = [&] (const juce::AudioBuffer<float>&, int, int n)
        {
            copy.getMasterSection().outputTap.copyLatest (n, tapL, tapR);
            tapL.resize ((size_t) n, 0.0f);
            tapR.resize ((size_t) n, 0.0f);
            left.insert (left.end(), tapL.begin(), tapL.end());
            right.insert (right.end(), tapR.begin(), tapR.end());
        };
        OfflineRender::render (copy, settings);
    }

    void addAfterMaster (Report& report, const Stems& stems, std::vector<std::pair<int, std::unique_ptr<PluginBase>>>& mutedCopies,
                         double bpm, int keyNote, const std::function<void (float)>& progress, const std::function<bool()>& cancelled)
    {
        static constexpr float bandEdges[shareBands + 1] = { 20.0f, 60.0f, 150.0f, 500.0f, 2000.0f, 0.0f };

        // Nivåer över hela analysfönstret: RMS och topp (dBFS) och RMS i fem band.
        auto measure = [] (const std::vector<float>& x, float& rmsDb, float& peakDb, std::array<float, shareBands>& bandDb)
        {
            auto level = [] (const std::vector<float>& v, float& rms, float* peak)
            {
                double sum = 0.0;
                float top = 0.0f;
                for (auto s : v)
                {
                    sum += (double) s * s;
                    top = std::max (top, std::abs (s));
                }
                rms = toDb (std::sqrt (sum / (double) std::max<size_t> (1, v.size())));
                if (peak != nullptr)
                    *peak = toDb (top);
            };
            level (x, rmsDb, &peakDb);
            for (int b = 0; b < shareBands; ++b)
            {
                const auto filtered = bandPass (x, (int) x.size(), analysisRate, bandEdges[b], bandEdges[b + 1]);
                level (filtered, bandDb[(size_t) b], nullptr);
            }
        };

        std::vector<float> fullL = stems.masterL, fullR = stems.masterR;
        auto done = 0;
        for (auto& [source, muted] : mutedCopies)
        {
            if (cancelled && cancelled())
                return;
            std::vector<float> mutedL, mutedR;
            renderMaster (*muted, bpm, keyNote, stems.totalBars, cancelled, mutedL, mutedR);
            muted.reset();
            const auto from = (size_t) std::max (0, stems.start);
            const auto n = std::min ({ fullL.size(), mutedL.size(), mutedR.size() });
            if (n <= from || stems.heard[(size_t) source].empty())
                continue;
            std::vector<float> contribution (n - from);
            for (size_t i = from; i < n; ++i)
                contribution[i - from] = 0.5f * ((fullL[i] - mutedL[i]) + (fullR[i] - mutedR[i]));

            MasterShare share;
            share.name = source == kickSource ? "Kick" : report.sources.size() > (size_t) source ? report.sources[(size_t) source].name : "Instr " + juce::String (source);
            measure (contribution, share.masterRmsDb, share.masterPeakDb, share.masterBandDb);
            const auto& heard = stems.heard[(size_t) source];
            measure (std::vector<float> (heard.begin() + (long) std::min (from, heard.size()), heard.end()), share.heardRmsDb, share.heardPeakDb, share.heardBandDb);
            report.afterMaster.push_back (share);
            if (progress)
                progress (0.5f + 0.5f * (float) ++done / (float) std::max<size_t> (1, mutedCopies.size()));
        }
        mutedCopies.clear();

        // Fynd: där mastern (limiter, kompressor, effekter) skär ett band mycket mer än resten av källans ljud.
        for (const auto& share : report.afterMaster)
        {
            const auto overall = share.masterRmsDb - share.heardRmsDb;
            if (share.heardRmsDb < -60.0f)
                continue;
            for (int b = 0; b < shareBands; ++b)
            {
                if (share.heardBandDb[(size_t) b] < share.heardRmsDb - 20.0f)
                    continue;
                const auto relative = (share.masterBandDb[(size_t) b] - share.heardBandDb[(size_t) b]) - overall;
                if (relative > -4.0f)
                    continue;
                Finding finding;
                finding.severity = relative < -8.0f ? Severity::warning : Severity::info;
                finding.category = "master";
                finding.title = share.name + ": the master cuts " + juce::String (shareBandName (b)) + " Hz more than the rest";
                finding.detail = "In the final mix this band ends " + juce::String (-relative, 1) + " dB lower, relative to the rest of the source, than before the mixer and master (overall change "
                                 + juce::String (overall, 1) + " dB). The limiter, compressor or an effect is taking it. Check the master and the channel it is routed to.";
                finding.sources.add (share.name);
                finding.lowHz = bandEdges[b];
                finding.highHz = b + 1 < shareBands ? bandEdges[b + 1] : 20000.0f;
                report.findings.push_back (finding);
            }
        }
    }

    Report run (PluginBase& copy, double bpm, int keyNote, const Options& options, const Reference* reference,
                const std::function<void (float)>& progress, const std::function<bool()>& cancelled, Stems* stems)
    {
        constexpr double rate = analysisRate;
        constexpr int blockSize = analysisBlock;

        Capture capture;
        capture.rate = rate;
        capture.bpm = bpm;
        const auto active = copy.activeInstruments();
        capture.present[(size_t) kickSource] = true;
        capture.names[(size_t) kickSource] = "Kick";
        for (int i = 0; i < numInstruments; ++i)
        {
            capture.present[(size_t) (firstInstrument + i)] = i < active;
            capture.names[(size_t) (firstInstrument + i)] = copy.getLayerName (i);
        }
        capture.present[(size_t) masterSource] = true;
        capture.names[(size_t) masterSource] = "Master";

        const auto lengths = Params::readPatternLengths (copy.apvts, copy.pattern, copy.activeInstruments());
        const auto total = std::max (1, lengths.total);
        const auto analyseBars = std::max (1, std::min (total, options.maxBars));
        auto preRoll = total;
        while (preRoll < 2)
            preRoll += total;
        capture.bars = analyseBars;

        const auto samplesPerBar = rate * 60.0 / bpm * 4.0;
        const auto preRollSamples = (int) std::round (preRoll * samplesPerBar);
        const auto totalSamples = (int) ((preRoll + analyseBars) * samplesPerBar);

        // Rådata per ram, samlas i listor och läggs sedan i Capture.
        std::vector<float> bandRows, lowRows, peakRows, rmsRows, duckRows;
        std::array<float, numInstruments> duckSinceFrame;
        duckSinceFrame.fill (1.0f);
        auto limiterSinceFrame = 1.0f;
        std::vector<float> limiterRows, modSourceRows, modTargetRows;

        // Modulationen som är kopplad: vilka källor och vilka parametrar, så att deras värden kan följas genom loopen.
        std::vector<int> routeSources, routeTargets;
        {
            const auto& parameterList = copy.getParameters();
            for (const auto& route : copy.modRoutes.routes)
            {
                const auto source = route.source.load(), target = route.target.load();
                if (source < 0 || ! juce::isPositiveAndBelow (target, parameterList.size()))
                    continue;
                if (std::find (routeSources.begin(), routeSources.end(), source) == routeSources.end())
                    routeSources.push_back (source);
                if (std::find (routeTargets.begin(), routeTargets.end(), target) == routeTargets.end())
                    routeTargets.push_back (target);
            }
        }
        std::vector<int> barRows;
        std::vector<double> frameSecondsRows;
        std::vector<float> barPower ((size_t) analyseBars * numSources * (size_t) (fftSize / 2), 0.0f);
        std::vector<int> barFrames ((size_t) analyseBars, 0);
        const auto nb = numBands();
        const auto map = binBands (rate);
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
        std::array<float, fftSize * 2> data {};
        std::vector<float> left, right, tapL, tapR;
        PhaseInput phaseInput;
        std::vector<std::array<float, numInstruments>> duckMin;
        std::vector<int> onsets; // sample
        std::vector<float> kickSignal; // hela kickens signal (mono), för svansmätningen
        auto previousKickPeak = 0.0f;
        auto lastOnsetBlock = -100;
        auto limiterMin = 1.0f;
        auto nextFrame = 0;

        copy.instrumentTapsOn = true;
        if (stems != nullptr)
        {
            stems->start = preRollSamples;
            stems->totalBars = preRoll + analyseBars;
        }

        OfflineRender::Settings settings;
        settings.sampleRate = rate;
        settings.blockSize = blockSize;
        settings.bpm = bpm;
        settings.bars = preRoll + analyseBars;
        settings.notes = { keyNote >= 0 ? keyNote : 60 };
        settings.extraBars = 0;
        settings.shouldStop = cancelled;
        settings.onBlock = [&] (const juce::AudioBuffer<float>&, int done, int n)
        {
            const auto end = done + n;

            // Kickens slag: bladets topp hoppar upp jämfört med förra blocket (ett nytt slag börjar nära full nivå även när det förra slagets svans
            // ännu klingar, så en enkel tystnadströskel missar slag som ligger tätt).
            copy.kickTap.copyLatest (n, left, right);
            if (stems != nullptr)
            {
                auto& kickHeard = stems->heard[(size_t) kickSource];
                for (size_t k = 0; k < left.size(); ++k)
                    kickHeard.push_back (0.5f * (left[k] + right[k]));
            }
            auto peak = 0.0f;
            for (auto v : left)
                peak = std::max (peak, std::abs (v));
            for (size_t k = 0; k < left.size(); ++k)
                kickSignal.push_back (0.5f * (left[k] + right[k]));
            phaseInput.kickL.insert (phaseInput.kickL.end(), left.begin(), left.end());
            phaseInput.kickR.insert (phaseInput.kickR.end(), right.begin(), right.end());
            {
                // Basen = summan av instrumentens signaler, mastern = utgången (för fas- och stereomåtten).
                std::vector<float> sumL (left.size(), 0.0f), sumR (left.size(), 0.0f);
                for (int i = 0; i < std::min (active, numInstruments); ++i)
                {
                    copy.instrumentTaps[(size_t) i].copyLatest (n, tapL, tapR);
                    if (stems != nullptr)
                        for (size_t k = 0; k < tapL.size(); ++k)
                            stems->heard[(size_t) (firstInstrument + i)].push_back (0.5f * (tapL[k] + tapR[k]));
                    for (size_t k = 0; k < sumL.size() && k < tapL.size(); ++k)
                    {
                        sumL[k] += tapL[k];
                        sumR[k] += tapR[k];
                    }
                }
                phaseInput.bassL.insert (phaseInput.bassL.end(), sumL.begin(), sumL.end());
                phaseInput.bassR.insert (phaseInput.bassR.end(), sumR.begin(), sumR.end());
                copy.getMasterSection().outputTap.copyLatest (n, tapL, tapR);
                tapL.resize (sumL.size(), 0.0f);
                tapR.resize (sumL.size(), 0.0f);
                if (stems != nullptr)
                {
                    stems->masterL.insert (stems->masterL.end(), tapL.begin(), tapL.end());
                    stems->masterR.insert (stems->masterR.end(), tapR.begin(), tapR.end());
                }
                phaseInput.masterL.insert (phaseInput.masterL.end(), tapL.begin(), tapL.end());
                phaseInput.masterR.insert (phaseInput.masterR.end(), tapR.begin(), tapR.end());
            }
            const auto blockIndex = done / blockSize;
            if (peak > 0.15f && peak > previousKickPeak * 1.8f + 0.05f && blockIndex - lastOnsetBlock > 3)
            {
                // Samplet där slaget börjar (första som är över 40 % av blockets topp), så att takt och taktslag blir rätt.
                auto offset = 0;
                while (offset < (int) left.size() - 1 && std::abs (left[(size_t) offset]) < 0.4f * peak)
                    ++offset;
                if (done + offset >= preRollSamples - 48)
                    onsets.push_back (std::max (preRollSamples, done + offset));
                lastOnsetBlock = blockIndex;
            }
            previousKickPeak = peak;

            // Duckningens förstärkning (lägsta) per instrument och block.
            std::array<float, numInstruments> minimum;
            minimum.fill (1.0f);
            for (int i = 0; i < std::min (active, numInstruments); ++i)
            {
                copy.duckGainTaps[(size_t) i].copyLatest (n, left, right);
                for (auto v : left)
                    minimum[(size_t) i] = std::min (minimum[(size_t) i], v);
            }
            duckMin.push_back (minimum);
            for (int i = 0; i < numInstruments; ++i)
                duckSinceFrame[(size_t) i] = std::min (duckSinceFrame[(size_t) i], minimum[(size_t) i]);

            if (done >= preRollSamples)
            {
                copy.getMasterSection().gainTap.copyLatest (n, left, right);
                for (auto v : left)
                {
                    limiterMin = std::min (limiterMin, v);
                    limiterSinceFrame = std::min (limiterSinceFrame, v);
                }
            }

            if (progress)
                progress (0.95f * (float) end / (float) totalSamples);

            if (end < nextFrame)
                return;
            nextFrame = end + hopSamples;
            const auto centre = end - fftSize / 2;
            if (centre < preRollSamples)
                return;
            const auto bar = (int) std::floor ((centre - preRollSamples) / samplesPerBar);
            if (bar < 0 || bar >= analyseBars)
                return;

            barRows.push_back (bar);
            for (int i = 0; i < numInstruments; ++i)
            {
                duckRows.push_back (-20.0f * std::log10 (std::max (duckSinceFrame[(size_t) i], 1.0e-3f)));
                duckSinceFrame[(size_t) i] = 1.0f;
            }
            limiterRows.push_back (-20.0f * std::log10 (std::max (limiterSinceFrame, 1.0e-3f)));
            limiterSinceFrame = 1.0f;
            for (const auto source : routeSources)
                modSourceRows.push_back (copy.modulator.sourceValue (source));
            for (const auto target : routeTargets)
            {
                auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (copy.getParameters()[target]);
                modTargetRows.push_back (parameter != nullptr ? parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->getValue() + copy.modulator.currentOffset (target))) : 0.0f);
            }
            const auto levelOffset = peakRows.size();
            peakRows.resize (levelOffset + (size_t) numSources, -120.0f);
            rmsRows.resize (levelOffset + (size_t) numSources, -120.0f);
            frameSecondsRows.push_back ((double) (centre - preRollSamples) / rate);
            ++barFrames[(size_t) bar];
            const auto bandOffset = bandRows.size(), lowOffset = lowRows.size();
            bandRows.resize (bandOffset + (size_t) numSources * (size_t) nb, -120.0f);
            lowRows.resize (lowOffset + (size_t) numSources * lowBins, 0.0f);
            for (int src = 0; src < numSources; ++src)
            {
                if (! capture.present[(size_t) src])
                    continue;
                if (src == kickSource)
                    copy.kickTap.copyLatest (fftSize, left, right);
                else if (src == masterSource)
                    copy.getMasterSection().outputTap.copyLatest (fftSize, left, right);
                else
                    copy.instrumentTaps[(size_t) (src - firstInstrument)].copyLatest (fftSize, left, right);

                auto peakValue = 0.0f;
                auto sumSquares = 0.0;
                for (int k = 0; k < fftSize; ++k)
                {
                    data[(size_t) k] = 0.5f * (left[(size_t) k] + right[(size_t) k]);
                    peakValue = std::max (peakValue, std::abs (data[(size_t) k]));
                    sumSquares += (double) data[(size_t) k] * data[(size_t) k];
                }
                peakRows[levelOffset + (size_t) src] = juce::Decibels::gainToDecibels (peakValue, -120.0f);
                rmsRows[levelOffset + (size_t) src] = (float) (10.0 * std::log10 (sumSquares / fftSize + 1.0e-12));
                std::fill (data.begin() + fftSize, data.end(), 0.0f);
                window.multiplyWithWindowingTable (data.data(), fftSize);
                fft.performFrequencyOnlyForwardTransform (data.data(), true);

                std::vector<double> power ((size_t) nb, 0.0);
                for (int k = 1; k < fftSize / 2; ++k)
                {
                    const auto m = data[(size_t) k] * binNorm;
                    const auto p = m * m;
                    if (map[(size_t) k] >= 0)
                        power[(size_t) map[(size_t) k]] += p;
                    if (k < lowBins)
                        lowRows[lowOffset + (size_t) src * lowBins + (size_t) k] = p;
                    barPower[((size_t) bar * numSources + (size_t) src) * (size_t) (fftSize / 2) + (size_t) k] += p;
                }
                for (int b = 0; b < nb; ++b)
                    bandRows[bandOffset + (size_t) src * (size_t) nb + (size_t) b] = (float) (10.0 * std::log10 (power[(size_t) b] / lobePower + 1.0e-12));
            }
        };

        OfflineRender::render (copy, settings);
        copy.instrumentTapsOn = false;

        if (cancelled && cancelled())
        {
            Report report;
            report.error = "Cancelled.";
            return report;
        }

        capture.resize ((int) barRows.size());
        capture.bandDb = bandRows;
        capture.lowPower = lowRows;
        capture.frameBar = barRows;
        capture.frameSeconds = frameSecondsRows;
        capture.framePeakDb = peakRows;
        capture.frameRmsDb = rmsRows;
        capture.frameDuckDb = duckRows;
        capture.frameLimiterDb = limiterRows;
        capture.frameModSources = modSourceRows;
        capture.frameModTargets = modTargetRows;
        for (const auto source : routeSources)
            capture.modSourceNames.push_back (Modulation::sourceName (source));
        for (const auto target : routeTargets)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (copy.getParameters()[target]))
                capture.modTargetIds.push_back (withId->paramID);
            else
                capture.modTargetIds.push_back ("?");
        capture.barPower = std::move (barPower);
        capture.barFrameCount = barFrames;
        for (const auto onset : onsets)
            capture.kickOnsetSeconds.push_back ((double) (onset - preRollSamples) / rate);

        // Kickens svansfrekvens: i fönstret 120-340 ms efter varje slag (före nästa slag), median över slagen.
        {
            std::vector<float> estimates;
            const auto step = std::max<size_t> (1, onsets.size() / 24);
            for (size_t i = 0; i < onsets.size(); i += step)
            {
                const auto from = onsets[i] + (int) (0.12 * rate);
                auto to = onsets[i] + (int) (0.34 * rate);
                if (i + 1 < onsets.size())
                    to = std::min (to, onsets[i + 1] - (int) (0.01 * rate));
                to = std::min (to, (int) kickSignal.size());
                if (to - from < (int) (0.06 * rate))
                    continue;
                if (const auto hz = periodHz (kickSignal, from, to, rate); hz > 0.0f)
                    estimates.push_back (hz);
            }
            if (! estimates.empty())
            {
                std::sort (estimates.begin(), estimates.end());
                capture.kickTailMeasuredHz = estimates[estimates.size() / 2];
            }

            const auto follow = copy.apvts.getRawParameterValue (ParamID::kickKeyFollow)->load() >= 0.5f;
            const auto tune = copy.apvts.getRawParameterValue (ParamID::kickTune)->load();
            const auto pitchClass = ((keyNote >= 0 ? keyNote : 60) % 12 + 12) % 12;
            capture.kickTailDesignHz = Params::midiToHz ((float) (follow ? Params::kickTailMidiNote (pitchClass) : 24) + tune);
        }
        for (int i = 0; i < numInstruments; ++i)
        {
            using F = InstrumentField;
            auto value = [&] (F f) { return copy.apvts.getRawParameterValue (ParamID::bass (i, f))->load(); };
            if (value (F::on) < 0.5f)
                continue;
            for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
                if (value (ParamID::oscField (osc, F::aOn)) >= 0.5f && value (ParamID::oscField (osc, F::aLevel)) > 0.05f)
                    capture.randomPhase[(size_t) i] = std::max (capture.randomPhase[(size_t) i], value (ParamID::oscField (osc, F::aRandom)));
        }
        capture.limiterMaxReductionDb = limiterMin < 0.999f ? -20.0f * std::log10 (std::max (limiterMin, 1.0e-3f)) : 0.0f;

        for (const auto onset : onsets)
        {
            const auto first = (size_t) (onset / blockSize);
            const auto last = std::min (duckMin.size() - 1, first + (size_t) std::ceil (0.1 * rate / blockSize));
            std::array<float, numInstruments> depth {};
            for (int i = 0; i < numInstruments; ++i)
            {
                auto lowest = 1.0f;
                for (size_t b = first; b <= last && b < duckMin.size(); ++b)
                    lowest = std::min (lowest, duckMin[b][(size_t) i]);
                depth[(size_t) i] = -20.0f * std::log10 (std::max (lowest, 1.0e-3f));
            }
            capture.duckDepthDb.push_back (depth);
        }

        if (! (cancelled && cancelled()))
        {
            phaseInput.onsets = onsets;
            phaseInput.start = preRollSamples;
            phaseInput.bars = analyseBars;
            phaseInput.samplesPerBar = samplesPerBar;
            phaseInput.rate = rate;
            capture.phase = measurePhase (phaseInput);
        }

        auto report = evaluate (capture, options, reference);
        report.capture = std::make_shared<const Capture> (std::move (capture));
        if (progress)
            progress (1.0f);
        return report;
    }
}

//==========================================================================================
MixAnalyser::MixAnalyser() : juce::Thread ("Kickbase mix analysis") {}

MixAnalyser::~MixAnalyser()
{
    stopThread (30000);
}

bool MixAnalyser::start (PluginBase& source, const MixAnalysis::Options& newOptions, std::shared_ptr<const MixAnalysis::Reference> newReference, juce::var newSynthState)
{
    if (isThreadRunning())
        return false;
    copy.reset();
    options = newOptions;
    reference = std::move (newReference);
    synthState = std::move (newSynthState);
    bpm = std::max (40.0, (double) source.transportStatus.bpm.load());
    keyNote = source.transportStatus.keyNote.load();
    copy = MixAnalysis::prepareCopy (source);
    MixAnalysis::applyTap (*copy, options.tap);
    mutedCopies.clear();
    if (options.tap == 2)
    {
        // En kopia per källa med källan tystad: mastern minus mastern utan källan = vad källan bidrar med i slutmixen.
        for (int s = MixAnalysis::kickSource; s < MixAnalysis::masterSource; ++s)
        {
            if (s != MixAnalysis::kickSource && s - MixAnalysis::firstInstrument >= source.activeInstruments())
                break;
            auto muted = MixAnalysis::prepareCopy (source);
            MixAnalysis::muteSource (*muted, s);
            mutedCopies.emplace_back (s, std::move (muted));
        }
    }
    progress = 0.0f;
    running = true;
    startThread (juce::Thread::Priority::background); // Windows THREAD_PRIORITY_IDLE: bara CPU som annars står oanvänd (fråga 3), DAW:en går före
    return true;
}

void MixAnalyser::cancel()
{
    signalThreadShouldExit();
}

MixAnalysis::Report MixAnalyser::getReport() const
{
    const juce::ScopedLock sl (lock);
    return report;
}

void MixAnalyser::run()
{
    const auto afterMaster = options.tap == 2;
    MixAnalysis::Stems stems;
    auto result = MixAnalysis::run (*copy, bpm, keyNote, options, reference.get(),
                                    [this, afterMaster] (float p) { progress = afterMaster ? 0.5f * p : p; }, [this] { return threadShouldExit(); },
                                    afterMaster ? &stems : nullptr);
    if (afterMaster && result.valid && ! threadShouldExit())
        MixAnalysis::addAfterMaster (result, stems, mutedCopies, bpm, keyNote, [this] (float p) { progress = p; }, [this] { return threadShouldExit(); });
    mutedCopies.clear();
    result.synthState = synthState;
    // Hela exporten sparas direkt (som get_mix_report/save_mix_report men utan att någon frågar), så att en AI kan läsa den färdiga analysen som en fil.
    if (result.valid && ! threadShouldExit())
        result.saveExport (Settings::rootFolder().getChildFile ("Analysis").getChildFile ("latest.json"), MixAnalysis::ExportOptions {});
    {
        const juce::ScopedLock sl (lock);
        report = std::move (result);
    }
    ++version;
    running = false;
}
