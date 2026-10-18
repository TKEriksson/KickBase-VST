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
                if ((Presets::bassLayerOf (id) > 0) || (Effects::isEffectParameter (id) && id[2] != 'A') || id.startsWith ("mixBass") && ! id.startsWith ("mixBass1")
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
        processor->loadBassSample (0, sample);
        const auto* first = processor->getActiveBassSample (0);
        processor->loadBassSample (0, sample);
        processor->clearBassSample (0);
        processor->loadBassSample (0, sample);
        // `first` ska fortfarande gå att läsa (förr frigjordes den vid det andra bytet).
        auto readable = first != nullptr && first->audio.getNumSamples() > 0 && std::isfinite (first->audio.getSample (0, 10));
        check (readable, "en sampling som bytts ut tre gånger i rad finns kvar en stund (ljudtråden kan läsa den)");

        for (int layer = 0; layer < 8; ++layer)
            processor->loadUserTable (layer, layer % 3, tableFile);
        processor->setInstrumentCount (10);
        processor->setInstrumentCount (1); // tar bort instrument 2-10 med sina tabeller på en gång
        check (processor->retiredTableCount() >= 7, "tabellerna från borttagna instrument frigörs inte direkt (" + juce::String ((int) processor->retiredTableCount()) + " väntar)");
    }

    // 5. Preset-laddningen sätter varje parameter en gång: en ratt som ändras passerar inte sitt standardvärde på vägen.
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        auto* cutoff = processor->apvts.getParameter (ParamID::bass (0, BassField::cutoff));
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
        auto* cutoff = processor->apvts.getParameter (ParamID::bass (0, BassField::cutoff));
        const auto factoryValue = cutoff->getValue();
        cutoff->setValueNotifyingHost (factoryValue > 0.5f ? 0.1f : 0.9f);
        Recorder recorder;
        cutoff->addListener (&recorder);
        processor->loadFactoryPreset (Presets::Section::bass, 3, 0);
        cutoff->removeListener (&recorder);
        check (recorder.values.size() == 1 && std::abs (recorder.values[0] - factoryValue) < 1.0e-4f,
               "ett fabriks-instrumentljud sätter cutoff en gång (" + juce::String ((int) recorder.values.size()) + " ändringar)");
    }

    std::puts (failed ? "FEL: sparandetestet misslyckades" : "Sparandetestet gick igenom");
    return failed ? 1 : 0;
}
