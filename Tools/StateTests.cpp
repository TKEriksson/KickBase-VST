// `tool persisttest`: sparande och laddning av det som inte är parametrar (egna wavetables, samplingar) och preset-laddningens ordning,
// samt att data som ljudtråden kan läsa inte frigörs för tidigt. Körs i en tillfällig preset-mapp (Scenarios::TemporaryUserFolders).

#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Ui/Help.h"

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

    std::puts (failed ? "FEL: sparandetestet misslyckades" : "Sparandetestet gick igenom");
    return failed ? 1 : 0;
}
