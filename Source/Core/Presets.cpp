#include "Presets.h"
#include "Settings.h"
#include "Params.h"
#include <map>

namespace
{
    using Section = Presets::Section;

    struct StepTweak
    {
        int step;          // steg i takten (upprepas i alla takter), i mallens rutnät
        int pitch;         // halvtoner
        bool slide;
        float gate = -1.0f; // gate-längd 0..1, eller -1 = mallens
    };

    // Ett parametervärde i parameterns egna enheter (Hz, dB, ... eller valets nummer). Konstruktorn tar
    // float, så att heltal som 1 blir float vid anropet.
    struct Value
    {
        Value (const char* idIn, float valueIn) : id (idIn), value (valueIn) {}
        Value (std::string idIn, float valueIn) : id (std::move (idIn)), value (valueIn) {}
        std::string id;
        float value;
    };

    struct Preset
    {
        const char* name;
        std::vector<Value> values;
        PatternTemplate pattern = PatternTemplate::rolling; // bara Full och Pattern
        std::vector<StepTweak> tweaks;                      // i mallens rutnät
        std::vector<SidechainCurve::Point> curve;           // bara Sidechain; tom = standardkurvan
        Presets::Extras extras;                             // bara Full och Kick: kickens envelopes; Full: LFO-kopplingar
    };

    using namespace ParamID;

    // Fabrikspresets: själva ljuden ligger i PresetData.inl (full, kick, bass och effects).
    const std::vector<Preset>& fullPresets();
    const std::vector<Preset>& kickPresets();
    const std::vector<Preset>& bassPresets();

    // FX-versionens "Bass" är gaten.
    const std::vector<Preset>& gatePresets()
    {
        static const std::vector<Preset> presets {
            { "Default", {} },
            { "Tight",  { { gateAttack, 0.5f }, { gateRelease, 5.0f } } },
            { "Plucky", { { gateAttack, 0.3f }, { gateRelease, 15.0f } } },
            { "Soft",   { { gateAttack, 5.0f }, { gateRelease, 40.0f } } },
            { "Smooth", { { gateAttack, 12.0f }, { gateRelease, 80.0f } } },
        };
        return presets;
    }

    const std::vector<Preset>& patternPresets()
    {
        static const std::vector<Preset> presets {
            { "Rolling", {}, PatternTemplate::rolling },
            { "Galloping", {}, PatternTemplate::galloping },
            { "Triplet", {}, PatternTemplate::triplet },
            { "Offbeat", {}, PatternTemplate::offbeat },
            { "Rolling + Octaves", {}, PatternTemplate::rolling, { { 3, 12, false }, { 7, 12, false }, { 11, 12, false }, { 15, 12, false } } },
            { "Galloping Slide", {}, PatternTemplate::galloping, { { 6, 0, true }, { 7, 7, false }, { 14, 0, true }, { 15, 12, false } } },
            { "Clear", {}, PatternTemplate::clear },
        };
        return presets;
    }

    const std::vector<Preset>& effectChainPresets();

    const std::vector<Preset>& sidechainPresets()
    {
        static const std::vector<Preset> presets {
            { "Follow Kick", { { "scMode", 0 } } },
            { "Classic Pump", { { "scMode", 1 }, { "scLength", 4 }, { "duckAmount", 0.9f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.05f, 1.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } } },
            { "Tight 1/16", { { "scMode", 1 }, { "scLength", 1 }, { "duckAmount", 0.85f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.4f, 0.9f, -0.4f }, { 1.0f, 0.0f, 0.0f } } },
            { "Soft 1/8", { { "scMode", 1 }, { "scLength", 2 }, { "duckAmount", 0.7f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.3f }, { 1.0f, 0.0f, 0.0f } } },
            { "Hold Then Release", { { "scMode", 1 }, { "scLength", 3 }, { "duckAmount", 1.0f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.45f, 1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } } },
        };
        return presets;
    }

