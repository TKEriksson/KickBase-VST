// Tester för LFO:erna och moduleringen.

#include "../Source/Core/Modulation.h"
#include "TestUtils.h"
#include <cmath>

namespace
{
    // Minsta möjliga processor med LFO-parametrarna och ett mål (bassCutoff).
    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor() : apvts (*this, nullptr, "Parameters", createLayout()) {}

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
        {
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            Modulation::addParameters (layout);
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "bassCutoff", 1 }, "Cutoff",
                                                                     juce::NormalisableRange<float> (0.0f, 1000.0f), 500.0f));
            return layout;
        }

        void set (const juce::String& id, float value)
        {
            auto* p = apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (value));
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

    void testShapes()
    {
        std::puts ("LFO: formerna");
        check (std::abs (Modulation::shapeValue (0, 0.25, 0) - 1.0f) < 1.0e-5f, "sinus är 1 vid en fjärdedel");
        check (std::abs (Modulation::shapeValue (2, 0.0, 0) + 1.0f) < 1.0e-5f, "såg upp börjar i -1");
        check (Modulation::shapeValue (4, 0.25, 0) == 1.0f && Modulation::shapeValue (4, 0.75, 0) == -1.0f, "fyrkant");
        check (Modulation::shapeValue (5, 0.1, 7) == Modulation::shapeValue (5, 0.9, 7), "random är konstant inom en period");
        check (Modulation::shapeValue (5, 0.0, 7) == Modulation::shapeValue (5, 0.0, 7), "random är samma varje gång");
        check (Modulation::shapeValue (5, 0.0, 7) != Modulation::shapeValue (5, 0.0, 8), "random ändras mellan perioder");
    }

    void testModulator()
    {
        std::puts ("LFO: modulering i parameterns skala");
        DummyProcessor processor;
        Modulator modulator (processor.apvts);

        modulator.update (0.0);
        check (! modulator.isModulated (Modulation::Destination::bassCutoff), "avstängd LFO modulerar inte");
        check (modulator.apply (Modulation::Destination::bassCutoff, 500.0f) == 500.0f, "orörd utan modulering");

        processor.set ("lfo1On", 1.0f);
        processor.set ("lfo1Shape", 4.0f);  // fyrkant
        processor.set ("lfo1Rate", 6.0f);   // 1/4
        processor.set ("lfo1DestA", (float) Modulation::Destination::bassCutoff);
        processor.set ("lfo1AmountA", 0.2f);

        modulator.update (0.1); // första halvan av perioden: +1
        const auto up = modulator.apply (Modulation::Destination::bassCutoff, 500.0f);
        check (std::abs (up - 700.0f) < 1.0f, "+20 % av omfånget", up);

        modulator.update (0.6); // andra halvan: -1
        const auto down = modulator.apply (Modulation::Destination::bassCutoff, 500.0f);
        check (std::abs (down - 300.0f) < 1.0f, "-20 % av omfånget", down);

        // Mål som inte finns i pluginet (t.ex. effekter här) ignoreras.
        processor.set ("lfo1DestB", (float) Modulation::Destination::reverbMix);
        processor.set ("lfo1AmountB", 0.5f);
        modulator.update (0.1);
        check (! modulator.isModulated (Modulation::Destination::reverbMix), "saknat mål ignoreras");
    }
}

void runModulationTests()
{
    testShapes();
    testModulator();
}
