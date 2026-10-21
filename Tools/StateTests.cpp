// `tool persisttest`: sparande och laddning av det som inte är parametrar (egna wavetables, samplingar) och preset-laddningens ordning,
// samt att data som ljudtråden kan läsa inte frigörs för tidigt. Körs i en tillfällig preset-mapp (Scenarios::TemporaryUserFolders).

#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Core/ToolLayer.h"
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
        check ((bool) modResult["ok"] && ! processor->codeMods.empty(), "testläge: CODE-modulering är aktiv före sparning");
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

    std::puts (failed ? "FEL: sparandetestet misslyckades" : "Sparandetestet gick igenom");
    return failed ? 1 : 0;
}
