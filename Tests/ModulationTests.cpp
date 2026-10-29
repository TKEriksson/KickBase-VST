// Tester för LFO:erna och moduleringen.

#include "../Source/Core/Modulation.h"
#include "../Source/Core/Params.h"
#include "../Source/Core/Effects.h"
#include "../Source/Core/Sidechain.h"
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
            Params::addMaxTimeParam (layout);
            if (extraLfos)
                Params::addEngineTimingParams (layout), Params::addEngineTimingExtraParams (layout);
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

        check (Modulation::rateNames().size() == 19 && Modulation::rateInQuarters (17) == 64.0 && Modulation::rateInQuarters (11) == 0.75, "fler synkade hastigheter (3/16 ... 16 takter)");
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

    // Live Code-källorna (CODE 1-8): LFO i Hz, ENV (AD) på instrumentets nya ton, utfasning och idle.
    void testCodeSources()
    {
        std::puts ("CODE-källor: LFO, ENV, utfasning och idle");
        DummyProcessor processor;
        ModRoutes routes;
        Modulator modulator (processor.apvts, routes);
        modulator.setSampleRate (3200.0); // ett kontrollsteg = 10 ms, så 100 steg = 1 s
        const auto src = Modulation::firstCodeSource;
        auto step = [&] (int n) { for (int i = 0; i < n; ++i) modulator.update (0.0); };
        auto& slot = modulator.codeSlots[0];

        step (1);
        check (slot.idle.load() && modulator.sourceValue (src) == 0.0f, "CODE: oanvänd plats är idle och 0");
        step (5);
        check (slot.idle.load() && modulator.sourceValue (src) == 0.0f, "CODE: plats med kind 0 förblir idle och 0");

        // LFO: spann, form och antal uppåtgående nollgenomgångar (perioder) under en viss tid.
        auto lfoStats = [&] (float hz, int shape, int steps, float& lo, float& hi, int& upward, bool& onlyPlusMinusOne)
        {
            slot.rate = hz;
            slot.shape = shape;
            slot.kind = 1;
            step (5); // tonar in på 20 ms
            lo = 2.0f; hi = -2.0f; upward = 0; onlyPlusMinusOne = true;
            auto previous = modulator.sourceValue (src);
            for (int i = 0; i < steps; ++i)
            {
                step (1);
                const auto v = modulator.sourceValue (src);
                lo = std::min (lo, v);
                hi = std::max (hi, v);
                if (previous < 0.0f && v >= 0.0f) ++upward;
                if (std::abs (std::abs (v) - 1.0f) > 1.0e-4f) onlyPlusMinusOne = false;
                previous = v;
            }
            slot.kind = 0;
            step (5); // helt utfasad: idle igen
        };
        float lo = 0.0f, hi = 0.0f;
        int upward = 0;
        bool pm = false;
        lfoStats (1.0f, 0, 300, lo, hi, upward, pm);
        check (hi > 0.99f && hi <= 1.0001f && lo < -0.99f && lo >= -1.0001f, "CODE-LFO sinus 1 Hz går -1..1", hi);
        check (upward >= 2 && upward <= 4, "CODE-LFO 1 Hz: tre perioder på 3 s", (double) upward);
        lfoStats (4.0f, 0, 300, lo, hi, upward, pm);
        check (upward >= 11 && upward <= 13, "CODE-LFO 4 Hz ger fyra gånger så många perioder", (double) upward);
        lfoStats (2.0f, 4, 300, lo, hi, upward, pm);
        check (pm && upward >= 5 && upward <= 7, "CODE-LFO fyrkant är bara +1/-1", (double) upward);
        for (int shape = 0; shape <= 5; ++shape)
        {
            lfoStats (3.0f, shape, 200, lo, hi, upward, pm);
            check (lo >= -1.0001f && hi <= 1.0001f && hi - lo > 0.1f, ("CODE-LFO form " + juce::String (shape) + " håller sig inom -1..1 och rör sig").toRawUTF8(), hi - lo);
        }

        // Utfasning: stäng under gång; värdet går ner mot 0 på ~20 ms och platsen blir idle först då.
        slot.rate = 1.0f;
        slot.shape = 4; // fyrkant börjar på +1
        slot.kind = 1;
        step (10);
        const auto onValue = modulator.sourceValue (src);
        slot.kind = 0;
        step (1);
        const auto fading = modulator.sourceValue (src);
        check (onValue > 0.99f && fading > 0.0f && fading < onValue && ! slot.idle.load(), "CODE: stängd plats tonar ut och är inte idle än", fading);
        step (5);
        check (modulator.sourceValue (src) == 0.0f && slot.idle.load(), "CODE: efter ~20 ms är platsen 0 och idle");

        // ENV: noll utan ny ton, stiger över attack, faller över decay, startar om på ny ton, bara instrumentets egna toner räknas.
        auto& env = modulator.codeSlots[1];
        const auto envSrc = src + 1;
        modulator.setInstrumentNoteCount (2, 5);
        env.layer = 2;
        env.attackMs = 100.0f;
        env.decayMs = 200.0f;
        env.kind = 2;
        step (10);
        check (modulator.sourceValue (envSrc) == 0.0f, "CODE-ENV: toner som redan klingat startar inte ENV");
        modulator.setInstrumentNoteCount (2, 6);
        float previousValue = 0.0f, peak = 0.0f;
        bool rising = true, falling = true;
        int peakStep = 0;
        for (int i = 1; i <= 40; ++i)
        {
            step (1);
            const auto v = modulator.sourceValue (envSrc);
            peak = std::max (peak, v);
            if (v >= peak) peakStep = i;
            if (i <= 9) rising = rising && v >= previousValue;
            if (i > 11 && i <= 29) falling = falling && v <= previousValue + 1.0e-6f;
            previousValue = v;
        }
        check (peak > 0.9f && peak <= 1.0001f, "CODE-ENV: toppar nära 1 efter attacken", peak);
        check (peakStep >= 9 && peakStep <= 11, "CODE-ENV: toppen ligger vid attack-tiden (100 ms)", (double) peakStep);
        check (rising && falling, "CODE-ENV: stiger under attack och faller under decay");
        check (modulator.sourceValue (envSrc) == 0.0f, "CODE-ENV: 0 efter attack + decay (300 ms)");
        modulator.setInstrumentNoteCount (2, 7);
        step (3);
        const auto restarted = modulator.sourceValue (envSrc);
        check (std::abs (restarted - 0.3f) < 0.02f, "CODE-ENV: ny ton startar om från botten (30 ms in = 0,3)", restarted);
        modulator.setInstrumentNoteCount (1, 99); // annat instrument: ska inte starta om ENV:n
        step (1);
        check (std::abs (modulator.sourceValue (envSrc) - 0.4f) < 0.02f, "CODE-ENV: andra instrumentets toner startar inte om (40 ms in = 0,4)", modulator.sourceValue (envSrc));
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

namespace
{
    // MAX-knappen (tidstak per instrument): standardvärdet ska ge exakt dagens tak, så att gamla presets låter och ser likadana ut.
    void testFmSourceDefault()
    {
        std::puts ("FM-modulator: standard OSC 2");
        DummyProcessor processor;
        auto* fmSource = processor.apvts.getRawParameterValue (ParamID::bass (0, InstrumentField::fmSource));
        check (fmSource != nullptr && (int) fmSource->load() == 0, "fmSource har standard OSC 2");
    }

    // SYNC-listorna fick 1/32 och 1/64: de nya värdena ligger sist så att index i gamla projekt och presets betyder samma sak som förut.
    void testSyncListsKeepOldIndexes()
    {
        std::puts ("SYNC-listor: gamla index oförändrade, 1/32 och 1/64 sist");
        const char* oldTiming[] = { "Off", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
        DummyProcessor processor (true);
        auto found = 0;
        for (int engine = 0; engine < ParamID::numTimingEngines; ++engine)
            for (const auto field : { ParamID::timingField (engine, 1), ParamID::timingExtraField (engine, 1) })
            {
                auto* param = processor.apvts.getParameter (ParamID::bass (0, field));
                check (param != nullptr, "TIMING-sync-parametern finns");
                if (param == nullptr)
                    continue;
                const auto names = param->getAllValueStrings();
                ++found;
                check (names.size() == 10 && names[8] == "1/32" && names[9] == "1/64", "TIMING-sync har 1/32 och 1/64 sist");
                for (int i = 0; i < 8; ++i)
                    check (names[i] == oldTiming[i], "TIMING-sync: gammalt index oförändrat", (float) i);
            }
        check (found > 0, "hittade TIMING-sync-parametrar", (float) found);

        const char* oldFx[] = { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "3/16", "1/4", "3/8", "1/2", "1 bar", "2 bars", "4 bars" };
        const auto fx = Effects::syncNames();
        check (fx.size() == 13 && fx[12] == "1/64", "FX-sync har 1/64 sist", (float) fx.size());
        for (int i = 0; i < 12; ++i)
            check (fx[i] == oldFx[i], "FX-sync: gammalt index oförändrat", (float) i);
        check (std::abs (Effects::syncInQuarters (12) - 0.0625) < 1.0e-9, "FX-sync 1/64 = 1/16 fjärdedel");

        const auto length = Sidechain::lengthNames();
        const double oldLength[] = { 0.0, 0.25, 0.5, 0.75, 1.0, 2.0 };
        check (length.size() == 8 && length[6] == "1/32" && length[7] == "1/64", "Sidechain-längd har 1/32 och 1/64 sist");
        for (int i = 0; i < 6; ++i)
            check (std::abs (Sidechain::lengthInQuarters (i) - oldLength[i]) < 1.0e-9, "Sidechain-längd: gammalt index oförändrat", (float) i);
        check (std::abs (Sidechain::lengthInQuarters (7) - 0.0625) < 1.0e-9, "Sidechain 1/64 = 1/16 fjärdedel");

        const auto rates = Modulation::rateNames();
        const char* oldRates[] = { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "1/4 T", "1/4", "1/2", "1 bar", "2 bars", "4 bars", "3/16", "3/8", "3/4", "3 bars", "6 bars", "8 bars", "16 bars" };
        check (rates.size() == 19 && rates[18] == "1/64", "LFO-rate har 1/64 sist", (float) rates.size());
        for (int i = 0; i < 18; ++i)
            check (rates[i] == oldRates[i], "LFO-rate: gammalt index oförändrat", (float) i);
        check (std::abs (Modulation::rateInQuarters (18) - 0.0625) < 1.0e-9, "LFO 1/64 = 1/16 fjärdedel");
    }

    void testMaxTimeDefault()
    {
        std::puts ("MAX-tid: standard 1 s ger de gamla taken");
        DummyProcessor processor;
        auto* maxTime = processor.apvts.getRawParameterValue (ParamID::bass (0, InstrumentField::maxTime));
        check (maxTime != nullptr && (int) maxTime->load() == ParamID::maxTimeDefault, "maxTime har standardsteget");
        check (ParamID::maxTimeMs (ParamID::maxTimeDefault) == 1000, "standardsteget är 1 s", (float) ParamID::maxTimeMs (ParamID::maxTimeDefault));
        const float expected[] = { 50.0f, 2000.0f, 300.0f, 1000.0f, 1000.0f, 400.0f }; // attack, decay, release, pitch-env decay, filter-env decay, FM env time
        for (int i = 0; i < ParamID::numTimeCaps; ++i)
            check (ParamID::timeCapMs (ParamID::timeCapSpec (i), ParamID::maxTimeDefault) == expected[i], "tidstaket vid standard = dagens max", ParamID::timeCapMs (ParamID::timeCapSpec (i), ParamID::maxTimeDefault));
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
    testCodeSources();
    testBezierEnvelope();
    testMaxTimeDefault();
    testFmSourceDefault();
    testSyncListsKeepOldIndexes();
}
