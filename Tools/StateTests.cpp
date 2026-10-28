// `tool persisttest`: sparande och laddning av det som inte är parametrar (egna wavetables, samplingar) och preset-laddningens ordning,
// samt att data som ljudtråden kan läsa inte frigörs för tidigt. Körs i en tillfällig preset-mapp (Scenarios::TemporaryUserFolders).

#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Core/ToolLayer.h"
#include "../Source/Core/ProjectFile.h"
#include "../Source/Ui/Help.h"
#include "../Source/Ui/JogPanel.h"

namespace
{
    std::unique_ptr<InstrumentProcessor> copyOf (PluginBase& source)
    {
        juce::MemoryBlock state;
        source.getStateInformation (state);
        auto copy = std::make_unique<InstrumentProcessor>();
        copy->setStateInformation (state.getData(), (int) state.getSize());
        return copy;
    }

    // Lyssnar på en parameter och minns varje värde den fått.
    struct Recorder : juce::AudioProcessorParameter::Listener
    {
        std::vector<float> values;
        void parameterValueChanged (int, float newValue) override { values.push_back (newValue); }
        void parameterGestureChanged (int, bool) override {}
    };
}

// `tool helpcheck`: parametrar och effektrattar som saknar en förklaring (tooltip). Instrument 2-10 och FX2-FX11 hoppas över (samma texter).
int helpCheck()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto missing = 0;
    for (const auto fx : { false, true })
    {
        std::unique_ptr<PluginBase> processor;
        if (fx)
            processor = std::make_unique<FxProcessor>();
        else
            processor = std::make_unique<InstrumentProcessor>();
        for (auto* p : processor->getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto id = ranged->getParameterID();
                if (id == ParamID::patternBars || id.startsWith ("rowBars")) // oanvända sedan R9 (kvar för gamla projekt)
                    continue;
                if ((Presets::instrumentLayerOf (id) > 0) || (Effects::isEffectParameter (id) && id[2] != 'A') || id.startsWith ("mixBass") && ! id.startsWith ("mixBass1")
                    || (id.startsWith ("mixFx") && ! id.startsWith ("mixFxA")))
                    continue;
                if (Help::forParameter (id).isEmpty())
                {
                    std::printf ("%s: %s (%s)\n", fx ? "fx" : "instrument", id.toRawUTF8(), ranged->getName (40).toRawUTF8());
                    ++missing;
                }
            }
    }
    for (int type = 1; type < numEffectTypes; ++type)
        for (const auto& knob : Effects::knobs ((EffectType) type))
            if (const auto text = Help::forEffectKnob (knob.name, Effects::typeNames()[type]); text.isEmpty() || text == knob.name) // (utan text ger den namnet)
            {
                std::printf ("effekt %s: %s\n", Effects::name ((EffectType) type).toRawUTF8(), knob.name.toRawUTF8());
                ++missing;
            }
    std::printf ("%d utan förklaring\n", missing);
    return 0;
}

int persistTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };
    const auto tableFile = Scenarios::writeTestWav ("persist_table", 21.533, 0.2);

    // 0. JOG-loopens tid: 1 tick = 1/64 takt, bar.beat.sixteenth.tick skrivs och tolkas åt båda hållen.
    {
        check (TransportClock::ticksToPpq (64, 4.0) == 4.0 && TransportClock::ticksToPpq (16, 4.0) == 1.0 && TransportClock::ticksToPpq (4, 4.0) == 0.25
                   && TransportClock::ticksToPpq (1, 4.0) == 1.0 / 16.0, "1 tick = 1/64 takt (64 = en takt, 16 = en fjärdedel, 4 = en sextondel)");
        check (JogTicks::format (0) == "1.1.1.0" && JogTicks::format (64 * 4) == "5.1.1.0" && JogTicks::format (64 + 16 * 2 + 4 * 3 + 1) == "2.3.4.1", "tid visas som bar.beat.sixteenth.tick");
        auto roundTrip = true;
        for (juce::int64 t = 0; t < 64 * 9; ++t)
            roundTrip &= JogTicks::parse (JogTicks::format (t)) == t;
        check (roundTrip, "visad tid tolkas tillbaka till samma tick");
        check (JogTicks::parse ("3") == 128 && JogTicks::parse ("2.2") == 64 + 16 && JogTicks::parse ("") == 0 && JogTicks::parse ("1.9.9.9") == 16 * 3 + 4 * 3 + 3, "saknade delar blir 1/1/1/0 och för stora delar klipps");
    }

    // 1. Egna wavetables på alla tre oscillatorerna följer med projektet (OSC 3 lästes inte tillbaka förut).
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            check (processor->loadUserTable (1, osc, tableFile), "egen wavetable laddas på instrument 2, OSC " + juce::String (osc + 1));
        auto copy = copyOf (*processor);
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            check (copy->userTable (1, osc) != nullptr && copy->getUserTableName (1, osc) == processor->getUserTableName (1, osc),
                   "projektet återskapar tabellen på OSC " + juce::String (osc + 1));

        // 2. Ett tillstånd utan tabeller i en instans som har tabeller: inga gamla tabeller får ligga kvar.
        auto empty = std::make_unique<InstrumentProcessor>();
        juce::MemoryBlock emptyState;
        empty->getStateInformation (emptyState);
        copy->setStateInformation (emptyState.getData(), (int) emptyState.getSize());
        auto none = true;
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            none = none && copy->userTable (1, osc) == nullptr;
        check (none, "ett tillstånd utan tabeller tar bort de gamla (t.ex. vid ångra eller byte av projekt)");
    }

    // 3. Full- och instrument-presets har med tabellerna.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->loadUserTable (2, 2, tableFile);
        check (processor->saveUserPreset (Presets::Section::full, "__persist_full__"), "Full-preset sparas");
        check (processor->saveUserPreset (Presets::Section::bass, "__persist_instr__", 2), "instrument-preset sparas");

        auto other = std::make_unique<InstrumentProcessor>();
        other->loadUserPreset (Presets::Section::full, processor->findUserPreset (Presets::Section::full, "__persist_full__"));
        check (other->userTable (2, 2) != nullptr, "Full-presetet ger instrument 3 sin tabell på OSC 3");

        auto third = std::make_unique<InstrumentProcessor>();
        third->loadUserTable (5, 0, tableFile); // ska inte röras av ett instrument-preset i ett annat lager
        third->loadUserPreset (Presets::Section::bass, processor->findUserPreset (Presets::Section::bass, "__persist_instr__"), 4);
        check (third->userTable (4, 2) != nullptr && third->userTable (4, 0) == nullptr, "instrument-presetet lägger tabellen i lagret det laddas i (instrument 5, OSC 3)");
        check (third->userTable (5, 0) != nullptr, "andra lagers tabeller rörs inte");

        // Ett äldre instrument-preset utan tabeller rör inte lagrets tabell (som förut).
        auto xml = juce::XmlDocument::parse (processor->findUserPreset (Presets::Section::bass, "__persist_instr__"));
        xml->removeChildElement (xml->getChildByName ("UserTables"), true);
        third->loadUserTable (6, 1, tableFile);
        third->loadPresetXml (*xml, Presets::Section::bass, 6, "old");
        check (third->userTable (6, 1) != nullptr, "ett äldre instrument-preset utan tabeller lämnar lagrets tabell orörd");

        // Fabriksljud (Init) har inga egna tabeller: de gamla försvinner.
        third->loadFactoryPreset (Presets::Section::full, 0);
        check (third->userTable (5, 0) == nullptr && third->userTable (4, 2) == nullptr, "Init tar bort egna tabeller");
    }

    // 4. Byts en sampling eller tabell många gånger i snabb följd frigörs ingen av de gamla direkt (ljudtråden kan läsa dem).
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        const auto sample = Scenarios::writeTestWav ("persist_sample", 110.0, 0.3);
        processor->loadInstrumentSample (0, sample);
        const auto* first = processor->getActiveInstrumentSample (0);
        processor->loadInstrumentSample (0, sample);
        processor->clearInstrumentSample (0);
        processor->loadInstrumentSample (0, sample);
        // `first` ska fortfarande gå att läsa (förr frigjordes den vid det andra bytet).
        auto readable = first != nullptr && first->audio.getNumSamples() > 0 && std::isfinite (first->audio.getSample (0, 10));
        check (readable, "en sampling som bytts ut tre gånger i rad finns kvar en stund (ljudtråden kan läsa den)");

        for (int layer = 0; layer < 8; ++layer)
            processor->loadUserTable (layer, layer % 3, tableFile);
        processor->setInstrumentCount (10);
        processor->setInstrumentCount (1); // tar bort instrument 2-10 med sina tabeller på en gång
        check (processor->retiredTableCount() >= 7, "tabellerna från borttagna instrument frigörs inte direkt (" + juce::String ((int) processor->retiredTableCount()) + " väntar)");
    }

    // 3b. Multisampling: zonerna följer med projektet i ett eget barn; utan zoner skrivs inget (gamla projekt oförändrade).
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        check (processor->instrumentMultiSamplesToValueTree().getNumChildren() == 0, "Multi: utan zoner är trädet tomt");
        juce::AudioBuffer<float> tone (1, 4410);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample (0, i, std::sin (0.05f * (float) i));
        processor->instrumentMultiSample (1).addZone (tone, 44100.0, "A_C3", { 48, 0, 60, 1, 127, 5.0f });
        processor->instrumentMultiSample (1).addZone (tone, 44100.0, "A_C5", { 72, 61, 127, 1, 127, 0.0f });
        auto tree = processor->instrumentMultiSamplesToValueTree();
        check (tree.getNumChildren() == 1, "Multi: bara instrumentet med zoner skrivs");

        auto other = std::make_unique<InstrumentProcessor>();
        other->instrumentMultiSamplesFromValueTree (tree);
        const auto& back = other->instrumentMultiSample (1);
        check (back.zoneCount() == 2 && back.zoneName (1) == "A_C5" && back.mapping (0).keyHigh == 60 && back.mapping (0).tuneCents == 5.0f && back.mapping (1).rootKey == 72,
               "Multi: zoner, namn och mappning följer med projektet");
        check (other->instrumentMultiSample (0).zoneCount() == 0, "Multi: övriga instrument förblir tomma");
    }

    // 4a. Slicer: Grid 8 ger 8 skivor (första 0, stigande), standardinställningar sparas som förut, och skivinställningarna följer med projektet.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->loadInstrumentSample (0, Scenarios::writeTestWav ("persist_slices", 110.0, 0.8)); // 0,8 s / 128 = 6,25 ms: över minsta skivan (5 ms), annars blir det ca 120
        const auto* sample = processor->getActiveInstrumentSample (0);
        check (sample != nullptr && sample->slices.size() == 8 && sample->slices.front() == 0 && std::is_sorted (sample->slices.begin(), sample->slices.end()), "Slicer: Grid 8 ger 8 stigande skivor");

        auto before = processor->instrumentSamplesToValueTree();
        check (! before.getChild (0).hasProperty ("sliceMode"), "Slicer: standardinställningar skriver inga nya attribut (gamla filer oförändrade)");

        processor->setInstrumentSlicing (0, SampleSlicing::grid, 0.5f, 200);
        check (processor->getActiveInstrumentSample (0)->slices.size() == (size_t) SampleSlicing::maxSlices, "Slicer: max 128 skivor");
        processor->setInstrumentSlicing (0, SampleSlicing::manual, 0.5f, 8);
        processor->setInstrumentSliceMarkers (0, { 0.5f, 0.25f });
        check (processor->getActiveInstrumentSample (0)->slices.size() == 3, "Slicer: manuella markörer ger skivor (0 + två)");

        auto other = std::make_unique<InstrumentProcessor>();
        other->instrumentSamplesFromValueTree (processor->instrumentSamplesToValueTree());
        check (other->getActiveInstrumentSample (0) != nullptr && other->getActiveInstrumentSample (0)->slices == processor->getActiveInstrumentSample (0)->slices
               && other->instrumentSampleSlot (0).sliceMode() == SampleSlicing::manual, "Slicer: skivinställningarna följer med projektet");

        // syncSliceRows: en rad (Kind::slices) när det finns skivor, ingen utan; ett undosteg per ändring; noterna i raden behålls när antalet skivor ändras.
        const auto sliceRowOf = [&] (const InstrumentProcessor& p) { return p.pattern.rows.find (0, RowRegistry::Kind::slices); };
        check (sliceRowOf (*processor) >= 0, "Slice-raden finns när det finns skivor (manuella markörer)");
        processor->setInstrumentSlicing (0, SampleSlicing::grid, 0.5f, 8);
        const auto sliceRow = sliceRowOf (*processor);
        check (sliceRow >= 0, "Slice-raden finns med Grid 8");
        processor->pattern.addNote (sliceRow, 0, 24, RowRegistry::pitchOfSlice (5));
        processor->setInstrumentSlicing (0, SampleSlicing::grid, 0.5f, 100);
        check (sliceRowOf (*processor) == sliceRow && processor->pattern.note (sliceRow, 0).on.load(), "Slice-raden och dess noter ligger kvar när antalet skivor ändras");
        processor->undoManager.undo();
        check (sliceRowOf (*processor) == sliceRow, "Slice-raden: ett undo tar tillbaka hela ändringen");
        processor->undoManager.redo();
        check (sliceRowOf (*processor) == sliceRow, "Slice-raden: redo ger samma rad");

        // Migrering: en gammal sparad rad "i0.slice2" läses in i "i0.slices" med pitch = skivindex (tre = skiva 2 räknat från 0).
        {
            auto state = processor->pattern.toValueTree();
            auto old = state.getChildWithProperty ("row", "i0.slices");
            check (old.isValid(), "Slice-raden sparas med nyckeln i0.slices");
            if (old.isValid())
            {
                state.removeChild (old, nullptr);
                auto legacy = old.createCopy();
                legacy.setProperty ("row", "i0.slice2", nullptr);
                state.appendChild (legacy, nullptr);
                Pattern loaded;
                loaded.fromValueTree (state);
                const auto id = loaded.rows.find (0, RowRegistry::Kind::slices);
                check (id >= 0, "Gammal rad i0.slice2 läses in som raden i0.slices");
                if (id >= 0)
                {
                    Pattern::StackNote got;
                    check (loaded.getStack (id, 0, 0, got) && got.pitch == RowRegistry::pitchOfSlice (2), "Gammal slice-rad: noterna får skivans index som pitch");
                }
            }
        }
    }

    // 4a2. Skivor i ljudet: raderna "Slice 1..8" (Grid 8) spelar var sin skiva, takterna utan skivton är tysta, REVERSE låter annorlunda
    //      och NOTE-raden spelar hela samplingen.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        using F = InstrumentField;
        Scenarios::setInstrument (*processor, 0, F::on, 1.0f);
        for (const auto field : { F::aOn, F::bOn, F::cOn })
            Scenarios::setInstrument (*processor, 0, field, 0.0f);
        Scenarios::setInstrument (*processor, 0, F::sub, 0.0f);
        Scenarios::setInstrument (*processor, 0, F::noise, 0.0f);
        Scenarios::setInstrument (*processor, 0, F::sampleOn, 1.0f);
        Scenarios::setInstrument (*processor, 0, F::sampleLevel, 1.0f);
        processor->loadInstrumentSample (0, Scenarios::writeTestWav ("persist_slicer_audio", 110.0, 0.6));
        for (int other = 1; other < 3; ++other)
            Scenarios::setInstrument (*processor, other, F::on, 0.0f);
        auto* kick = processor->apvts.getParameter (ParamID::kickOn);
        kick->setValueNotifyingHost (kick->convertTo0to1 (0.0f));

        const std::array<int, 10> pitches { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; // 1 och 10 ligger utanför skivorna 2..9
        processor->pattern.clearNotes (0);
        for (int bar = 0; bar < (int) pitches.size(); ++bar)
            processor->pattern.addNote (0, bar * Pattern::ticksPerBar, Pattern::ticksPerBar - 12, pitches[(size_t) bar]);
        processor->pattern.setCustom (0, true);
        processor->patternChanged();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);

        const auto bpm = 140.0, rate = 48000.0;
        const auto barSamples = (int) (rate * 60.0 / bpm * 4.0);
        const auto window = (int) (0.07 * rate); // en skiva är 0,6 s / 8 = 75 ms
        auto render = [&]
        {
            OfflineRender::Settings settings;
            settings.sampleRate = rate;
            settings.bpm = bpm;
            settings.bars = (int) pitches.size();
            settings.notes = { 52 };
            return OfflineRender::render (*processor, settings);
        };
        auto peakOf = [&] (const juce::AudioBuffer<float>& audio, int bar) { return audio.getMagnitude (0, bar * barSamples, window); };
        auto differs = [&] (const juce::AudioBuffer<float>& a, int barA, const juce::AudioBuffer<float>& b, int barB)
        {
            for (int i = 0; i < window; ++i)
                if (std::abs (a.getSample (0, barA * barSamples + i) - b.getSample (0, barB * barSamples + i)) > 1.0e-4f)
                    return true;
            return false;
        };

        const auto whole = render(); // vanliga NOTE-radens toner: hela samplingen i varje takt

        // Skivraden: en ton med pitch = skiva k i takt k+1, takt 0 och 9 tomma.
        processor->pattern.clearNotes (0);
        {
            auto row = processor->pattern.rows.find (0, RowRegistry::Kind::slices);
            if (row < 0)
                row = processor->pattern.rows.add (0, RowRegistry::Kind::slices, 0);
            processor->pattern.setManualBars (row, (int) pitches.size());
            for (int slice = 0; slice < 8; ++slice)
                processor->pattern.addNote (row, (slice + 1) * Pattern::ticksPerBar, Pattern::ticksPerBar - 12, RowRegistry::pitchOfSlice (slice));
        }
        processor->patternChanged();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
        const auto sliced = render();

        // Diagnos: peak per takt (fönstret är 70 ms) så att loggen visar varför ett krav faller.
        for (int bar = 0; bar < (int) pitches.size(); ++bar)
            std::printf ("  slicer takt %d (ton %d): av %.5f  på %.5f\n", bar, pitches[(size_t) bar], peakOf (whole, bar), peakOf (sliced, bar));
        std::printf ("  skivor: %d, hela samplingen %d samplar, fönster %d samplar\n",
                     (int) processor->getActiveInstrumentSample (0)->slices.size(), processor->getActiveInstrumentSample (0)->audio.getNumSamples(), window);

        auto audible = true;
        for (int bar = 1; bar <= 8; ++bar)
            audible = audible && peakOf (sliced, bar) > 0.001f;
        check (audible, "Slicer: tonerna 2..9 spelar var sin skiva (hörbara)");
        check (peakOf (sliced, 0) < 1.0e-4f && peakOf (sliced, 9) < 1.0e-4f, "Slicer: tonen före och efter skivorna är tyst (ingen wrap)");
        check (differs (sliced, 1, sliced, 2) && differs (sliced, 2, sliced, 5) && peakOf (sliced, 1) > peakOf (sliced, 8), "Slicer: skivorna har olika innehåll (decay-tonen sjunker)");
        check (differs (whole, 1, sliced, 2) && peakOf (whole, 0) > 0.001f, "NOTE-raden spelar hela samplingen (tonen i takt 0 hörs), skivljudet är annat");

        // Slice 3-ekvivalens: en not i raden "Slice 3" (takt 3) låter som skivan spelad direkt på skivindex 2 (samma väg som provlyssningen i skivvyn).
        {
            constexpr int block = 512;
            processor->setPlayConfigDetails (0, 2, rate, block);
            processor->prepareToPlay (rate, block);
            juce::MidiBuffer noMidi;
            const auto slice3Start = (int) std::lround (3 * rate * 60.0 / bpm * 4.0); // takt 3 exakt (barSamples är avrundad nedåt)
            processor->postNoteAudition (0, 60, 102, true, 2); // 102/127 = 0,803 mot mönsternotens 0,8: skillnaden är ~0,4 % av peak, därför 1 % tolerans
            juce::AudioBuffer<float> live (2, block);
            auto worst = 0.0f, livePeak = 0.0f;
            for (int start = 0; start < window; start += block)
            {
                live.clear();
                processor->processBlock (live, noMidi);
                for (int i = 0; i < std::min (block, window - start); ++i)
                {
                    worst = std::max (worst, std::abs (live.getSample (0, i) - sliced.getSample (0, slice3Start + start + i)));
                    livePeak = std::max (livePeak, std::abs (live.getSample (0, i)));
                }
            }
            processor->postNoteAudition (0, 60, 0, false, 2);
            std::printf ("  Slice 3 mot direkt skivindex 2: peak %.5f, största skillnad %.6f\n", livePeak, worst);
            check (livePeak > 0.001f && worst < 0.01f * livePeak, "Slice 3-raden låter som skivindex 2 spelat direkt (skillnad " + juce::String (worst, 6) + ")");
        }

        Scenarios::setInstrument (*processor, 0, F::slicerReverse, 1.0f);
        const auto reversed = render();
        std::printf ("  REVERSE takt 1: peak framåt %.5f, bakåt %.5f\n", peakOf (sliced, 1), peakOf (reversed, 1));
        check (differs (sliced, 1, reversed, 1), "Slicer: REVERSE låter annorlunda än framåt");

        Scenarios::setInstrument (*processor, 0, F::slicerReverse, 0.0f);
        const auto again = render();
        auto same = sliced.getNumSamples() == again.getNumSamples();
        for (int i = 0; same && i < sliced.getNumSamples(); ++i)
            same = sliced.getSample (0, i) == again.getSample (0, i);
        std::printf ("  REVERSE av igen: längd %d mot %d\n", again.getNumSamples(), sliced.getNumSamples());
        check (same, "Slicer: REVERSE av igen ger samma ljud som före (bit för bit)");
    }

    // 4b. Morph av: en WAV med fyra cykler ger fyra steg, och Morph av + antalet steg följer med projektet, Full- och instrument-presets (annars låter
    //     ett projekt annorlunda efter omladdning).
    {
        constexpr int cycle = 2048;
        juce::AudioBuffer<float> audio (1, 4 * cycle);
        for (int c = 0; c < 4; ++c)
            for (int i = 0; i < cycle; ++i)
                audio.setSample (0, c * cycle + i, 0.8f * (float) std::sin (juce::MathConstants<double>::twoPi * (c + 1) * (double) i / cycle));
        const auto fourCycles = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_scenario_persist_four.wav");
        OfflineRender::writeWav (audio, 44100.0, fourCycles);

        auto morphOf = [] (PluginBase& p, int layer, int osc) { return p.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscMorphField (osc)))->load(); };
        // Tabellen sparas som 16 bitar: det som mäts är att samma steg spelas, inte bit för bit (under -70 dB).
        auto sameSound = [] (const Wavetable& a, const Wavetable& b)
        {
            auto worst = 0.0f;
            for (const auto position : { 0.0f, 0.2f, 0.45f, 0.5f, 0.8f, 1.0f })
                for (const auto phase : { 0.05, 0.4, 0.9 })
                    worst = std::max (worst, std::abs (a.sample (position, phase, 512.0, false) - b.sample (position, phase, 512.0, false)));
            return worst < 3.0e-4f;
        };

        auto processor = std::make_unique<InstrumentProcessor>();
        check (processor->loadUserTable (0, 1, fourCycles) && processor->userTable (0, 1)->getSteps() == 4, "en WAV med 4 cykler ger en tabell med 4 steg");
        check (morphOf (*processor, 0, 1) > 0.5f, "Morph är på som standard");
        processor->apvts.getParameter (ParamID::bass (0, ParamID::oscMorphField (1)))->setValueNotifyingHost (0.0f);

        auto copy = copyOf (*processor);
        check (copy->userTable (0, 1) != nullptr && copy->userTable (0, 1)->getSteps() == 4 && morphOf (*copy, 0, 1) < 0.5f && sameSound (*copy->userTable (0, 1), *processor->userTable (0, 1)),
               "spara och ladda: Morph av och 4 steg, samma ljud med Morph av");

        check (processor->saveUserPreset (Presets::Section::bass, "__persist_morph__", 0), "instrument-preset med Morph av sparas");
        auto other = std::make_unique<InstrumentProcessor>();
        other->loadUserPreset (Presets::Section::bass, processor->findUserPreset (Presets::Section::bass, "__persist_morph__"), 3);
        check (other->userTable (3, 1) != nullptr && other->userTable (3, 1)->getSteps() == 4 && morphOf (*other, 3, 1) < 0.5f && sameSound (*other->userTable (3, 1), *processor->userTable (0, 1)),
               "instrument-preset: 4 steg och Morph av följer med (instrument 4)");
    }

    // 4c. ENV 2/3 i läget Sync: läget och notvärdena följer med projektet, och ett projekt utan fälten får ms (som förut).
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto value = [] (PluginBase& p, InstrumentField field) { return p.apvts.getRawParameterValue (ParamID::bass (1, field))->load(); };
        check (value (*processor, ParamID::envModeField (0)) < 0.5f && value (*processor, ParamID::envModeField (1)) < 0.5f, "ENV 2 och ENV 3 går i ms som standard");
        Scenarios::setInstrument (*processor, 1, ParamID::envModeField (1), 1.0f);
        Scenarios::setInstrument (*processor, 1, ParamID::envSyncField (1, 0), 10.0f);
        auto copy = copyOf (*processor);
        check (value (*copy, ParamID::envModeField (1)) > 0.5f && (int) value (*copy, ParamID::envSyncField (1, 0)) == 10 && value (*copy, ParamID::envModeField (0)) < 0.5f,
               "spara och ladda: ENV 3 Sync och 1/4 följer med, ENV 2 är kvar i ms");
    }

    // 4e. Live Code (steg D): koden och låsen följer med projektet och Full-presets; saknas elementet rörs inget; <LiveCode> sparas sist och bara när det behövs.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        juce::MemoryBlock plain;
        processor->getStateInformation (plain);
        auto plainXml = juce::AudioProcessor::getXmlFromBinary (plain.getData(), (int) plain.getSize());
        check (plainXml != nullptr && plainXml->getChildByName ("LiveCode") == nullptr, "standardtillstånd: inget <LiveCode> sparas");

        processor->setLiveCodeText ("$: s(\"bd*4\")\n// åäö");
        processor->liveCodeLocks = (1 << 2) | (1 << 10) | (1 << 12); // bit 12 ligger utanför de 11 giltiga
        auto* modArgs = new juce::DynamicObject();
        modArgs->setProperty ("code", "note(\"c2\").lfo(\"bass1Cutoff\",2,0.5).s(\"i1\")");
        modArgs->setProperty ("cycles", 1);
        modArgs->setProperty ("overwrite", true);
        ToolLayer tools (*processor);
        const auto modResult = tools.call ("live_code_create", juce::var (modArgs));
        check ((bool) modResult["ok"] && ! processor->codeMods.empty(), "testläge: CODE-modulering är aktiv före sparning: " + juce::JSON::toString (modResult).substring (0, 300));
        processor->liveCodeCodeLocks = 1 << 3; // F2: lås från koden sparas aldrig
        processor->liveCodeCodeNoteLocks = 1 << 4;
        juce::MemoryBlock saved;
        processor->getStateInformation (saved);
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        check (xml != nullptr && xml->getNumChildElements() > 0 && xml->getChildElement (xml->getNumChildElements() - 1)->hasTagName ("LiveCode"), "<LiveCode> sparas sist");
        auto savedCodeRoute = false;
        if (xml != nullptr)
            if (auto* routes = xml->getChildByName (ModRoutes::treeType.toString()))
                for (auto* route = routes->getFirstChildElement(); route != nullptr; route = route->getNextElement())
                    savedCodeRoute = savedCodeRoute || Modulation::isCodeSource (route->getIntAttribute ("source"));
        check (! savedCodeRoute, "CODE-modulationsrutter sparas aldrig");

        auto copy = copyOf (*processor);
        check (copy->getLiveCodeText() == processor->getLiveCodeText(), "spara och ladda: koden följer med");
        check (copy->liveCodeLocks.load() == ((1 << 2) | (1 << 10)), "spara och ladda: låsen följer med (bit utanför 11 bitar tas bort)");
        check (copy->liveCodeOwned.load() == 0, "inget ägs efter laddning (LIVE startar aldrig av sig själv)");
        check (copy->liveCodeCodeLocks.load() == 0 && copy->liveCodeCodeNoteLocks.load() == 0 && copy->liveCodeLocks.load() == ((1 << 2) | (1 << 10)), "kodlås sparas aldrig; handlåsen följer med");
        auto restoredCodeRoute = false;
        for (const auto& route : copy->modRoutes.routes)
            restoredCodeRoute = restoredCodeRoute || Modulation::isCodeSource (route.source.load());
        check (copy->codeMods.empty() && restoredCodeRoute == false, "laddning återställer inga CODE-modulationer");

        // Ett gammalt tillstånd utan elementet lämnar användarens kod och lås orörda.
        copy->setLiveCodeText ("$: s(\"hh*8\")");
        copy->setStateInformation (plain.getData(), (int) plain.getSize());
        check (copy->getLiveCodeText() == "$: s(\"hh*8\")" && copy->liveCodeLocks.load() == ((1 << 2) | (1 << 10)), "tillstånd utan <LiveCode> lämnar koden och låsen som de var");

        // Full-preset har med koden; instrument-preset inte.
        check (processor->saveUserPreset (Presets::Section::full, "__persist_lc__"), "Full-preset med Live Code sparas");
        check (processor->saveUserPreset (Presets::Section::bass, "__persist_lc_i__", 0), "instrument-preset sparas");
        auto other = std::make_unique<InstrumentProcessor>();
        other->loadUserPreset (Presets::Section::bass, processor->findUserPreset (Presets::Section::bass, "__persist_lc_i__"), 0);
        check (other->getLiveCodeText().isEmpty() && other->liveCodeLocks.load() == 0, "instrument-preset rör inte Live Code");
        other->loadUserPreset (Presets::Section::full, processor->findUserPreset (Presets::Section::full, "__persist_lc__"));
        check (other->getLiveCodeText() == processor->getLiveCodeText() && other->liveCodeLocks.load() == ((1 << 2) | (1 << 10)), "Full-preset ger koden och låsen");

        // Init rör inte användarens kod och kraschar inte.
        other->loadFactoryPreset (Presets::Section::full, 0);
        check (other->getLiveCodeText() == processor->getLiveCodeText(), "Init lämnar Live Code-koden orörd");
    }

    // 4d. Filtermodellerna (fas D2): modell och P1/P2 följer med projektet; standard är Kickbase med P1 0 och P2 1 (gamla projekt utan fälten).
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto value = [] (PluginBase& p, InstrumentField field) { return p.apvts.getRawParameterValue (ParamID::bass (2, field))->load(); };
        check ((int) value (*processor, InstrumentField::filterModel) == 0 && value (*processor, InstrumentField::filterP1) == 0.0f && value (*processor, InstrumentField::filterP2) == 1.0f,
               "filtermodell Kickbase, P1 0 och P2 1 som standard");
        Scenarios::setInstrument (*processor, 2, InstrumentField::filterModel, 3.0f);
        Scenarios::setInstrument (*processor, 2, InstrumentField::filterP1, 0.6f);
        Scenarios::setInstrument (*processor, 2, InstrumentField::filterP2, 0.25f);
        auto copy = copyOf (*processor);
        check ((int) value (*copy, InstrumentField::filterModel) == 3 && std::abs (value (*copy, InstrumentField::filterP1) - 0.6f) < 1.0e-4f && std::abs (value (*copy, InstrumentField::filterP2) - 0.25f) < 1.0e-4f,
               "spara och ladda: Formant, P1 0,6 och P2 0,25 följer med");
        // TB-303 (index 1) ligger kvar på sin plats efter att listan blev längre.
        Scenarios::setInstrument (*processor, 2, InstrumentField::filterModel, 1.0f);
        auto tb = copyOf (*processor);
        check ((int) value (*tb, InstrumentField::filterModel) == 1, "TB-303 (index 1) överlever spara och ladda");
    }

    // 4e. Filter 2 (fas D3): av som standard (gamla projekt utan fälten får samma ljud som förut); alla inställningar följer med projektet.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto value = [] (PluginBase& p, InstrumentField field) { return p.apvts.getRawParameterValue (ParamID::bass (3, field))->load(); };
        check (value (*processor, InstrumentField::filter2On) < 0.5f && (int) value (*processor, InstrumentField::filterRouting) == 0 && value (*processor, InstrumentField::filterMix) == 1.0f,
               "Filter 2 är av som standard (Serial, MIX 100 %)");
        using F = InstrumentField;
        Scenarios::setInstrument (*processor, 3, F::filter2On, 1.0f);
        Scenarios::setInstrument (*processor, 3, F::filter2Model, 4.0f);
        Scenarios::setInstrument (*processor, 3, F::filter2Type, 1.0f);
        Scenarios::setInstrument (*processor, 3, F::filter2Cutoff, 777.0f);
        Scenarios::setInstrument (*processor, 3, F::filter2Reso, 0.35f);
        Scenarios::setInstrument (*processor, 3, F::filter2P1, 0.6f);
        Scenarios::setInstrument (*processor, 3, F::filter2P2, 0.2f);
        Scenarios::setInstrument (*processor, 3, F::filterRouting, 1.0f);
        Scenarios::setInstrument (*processor, 3, F::filterMix, 0.45f);
        auto copy = copyOf (*processor);
        check (value (*copy, F::filter2On) > 0.5f && (int) value (*copy, F::filter2Model) == 4 && (int) value (*copy, F::filter2Type) == 1
                   && std::abs (value (*copy, F::filter2Cutoff) - 777.0f) < 0.5f && std::abs (value (*copy, F::filter2Reso) - 0.35f) < 1.0e-4f
                   && std::abs (value (*copy, F::filter2P1) - 0.6f) < 1.0e-4f && std::abs (value (*copy, F::filter2P2) - 0.2f) < 1.0e-4f
                   && (int) value (*copy, F::filterRouting) == 1 && std::abs (value (*copy, F::filterMix) - 0.45f) < 1.0e-4f,
               "spara och ladda: Filter 2 (på, Comb, LP 12, 777 Hz, Parallel, MIX 45 %) följer med");
    }

    // fas S: FILTER av/på och per källa: standard (på, SAMPLE av) och spara/ladda.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        using F = InstrumentField;
        auto value = [] (PluginBase& p, F field) { return p.apvts.getRawParameterValue (ParamID::bass (3, field))->load(); };
        check (value (*processor, F::filterOn) > 0.5f && value (*processor, F::filterOsc1) > 0.5f && value (*processor, F::filterOsc2) > 0.5f
                   && value (*processor, F::filterOsc3) > 0.5f && value (*processor, F::filterSample) < 0.5f && value (*processor, F::filterNoise) > 0.5f,
               "FILTER på för alla källor utom SAMPLE som standard");
        Scenarios::setInstrument (*processor, 3, F::filterOn, 0.0f);
        Scenarios::setInstrument (*processor, 3, F::filterOsc2, 0.0f);
        Scenarios::setInstrument (*processor, 3, F::filterSample, 1.0f);
        auto copy = copyOf (*processor);
        check (value (*copy, F::filterOn) < 0.5f && value (*copy, F::filterOsc1) > 0.5f && value (*copy, F::filterOsc2) < 0.5f
                   && value (*copy, F::filterSample) > 0.5f && value (*copy, F::filterNoise) > 0.5f,
               "spara och ladda: FILTER av/på och per källa följer med");
        Scenarios::setInstrument (*processor, 3, F::noiseType, 3.0f);
        check (value (*copyOf (*processor), F::noiseType) > 2.5f && value (*copyOf (*processor), F::noiseType) < 3.5f, "spara och ladda: NOISE TYPE följer med");
        Scenarios::setInstrument (*processor, 3, F::warpEnvSource, 1.0f);
        Scenarios::setInstrument (*processor, 3, F::warpEnvDecay, 12.0f);
        auto warpCopy = copyOf (*processor);
        check (value (*warpCopy, F::warpEnvSource) > 0.5f && value (*warpCopy, F::warpEnvSource) < 1.5f && std::abs (value (*warpCopy, F::warpEnvDecay) - 12.0f) < 0.1f,
               "spara och ladda: WARP ENV-källa och Direct-decay följer med");
        Scenarios::setInstrument (*processor, 3, F::polarity, 1.0f);
        Scenarios::setInstrument (*processor, 3, F::phaseDeg, 123.4f);
        Scenarios::setInstrument (*processor, 3, F::nudgeMs, -0.75f);
        auto phaseCopy = copyOf (*processor);
        check (value (*phaseCopy, F::polarity) > 0.5f && std::abs (value (*phaseCopy, F::phaseDeg) - 123.4f) < 0.11f && std::abs (value (*phaseCopy, F::nudgeMs) + 0.75f) < 0.03f,
               "spara och ladda: POLARITY, START PHASE och NUDGE följer med");
        check (value (*copyOf (*std::make_unique<InstrumentProcessor>()), F::polarity) < 0.5f, "PH: nya parametrar har standardvärden som ger dagens ljud");
        {
            auto fresh = std::make_unique<InstrumentProcessor>();
            check (value (*copyOf (*fresh), F::fmOn) < 0.5f && value (*copyOf (*fresh), F::fmLevel) < 1.0e-6f && value (*copyOf (*fresh), F::fmFilter) > 0.5f,
                   "S2: FM är av som standard (nivå 0, genom filtren)");
        }
        Scenarios::setInstrument (*processor, 3, F::fmOn, 1.0f);
        Scenarios::setInstrument (*processor, 3, F::fmLevel, 0.6f);
        Scenarios::setInstrument (*processor, 3, F::fmRatio, 3.5f);
        Scenarios::setInstrument (*processor, 3, F::fmIndex, 0.75f);
        Scenarios::setInstrument (*processor, 3, F::fmEnvAmount, 0.4f);
        Scenarios::setInstrument (*processor, 3, F::fmEnvTime, 120.0f);
        Scenarios::setInstrument (*processor, 3, F::fmSemis, -12.0f);
        Scenarios::setInstrument (*processor, 3, F::fmPreset, 2.0f);
        Scenarios::setInstrument (*processor, 3, F::fmFilter, 0.0f);
        auto fmCopy = copyOf (*processor);
        check (value (*fmCopy, F::fmOn) > 0.5f && std::abs (value (*fmCopy, F::fmLevel) - 0.6f) < 0.01f && std::abs (value (*fmCopy, F::fmRatio) - 3.5f) < 0.01f
                   && std::abs (value (*fmCopy, F::fmIndex) - 0.75f) < 0.01f && std::abs (value (*fmCopy, F::fmEnvAmount) - 0.4f) < 0.01f
                   && std::abs (value (*fmCopy, F::fmEnvTime) - 120.0f) < 0.5f && std::abs (value (*fmCopy, F::fmSemis) + 12.0f) < 0.01f
                   && value (*fmCopy, F::fmPreset) > 1.5f && value (*fmCopy, F::fmPreset) < 2.5f && value (*fmCopy, F::fmFilter) < 0.5f,
               "spara och ladda: alla FM-parametrar följer med");
    }

    // 5. Preset-laddningen sätter varje parameter en gång: en ratt som ändras passerar inte sitt standardvärde på vägen.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto* cutoff = processor->apvts.getParameter (ParamID::bass (0, InstrumentField::cutoff));
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (2345.0f));
        processor->saveUserPreset (Presets::Section::bass, "__persist_order__", 0);
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (1234.0f));
        Recorder recorder;
        cutoff->addListener (&recorder);
        processor->loadUserPreset (Presets::Section::bass, processor->findUserPreset (Presets::Section::bass, "__persist_order__"), 0);
        cutoff->removeListener (&recorder);
        const auto target = cutoff->convertTo0to1 (2345.0f);
        check (recorder.values.size() == 1 && std::abs (recorder.values[0] - target) < 1.0e-4f,
               "cutoff sattes en gång, direkt till presetets värde (" + juce::String ((int) recorder.values.size()) + " ändringar)");
        check (std::abs (cutoff->convertFrom0to1 (cutoff->getValue()) - 2345.0f) < 1.0f, "och har presetets värde efteråt");
    }

    // 6. Ett kick-preset rör inte kickens på/av (radens M-knapp), precis som instrument-presets.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto* kickOn = processor->apvts.getParameter (ParamID::kickOn);
        kickOn->setValueNotifyingHost (0.0f);
        processor->saveUserPreset (Presets::Section::kick, "__persist_kick_off__");
        kickOn->setValueNotifyingHost (1.0f);
        processor->loadUserPreset (Presets::Section::kick, processor->findUserPreset (Presets::Section::kick, "__persist_kick_off__"));
        check (kickOn->getValue() > 0.5f, "ett kick-preset sparat med kicken avstängd stänger inte av kicken");
        kickOn->setValueNotifyingHost (0.0f);
        processor->loadFactoryPreset (Presets::Section::kick, 2);
        check (kickOn->getValue() < 0.5f, "ett fabriks-kickljud slår inte på en avstängd kick");
    }

    // 7. Fabriksljud sätter också varje parameter en gång (ingen omväg via standardvärdet).
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->loadFactoryPreset (Presets::Section::bass, 3, 0);
        auto* cutoff = processor->apvts.getParameter (ParamID::bass (0, InstrumentField::cutoff));
        const auto factoryValue = cutoff->getValue();
        cutoff->setValueNotifyingHost (factoryValue > 0.5f ? 0.1f : 0.9f);
        Recorder recorder;
        cutoff->addListener (&recorder);
        processor->loadFactoryPreset (Presets::Section::bass, 3, 0);
        cutoff->removeListener (&recorder);
        check (recorder.values.size() == 1 && std::abs (recorder.values[0] - factoryValue) < 1.0e-4f,
               "ett fabriks-instrumentljud sätter cutoff en gång (" + juce::String ((int) recorder.values.size()) + " ändringar)");
    }

    // 8. Sidechain-fabriksljuden sätter djupet (DUCK) på alla instrument (fråga 4), och "Follow Kick" ger standarddjupet.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto duck = [&processor] (int instrument) { return processor->apvts.getRawParameterValue (ParamID::bass (instrument, InstrumentField::duck))->load(); };
        processor->loadFactoryPreset (Presets::Section::sidechain, 1); // Classic Pump: 0,9
        auto all = true;
        for (int instrument = 0; instrument < ParamID::numInstrumentLayers; ++instrument)
            all = all && std::abs (duck (instrument) - 0.9f) < 1.0e-4f;
        check (all, "Classic Pump sätter DUCK 0,9 på alla tio instrument");
        processor->loadFactoryPreset (Presets::Section::sidechain, 0); // Follow Kick
        check (std::abs (duck (0) - processor->apvts.getParameter (ParamID::bass (0, InstrumentField::duck))->convertFrom0to1 (
                   processor->apvts.getParameter (ParamID::bass (0, InstrumentField::duck))->getDefaultValue())) < 1.0e-4f,
               "Follow Kick ger standarddjupet");
    }

    // 9. Radens längd överlever sparande: låst rad, AUTO-rad med + BAR, och ett gammalt (version 6) projekt med radlängd i parametern rowBars.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto& p = processor->pattern;
        p.addNote (Pattern::kickLayer, 0, 96, 0);
        p.setManualBars (Pattern::kickLayer, 3); // 1 takt toner låst till 3 takter
        p.addNote (0, 0, 96, 0);
        p.setExtraBars (0, 4); // AUTO med + BAR
        auto copy = copyOf (*processor);
        auto& q = copy->pattern;
        check (q.manualBars (Pattern::kickLayer) == 3 && q.lengthBars (Pattern::kickLayer) == 3 && ! q.isLengthAuto (Pattern::kickLayer), "låst rad (3 takter) sparas och laddas");
        check (q.isLengthAuto (0) && q.extraBars (0) == 4 && q.lengthBars (0) == 4, "AUTO-rad med + BAR (4 takter) sparas och laddas");

        // Gammalt projekt: version 6, ingen length/extra, rowBarsKick = 2 takter, toner i 4 takter.
        auto old = std::make_unique<InstrumentProcessor>();
        for (int bar = 0; bar < 4; ++bar)
            old->pattern.addNote (Pattern::kickLayer, bar * Pattern::ticksPerBar, 96, 0);
        old->apvts.getParameter (ParamID::rowBars (0))->setValueNotifyingHost (old->apvts.getParameter (ParamID::rowBars (0))->convertTo0to1 (2.0f)); // val 2 = "2 bars"
        juce::MemoryBlock state;
        old->getStateInformation (state);
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        check (xml != nullptr, "tillståndet går att läsa som XML");
        if (xml != nullptr)
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            auto patternTree = tree.getChildWithName (Pattern::treeType);
            patternTree.setProperty ("version", 6, nullptr);
            for (auto child : patternTree)
            {
                child.removeProperty ("length", nullptr);
                child.removeProperty ("extra", nullptr);
            }
            juce::MemoryBlock legacy;
            if (auto text = tree.createXml())
                juce::AudioProcessor::copyXmlToBinary (*text, legacy);
            auto loaded = std::make_unique<InstrumentProcessor>();
            loaded->setStateInformation (legacy.getData(), (int) legacy.getSize());
            check (loaded->pattern.lengthBars (Pattern::kickLayer) == 2,
                   "gammalt projekt laddas (kickraden är " + juce::String (loaded->pattern.lengthBars (Pattern::kickLayer)) + " takter)");
            // Gamla regeln: totalen är minst den längsta raden, och en rad på 2 takter trimmas till 2 takter.
            auto lateNotes = 0;
            loaded->pattern.forEachNote (Pattern::kickLayer, [&] (int tick, const Pattern::Note&) { lateNotes += tick >= 2 * Pattern::ticksPerBar ? 1 : 0; });
            check (lateNotes == 0, "gamla radlängden (2 takter) tog bort tonerna efter slutet (" + juce::String (lateNotes) + " kvar)");
        }
    }

    // 10. Aktiva LFO:er (fas M): lägga till och ta bort, spara och ladda (projekt och preset), gamla projekt och fabrikspresets ger LFO 1-3.
    {
        using Modulation::paramID;
        auto set = [] (PluginBase& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        auto get = [] (PluginBase& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); };
        auto isDefault = [] (PluginBase& p, int lfo)
        {
            for (const auto* suffix : { "On", "Shape", "Rate", "Mode", "Hz", "Snap", "SnapRange" })
                if (auto* q = p.apvts.getParameter (paramID (lfo, suffix)); std::abs (q->getValue() - q->getDefaultValue()) > 1.0e-6f)
                    return false;
            return true;
        };
        const auto fresh = std::make_unique<InstrumentProcessor>(); // (på heapen: processorn är för stor för stacken)
        auto sameCurve = [&fresh] (PluginBase& p, int lfo) { return p.lfoCurves[(size_t) lfo].toValueTree ("c").isEquivalentTo (fresh->lfoCurves[(size_t) lfo].toValueTree ("c")); };
        auto target = [] (PluginBase& p, int nth) // index för den n:te modulerbara parametern
        {
            const auto& parameters = p.getParameters();
            for (int i = 0; i < parameters.size(); ++i)
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]); r != nullptr && Modulation::isModulatable (*r) && nth-- == 0)
                    return i;
            return -1;
        };
        const std::vector<NodeEnvelope::Point> ramp { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };

        auto processor = std::make_unique<InstrumentProcessor>();
        check (processor->modulator.activeMask.load() == 7u && processor->availableLfos() == 10, "LFO 1-3 aktiva från början, 10 möjliga");
        check (processor->setLfoActive (4, true) == 0 && processor->setLfoActive (8, true) == 0 && processor->isLfoActive (4) && processor->isLfoActive (8), "lägg till LFO 5 och 9");
        set (*processor, paramID (4, "Rate"), 3.0f);
        set (*processor, paramID (8, "Shape"), 4.0f);
        processor->lfoCurves[4].setPoints (ramp);
        processor->modRoutes.add (Modulation::lfoSource (4), target (*processor, 0), 0.5f);
        processor->modRoutes.add (Modulation::lfoSource (8), target (*processor, 1), -0.25f);
        processor->modRoutes.add (Modulation::lfoSource (8), target (*processor, 2), 0.75f);
        {
            auto copy = copyOf (*processor);
            check (copy->modulator.activeMask.load() == (7u | 1u << 4 | 1u << 8), "spara och ladda: LFO 5 och 9 är aktiva");
            check ((int) get (*copy, paramID (4, "Rate")) == 3 && (int) get (*copy, paramID (8, "Shape")) == 4 && copy->lfoRouteCount (4) == 1 && copy->lfoRouteCount (8) == 2,
                   "LFO 5 och 9: parametrar och kopplingar följer med");
            check (! sameCurve (*copy, 4) && sameCurve (*copy, 8), "LFO 5:s egna kurva följer med");
        }

        check (processor->setLfoActive (4, false) == 1, "ta bort LFO 5 tar bort dess koppling");
        check (! processor->isLfoActive (4) && processor->lfoRouteCount (4) == 0 && isDefault (*processor, 4) && sameCurve (*processor, 4), "borttagen LFO 5: standardvärden och sinus");
        check (processor->lfoRouteCount (8) == 2 && (int) get (*processor, paramID (8, "Shape")) == 4, "LFO 9 rörs inte");
        check (processor->setLfoActive (1, false) == 0 && ! processor->isLfoActive (1), "ta bort LFO 2 (utan kopplingar)");
        check (processor->setLfoActive (4, false) == 0 && processor->setLfoActive (10, true) == 0 && ! processor->isLfoActive (10), "ogiltiga anrop gör inget");
        {
            auto copy = copyOf (*processor);
            check (copy->modulator.activeMask.load() == (1u | 4u | 1u << 8) && isDefault (*copy, 4), "spara och ladda efter borttagning: LFO 1, 3 och 9");
        }

        // Preset (Full och LFO) bär masken.
        check (processor->saveUserPreset (Presets::Section::lfo, "__persist_lfos__"), "LFO-preset sparas");
        auto other = std::make_unique<InstrumentProcessor>();
        other->setLfoActive (6, true);
        set (*other, paramID (6, "Rate"), 2.0f);
        other->loadUserPreset (Presets::Section::lfo, processor->findUserPreset (Presets::Section::lfo, "__persist_lfos__"));
        check (other->modulator.activeMask.load() == (1u | 4u | 1u << 8) && isDefault (*other, 6) && other->lfoRouteCount (8) == 2, "LFO-presetet ger LFO 1, 3 och 9 och återställer LFO 7");
        other->loadFactoryPreset (Presets::Section::full, 1);
        check ((other->modulator.activeMask.load() & 7u) == 7u && other->lfoRouteCount (8) == 0 && ! other->isLfoActive (8), "fabrikspreset: LFO 1-3 aktiva igen");

        // Gammalt projekt: ingen activeLfos och inga LFO 4-10-parametrar.
        juce::MemoryBlock state;
        processor->modRoutes.clear();
        processor->getStateInformation (state);
        if (auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            tree.removeProperty ("activeLfos", nullptr);
            for (int i = tree.getNumChildren(); --i >= 0;)
                for (int lfo = Modulation::numLfos; lfo < Modulation::maxLfos; ++lfo)
                    if (tree.getChild (i).getProperty ("id").toString().startsWith (paramID (lfo, "")))
                        tree.removeChild (i, nullptr);
            juce::MemoryBlock legacy;
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), legacy);
            auto loaded = std::make_unique<InstrumentProcessor>();
            loaded->setLfoActive (5, true);
            set (*loaded, paramID (5, "Hz"), 7.0f);
            loaded->setStateInformation (legacy.getData(), (int) legacy.getSize());
            check (loaded->modulator.activeMask.load() == 7u && isDefault (*loaded, 5), "gammalt projekt utan mask: LFO 1-3, LFO 6 återställd");
        }

        auto fx = std::make_unique<FxProcessor>();
        check (fx->availableLfos() == 3 && fx->setLfoActive (3, true) == 0 && fx->modulator.activeMask.load() == 7u, "FX: bara LFO 1-3");
    }

    // Fas L: AMP ENV / FILT ENV är källa 54-73. Ett äldre projekt (källor 0-53) läses oförändrat, och de nya sparas och laddas.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        const auto parameters = processor->getParameters();
        juce::ValueTree old (ModRoutes::treeType);
        for (int source : { 0, 6, 7, 46, 47, 53 })
        {
            juce::ValueTree child ("Route");
            child.setProperty ("source", source, nullptr);
            child.setProperty ("target", ParamID::bass (0, InstrumentField::cutoff), nullptr);
            child.setProperty ("amount", 0.25, nullptr);
            old.appendChild (child, nullptr);
        }
        processor->modRoutes.fromValueTree (old, parameters);
        auto oldOk = true;
        for (int r = 0, i = 0; r < Modulation::maxRoutes; ++r)
            if (const auto s = processor->modRoutes.routes[(size_t) r].source.load(); s >= 0)
                oldOk = oldOk && s == old.getChild (i++)["source"].operator int();
        check (oldOk && processor->modRoutes.toValueTree (parameters).isEquivalentTo (old), "ett äldre projekt med källor 0-53 läses och skrivs oförändrat");

        const auto amp = Modulation::instrumentSource (1, Modulation::instrumentSources), filt = Modulation::instrumentSource (9, Modulation::instrumentSources + 1);
        check (amp == 56 && filt == 73 && Modulation::sourceName (amp) == "INSTR 2 AMP ENV" && Modulation::sourceName (filt) == "INSTR 10 FILT ENV",
               "AMP ENV/FILT ENV har nummer 54 + instrument x 2 (+1) och namn med instrumentet");
        int cutoffIndex = 0;
        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == ParamID::bass (0, InstrumentField::cutoff))
                cutoffIndex = i;
        processor->modRoutes.clear();
        processor->modRoutes.add (amp, cutoffIndex, 0.5f);
        processor->modRoutes.add (filt, cutoffIndex, -0.3f);
        auto copy = copyOf (*processor);
        std::vector<int> sources;
        for (const auto& route : copy->modRoutes.routes)
            if (route.source.load() >= 0)
                sources.push_back (route.source.load());
        check (sources == std::vector<int> { amp, filt }, "kopplingar från AMP ENV/FILT ENV sparas och laddas");
    }

    // Fas P: ackord följer med projektet; en ton utan ackord skrivs som förut (fem fält) och en ton som läggs till på nytt ärver inget ackord.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->pattern.clearNotes (1);
        processor->pattern.addNote (1, 0, 48, 2).setChord ({ 0, 7, 3 });
        processor->pattern.addNote (1, 96, 24, 0);
        auto copy = copyOf (*processor);
        check (copy->pattern.note (1, 0).getChord() == Pattern::Chord { 7, 3, 0 } && ! copy->pattern.note (1, 96).hasChord(), "ackord sparas och laddas");
        const auto notes = processor->pattern.toValueTree().getChildWithProperty ("layer", 1)["notes"].toString();
        check (notes.contains ("0/48/2/0/0.800/7/3;") && notes.endsWith ("96/24/0/0/0.800"), "ackordet skrivs bara på tonen som har ett: " + notes);
        copy->pattern.addNote (1, 0, 48, 2);
        check (! copy->pattern.note (1, 0).hasChord(), "en ny ton ärver inte ett gammalt ackord");
    }

    // kbproj: projektfilen (XML) ger samma tillstånd tillbaka; fel filer avvisas utan att något ändras.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->pattern.clearNotes (1);
        processor->pattern.addNote (1, 0, 48, 2);
        processor->loadUserTable (1, 0, tableFile);
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("persist_test.kbproj");
        juce::String error;
        check (ProjectFile::save (*processor, file, "0.0.0", error), "kbproj: projektet sparas");
        const auto text = file.loadFileAsString();
        check (text.contains ("kbprojVersion=\"1\"") && text.contains ("pluginVersion=\"0.0.0\""), "kbproj: filen är XML-text med versionsattribut");

        auto loaded = std::make_unique<InstrumentProcessor>();
        check (ProjectFile::load (*loaded, loaded->apvts.state.getType(), file, error), "kbproj: projektet öppnas");
        juce::MemoryBlock before, after;
        processor->getStateInformation (before);
        loaded->getStateInformation (after);
        check (before == after, "kbproj: öppnat projekt ger samma tillstånd som det sparade (byte för byte)");
        check (loaded->pattern.note (1, 0).length == 48 && loaded->userTable (1, 0) != nullptr, "kbproj: toner och inbäddad tabell följer med");

        // Fel filer: inget ändras och felet förklaras.
        juce::MemoryBlock untouched;
        loaded->getStateInformation (untouched);
        auto rejects = [&] (const juce::String& content, const juce::String& what)
        {
            const auto bad = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("persist_bad.kbproj");
            bad.replaceWithText (content);
            juce::String why;
            juce::MemoryBlock now;
            const auto ok = ProjectFile::load (*loaded, loaded->apvts.state.getType(), bad, why);
            loaded->getStateInformation (now);
            check (! ok && why.isNotEmpty() && now == untouched, "kbproj avvisar " + what + ": " + why);
            bad.deleteFile();
        };
        rejects ("inte xml alls", "text som inte är XML");
        rejects ("<Annat kbprojVersion=\"1\"/>", "fel rotelement");
        rejects (text.replace ("kbprojVersion=\"1\"", "kbprojVersion=\"99\""), "ett nyare format");
        rejects (text.replace ("kbprojVersion=\"1\"", ""), "en fil utan version");
        check (! ProjectFile::load (*loaded, loaded->apvts.state.getType(), file.getSiblingFile ("finns_inte.kbproj"), error) && error.isNotEmpty(), "kbproj: saknad fil ger ett fel");
        file.deleteFile();
    }

    // Autospara: orörd instans ger ingen fil, en ändring ger en som går att öppna, oförändrat ger ingen ny, mappen rensas till keepFiles.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("persist_autosave");
        folder.deleteRecursively();
        ProjectFile::AutoSaver saver (*processor, folder);
        check (! saver.tick() && ! folder.exists(), "autospara: första tick sparar bara utgångsläget (ingen fil)");
        check (! saver.tick(), "autospara: oförändrat tillstånd ger ingen fil");
        processor->pattern.clearNotes (1);
        processor->pattern.addNote (1, 0, 48, 2);
        check (saver.tick() && saver.lastFile().existsAsFile(), "autospara: ändring ger en fil");
        check (! saver.tick(), "autospara: samma ändring skrivs inte två gånger");
        auto loaded = std::make_unique<InstrumentProcessor>();
        juce::String error;
        check (ProjectFile::load (*loaded, loaded->apvts.state.getType(), saver.lastFile(), error) && loaded->pattern.note (1, 0).length == 48,
               "autospara: filen öppnas som ett projekt med ändringen");
        for (int i = 0; i < 20; ++i)
        {
            processor->pattern.addNote (1, 4 + i, 12, 2);
            saver.tick();
        }
        check (folder.findChildFiles (juce::File::findFiles, false, "autosave-*.kbproj").size() <= ProjectFile::AutoSaver::keepFiles,
               "autospara: mappen rensas till keepFiles");
        folder.deleteRecursively();
    }

    std::puts (failed ? "FEL: sparandetestet misslyckades" : "Sparandetestet gick igenom");
    return failed ? 1 : 0;
}

