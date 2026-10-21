// Testtäckning för content-s2: Bézier-kurvor genom spara/ladda (XML) och FM-källans robusthet.

#include "../Source/Core/InstrumentSynth.h"
#include "../Source/Core/NodeEnvelope.h"
#include "TestUtils.h"
#include <cmath>
#include <vector>

namespace
{
    bool finiteAll (const std::vector<float>& audio)
    {
        for (auto v : audio)
            if (! std::isfinite (v))
                return false;
        return true;
    }

    // En kick-lik (pitch) och en LFO-lik kurva: olika maxvärden och punktantal, Bézier med handtag.
    NodeEnvelope emptyCurve (bool lfoLike)
    {
        return lfoLike ? NodeEnvelope (1.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } })
                       : NodeEnvelope (0.5f, 72.0f, { { 0.0f, 42.0f, -0.6f }, { 0.045f, 0.0f, 0.0f } });
    }

    NodeEnvelope makeCurve (bool lfoLike)
    {
        auto e = emptyCurve (lfoLike);
        std::vector<NodeEnvelope::Point> pts;
        if (lfoLike)
            pts = { { 0.0f, 0.1f, 0.0f }, { 0.3f, 0.9f, 0.4f }, { 0.7f, 0.2f, -0.5f }, { 1.0f, 0.6f, 0.0f } };
        else
            pts = { { 0.0f, 40.0f, 0.0f }, { 0.02f, 12.0f, 0.0f }, { 0.12f, 0.0f, 0.0f } };

        const auto span = pts.back().x;
        for (auto& p : pts)
        {
            p.outDx = 0.1f * span;
            p.outDy = -0.1f * e.getMaxValue();
            p.inDx = -0.08f * span;
            p.inDy = 0.05f * e.getMaxValue();
        }
        e.setPoints (pts);
        e.setMode (NodeEnvelope::Mode::bezier);
        return e;
    }

    float maxDiff (const NodeEnvelope& a, const NodeEnvelope& b, float span)
    {
        float worst = 0.0f;
        for (int i = 0; i <= 400; ++i)
            worst = std::max (worst, std::abs (a.evaluate (span * (float) i / 400.0f) - b.evaluate (span * (float) i / 400.0f)));
        return worst;
    }

    void testBezierProjectRoundTrip()
    {
        std::puts ("BZ: Bézier-läge och handtag överlever spara/ladda (XML), gamla projekt läses som tension");

        for (bool lfoLike : { false, true })
        {
            const auto original = makeCurve (lfoLike);
            const juce::Identifier type (lfoLike ? "LfoCurve1" : "KickPitchEnv");
            const auto span = original.getPoints().back().x;

            // Hela vägen via XML-text, som en sparad plugin-state.
            const auto xml = original.toValueTree (type).createXml();
            check (xml != nullptr, "XML skapas");
            if (xml == nullptr)
                continue;
            const auto parsed = juce::XmlDocument::parse (xml->toString());
            check (parsed != nullptr, "XML går att läsa");
            if (parsed == nullptr)
                continue;

            auto loaded = emptyCurve (lfoLike);
            loaded.fromValueTree (juce::ValueTree::fromXml (*parsed));

            check (loaded.getMode() == NodeEnvelope::Mode::bezier, "läget Bézier laddas");
            check (loaded.getPoints().size() == original.getPoints().size(), "punktantalet laddas");
            check (maxDiff (original, loaded, span) < 1.0e-4f, "kurvan är identisk efter laddning", maxDiff (original, loaded, span));

            // Gammalt projekt: samma punkter men utan mode- och handtagsattribut.
            auto old = original.toValueTree (type);
            old.removeProperty ("mode", nullptr);
            for (auto child : old)
                for (auto name : { "outDx", "outDy", "inDx", "inDy" })
                    child.removeProperty (name, nullptr);

            auto legacy = emptyCurve (lfoLike);
            legacy.fromValueTree (old);
            check (legacy.getMode() == NodeEnvelope::Mode::tension, "gammal XML utan mode läses som tension");

            // Förväntad kurva: samma punkter, tension, inga handtag.
            auto plainPoints = original.getPoints();
            for (auto& p : plainPoints)
                p.outDx = p.outDy = p.inDx = p.inDy = 0.0f;
            auto expected = emptyCurve (lfoLike);
            expected.setPoints (plainPoints);
            check (maxDiff (legacy, expected, span) < 1.0e-4f, "gammal kurva = tensionkurvan av samma punkter", maxDiff (legacy, expected, span));
        }
    }

    InstrumentParams quietBase()
    {
        InstrumentParams p;
        p.rootHz = 55.0f;
        p.sustain = 1.0f;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        return p;
    }

    // Blockstorleken styr hur ofta tempot sätts, som i motorn; samplen hämtas en och en.
    std::vector<float> render (const InstrumentParams& p, double rate, int block, int hundredths, int note = 0)
    {
        InstrumentSynth synth;
        synth.prepare (rate);
        synth.setSeed (7);
        synth.setParams (p);
        synth.reset();
        synth.noteOn (note, 1.0f, (int) rate);
        std::vector<float> audio ((size_t) (rate * hundredths / 100.0));
        for (size_t i = 0; i < audio.size();)
        {
            synth.setTempo (140.0);
            for (int k = 0; k < block && i < audio.size(); ++k, ++i)
                audio[i] = synth.nextSample();
        }
        return audio;
    }

    float maxDiffAudio (const std::vector<float>& a, const std::vector<float>& b)
    {
        float worst = 0.0f;
        for (size_t i = 0; i < a.size(); ++i)
            worst = std::max (worst, std::abs (a[i] - b[i]));
        return worst;
    }

    void testFmOffIsIdentical()
    {
        std::puts ("FM: av (eller nivå 0) ger samma ljud som utan FM-vägen");
        constexpr double rate = 48000.0;
        const auto reference = render (quietBase(), rate, 64, 30);

        auto off = quietBase();
        off.fmOn = false;
        off.fmLevel = 1.0f;
        off.fmIndex = 1.0f;
        off.fmRatio = 8.0f;
        off.fmEnvAmount = 1.0f;
        off.fmSemis = 12.0f;
        check (maxDiffAudio (reference, render (off, rate, 64, 30)) == 0.0f, "FM av: nolldiff mot referens");

        auto zeroLevel = off;
        zeroLevel.fmOn = true;
        zeroLevel.fmLevel = 0.0f;
        check (maxDiffAudio (reference, render (zeroLevel, rate, 64, 30)) == 0.0f, "FM på med nivå 0: nolldiff mot referens");

        auto on = quietBase();
        on.fmOn = true;
        on.fmLevel = 0.8f;
        const auto diff = maxDiffAudio (reference, render (on, rate, 64, 30));
        check (diff > 0.01f, "kontroll: FM på ändrar ljudet", diff);
    }

    void testFmExtremesAreFinite()
    {
        std::puts ("FM: ytterlägen och blockstorlekar ger ändliga värden");
        struct Setup { double rate; int block; };
        for (auto s : { Setup { 48000.0, 64 }, Setup { 96000.0, 1024 }, Setup { 48000.0, 37 } })
            for (float level : { 0.0f, 1.0f })
                for (float index : { 0.0f, 1.0f })
                    for (float ratio : { 0.25f, 8.0f })
                        for (float semis : { -24.0f, 24.0f })
                            for (int note : { -36, 0, 48 })
                            {
                                auto p = quietBase();
                                p.fmOn = true;
                                p.fmLevel = level;
                                p.fmIndex = index;
                                p.fmRatio = ratio;
                                p.fmSemis = semis;
                                p.fmEnvAmount = 1.0f;
                                p.fmEnvMs = 1.0f;
                                check (finiteAll (render (p, s.rate, s.block, 20, note)), "FM: inga NaN/inf", s.rate);
                            }

        // Utanför områdena (automation eller gamla projekt): klämningen ska hålla.
        auto wild = quietBase();
        wild.fmOn = true;
        wild.fmLevel = 4.0f;
        wild.fmIndex = 50.0f;
        wild.fmRatio = 1000.0f;
        wild.fmSemis = 200.0f;
        wild.fmEnvAmount = 9.0f;
        wild.fmEnvMs = 0.0f;
        check (finiteAll (render (wild, 96000.0, 1024, 20)), "FM: orimliga värden ger ändligt ljud");
    }
}

void runPersistFmTests()
{
    testBezierProjectRoundTrip();
    testFmOffIsIdentical();
    testFmExtremesAreFinite();
}
