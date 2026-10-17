#include "MixAnalysis.h"
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <set>
#include "PluginBase.h"
#include "OfflineRender.h"

namespace MixAnalysis
{
    namespace
    {
        constexpr float binNorm = 4.0f / (float) fftSize; // en sinus i full skala blir 0 dB
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
        r.kickTailHz = kickTail;

        for (int src = 0; src < numSources; ++src)
        {
            SourceInfo info;
            info.name = c.names[(size_t) src];
            info.present = c.present[(size_t) src];
            info.peakDb = ownMax[(size_t) src];
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
                        f.detail += tuningHint (i, j) + " Give them different registers (octave or tune), EQ one of them (SPECTRUM > EQ), or duck one under the other (EFFECTS tab: a SIDECHAIN effect with one as SOURCE, and send the other 100 % to that FX channel).";
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
                             + duckHint (j) + tuningHint (j, kickSource) + " You can also lower " + c.names[(size_t) j] + " or cut it here with the EQ (SPECTRUM > EQ).";
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
                           + (src == masterSource ? "Check which source it comes from, and high-pass it (SPECTRUM > EQ, HP about 30 Hz)."
                                                  : "Raise its octave or tune, or high-pass it with the EQ (SPECTRUM > EQ, HP 30-40 Hz).");
                r.findings.push_back (std::move (f));
            }
        }

        // --- Mastern: limitern.
        r.limiterMaxDb = c.limiterMaxReductionDb;
        if (c.limiterMaxReductionDb >= 5.0f)
        {
            Finding f;
            f.severity = c.limiterMaxReductionDb >= 9.0f ? Severity::problem : Severity::warning;
            f.category = "master";
            f.sources = { c.names[(size_t) masterSource] };
            f.title = "The master limiter works hard (up to " + juce::String (c.limiterMaxReductionDb, 1) + " dB)";
            f.detail = "Kick and instruments together are louder than the master's ceiling, so the limiter squashes the peaks (usually the kick). Lower levels in the MIXER tab or the loudest instruments instead of letting the limiter do it.";
            r.findings.push_back (std::move (f));
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
                               + (average > 0.0f ? "To match it, cut here with the EQ (SPECTRUM > EQ) on the instrument that fills this range, or on the master."
                                                 : "To match it, boost here with the EQ (SPECTRUM > EQ), or give an instrument more energy in this range.");
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
        root->setProperty ("limiter_max_reduction_db", limiterMaxDb);
        if (referenceName.isNotEmpty())
            root->setProperty ("reference", referenceName);

        juce::Array<juce::var> sourceList;
        for (const auto& source : sources)
        {
            if (! source.present)
                continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", source.name);
            o->setProperty ("peak_db", source.peakDb);
            if (source.fundamentalHz > 0.0f)
            {
                o->setProperty ("fundamental_hz", source.fundamentalHz);
                o->setProperty ("note", source.note);
            }
            sourceList.add (juce::var (o));
        }
        root->setProperty ("sources", sourceList);

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
    std::unique_ptr<PluginBase> prepareCopy (PluginBase& source)
    {
        auto copy = source.cloneForOffline();
        juce::MemoryBlock state;
        source.getStateInformation (state);
        copy->setStateInformation (state.getData(), (int) state.getSize());
        copy->ensureTablesInUse();
        return copy;
    }

    Report run (PluginBase& copy, double bpm, int keyNote, const Options& options, const Reference* reference,
                const std::function<void (float)>& progress, const std::function<bool()>& cancelled)
    {
        constexpr double rate = 48000.0;
        constexpr int blockSize = 512;

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

        const auto lengths = Params::readPatternLengths (copy.apvts);
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
        std::vector<float> bandRows, lowRows;
        std::vector<int> barRows;
        const auto nb = numBands();
        const auto map = binBands (rate);
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
        std::array<float, fftSize * 2> data {};
        std::vector<float> left, right;
        std::vector<std::array<float, numInstruments>> duckMin;
        std::vector<int> onsets; // sample
        auto kickHeld = false;
        auto limiterMin = 1.0f;
        auto nextFrame = 0;

        copy.instrumentTapsOn = true;

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

            // Kickens slag: bladets topp stiger över ett tröskelvärde efter tystnad.
            copy.kickTap.copyLatest (n, left, right);
            auto peak = 0.0f;
            for (auto v : left)
                peak = std::max (peak, std::abs (v));
            if (peak < 0.03f)
                kickHeld = false;
            else if (peak > 0.12f && ! kickHeld)
            {
                kickHeld = true;
                if (done >= preRollSamples)
                    onsets.push_back (done);
            }

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

            if (done >= preRollSamples)
            {
                copy.getMasterSection().gainTap.copyLatest (n, left, right);
                for (auto v : left)
                    limiterMin = std::min (limiterMin, v);
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

                for (int k = 0; k < fftSize; ++k)
                    data[(size_t) k] = 0.5f * (left[(size_t) k] + right[(size_t) k]);
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

        auto report = evaluate (capture, options, reference);
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

bool MixAnalyser::start (PluginBase& source, const MixAnalysis::Options& newOptions, std::shared_ptr<const MixAnalysis::Reference> newReference)
{
    if (isThreadRunning())
        return false;
    copy.reset();
    options = newOptions;
    reference = std::move (newReference);
    bpm = std::max (40.0, (double) source.transportStatus.bpm.load());
    keyNote = source.transportStatus.keyNote.load();
    copy = MixAnalysis::prepareCopy (source);
    progress = 0.0f;
    running = true;
    startThread (juce::Thread::Priority::low); // lågt: ljudtråden i värden ska inte få konkurrens
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
    auto result = MixAnalysis::run (*copy, bpm, keyNote, options, reference.get(),
                                    [this] (float p) { progress = p; }, [this] { return threadShouldExit(); });
    {
        const juce::ScopedLock sl (lock);
        report = std::move (result);
    }
    ++version;
    running = false;
}
