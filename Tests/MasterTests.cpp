// Tester för sidechain-kurvan och mastersektionens limiter.

#include "../Source/Core/Effects.h"
#include "../Source/Core/LiveCodeComplete.h"
#include "../Source/Core/Master.h"
#include "../Source/Core/Sidechain.h"
#include "TestUtils.h"
#include <cmath>

namespace
{
    constexpr double rate = 48000.0;

    // Minsta möjliga processor, bara för att äga mastersektionens parametrar.
    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor() : apvts (*this, nullptr, "Parameters", createLayout()) {}

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
        {
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            MasterSection::addParameters (layout);
            return layout;
        }

        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };

    void testCurveShapeAndTable()
    {
        std::puts ("Sidechain: standardkurvan och tabellen");
        SidechainCurve curve;
        check (std::abs (curve.evaluate (0.0f) - 1.0f) < 1.0e-6f, "full duck i början", curve.evaluate (0.0f));
        check (std::abs (curve.evaluate (1.0f)) < 1.0e-6f, "ingen duck i slutet", curve.evaluate (1.0f));

        auto maxDifference = 0.0f;
        for (int i = 0; i <= 100; ++i)
        {
            const auto x = (float) i / 100.0f;
            maxDifference = std::max (maxDifference, std::abs (curve.lookup (x) - curve.evaluate (x)));
        }
        // Brantast precis efter ett knä med negativ tension (en potenskurva med oändlig lutning
        // i början), där den linjära interpolationen avviker ett par procent under några samples.
        check (maxDifference < 0.03f, "tabellen följer kurvan", maxDifference);

        // Punkter utanför 0..1 och i fel ordning rättas till.
        curve.setPoints ({ { 0.7f, 0.5f, 0.0f }, { -1.0f, 2.0f, 0.0f }, { 0.3f, 0.8f, 0.0f } });
        const auto& points = curve.getPoints();
        check (points.size() == 3 && points.front().x == 0.0f && points.back().x == 1.0f, "kanterna ligger på 0 och 1");
        check (points.front().y == 1.0f, "y begränsas till 1", points.front().y);
        check (points[1].x == 0.3f, "sorterade efter x", points[1].x);
    }

    void testCurvePlayer()
    {
        std::puts ("Sidechain: kurvan spelas från början vid varje kick");
        SidechainCurve curve;
        curve.setPoints ({ { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }); // rak linje 1 -> 0
        DuckCurvePlayer player;

        check (player.next (curve, 100.0) == 0.0f, "ingen duck före första kicken");
        player.trigger();
        check (std::abs (player.next (curve, 100.0) - 1.0f) < 1.0e-4f, "full duck vid kicken");
        for (int i = 0; i < 49; ++i)
            player.next (curve, 100.0);
        const auto halfway = player.next (curve, 100.0);
        check (std::abs (halfway - 0.5f) < 0.02f, "halvvägs efter halva längden", halfway);
        for (int i = 0; i < 100; ++i)
            player.next (curve, 100.0);
        check (player.next (curve, 100.0) < 1.0e-4f, "slut efter hela längden");
    }

    void testLimiter()
    {
        std::puts ("Master: limitern håller taket och latensen stämmer");
        DummyProcessor processor;
        // På heapen: signaluttagen i mastern är flera MB, mer än en konsolprograms stack.
        const auto masterOwner = std::make_unique<MasterSection> (processor.apvts);
        auto& master = *masterOwner;
        master.prepare (rate, 2);

        const auto latency = master.getLatencySamples();
        check (latency == (int) std::round (MasterSection::lookaheadSeconds * rate) - 1, "latens = lookahead - 1", latency);

        // Ett impulsljud ska komma ut exakt `latency` samples senare (under taket: orört).
        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        buffer.setSample (0, 0, 0.5f);
        buffer.setSample (1, 0, 0.5f);
        master.process (buffer, 512);
        check (std::abs (buffer.getSample (0, latency) - 0.5f) < 1.0e-4f, "impulsen kommer efter latensen", buffer.getSample (0, latency));

        // En sinus på +6 dB får aldrig gå över taket (-0.3 dB).
        master.reset();
        const auto ceiling = juce::Decibels::decibelsToGain (-0.3f);
        auto peak = 0.0f;
        double phase = 0.0;
        for (int block = 0; block < 40; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const auto s = 2.0f * (float) std::sin (phase);
                phase += juce::MathConstants<double>::twoPi * 110.0 / rate;
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
            }
            master.process (buffer, 512);
            for (int i = 0; i < 512; ++i)
                peak = std::max (peak, std::abs (buffer.getSample (0, i)));
        }
        check (peak <= ceiling + 1.0e-5f, "toppen under taket", peak);
        check (peak > ceiling * 0.9f, "men inte onödigt mycket lägre", peak);
        check (master.gainReductionDb.load() < -4.0f, "mätaren visar förstärkningsminskning", master.gainReductionDb.load());
    }

    // H1: ett gammalt projekt (före Maximizerns MODE) har ett oanvänt sparat värde på ratt 6; det ska bli Classic, inte Smooth.
    void testMaximizerModeMigration()
    {
        std::puts ("H1: gammalt Maximizer-projekt får MODE = Classic");
        juce::ValueTree state ("Parameters");
        auto add = [&] (const juce::String& id, float v)
        {
            juce::ValueTree p ("PARAM");
            p.setProperty ("id", id, nullptr);
            p.setProperty ("value", v, nullptr);
            state.appendChild (p, nullptr);
        };
        add (Effects::slotParamID (0, 0, "Type"), (float) (int) EffectType::maximizer);
        add (Effects::knobParamID (0, 0, 5), 1.0f);
        state.setProperty ("fxKnobsV", 2, nullptr);
        Effects::migrateLegacyKnobs (state);
        const auto knob = state.getChildWithProperty ("id", Effects::knobParamID (0, 0, 5));
        check (knob.isValid() && (float) (double) knob.getProperty ("value") == 0.0f, "ratt 6 nollad till Classic", knob.isValid() ? (double) knob.getProperty ("value") : -1.0);
    }

    // D4: autokomplettering inuti strängar och på tom rad
    void testLiveCodeStringComplete()
    {
        std::puts ("D4: Live Code-förslag i strängar");
        const LiveCodeComplete::Source src;
        auto has = [] (const LiveCodeComplete::Result& r, const char* label)
        {
            for (auto& i : r.items) if (i.label == label) return true;
            return false;
        };
        const juce::String a = "$: s(\"";
        const auto s = LiveCodeComplete::complete (a, a.length(), src);
        check (has (s, "i1") && has (s, "bd") && ! has (s, "c2"), "s(\" ger instrumentnamn", (int) s.items.size());
        const juce::String b = "$: s(\"i1\").note(\"c2 e";
        const auto n = LiveCodeComplete::complete (b, b.length(), src);
        check (has (n, "eb1") && ! has (n, "i1") && n.replaceFrom == b.length() - 1, "note(\" ger tonnamn från senaste ordet", n.replaceFrom);
        const auto empty = LiveCodeComplete::complete ("", 0, src);
        check (! empty.items.empty() && empty.items.front().label == "$:", "tom rad börjar med $:");
        const juce::String c = "s(\"i1\").";
        check (has (LiveCodeComplete::complete (c, c.length(), src), "note"), "efter stängd sträng ger metoder");
    }
}

void runMasterTests()
{
    testLiveCodeStringComplete();
    testCurveShapeAndTable();
    testCurvePlayer();
    testLimiter();
    testMaximizerModeMigration();
}