// `tool fulltest` (E0 punkt 2): "allt på"-round-trip. Fyller varje sorts data, och kontrollerar att projektet (get/setStateInformation) och
// Full-presetet (buildPresetXml/loadUserPreset) ger tillbaka exakt samma XML. Bara test: ingen ändring i PluginBase.
int fullStateTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // KÄNDA LUCKOR (docs/work/E0-report.md, fynd 1 och 2): instrumentnamn, färger och previewNote sparas i projektet men inte i Full-presetet.
    // De kontrollerna är AVSTÄNGDA (räknas inte som fel) tills Tobias beslutat om presetformatet; sätt till true för att slå på dem.
    constexpr bool strictKnownPresetGaps = false;
    auto gap = [&] (bool ok, const juce::String& what)
    {
        if (strictKnownPresetGaps)
            check (ok, what);
        else
            std::printf ("%s: [avstängd, känd lucka] %s\n", ok ? "ok (kan slås på)" : "LUCKA", what.toRawUTF8());
    };

    auto stateXml = [] (PluginBase& p) -> std::unique_ptr<juce::XmlElement>
    {
        juce::MemoryBlock state;
        p.getStateInformation (state);
        return juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    };
    // Namnger de barn (i ordning) som skiljer sig, så att ett fel pekar på rätt del.
    auto describeDiff = [] (const juce::XmlElement& a, const juce::XmlElement& b)
    {
        juce::StringArray differing;
        const auto count = std::max (a.getNumChildElements(), b.getNumChildElements());
        for (int i = 0; i < count; ++i)
        {
            const auto* x = a.getChildElement (i);
            const auto* y = b.getChildElement (i);
            if (x == nullptr || y == nullptr || ! x->isEquivalentTo (y, false))
                differing.add ((x != nullptr ? x->getTagName() + ":" + x->getStringAttribute ("id") : juce::String ("saknas")) + " #" + juce::String (i));
        }
        return differing.joinIntoString (", ");
    };

    const auto tableFile = Scenarios::writeTestWav ("full_table", 21.533, 0.2);
    const auto kickFile = Scenarios::writeTestWav ("full_kick", 55.0, 0.3);
    const auto bassFile = Scenarios::writeTestWav ("full_bass", 110.0, 0.3);
    const std::vector<NodeEnvelope::Point> ramp { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };

    auto original = std::make_unique<InstrumentProcessor>(); // (på heapen: processorn är för stor för stacken)
    {
        auto& p = *original;
        using F = InstrumentField;

        // Parametrar på flera instrument.
        for (int layer = 0; layer < ParamID::numInstrumentLayers; layer += 3)
        {
            Scenarios::setInstrument (p, layer, F::cutoff, 700.0f + 100.0f * (float) layer);
            Scenarios::setInstrument (p, layer, F::filterModel, 3.0f);
            Scenarios::setInstrument (p, layer, F::filter2On, 1.0f);
        }

        // Mönster: toner, ackord och en låst raddlängd på flera rader.
        p.pattern.clearNotes (0);
        p.pattern.addNote (0, 0, 48, 2).setChord ({ 0, 7, 3 });
        p.pattern.addNote (0, 96, 24, 5);
        p.pattern.addNote (4, 48, 24, -3);
        p.pattern.addNote (Pattern::kickLayer, 0, 96, 0);
        p.pattern.setManualBars (Pattern::kickLayer, 2);
        p.pattern.setExtraBars (4, 2);

        // Samplingar: kick och instrument 2, samt egna wavetables på flera lager och oscillatorer.
        check (p.loadClickSample (kickFile), "kick-sampling laddas");
        check (p.loadInstrumentSample (1, bassFile), "instrument-sampling laddas på instrument 2");
        check (p.loadUserTable (0, 0, tableFile) && p.loadUserTable (3, 2, tableFile) && p.loadUserTable (9, 1, tableFile), "egna wavetables laddas (instrument 1, 4 och 10)");

        // Kickens envelopes och sidechainkurvan.
        p.kickPitchEnv.setPoints ({ { 0.0f, 50.0f, -0.4f }, { 0.08f, 0.0f, 0.0f } });
        p.kickAmpEnv.setPoints ({ { 0.0f, 0.0f, 0.0f }, { 0.001f, 1.0f, 0.0f }, { 0.2f, 0.0f, 0.0f } });
        p.sidechainCurve.setPoints ({ { 0.0f, 1.0f, 0.0f }, { 0.3f, 0.4f, -0.5f }, { 1.0f, 0.0f, 0.0f } });

        // LFO 4-10 på, med egna kurvor, parametrar och kopplingar.
        for (int lfo = 3; lfo < Modulation::maxLfos; ++lfo)
        {
            p.setLfoActive (lfo, true);
            p.lfoCurves[(size_t) lfo].setPoints (ramp);
        }
        auto* rate = p.apvts.getParameter (Modulation::paramID (8, "Rate"));
        rate->setValueNotifyingHost (rate->convertTo0to1 (3.0f));
        auto target = [&p] (int nth)
        {
            const auto& parameters = p.getParameters();
            for (int i = 0; i < parameters.size(); ++i)
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]); r != nullptr && Modulation::isModulatable (*r) && nth-- == 0)
                    return i;
            return -1;
        };
        p.modRoutes.add (Modulation::lfoSource (0), target (0), 0.5f);
        p.modRoutes.add (Modulation::lfoSource (4), target (1), -0.25f);
        p.modRoutes.add (Modulation::lfoSource (8), target (2), 0.75f);

        // Live Code: text och lås.
        p.setLiveCodeText ("$: note(\"c2 eb2\").s(\"i1\")\n// åäö");
        p.liveCodeLocks = (1 << 2) | (1 << 10);

        // Instrumentens namn och färger, och förhandsspelningens ton.
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        {
            p.setLayerName (layer, "NAME" + juce::String (layer + 1));
            p.setLayerColour (layer, 0xff102030u + (juce::uint32) layer * 0x010101u);
        }
        p.previewNote = 67;
    }

    // 1. Projektet: det sparade tillståndet är identiskt efter spara och ladda.
    {
        auto before = stateXml (*original);
        check (before != nullptr, "tillståndet går att läsa som XML");
        auto copy = copyOf (*original);
        auto after = stateXml (*copy);
        check (before != nullptr && after != nullptr, "kopian har ett tillstånd");
        if (before != nullptr && after != nullptr)
            check (before->isEquivalentTo (after.get(), false),
                   "projekt: sparat -> laddat -> sparat ger samma XML" + (before->isEquivalentTo (after.get(), false) ? juce::String() : " (skiljer: " + describeDiff (*before, *after) + ")"));

        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            check (copy->getLayerName (layer) == original->getLayerName (layer) && copy->getLayerColour (layer) == original->getLayerColour (layer),
                   "projekt: namn och färg på instrument " + juce::String (layer + 1));
        check (copy->previewNote.load() == 67, "projekt: previewNote");
        check (copy->userTable (0, 0) != nullptr && copy->userTable (3, 2) != nullptr && copy->userTable (9, 1) != nullptr && copy->userTable (1, 0) == nullptr, "projekt: egna wavetables på rätt plats");
        check (copy->getClickSampleName() == original->getClickSampleName() && copy->getInstrumentSampleName (1) == original->getInstrumentSampleName (1)
                   && ! copy->getClickSampleName().isEmpty() && ! copy->getInstrumentSampleName (1).isEmpty(),
               "projekt: kick- och instrumentsampling");
        check (copy->getLiveCodeText() == original->getLiveCodeText() && copy->liveCodeLocks.load() == ((1 << 2) | (1 << 10)), "projekt: Live Code och lås");
        check (copy->modulator.activeMask.load() == original->modulator.activeMask.load() && copy->lfoRouteCount (8) == 1 && copy->lfoRouteCount (4) == 1, "projekt: aktiva LFO:er och kopplingar");
        check (copy->pattern.note (0, 0).getChord() == original->pattern.note (0, 0).getChord() && copy->pattern.note (0, 0).getChord() != Pattern::Chord {}
                   && copy->pattern.lengthBars (Pattern::kickLayer) == 2,
               "projekt: mönster med ackord och radlängd");
    }

    // 2. Full-presetet: samma innehåll tillbaka i en ny instans (jämförs som preset-XML, namnet borträknat).
    {
        check (original->saveUserPreset (Presets::Section::full, "__full_all__"), "Full-preset sparas");
        const auto file = original->findUserPreset (Presets::Section::full, "__full_all__");
        auto other = std::make_unique<InstrumentProcessor>();
        check (other->loadUserPreset (Presets::Section::full, file), "Full-preset laddas");

        auto sent = original->buildPresetXml (Presets::Section::full, 0);
        auto got = other->buildPresetXml (Presets::Section::full, 0);
        check (sent != nullptr && got != nullptr, "preset-XML byggs för båda");
        if (sent != nullptr && got != nullptr)
        {
            sent->removeAttribute ("name");
            got->removeAttribute ("name");
            const auto same = sent->isEquivalentTo (got.get(), false);
            check (same, "Full-preset: sparat -> laddat -> byggt ger samma XML" + (same ? juce::String() : " (skiljer: " + describeDiff (*sent, *got) + ")"));
            check (sent->getIntAttribute ("version", -1) == got->getIntAttribute ("version", -1), "Full-preset: versionsattributet är detsamma (OBS: version=\"1\" har aldrig höjts, E0 fynd 5)");
        }

        check (other->userTable (0, 0) != nullptr && other->userTable (3, 2) != nullptr && other->userTable (9, 1) != nullptr, "Full-preset: egna wavetables på rätt plats");
        check (! other->getClickSampleName().isEmpty() && ! other->getInstrumentSampleName (1).isEmpty(), "Full-preset: kick- och instrumentsampling");
        check (other->getLiveCodeText() == original->getLiveCodeText() && other->liveCodeLocks.load() == ((1 << 2) | (1 << 10)), "Full-preset: Live Code och lås");
        check (other->modulator.activeMask.load() == original->modulator.activeMask.load() && other->lfoRouteCount (8) == 1, "Full-preset: aktiva LFO:er och kopplingar");
        check (other->pattern.note (0, 0).getChord() == original->pattern.note (0, 0).getChord() && other->pattern.note (0, 0).getChord() != Pattern::Chord {}
                   && other->pattern.lengthBars (Pattern::kickLayer) == 2,
               "Full-preset: mönster med ackord och radlängd");

        // AVSTÄNGDA: sparas i projektet men inte i Full-presetet (E0 fynd 1 och 2). Beslut väntar på Tobias.
        auto namesAndColours = true;
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            namesAndColours = namesAndColours && other->getLayerName (layer) == original->getLayerName (layer) && other->getLayerColour (layer) == original->getLayerColour (layer);
        gap (namesAndColours, "Full-preset: instrumentnamn och färger (E0 fynd 1)");
        gap (other->previewNote.load() == 67, "Full-preset: previewNote (E0 fynd 2)");
    }

    std::puts (failed ? "FEL: allt-på-testet misslyckades" : "Allt-på-testet gick igenom (kända luckor är avstängda, se raderna med LUCKA)");
    return failed ? 1 : 0;
}

