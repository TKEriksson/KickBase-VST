// `tool persisttest`: sparande och laddning av det som inte är parametrar (egna wavetables, samplingar) och preset-laddningens ordning,
// samt att data som ljudtråden kan läsa inte frigörs för tidigt. Körs i en tillfällig preset-mapp (Scenarios::TemporaryUserFolders).

#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"

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

    std::puts (failed ? "FEL: sparandetestet misslyckades" : "Sparandetestet gick igenom");
    return failed ? 1 : 0;
}
