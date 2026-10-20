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
        ModRoutes routes;
        Modulator modulator (processor.apvts, routes);
        const auto cutoff = modulator.indexOf ("bassCutoff");
        check (cutoff >= 0, "parametern hittas");

        modulator.update (0.0);
        check (modulator.apply (cutoff, 500.0f) == 500.0f, "orörd utan kopplingar");

        processor.set ("lfo1Shape", 4.0f);  // fyrkant
        processor.set ("lfo1Rate", 6.0f);   // 1/4
        check (routes.add (0, cutoff, 0.2f) == 0, "kopplingen läggs i första platsen");
        check (routes.add (0, cutoff, 0.5f) == 0, "samma koppling två gånger blir en");

        modulator.update (0.1); // första halvan av perioden: +1
        const auto up = modulator.apply (cutoff, 500.0f);
        check (std::abs (up - 700.0f) < 1.0f, "+20 % av omfånget", up);
        check (std::abs (modulator.currentOffset (cutoff) - 0.2f) < 1.0e-5f, "gränssnittet ser moduleringen");

        modulator.update (0.6); // andra halvan: -1
        const auto down = modulator.apply (cutoff, 500.0f);
        check (std::abs (down - 300.0f) < 1.0f, "-20 % av omfånget", down);

        processor.set ("lfo1On", 0.0f);
        modulator.update (0.1);
        check (modulator.apply (cutoff, 500.0f) == 500.0f, "avstängd LFO modulerar inte");

        processor.set ("lfo1On", 1.0f);
        routes.remove (0);
        modulator.update (0.1);
        check (modulator.apply (cutoff, 500.0f) == 500.0f, "borttagen koppling modulerar inte");

        // Sparas med parameterns ID och läses tillbaka.
        routes.add (2, cutoff, -0.3f);
        const auto tree = routes.toValueTree (processor.getParameters());
        routes.clear();
        routes.fromValueTree (tree, processor.getParameters());
        const auto found = routes.find (2, cutoff);
        check (found >= 0 && std::abs (routes.routes[(size_t) found].amount.load() + 0.3f) < 1.0e-5f, "sparas och läses tillbaka");
    }
}

namespace
{
    void testSources()
    {
        std::puts("Modulering: makron och instrumentens källor");
        DummyProcessor processor;
        ModRoutes routes;
        Modulator modulator (processor.apvts, routes);
        const auto cutoff = modulator.indexOf ("bassCutoff");

        processor.set ("macro2", 0.5f);
        routes.add (Modulation::firstMacro + 1, cutoff, 0.4f);
        modulator.update (0.0);
        const auto withMacro = modulator.apply (cutoff, 500.0f);
        check (std::abs (withMacro - 700.0f) < 1.0f, "makro 2 på 50 % med 40 % koppling ger +20 %", withMacro);

        routes.add (Modulation::instrumentSource (2, 0), cutoff, 1.0f);
        modulator.setInstrumentSources (2, 0.25f, 0.0f, 0.0f, 0.0f);
        modulator.update (0.0);
        const auto withEnvelope = modulator.apply (cutoff, 500.0f);
        check (std::abs (withEnvelope - 950.0f) < 1.0f, "instrument 3:s ENV 2 läggs till", withEnvelope);
        check (std::abs (modulator.sourceValue (Modulation::instrumentSource (2, 0)) - 0.25f) < 1.0e-5f, "gränssnittet ser källans värde");

        check (Modulation::sourceName (Modulation::instrumentSource (2, 1)) == "INSTR 3 ENV 3" && Modulation::sourceName (4) == "MACRO 2"
                   && Modulation::layerOfSource (Modulation::instrumentSource (9, 3)) == 9 && Modulation::kindOfSource (Modulation::instrumentSource (9, 3)) == 3,
               "källornas namn och nummer");

        // Många kopplingar ryms, och de sparas med källans nummer.
        routes.clear();
        for (int source = 0; source < Modulation::numSources; ++source)
            check (routes.add (source, cutoff, 0.01f) >= 0, "alla källor ryms");
        const auto tree = routes.toValueTree (processor.getParameters());
        routes.clear();
        routes.fromValueTree (tree, processor.getParameters());
        check (routes.find (Modulation::instrumentSource (9, 3), cutoff) >= 0 && routes.find (4, cutoff) >= 0, "källor över 2 sparas och läses tillbaka");
    }
}

