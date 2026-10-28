// Tests for the FX core (Flanger, Echo, RING FX) through EffectChain and an APVTS.

#include "../Source/Core/Effects.h"
#include "TestUtils.h"
#include <cmath>

namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 512;

    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor() : apvts (*this, nullptr, "Parameters", createLayout()) {}

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
        {
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            Effects::addParameters (layout);
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

    // Sets knob k of slot 0 in channel 0 to a real value.
    void setKnob (DummyProcessor& p, EffectType type, int k, float value)
    {
        const auto& knob = Effects::knobs (type)[(size_t) k];
        if (auto* param = p.apvts.getParameter (Effects::knobParamID (0, 0, k)))
            param->setValueNotifyingHost (knob.toNormalised (value));
    }

    // Runs `input` (stereo) through slot 0 of channel 0 and returns the output.
    juce::AudioBuffer<float> render (DummyProcessor& p, const juce::AudioBuffer<float>& input)
    {
        EffectChain chain (p.apvts, 0);
        chain.prepare (rate, block);
        chain.update (140.0, 0.0);

        juce::AudioBuffer<float> out (2, input.getNumSamples());
        for (int pos = 0; pos < input.getNumSamples(); pos += block)
        {
            const auto n = std::min (block, input.getNumSamples() - pos);
            juce::AudioBuffer<float> b (2, block);
            b.clear();
            for (int ch = 0; ch < 2; ++ch)
                b.copyFrom (ch, 0, input, ch, pos, n);
            chain.update (140.0, 0.0);
            chain.process (b, n);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, b, ch, 0, n);
        }
        return out;
    }

    juce::AudioBuffer<float> sine (int samples, float hz)
    {
        juce::AudioBuffer<float> b (2, samples);
        for (int i = 0; i < samples; ++i)
        {
            const auto v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
        return b;
    }

    bool finite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    float maxDifference (const juce::AudioBuffer<float>& a, int chA, const juce::AudioBuffer<float>& b, int chB)
    {
        auto m = 0.0f;
        for (int i = 0; i < a.getNumSamples(); ++i)
            m = std::max (m, std::abs (a.getSample (chA, i) - b.getSample (chB, i)));
        return m;
    }

    void testFlangerTable()
    {
        std::puts ("FX: Flanger knob table and legacy defaults");
        const auto& list = Effects::knobs (EffectType::flanger);
        check (list.size() == 7, "flanger has 7 knobs", (double) list.size());
        check (list.size() == 7 && list[5].name == "SHAPE" && list[5].isChoice() && list[5].defaultValue == 0.0f, "SHAPE: Sine by default");
        check (list.size() == 7 && list[6].name == "SPREAD" && std::abs (list[6].defaultValue - 0.25f) < 1.0e-6f, "SPREAD: 0.25 = the original sound");

        float n = 0.0f;
        check (! Effects::newKnobDefault (EffectType::flanger, 4, n), "knob 5 (MIX) is not new");
        check (Effects::newKnobDefault (EffectType::flanger, 5, n) && n == 0.0f, "old files get SHAPE = Sine", n);
        check (Effects::newKnobDefault (EffectType::flanger, 6, n) && std::abs (list[6].fromNormalised (n) - 0.25f) < 1.0e-4f, "old files get SPREAD = 0.25", n);
    }

    void testFlanger()
    {
        std::puts ("FX: Flanger SHAPE and SPREAD");
        DummyProcessor p;
        Effects::setSlotType (p.apvts, 0, 0, EffectType::flanger);
        const auto in = sine (24000, 220.0f);

        const auto original = render (p, in);
        check (finite (original), "default render is finite");
        check (maxDifference (original, 0, original, 1) > 1.0e-3f, "default SPREAD 0.25 gives different left and right");

        setKnob (p, EffectType::flanger, 6, 0.0f);
        const auto mono = render (p, in);
        check (maxDifference (mono, 0, mono, 1) < 1.0e-6f, "SPREAD 0 gives identical left and right");
        check (maxDifference (original, 0, mono, 0) < 1.0e-5f, "left channel does not depend on SPREAD");

        setKnob (p, EffectType::flanger, 6, 0.25f);
        setKnob (p, EffectType::flanger, 5, 1.0f);
        const auto triangle = render (p, in);
        check (finite (triangle), "triangle render is finite");
        check (maxDifference (original, 0, triangle, 0) > 1.0e-3f, "SHAPE Triangle sounds different from Sine");
    }

    void testEcho()
    {
        std::puts ("FX: Echo repeats at the synced time");
        DummyProcessor p;
        Effects::setSlotType (p.apvts, 0, 0, EffectType::echo);
        setKnob (p, EffectType::echo, 0, 6.0f); // 1/4 note
        setKnob (p, EffectType::echo, 4, 0.0f); // Normal feedback mode
        setKnob (p, EffectType::echo, 5, 0.0f); // no feedback
        setKnob (p, EffectType::echo, 3, 1.0f); // IN VOL
        setKnob (p, EffectType::echo, 7, 0.0f); // DRY VOL

        juce::AudioBuffer<float> in (2, 48000);
        in.clear();
        in.setSample (0, 0, 1.0f);
        in.setSample (1, 0, 1.0f);

        const auto out = render (p, in);
        check (finite (out), "echo render is finite");

        const auto expected = (int) std::lround (Effects::syncInQuarters (6) * 60.0 / 140.0 * rate);
        auto peakAt = 0, peakIndex = 0;
        auto peak = 0.0f;
        for (int i = 100; i < out.getNumSamples(); ++i)
            if (std::abs (out.getSample (0, i)) > peak)
            {
                peak = std::abs (out.getSample (0, i));
                peakIndex = i;
            }
        peakAt = peakIndex;
        check (peak > 0.05f, "an echo is heard", peak);
        check (std::abs (peakAt - expected) < 400, "echo lands on 1/4 note at 140 BPM", (double) (peakAt - expected));
    }

    // Position of the loudest sample after the first 100 in the left channel.
    int echoPeakIndex (const juce::AudioBuffer<float>& out)
    {
        auto peak = 0.0f;
        auto index = 0;
        for (int i = 100; i < out.getNumSamples(); ++i)
            if (std::abs (out.getSample (0, i)) > peak)
            {
                peak = std::abs (out.getSample (0, i));
                index = i;
            }
        return index;
    }

    void testEchoMs()
    {
        std::puts ("FX: Echo TIME MS (knob 11) and knobsPerSlot");
        check (knobsPerSlot == 11, "knobsPerSlot is 11", knobsPerSlot);
        const auto& list = Effects::knobs (EffectType::echo);
        check (list.size() == 11 && list[10].name == "TIME MS" && list[10].defaultValue == 0.0f, "TIME MS is last and defaults to 0 (off)");

        DummyProcessor p;
        check (p.apvts.getParameter (Effects::knobParamID (0, 0, 10)) != nullptr, "layout has P11");
        check (p.apvts.getParameter (Effects::knobParamID (0, 0, 11)) == nullptr, "layout has no P12");

        float n = 0.0f;
        check (Effects::newKnobDefault (EffectType::echo, 10, n) && n == 0.0f, "old files get TIME MS = 0", n);
        check (! Effects::newKnobDefault (EffectType::distortion, 10, n), "other types have no new P11");

        juce::AudioBuffer<float> in (2, 48000);
        in.clear();
        in.setSample (0, 0, 1.0f);
        in.setSample (1, 0, 1.0f);

        Effects::setSlotType (p.apvts, 0, 0, EffectType::echo);
        setKnob (p, EffectType::echo, 0, 6.0f); // 1/4 note
        setKnob (p, EffectType::echo, 4, 0.0f);
        setKnob (p, EffectType::echo, 5, 0.0f);
        setKnob (p, EffectType::echo, 3, 1.0f);
        setKnob (p, EffectType::echo, 7, 0.0f);

        const auto synced = echoPeakIndex (render (p, in));
        const auto expectedSynced = (int) std::lround (Effects::syncInQuarters (6) * 60.0 / 140.0 * rate);
        check (std::abs (synced - expectedSynced) < 400, "TIME MS 0 keeps the synced time", (double) (synced - expectedSynced));

        setKnob (p, EffectType::echo, 10, 250.0f);
        const auto ms = echoPeakIndex (render (p, in));
        check (std::abs (ms - 12000) < 400, "TIME MS 250 puts the echo at 250 ms", (double) (ms - 12000));
    }

    void testRingMod()
    {
        std::puts ("FX: RING FX modulates and stays finite");
        DummyProcessor p;
        Effects::setSlotType (p.apvts, 0, 0, EffectType::ringMod);
        setKnob (p, EffectType::ringMod, 2, 1.0f); // MIX fully wet
        const auto in = sine (24000, 220.0f);
        const auto out = render (p, in);
        check (finite (out), "ring render is finite");
        check (maxDifference (in, 0, out, 0) > 0.05f, "output differs from the input");
    }
}

void runFxTests()
{
    testFlangerTable();
    testFlanger();
    testEcho();
    testEchoMs();
    testRingMod();
}