    const std::vector<Preset>& masterPresets()
    {
        static const std::vector<Preset> presets {
            { "Clean", {} },
            { "Gentle Glue", { { "masterDrive", 2.0f }, { "masterClip", 0.15f }, { "masterRelease", 150.0f } } },
            { "Loud",        { { "masterDrive", 6.0f }, { "masterClip", 0.3f } } },
            { "Smash",       { { "masterDrive", 12.0f }, { "masterClip", 0.6f }, { "masterRelease", 40.0f } } },
            { "Bypass",      { { "masterOn", 0 } } },
        };
        return presets;
    }

    const std::vector<Preset>& soundstagePresets()
    {
        static const std::vector<Preset> presets {
            { "Neutral", {} },
            { "Wide", { { "masterWidth", 1.3f }, { "masterEnhance", 0.2f } } },
            { "Narrow", { { "masterWidth", 0.7f } } },
            { "Tight Low End", { { "masterMonoBelow", 160.0f } } },
        };
        return presets;
    }

    const std::vector<Preset>& lfoPresets()
    {
        static const std::vector<Preset> presets { { "Off", {} } };
        return presets;
    }

    const std::vector<Preset>& listFor (Section section, bool effectVersion)
    {
        switch (section)
        {
            case Section::full:    return fullPresets();
            case Section::kick:    return kickPresets();
            case Section::bass:    return effectVersion ? gatePresets() : bassPresets();
            case Section::pattern: return patternPresets();
            case Section::effects:   return effectChainPresets();
            case Section::sidechain: return sidechainPresets();
            case Section::master:    return masterPresets();
            case Section::lfo:       return lfoPresets();
            case Section::soundstage: return soundstagePresets();
            default:                 break;
        }
        return fullPresets();
    }
}

#include "PresetData.inl"

namespace Presets
{
    // Skriver en bas-rad (se Presets::Extras::PatternLine) till mönstret.
    void writePatternLine (Pattern& pattern, const Extras::PatternLine& line)
    {
        if (line.bars.empty() || ! juce::isPositiveAndBelow (line.layer, Pattern::numRowSlots) || (line.layer >= Pattern::numNoteLayers && ! pattern.rows.isActive (line.layer)))
            return;

        pattern.clearNotes (line.layer);
        auto melodic = false; // toner med tonhöjd eller glide visas som pianorulle (miniatyr) i stället för stegknappar
        const auto stepTicks = Pattern::ticksPerStep (line.grid);
        const auto stepsPerBar = Pattern::instrumentStepsPerBar (line.grid);

        for (int bar = 0; bar < (int) line.bars.size(); ++bar) // raden blir lika lång som sina takter (AUTO)
        {
            const auto steps = juce::StringArray::fromTokens (juce::String (line.bars[(size_t) bar]), " |", "");
            for (int step = 0; step < steps.size() && step < stepsPerBar; ++step)
            {
                const auto token = steps[step];
                if (token == "." || token.isEmpty())
                    continue;

                const auto isKickLine = line.layer == Pattern::kickLayer; // kicken: x och X är full volym
                const auto velocity = token[0] == 'X' ? 1.0f : token[0] == 'm' ? (isKickLine ? 0.7f : 0.62f) : token[0] == 'g' ? (isKickLine ? 0.45f : 0.42f)
                                                                                                                        : (isKickLine ? 1.0f : 0.8f);
                auto rest = token.substring (1);
                const auto slide = rest.contains ("~");
                const auto gatePercent = rest.contains (":") ? rest.fromFirstOccurrenceOf (":", false, false).getFloatValue() : line.gate * 100.0f;
                const auto pitch = rest.upToFirstOccurrenceOf ("~", false, false).upToFirstOccurrenceOf (":", false, false).getIntValue();

                pattern.addNote (line.layer, bar * Pattern::ticksPerBar + step * stepTicks,
                                 std::max (1, juce::roundToInt ((float) stepTicks * gatePercent / 100.0f)), pitch, velocity, slide);
                if (pitch != 0 || slide)
                    melodic = true;
            }
        }
        pattern.setCustom (line.layer, melodic);
    }

    const Extras* factoryExtras (Section section, bool effectVersion, int index)
    {
        const auto& list = listFor (section, effectVersion);
        return juce::isPositiveAndBelow (index, (int) list.size()) ? &list[(size_t) index].extras : nullptr;
    }