namespace
{
    void testCustomShapeAndFreeRate()
    {
        std::puts ("LFO: egen kurva och fri hastighet");
        DummyProcessor processor;
        ModRoutes routes;
        Modulator modulator (processor.apvts, routes);
        const auto cutoff = modulator.indexOf ("bassCutoff");

        // En rak ramp: y går från 0 till 1, så värdet (1 - 2 y) går från +1 till -1.
        const std::array<NodeEnvelope, Modulation::numLfos> curves { NodeEnvelope (1.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
                                                                     NodeEnvelope (1.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }),
                                                                     NodeEnvelope (1.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }) };
        modulator.setLfoCurves (&curves);
        routes.add (0, cutoff, 0.2f);
        processor.set ("lfo1Shape", 6.0f); // Custom
        processor.set ("lfo1Rate", 6.0f);  // 1/4: en period per fjärdedel

        modulator.update (0.0);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 700.0f) < 1.0f, "kurvan börjar på +1");
        modulator.update (0.5);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 500.0f) < 1.0f, "kurvan är 0 vid halva perioden", modulator.apply (cutoff, 500.0f));

        // Fri hastighet: 2 Hz vid 120 BPM är en period per halv sekund = per fjärdedel; ppq 0.125 = 1/16 period... 0.25 av en period.
        processor.set ("lfo1Mode", 1.0f);
        processor.set ("lfo1Hz", 1.0f);
        modulator.update (0.5, 120.0); // 0.25 s * 1 Hz = 0.25 period: värdet 1 - 2 * 0.25 = 0.5
        check (std::abs (modulator.apply (cutoff, 500.0f) - 600.0f) < 1.0f, "fri hastighet i Hz följer tempot", modulator.apply (cutoff, 500.0f));
        modulator.update (0.5, 60.0);  // tempoändring mitt i en period: fasen fortsätter där den är (värdet 0.5 -> +600), den hoppar inte
        check (std::abs (modulator.apply (cutoff, 500.0f) - 600.0f) < 1.0f, "ett nytt tempo behåller fasen", modulator.apply (cutoff, 500.0f));
        modulator.update (1.0, 60.0);  // 0.5 fjärdedel till vid 60 BPM och 1 Hz = 0.5 period till: fasen 0.75, värdet 1 - 1.5 = -0.5
        check (std::abs (modulator.apply (cutoff, 500.0f) - 400.0f) < 1.0f, "och går vidare i det nya tempot", modulator.apply (cutoff, 500.0f));

        // Byte av hastighet (Hz, Sync) mitt i en cykel nollställer inte fasen: samma värde direkt före och efter, vid samma position.
        processor.set ("lfo1Mode", 0.0f);
        processor.set ("lfo1Rate", 6.0f);                // 1/4
        modulator.update (2.0);                          // ny cykel: (utan offset hade fasen varit 0 här)
        const auto before = modulator.apply (cutoff, 500.0f);
        processor.set ("lfo1Rate", 8.0f);                // annan hastighet vid samma position
        modulator.update (2.0);
        check (std::abs (modulator.apply (cutoff, 500.0f) - before) < 1.0e-3f, "ändrad Sync-hastighet behåller fasen", modulator.apply (cutoff, 500.0f));
        processor.set ("lfo1Mode", 1.0f);                // Sync -> Hz
        processor.set ("lfo1Hz", 3.3f);
        modulator.update (2.0, 133.0);
        check (std::abs (modulator.apply (cutoff, 500.0f) - before) < 1.0e-3f, "Sync -> Hz behåller fasen", modulator.apply (cutoff, 500.0f));
        modulator.update (0.0, 133.0);                   // transporten börjar om: LFO:n följer takten igen (fasen 0 = +1 med rampen -> +700)
        check (std::abs (modulator.apply (cutoff, 500.0f) - 700.0f) < 1.0f, "ett omstartat transport nollställer fasen", modulator.apply (cutoff, 500.0f));

        check (Modulation::rateNames().size() == 18 && Modulation::rateInQuarters (17) == 64.0 && Modulation::rateInQuarters (11) == 0.75, "fler synkade hastigheter (3/16 ... 16 takter)");
        check (Modulation::shapeNames().size() == 7 && Modulation::shapeNames()[6] == "Custom", "formen Custom finns");
    }
}

void runModulationTests()
{
    testShapes();
    testModulator();
    testSources();
    testCustomShapeAndFreeRate();
}
