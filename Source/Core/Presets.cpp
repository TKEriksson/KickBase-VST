#include "Presets.h"
#include "Params.h"

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
        if (line.bars.empty() || ! juce::isPositiveAndBelow (line.layer, Pattern::numNoteLayers))
            return;

        pattern.clearNotes (line.layer);
        auto melodic = false; // toner med tonhöjd eller glide visas som pianorulle (miniatyr) i stället för stegknappar
        const auto stepTicks = Pattern::ticksPerStep (line.grid);
        const auto stepsPerBar = Pattern::bassStepsPerBar (line.grid);

        for (int bar = 0; bar < Pattern::maxBars; ++bar)
        {
            const auto steps = juce::StringArray::fromTokens (juce::String (line.bars[(size_t) bar % line.bars.size()]), " |", "");
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
            default:                 break;
        }
        return {};
    }

    int bassLayerOf (const juce::String& id)
    {
        if (! id.startsWith ("bass") || id.length() < 5 || ! juce::CharacterFunctions::isDigit (id[4]))
            return -1;
        const auto digits = id.length() > 5 && juce::CharacterFunctions::isDigit (id[5]) ? 2 : 1; // bass1 ... bass10
        const auto layer = id.substring (4, 4 + digits).getIntValue() - 1;
        return juce::isPositiveAndBelow (layer, ParamID::numBassLayers) ? layer : -1;
    }

    juce::String toBassLayer (const juce::String& id, int layer)
    {
        if (bassLayerOf (id) < 0)
            return id;
        const auto digits = id.length() > 5 && juce::CharacterFunctions::isDigit (id[5]) ? 2 : 1;
        return "bass" + juce::String (layer + 1) + id.substring (4 + digits);
    }

    bool belongsTo (Section section, const juce::String& id, int layer)
    {
        // Ett baslagers fält som hör till mönstret eller sidechainen snarare än till ljudet.
        auto isLayerField = [&id] (BassField field)
        {
            const auto l = bassLayerOf (id);
            return l >= 0 && id == ParamID::bass (l, field);
        };

        switch (section)
        {
            case Section::full:
                return true;
            case Section::kick:
                return id.startsWith ("kick") && id != ParamID::kickFxChannel && id != ParamID::kickFxAmount;
            case Section::bass:
                return (bassLayerOf (id) == layer && ! isLayerField (BassField::on) && ! isLayerField (BassField::gate)
                        && ! isLayerField (BassField::duck) && ! isLayerField (BassField::grid)
                        && ! isLayerField (BassField::fxChannel) && ! isLayerField (BassField::fxAmount)
                        && ! isLayerField (BassField::gateOn) && ! isLayerField (BassField::gateGrid)
                        && ! isLayerField (BassField::gateDepth) && ! isLayerField (BassField::gateSmooth))
                    || id == ParamID::gateAttack || id == ParamID::gateRelease;
            case Section::pattern:
                return id == ParamID::patternBars || id == ParamID::noteEnd || id.startsWith ("rowLoop") || id == ParamID::rowGridKick || id.startsWith ("rowBars") || isLayerField (BassField::grid) || isLayerField (BassField::gate)
                    || isLayerField (BassField::gateOn) || isLayerField (BassField::gateGrid)
                    || isLayerField (BassField::gateDepth) || isLayerField (BassField::gateSmooth);
            case Section::effects:
                return Effects::isEffectParameter (id) || id == ParamID::kickFxChannel || id == ParamID::kickFxAmount || isLayerField (BassField::fxChannel) || isLayerField (BassField::fxAmount);
            case Section::sidechain:
                return id.startsWith ("sc") || isLayerField (BassField::duck);
            case Section::master:
                return id.startsWith ("master") || id.startsWith ("mix");
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
            name = effectVersion ? "Gate" : "Bass";

        auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("Kickbase").getChildFile ("Presets");
        return root.getChildFile (name);
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

        // Delens parametrar till standard först, så att presets inte ärver värden från varandra.
        // Mönsterlängden behålls av Full: den hör till låten snarare än ljudet.
        if (section != Section::pattern)
        {
            for (auto* p : apvts.processor.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
                if (ranged == nullptr || ! belongsTo (section, ranged->getParameterID(), layer) || ranged->getParameterID() == ParamID::patternBars)
                    continue;
                if (ranged->getParameterID().startsWith ("rowLoop") || ranged->getParameterID() == ParamID::rowGridKick || ranged->getParameterID().startsWith ("rowBars"))
                    continue;
                setParameter (*ranged, ranged->getDefaultValue());
            }
        }

        // Basens presets skrivs för lager 1 och flyttas till det lager de laddas i.
        for (const auto& [id, value] : preset.values)
            if (auto* parameter = apvts.getParameter (section == Section::bass ? toBassLayer (id, layer) : juce::String (id)))
                setParameter (*parameter, parameter->convertTo0to1 (value));

        if (section == Section::full || section == Section::sidechain)
        {
            if (preset.curve.empty())
                curve.reset();
            else
                curve.setPoints (preset.curve);
        }

        if (section != Section::full && section != Section::pattern)
            return;

        const auto grid = pattern.applyTemplate (preset.pattern);

        for (const auto& tweak : preset.tweaks)
        {
            const auto stepTicks = Pattern::ticksPerStep (grid);
            for (int bar = 0; bar < Pattern::maxBars; ++bar)
            {
                const auto tick = bar * Pattern::ticksPerBar + tweak.step * stepTicks;
                const auto gate = tweak.gate >= 0.0f ? tweak.gate : Pattern::defaultGate;
                auto& note = pattern.note (0, tick);
                if (! note.on.load())
                    pattern.addNote (0, tick, juce::roundToInt (gate * (float) stepTicks));
                note.pitch = tweak.pitch;
                note.slide = tweak.slide;
                if (tweak.gate >= 0.0f)
                    note.length = juce::roundToInt (tweak.gate * (float) stepTicks);
            }
        }

        if (auto* gridParam = apvts.getParameter (ParamID::bass (0, BassField::grid)))
            setParameter (*gridParam, gridParam->convertTo0to1 ((float) grid));

        // Egna rader per lager (rutnätet sätts med).
        for (const auto& line : preset.extras.pattern)
        {
            writePatternLine (pattern, line);
            if (auto* lineGrid = apvts.getParameter (ParamID::bass (line.layer, BassField::grid)))
                setParameter (*lineGrid, lineGrid->convertTo0to1 ((float) line.grid));
        }
    }
}
