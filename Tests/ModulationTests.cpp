// Tester för LFO:erna och moduleringen.

#include "../Source/Core/Modulation.h"
#include "TestUtils.h"
#include <cmath>

namespace
{
    // Minsta möjliga processor med LFO-parametrarna och ett mål (bassCutoff).
    struct DummyProcessor : juce::AudioProcessor
    {
        explicit DummyProcessor (bool extraLfos = false) : apvts (*this, nullptr, "Parameters", createLayout (extraLfos)) {}

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (bool extraLfos)
        {
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            Modulation::addParameters (layout);
            if (extraLfos)
                Modulation::addExtraLfoParameters (layout); // LFO 4-10 som i instrumentet (FX saknar dem)
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
        // Fler källor (82) än platser (64) sedan CODE-källorna kom: fyll listan.
        for (int source = 0; source < juce::jmin (Modulation::numSources, Modulation::maxRoutes); ++source)
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
        const auto ramp = [] { return NodeEnvelope (1.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }); }; // NodeEnvelope går inte att kopiera
        const std::array<NodeEnvelope, Modulation::maxLfos> curves { ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp() };
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
        modulator.update (0.0, 120.0); // transporten börjar om (nollställer fasoffseten som Sync -> Hz-bytet gav)
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

        // SNAP: 120 BPM, 1/8 = rate 4 = 0.5 fjärdedel per period = 2 perioder per fjärdedel = 4 Hz.
        processor.set ("lfo1Hz", 4.7f);                  // utanför ±5 % (nära 1/8 T-området? nej: bara visning)
        processor.set ("lfo1Snap", 1.0f);
        processor.set ("lfo1SnapRange", 5.0f);
        modulator.update (1.0, 120.0);
        check (! modulator.snapLocked[0].load() && modulator.snapNote[0].load() == 4, "SNAP: långt från notvärdet är fri, närmaste är 1/8", modulator.snapNote[0].load());
        const auto freeValue = modulator.apply (cutoff, 500.0f);
        processor.set ("lfo1Hz", 4.1f);                  // inom 5 % av 4 Hz
        modulator.update (1.0, 120.0);
        check (modulator.snapLocked[0].load() && modulator.snapNote[0].load() == 4, "SNAP: inom zonen låser mot 1/8");
        check (std::abs (modulator.apply (cutoff, 500.0f) - freeValue) < 1.0e-3f, "SNAP: fasen hoppar inte när den låser", modulator.apply (cutoff, 500.0f));
        modulator.update (1.25, 120.0);                  // 0.25 fjärdedel = exakt en halv period vid låst 2 perioder/fjärdedel: fasen går 0.5 vidare
        const auto lockedLater = modulator.apply (cutoff, 500.0f);
        processor.set ("lfo1Snap", 0.0f);                // av: samma Hz (4.1) är fri, snapNote visas fortfarande
        modulator.update (1.25, 120.0);
        check (! modulator.snapLocked[0].load() && modulator.snapNote[0].load() == 4, "SNAP av: ingen låsning");
        check (std::abs (modulator.apply (cutoff, 500.0f) - lockedLater) < 1.0e-3f, "SNAP av: fasen hoppar inte heller när den släpper", modulator.apply (cutoff, 500.0f));
        processor.set ("lfo1Mode", 0.0f);
        modulator.update (1.25, 120.0);
        check (modulator.snapNote[0].load() == -1, "Sync-läget har ingen SNAP");

        check (Modulation::rateNames().size() == 18 && Modulation::rateInQuarters (17) == 64.0 && Modulation::rateInQuarters (11) == 0.75, "fler synkade hastigheter (3/16 ... 16 takter)");
        check (Modulation::shapeNames().size() == 7 && Modulation::shapeNames()[6] == "Custom", "formen Custom finns");
    }
}