    juce::String sectionName (Section section)
    {
        switch (section)
        {
            case Section::full:    return "Full";
            case Section::kick:    return "Kick";
            case Section::bass:    return "Bass";
            case Section::pattern: return "Pattern";
            case Section::effects:   return "Effects";
            case Section::sidechain: return "Sidechain";
            case Section::master:    return "Master";
            case Section::lfo:       return "LFO";
            case Section::soundstage: return "Soundstage";
            default:                 break;
        }
        return {};
    }

    int instrumentLayerOf (const juce::String& id)
    {
        if (! id.startsWith ("bass") || id.length() < 5 || ! juce::CharacterFunctions::isDigit (id[4]))
            return -1;
        const auto digits = id.length() > 5 && juce::CharacterFunctions::isDigit (id[5]) ? 2 : 1; // bass1 ... bass10
        const auto layer = id.substring (4, 4 + digits).getIntValue() - 1;
        return juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers) ? layer : -1;
    }

    juce::String toInstrumentLayer (const juce::String& id, int layer)
    {
        if (instrumentLayerOf (id) < 0)
            return id;
        const auto digits = id.length() > 5 && juce::CharacterFunctions::isDigit (id[5]) ? 2 : 1;
        return "bass" + juce::String (layer + 1) + id.substring (4 + digits);
    }

    juce::String effectIdToCanonical (const juce::String& id, int)
    {
        return Effects::isEffectParameter (id) ? "fxA" + id.substring (3) : id;
    }

    juce::String effectIdFromCanonical (const juce::String& id, int channel)
    {
        return Effects::isEffectParameter (id) ? "fx" + Effects::channelName (channel) + id.substring (3) : id;
    }

    juce::String patternIdToCanonical (const juce::String& id, int layer)
    {
        if (layer == Pattern::kickLayer)
            return id;
        for (const char* prefix : { "rowBars", "rowLoop" })
            if (id == juce::String (prefix) + "Bass" + juce::String (layer + 1))
                return juce::String (prefix) + "Bass1";
        return toInstrumentLayer (id, 0);
    }

    juce::String patternIdFromCanonical (const juce::String& id, int layer)
    {
        if (layer == Pattern::kickLayer)
            return id;
        for (const char* prefix : { "rowBars", "rowLoop" })
            if (id == juce::String (prefix) + "Bass1")
                return juce::String (prefix) + "Bass" + juce::String (layer + 1);
        return toInstrumentLayer (id, layer);
    }

    bool belongsTo (Section section, const juce::String& id, int layer)
    {
        // Ett baslagers fält som hör till mönstret eller sidechainen snarare än till ljudet.
        auto isLayerField = [&id] (InstrumentField field)
        {
            const auto l = instrumentLayerOf (id);
            return l >= 0 && id == ParamID::bass (l, field);
        };
        // FX-matrisens send-belopp (kickens och lagrens) hör till routingen, inte till ljudet: de följer inte med ett kick- eller basljud.
        auto isMatrixSend = [&id, &isLayerField]
        {
            for (int ch = 0; ch < numEffectChannels; ++ch)
                if (id == ParamID::kickFxSendToIds[ch] || isLayerField (ParamID::fxSendToField (ch)))
                    return true;
            return false;
        };

        switch (section)
        {
            case Section::full:
                return true;
            case Section::kick:
                // (På/av är radens M-knapp i mönstret, som för instrumenten: ett kick-preset ska inte stänga av eller slå på kicken.)
                return id.startsWith ("kick") && id != ParamID::kickFxChannel && id != ParamID::kickFxAmount && id != ParamID::kickFxSend && ! isMatrixSend() && id != ParamID::kickOn;
            case Section::bass:
                return (instrumentLayerOf (id) == layer && ! isLayerField (InstrumentField::on) && ! isLayerField (InstrumentField::gate) && ! isLayerField (InstrumentField::delay) && ! isLayerField (InstrumentField::swing) && ! isLayerField (InstrumentField::humanize)
                        && ! isLayerField (InstrumentField::duck) && ! isLayerField (InstrumentField::grid)
                        && ! isLayerField (InstrumentField::fxChannel) && ! isLayerField (InstrumentField::fxAmount) && ! isLayerField (InstrumentField::fxSend) && ! isMatrixSend()
                        && ! isLayerField (InstrumentField::gateOn) && ! isLayerField (InstrumentField::gateGrid)
                        && ! isLayerField (InstrumentField::gateDepth) && ! isLayerField (InstrumentField::gateSmooth))
                    || id == ParamID::gateAttack || id == ParamID::gateRelease;
            case Section::pattern:
                // En rad: dess rutnät, noternas längdskala (GATE-ratten), GATE, fördröjning, längd och loop. Inte tonart, swing eller antal instrument.
                if (layer == Pattern::kickLayer)
                    return id == "rowBarsKick" || id == "rowLoopKick" || id == ParamID::rowGridKick;
                return (instrumentLayerOf (id) == layer
                        && (isLayerField (InstrumentField::grid) || isLayerField (InstrumentField::gate) || isLayerField (InstrumentField::gateOn) || isLayerField (InstrumentField::gateGrid)
                            || isLayerField (InstrumentField::gateDepth) || isLayerField (InstrumentField::gateSmooth) || isLayerField (InstrumentField::delay)
                            || isLayerField (InstrumentField::swing) || isLayerField (InstrumentField::humanize)))
                    || id == ParamID::rowBars (layer + 1) || id == ParamID::rowLoop (layer + 1);
            case Section::effects:
                // En kanal: dess fem platser (typ och rattar), EQ, kompressor, nivå och duck. Inte vem som skickar dit (det hör till instrumentet).
                return Effects::isEffectParameter (id) && (int) (id[2] - 'A') == layer;
            case Section::sidechain:
                return id.startsWith ("sc") || isLayerField (InstrumentField::duck);
            case Section::master:
                // MAXIMIZER: på/av, drive, clip, ceiling och release. Mixerns reglage och soundstage hör inte hit (de skulle annars nollas av ett maximizer-preset).
                return id == "masterOn" || id == "masterDrive" || id == "masterClip" || id == "masterCeiling" || id == "masterRelease";
            case Section::soundstage:
                return id == "masterWidth" || id == "masterMonoBelow" || id == "masterEnhance";
            case Section::lfo:
                return id.startsWith ("lfo");
            default:
                break;
        }
        return false;
    }

    juce::File folder (Section section, bool effectVersion)
    {
        auto name = sectionName (section);
        if (section == Section::full)
            name = effectVersion ? "Full FX" : "Full Instrument";
        else if (section == Section::bass)
            name = effectVersion ? "Gate" : "Bass"; // (mappnamnet behålls så att befintliga presets hittas)

        return Settings::presetsFolder().getChildFile (name);
    }

    int factoryCount (Section section, bool effectVersion)
    {
        return (int) listFor (section, effectVersion).size();
    }

    juce::String factoryName (Section section, bool effectVersion, int index)
    {
        const auto& list = listFor (section, effectVersion);
        return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
    }

    void setParameter (juce::RangedAudioParameter& parameter, float normalisedValue)
    {
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (normalisedValue);
        parameter.endChangeGesture();
    }

    void applyFactory (Section section, bool effectVersion, int index,
                       juce::AudioProcessorValueTreeState& apvts, Pattern& pattern, SidechainCurve& curve, int layer)
    {
        const auto& list = listFor (section, effectVersion);
        if (! juce::isPositiveAndBelow (index, (int) list.size()))
            return;

        const auto& preset = list[(size_t) index];

        // Delens parametrar får presetets värden, övriga i delen sitt standardvärde (så att presets inte ärver värden från varandra).
        // Mönsterlängden behålls av Full: den hör till låten snarare än ljudet. Slutvärdet räknas ut först och varje parameter sätts en gång,
        // som i PluginBase::loadPresetXml: ljudtråden spelar under tiden och ska inte få ett block med standardvärden mitt i laddningen.
        std::vector<juce::RangedAudioParameter*> sectionParameters;
        std::vector<float> targets;
        std::map<juce::String, size_t> indexOfId;
        if (section != Section::pattern)
        {
            for (auto* p : apvts.processor.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
                if (ranged == nullptr || ! belongsTo (section, ranged->getParameterID(), layer) || ranged->getParameterID() == ParamID::patternBars)
                    continue;
                if (ranged->getParameterID().startsWith ("rowLoop") || ranged->getParameterID() == ParamID::rowGridKick || ranged->getParameterID().startsWith ("rowBars"))
                    continue;
                indexOfId[ranged->getParameterID()] = sectionParameters.size();
                sectionParameters.push_back (ranged);
                targets.push_back (ranged->getDefaultValue());
            }
        }

        // Basens presets skrivs för lager 1 och flyttas till det lager de laddas i. Ett värde utanför delen sätts direkt, som förut.
        std::vector<std::pair<juce::RangedAudioParameter*, float>> outside;
        for (const auto& [id, value] : preset.values)
        {
            // "duckAmount" (sidechain-fabriksljuden) fanns när djupet var gemensamt: nu sätts det på varje instruments DUCK.
            if (juce::String (id) == "duckAmount")
            {
                for (int instrument = 0; instrument < ParamID::numInstrumentLayers; ++instrument)
                    if (const auto found = indexOfId.find (ParamID::bass (instrument, InstrumentField::duck)); found != indexOfId.end())
                        targets[found->second] = sectionParameters[found->second]->convertTo0to1 (value);
                continue;
            }
            const auto target = section == Section::bass ? toInstrumentLayer (id, layer) : juce::String (id);
            if (const auto found = indexOfId.find (target); found != indexOfId.end())
                targets[found->second] = sectionParameters[found->second]->convertTo0to1 (value);
            else if (auto* parameter = apvts.getParameter (target))
                outside.push_back ({ parameter, parameter->convertTo0to1 (value) });
        }
        for (size_t i = 0; i < sectionParameters.size(); ++i)
            setParameter (*sectionParameters[i], targets[i]);
        for (const auto& [parameter, value] : outside)
            setParameter (*parameter, value);

        if (section == Section::full || section == Section::sidechain)
        {
            if (preset.curve.empty())
                curve.reset();
            else
                curve.setPoints (preset.curve);
        }

        if (section != Section::full && section != Section::pattern)
            return;

        // Ett mönster-preset (inte Full) rör bara kicken och instrument 1, inte de andra instrumentens toner (utom Clear, som tömmer allt).
        const auto grid = pattern.applyTemplate (preset.pattern, section == Section::pattern && preset.pattern != PatternTemplate::clear);

        pattern.refreshLengths();
        for (const auto& tweak : preset.tweaks)
        {
            const auto stepTicks = Pattern::ticksPerStep (grid);
            for (int bar = 0; bar < pattern.lengthBars (0); ++bar)
            {
                const auto tick = bar * Pattern::ticksPerBar + tweak.step * stepTicks;
                const auto gate = tweak.gate >= 0.0f ? tweak.gate : Pattern::defaultGate;
                auto& note = pattern.edit (0, tick);
                if (! note.on.load())
                    pattern.addNote (0, tick, juce::roundToInt (gate * (float) stepTicks));
                note.pitch = tweak.pitch;
                note.slide = tweak.slide;
                if (tweak.gate >= 0.0f)
                    note.length = juce::roundToInt (tweak.gate * (float) stepTicks);
            }
        }

        if (auto* gridParam = apvts.getParameter (ParamID::bass (0, InstrumentField::grid)))
            setParameter (*gridParam, gridParam->convertTo0to1 ((float) grid));

        // Egna rader per lager (rutnätet sätts med).
        for (const auto& line : preset.extras.pattern)
        {
            writePatternLine (pattern, line);
            if (auto* lineGrid = apvts.getParameter (ParamID::bass (line.layer, InstrumentField::grid)))
                setParameter (*lineGrid, lineGrid->convertTo0to1 ((float) line.grid));
        }
    }
}