// `tool undotest` (fas C): ångra/gör om. (a) en parametergest är ett steg, (b) radredigering, (c) huvudpreset tömmer historiken, (d) delpreset är ett steg.
int undoTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    { // (a) tre setValue i en gest = ett steg
        auto holder = std::make_unique<InstrumentProcessor>();
        auto& p = *holder; // (stor: på heapen, som i de andra testerna)
        juce::RangedAudioParameter* prm = nullptr;
        for (auto* q : p.getParameters())
            if (prm == nullptr)
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
                    if (r->getNumSteps() > 1000) // en vanlig flyttalsknapp, inte ett heltal som processorn styr
                        prm = r;
        p.clearHistory ({});
        const auto start = prm->getValue();
        const auto target = start < 0.5f ? 0.9f : 0.1f;
        p.undoManager.beginNewTransaction();
        for (auto v : { 0.3f, 0.6f, target })
            prm->setValueNotifyingHost (v);
        p.nameParameterGesture ("Test");
        check (p.undoManager.getUndoDescriptions().size() == 1, "a: en gest är ett steg");
        p.undoManager.undo();
        check (std::abs (prm->getValue() - start) < 1.0e-3f, "a: ångra ger startvärdet");
        p.undoManager.redo();
        check (std::abs (prm->getValue() - target) < 1.0e-3f, "a: gör om ger slutvärdet");
    }

    { // (b) radredigering med tre toner
        auto holder = std::make_unique<InstrumentProcessor>();
        auto& p = *holder; // (stor: på heapen, som i de andra testerna)
        p.clearHistory ({});
        const auto before = p.snapshotRow (0);
        p.pattern.addNote (0, 0, 24, 0);
        p.pattern.addNote (0, 96, 24, 3);
        p.pattern.addNote (0, 192, 24, 5);
        const auto after = p.pattern.rowToValueTree (0);
        p.recordRowEdit (0, before, "Edit notes");
        check (p.undoManager.canUndo(), "b: kan ångra");
        p.undoManager.undo();
        check (p.pattern.rowToValueTree (0).isEquivalentTo (before), "b: ångra ger raden före");
        p.undoManager.redo();
        check (p.pattern.rowToValueTree (0).isEquivalentTo (after), "b: gör om ger raden efter");
        const auto steps = p.undoManager.getUndoDescriptions().size();
        p.recordRowEdit (0, p.snapshotRow (0), "Edit notes");
        check (p.undoManager.getUndoDescriptions().size() == steps, "b: oförändrad gest lägger inget");
    }

    { // (c) huvudpreset tömmer historiken
        auto holder = std::make_unique<InstrumentProcessor>();
        auto& p = *holder; // (stor: på heapen, som i de andra testerna)
        const auto before = p.snapshotRow (0);
        p.pattern.addNote (0, 48, 24, 2);
        p.recordRowEdit (0, before, "Edit notes");
        check (p.undoManager.canUndo(), "c: steg finns före laddning");
        p.loadFactoryPreset (Presets::Section::full, 0);
        check (! p.undoManager.canUndo(), "c: ingen historik efter huvudpreset");
    }

    { // (d) delpreset = ett steg som återställer både parametrar och mönster
        auto holder = std::make_unique<InstrumentProcessor>();
        auto& p = *holder; // (stor: på heapen, som i de andra testerna)
        p.loadFactoryPreset (Presets::Section::full, 0);
        if (Presets::factoryCount (Presets::Section::bass, false) < 2)
            check (false, "d: bas-fabrikspreset saknas");
        else
        {
            p.clearHistory ({});
            std::vector<float> values;
            for (auto* prm : p.getParameters())
                values.push_back (prm->getValue());
            const auto patternBefore = p.pattern.toValueTree();
            p.loadFactoryPreset (Presets::Section::bass, 1, 0);
            check (p.undoManager.getUndoDescriptions().size() == 1, "d: ett steg");
            p.undoManager.undo();
            auto same = true;
            for (size_t i = 0; i < values.size(); ++i)
                same = same && std::abs (p.getParameters()[(int) i]->getValue() - values[i]) < 1.0e-3f;
            check (same, "d: parametrarna är tillbaka");
            check (p.pattern.toValueTree().isEquivalentTo (patternBefore), "d: mönstret är tillbaka");
        }
    }

    {
        // Mätning: storlek och tid för en snapshot (före/efter) med en tät pianorulle på 64 takter.
        auto processor = std::make_unique<InstrumentProcessor>();
        auto& p = *processor;
        for (int bar = 0; bar < 64; ++bar)
            for (int step = 0; step < 16; ++step)
                p.pattern.addNote (0, bar * Pattern::ticksPerBar + step * 24, 12, step % 12);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto snap = p.snapshotAux();
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        const auto bytes = snap.createXml()->toString (juce::XmlElement::TextFormat().singleLine()).length();
        std::printf ("mått: 1024 toner -> snapshot %d byte, %.2f ms\n", (int) bytes, t1 - t0);
    }

    std::puts (failed ? "FEL: ångra-testet misslyckades" : "Ångra-testet gick igenom");
    return failed ? 1 : 0;
}