namespace
{
    // Fas M steg 1: LFO 4-10 (källa 47-53), aktiv-masken och att LFO 1-3 är orörda.
    void testExtraLfos()
    {
        using namespace Modulation;
        std::puts ("LFO 4-10: källor, mask och parametrar");

        bool bijective = true;
        for (int i = 0; i < maxLfos; ++i)
            bijective = bijective && lfoIndexOfSource (lfoSource (i)) == i && isLfoSource (lfoSource (i));
        for (int s = 0; s < numSources; ++s)
            if (lfoIndexOfSource (s) >= 0)
                bijective = bijective && lfoSource (lfoIndexOfSource (s)) == s;
        check (bijective, "lfoSource och lfoIndexOfSource är varandras inverser");
        check (lfoSource (3) == 47 && lfoSource (9) == 53 && firstEnvSource == 54
                   && firstCodeSource == 74 && numCodeSources == 8 && numSources == 82,
               "LFO 4-10 är 47-53 och CODE 1-8 sist på 74-81");
        check (sourceName (47) == "LFO 4" && sourceName (53) == "LFO 10" && sourceName (2) == "LFO 3", "namnen LFO 4 och LFO 10");
        check (! isLfoSource (3) && sourceName (3) == "MACRO 1", "källa 3 är fortfarande makro 1");

        DummyProcessor plain;
        ModRoutes plainRoutes;
        Modulator plainModulator (plain.apvts, plainRoutes);
        check (plainModulator.activeMask.load() == 7u, "masken är 7 (LFO 1-3) från början");
        check (plain.apvts.getParameter ("lfo4On") == nullptr, "utan addExtraLfoParameters finns ingen LFO 4 (FX)");
        plainModulator.activeMask = 0x3FFu;                     // FX: nullptr-pekarna räknas aldrig
        plainRoutes.add (lfoSource (3), plainModulator.indexOf ("bassCutoff"), 0.5f);
        plainModulator.update (0.25);
        check (std::abs (plainModulator.apply (plainModulator.indexOf ("bassCutoff"), 500.0f) - 500.0f) < 1.0e-3f, "LFO 4 utan parametrar ger inget");

        DummyProcessor processor (true);
        ModRoutes routes;
        Modulator modulator (processor.apvts, routes);
        const auto cutoff = modulator.indexOf ("bassCutoff");
        check (processor.apvts.getParameter ("lfo10SnapRange") != nullptr, "LFO 10 har alla parametrar");

        routes.add (lfoSource (3), cutoff, 0.2f);
        processor.set ("lfo4Rate", 6.0f);                       // 1/4: sinus 1 vid en sextondel
        modulator.update (0.25);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 500.0f) < 1.0e-3f, "inaktiv LFO 4 ger inget");
        modulator.activeMask = 7u | (1u << 3);
        modulator.update (0.25);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 700.0f) < 1.0f, "aktiv LFO 4 modulerar", modulator.apply (cutoff, 500.0f));
        processor.set ("lfo4On", 0.0f);
        modulator.update (0.25);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 500.0f) < 1.0e-3f, "LFO 4 avstängd med On");
        processor.set ("lfo4On", 1.0f);

        // Form, egen kurva, Hz och SNAP på LFO 10.
        routes.clear();
        routes.add (lfoSource (9), cutoff, 0.2f);
        modulator.activeMask = 0x3FFu;
        const auto ramp = [] { return NodeEnvelope (1.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }); }; // NodeEnvelope går inte att kopiera
        const std::array<NodeEnvelope, maxLfos> curves { ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp(), ramp() };
        modulator.setLfoCurves (&curves);
        processor.set ("lfo10Shape", 6.0f);
        processor.set ("lfo10Rate", 6.0f);
        modulator.update (0.0);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 700.0f) < 1.0f, "LFO 10: egen kurva börjar på +1", modulator.apply (cutoff, 500.0f));
        processor.set ("lfo10Shape", 4.0f);                     // fyrkant: -1 i andra halvan
        modulator.update (0.75);
        check (std::abs (modulator.apply (cutoff, 500.0f) - 300.0f) < 1.0f, "LFO 10: fyrkant", modulator.apply (cutoff, 500.0f));
        processor.set ("lfo10Mode", 1.0f);
        processor.set ("lfo10Hz", 4.1f);
        processor.set ("lfo10Snap", 1.0f);
        processor.set ("lfo10SnapRange", 5.0f);
        modulator.update (1.0, 120.0);
        check (modulator.snapLocked[9].load() && modulator.snapNote[9].load() == 4, "LFO 10: SNAP låser mot 1/8");
        modulator.activeMask = 7u;                              // borttagen: fälten nollställs
        modulator.update (1.0, 120.0);
        check (! modulator.snapLocked[9].load() && modulator.snapNote[9].load() == -1, "borttagen LFO 10 nollställs");
    }

    // LFO 1-3 ska vara bitidentiska oavsett vad som händer med LFO 4-10.
    void testExtraLfosLeaveFirstThree()
    {
        std::puts ("LFO 4-10: LFO 1-3 orörda när 4-10 slås av och på");
        DummyProcessor a (true), b (true);
        ModRoutes routesA, routesB;
        Modulator modA (a.apvts, routesA), modB (b.apvts, routesB);
        const auto cutoffA = modA.indexOf ("bassCutoff");
        const auto cutoffB = modB.indexOf ("bassCutoff");
        for (auto* p : { &a, &b })
        {
            p->set ("lfo1Rate", 5.0f);
            p->set ("lfo2Shape", 2.0f);
            p->set ("lfo3Mode", 1.0f);
            p->set ("lfo3Hz", 2.3f);
        }
        for (int s = 0; s < Modulation::numLfos; ++s)
        {
            routesA.add (s, cutoffA, 0.1f * (float) (s + 1));
            routesB.add (s, cutoffB, 0.1f * (float) (s + 1));
        }
        bool same = true;
        for (int step = 0; step < 400; ++step)
        {
            if (step % 7 == 0)
                modB.activeMask = 7u | ((uint32_t) (step * 2654435761u) & 0x3F8u); // slumpvis LFO 4-10 av och på
            const auto ppq = step * 0.013;
            modA.update (ppq, 128.0);
            modB.update (ppq, 128.0);
            same = same && modA.apply (cutoffA, 500.0f) == modB.apply (cutoffB, 500.0f);
        }
        check (same, "LFO 1-3 ger exakt samma värden");
    }

    // BZ: Bézierläge i NodeEnvelope.
    void testBezierEnvelope()
    {
        std::puts ("BZ: Bézier-envelope (tension orörd, handtag, monoton, rundtur)");
        const std::vector<NodeEnvelope::Point> pts { { 0.0f, 0.0f, -0.6f }, { 0.5f, 1.0f, 0.4f }, { 1.0f, 0.2f, 0.0f } };

        NodeEnvelope plain (1.0f, 1.0f, pts);
        NodeEnvelope bezier (1.0f, 1.0f, pts);
        check (bezier.getMode() == NodeEnvelope::Mode::tension, "standardläget är tension");

        // Gamla ValueTree utan mode/handtag läses som tension och ger samma kurva.
        NodeEnvelope loaded (1.0f, 1.0f, { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f } });
        loaded.fromValueTree (plain.toValueTree ("Curve"));
        auto same = true;
        for (int i = 0; i <= 100; ++i)
            same = same && loaded.evaluate ((float) i / 100.0f) == plain.evaluate ((float) i / 100.0f);
        check (same && loaded.getMode() == NodeEnvelope::Mode::tension, "tension-kurvan är oförändrad efter rundtur");
        check (! plain.toValueTree ("Curve").hasProperty ("mode"), "tension skriver inget mode-attribut");

        // Handtag från tension: Bézierkurvan följer tension-kurvan (liten avvikelse tillåten).
        bezier.setPoints (NodeEnvelope::withHandlesFromTension (pts));
        bezier.setMode (NodeEnvelope::Mode::bezier);
        auto worst = 0.0f;
        for (int i = 0; i <= 200; ++i)
            worst = std::max (worst, std::abs (bezier.evaluate ((float) i / 200.0f) - plain.evaluate ((float) i / 200.0f)));
        // En kubisk kurva träffar tension-kurvan exakt bara vid t=1/3 och 2/3; för branta tensioner (-0.6) blir största avvikelsen ca 0.14.
        check (worst < 0.15f, "Bézier med handtag från tension ligger nära tension-kurvan", worst);

        // Måttlig tension: avvikelsen är liten.
        const std::vector<NodeEnvelope::Point> mild { { 0.0f, 0.0f, -0.3f }, { 0.5f, 1.0f, 0.4f }, { 1.0f, 0.2f, 0.0f } };
        NodeEnvelope mildPlain (1.0f, 1.0f, mild), mildBezier (1.0f, 1.0f, mild);
        mildBezier.setPoints (NodeEnvelope::withHandlesFromTension (mild));
        mildBezier.setMode (NodeEnvelope::Mode::bezier);
        auto mildWorst = 0.0f;
        for (int i = 0; i <= 200; ++i)
            mildWorst = std::max (mildWorst, std::abs (mildBezier.evaluate ((float) i / 200.0f) - mildPlain.evaluate ((float) i / 200.0f)));
        check (mildWorst < 0.08f, "Bézier med handtag från måttlig tension ligger nära tension-kurvan", mildWorst);

        // Handtag med x utanför segmentet klipps, och kurvan är monoton i tiden (ger exakt ett värde per x, inga hopp).
        auto wild = pts;
        wild[0].outDx = 5.0f;
        wild[1].inDx = -5.0f;
        wild[0].outDy = 0.3f;
        bezier.setPoints (wild);
        check (bezier.getPoints()[0].outDx <= 0.5f && bezier.getPoints()[1].inDx >= -0.5f, "handtagens x klipps till segmentet");
        auto bounded = true;
        for (int i = 0; i <= 200; ++i)
        {
            const auto v = bezier.evaluate ((float) i / 200.0f);
            bounded = bounded && v >= 0.0f && v <= 1.0f;
        }
        check (bounded, "Bézier-värden hålls inom 0..maxValue");

        // Rundtur med handtag.
        NodeEnvelope again (1.0f, 1.0f, { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f } });
        again.fromValueTree (bezier.toValueTree ("Curve"));
        auto equal = again.getMode() == NodeEnvelope::Mode::bezier;
        for (int i = 0; i <= 100; ++i)
            equal = equal && std::abs (again.evaluate ((float) i / 100.0f) - bezier.evaluate ((float) i / 100.0f)) < 1.0e-5f;
        check (equal, "Bézier-kurvan överlever rundtur i ValueTree");
    }
}

void runModulationTests()
{
    testShapes();
    testModulator();
    testSources();
    testCustomShapeAndFreeRate();
    testExtraLfos();
    testExtraLfosLeaveFirstThree();
    testBezierEnvelope();
}