// `tool rendertooltest`: render_preview via verktygslagret är deterministiskt. Samma tillstånd ger samma mått och en bytelik WAV, i samma instans,
// efter en ändring + undo och i en ny instans; och rendering ändrar inte tillståndet. Det som skillens loop (render, analysera, justera) bygger på.
int renderToolTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    struct Rendered { bool ok = false; juce::String metrics; juce::File wav; };
    auto render = [] (ToolLayer& tools, const juce::String& name, const juce::String& part)
    {
        const auto args = juce::JSON::parse ("{\"bars\":2,\"spectrogram\":false,\"name\":\"" + name + "\",\"part\":\"" + part + "\"}");
        const auto result = tools.call ("render_preview", args);
        Rendered r;
        r.ok = (bool) result["ok"];
        const auto data = result["data"];
        r.metrics = juce::JSON::toString (data["metrics"], true) + juce::JSON::toString (data["checks"], true);
        r.wav = juce::File (data["wav_file"].toString());
        return r;
    };
    auto same = [] (const Rendered& a, const Rendered& b)
    {
        return a.ok && b.ok && a.metrics == b.metrics && a.wav.existsAsFile() && b.wav.existsAsFile() && a.wav.getSize() > 1000 && a.wav.hasIdenticalContentTo (b.wav);
    };
    auto stateText = [] (ToolLayer& tools)
    {
        return juce::JSON::toString (tools.call ("get_state", juce::JSON::parse ("{\"all_parameters\":true}")), true);
    };

    auto processor = std::make_unique<InstrumentProcessor>();
    ToolLayer tools (*processor);

    const auto before = stateText (tools);
    for (const char* part : { "", "kick", "bass" })
    {
        const juce::String label = juce::String (part).isEmpty() ? juce::String ("full") : juce::String (part);
        const auto a = render (tools, "det-a-" + label, part);
        const auto b = render (tools, "det-b-" + label, part);
        check (a.ok && a.wav.getSize() > 1000, "render_preview " + label + ": ger en WAV");
        check (same (a, b), "render_preview " + label + ": två anrop i rad ger samma mått och samma WAV");
    }
    check (stateText (tools) == before, "rendering ändrar inte tillståndet (get_state före och efter)");

    // En ny instans med samma (Init-)tillstånd.
    {
        auto other = std::make_unique<InstrumentProcessor>();
        ToolLayer otherTools (*other);
        check (same (render (tools, "det-a-inst", ""), render (otherTools, "det-b-inst", "")), "ny instans med samma tillstånd ger samma rendering");
    }

    // Provet ser en ändring, och undo ger tillbaka exakt det första.
    {
        const auto base = render (tools, "det-base", "");
        const auto set = tools.call ("set_parameters", juce::JSON::parse ("{\"values\":{\"bass1Cutoff\":250,\"kickDrive\":0.4}}"));
        check ((bool) set["ok"], "set_parameters tas emot");
        const auto changed = render (tools, "det-changed", "");
        check (! same (base, changed), "en ändrad parameter syns i renderingen (testet kan se skillnad)");
        check ((bool) tools.call ("undo", juce::JSON::parse ("{}"))["ok"], "undo lyckas");
        check (same (base, render (tools, "det-undone", "")), "efter undo ger rendering samma mått och WAV som före ändringen");

        // compare_renders: ändringen syns som delta, två lika renderingar ger noll.
        const auto diff = tools.call ("compare_renders", juce::JSON::parse ("{\"before\":\"det-base\",\"after\":\"det-changed\"}"));
        check ((bool) diff["ok"] && diff["data"]["metric_deltas"].getDynamicObject() != nullptr, "compare_renders ger deltan för två renderingar");
        auto anyDelta = false;
        if (auto* deltas = diff["data"]["metric_deltas"].getDynamicObject())
            for (const auto& property : deltas->getProperties())
                anyDelta = anyDelta || std::abs ((double) property.value["delta"]) > 1.0e-9;
        check (anyDelta, "compare_renders: en ändrad parameter ger minst ett delta över noll");
        const auto none = tools.call ("compare_renders", juce::JSON::parse ("{\"before\":\"det-base\",\"after\":\"det-undone\"}"));
        auto allZero = (bool) none["ok"] && none["data"]["checks_changed"].size() == 0;
        if (auto* deltas = none["data"]["metric_deltas"].getDynamicObject())
            for (const auto& property : deltas->getProperties())
                allZero = allZero && std::abs ((double) property.value["delta"]) < 1.0e-9;
        check (allZero, "compare_renders: före ändring och efter undo ger noll i alla delta");
        check (! (bool) tools.call ("compare_renders", juce::JSON::parse ("{\"before\":\"finns-inte\",\"after\":\"det-base\"}"))["ok"], "compare_renders: okänt namn ger ett fel");
    }

    // sweep_parameter: en rad per värde, olika värden ger olika mått, och ljudet är orört efteråt.
    {
        const auto stateBefore = stateText (tools);
        const auto sweep = tools.call ("sweep_parameter", juce::JSON::parse ("{\"parameter\":\"kickDrive\",\"values\":[0.1,0.6],\"part\":\"kick\",\"bars\":1}"));
        const auto rows = sweep["data"]["rows"];
        check ((bool) sweep["ok"] && rows.size() == 2 && rows[0]["metrics"].getDynamicObject() != nullptr, "sweep_parameter ger en rad med mått per värde");
        check (juce::JSON::toString (rows[0]["metrics"]) != juce::JSON::toString (rows[1]["metrics"]), "sweep_parameter: olika värden ger olika mått");
        check (stateText (tools) == stateBefore, "sweep_parameter lämnar ljudet som det var");
        const auto repeat = tools.call ("sweep_parameter", juce::JSON::parse ("{\"parameter\":\"kickDrive\",\"values\":[0.1,0.6],\"part\":\"kick\",\"bars\":1}"));
        check (juce::JSON::toString (repeat["data"]["rows"]) == juce::JSON::toString (rows), "sweep_parameter är upprepbar");
        check (! (bool) tools.call ("sweep_parameter", juce::JSON::parse ("{\"parameter\":\"finnsInte\",\"values\":[1]}"))["ok"], "sweep_parameter: okänd parameter ger ett fel");
        check ((bool) tools.call ("compare_renders", juce::JSON::parse ("{\"before\":\"sweep_kickDrive_1\",\"after\":\"sweep_kickDrive_2\"}"))["ok"], "sweep-renderingarna kan jämföras med compare_renders");
    }

    // score_vs_reference (seconds 60 = hela filen, så rendering och referens mäts över samma stycke): en rendering mot sin egen WAV är nära noll, mot en ändrad rendering längre bort.
    {
        const auto reference = render (tools, "ref-a", "kick");
        const auto self = tools.call ("score_vs_reference", juce::JSON::parse ("{\"reference_file\":" + juce::JSON::toString (reference.wav.getFullPathName()) + ",\"kind\":\"kick\",\"seconds\":60,\"name\":\"ref-a\"}"));
        check ((bool) self["ok"] && self["data"]["terms"].size() >= 7, "score_vs_reference ger termer för en rendering och en referens");
        tools.call ("set_parameters", juce::JSON::parse ("{\"values\":{\"kickDrive\":0.9,\"kickPitchAmount\":2.0}}"));
        render (tools, "ref-b", "kick");
        const auto other = tools.call ("score_vs_reference", juce::JSON::parse ("{\"reference_file\":" + juce::JSON::toString (reference.wav.getFullPathName()) + ",\"kind\":\"kick\",\"seconds\":60,\"name\":\"ref-b\"}"));
        check (other["data"]["terms"].size() == self["data"]["terms"].size(), "score_vs_reference: lika många termer, så avstånden går att jämföra");
        check ((bool) other["ok"] && (double) other["data"]["distance"] > (double) self["data"]["distance"], "score_vs_reference: en ändrad kick ligger längre från referensen än kicken själv");
        check (! (bool) tools.call ("score_vs_reference", juce::JSON::parse ("{\"reference_file\":\"finns-inte.wav\",\"name\":\"ref-a\"}"))["ok"], "score_vs_reference: saknad fil ger ett fel");
        tools.call ("undo", juce::JSON::parse ("{}"));
    }

    std::puts (failed ? "FEL: renderingstestet misslyckades" : "Renderingstestet gick igenom");
    return failed ? 1 : 0;
}
