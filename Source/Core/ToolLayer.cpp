#include "ToolLayer.h"
#include "AudioAnalysis.h"
#include "RollFiles.h"
#include <BinaryData.h>

using juce::var;

namespace
{
    var object() { return var (new juce::DynamicObject()); }
    void put (var& target, const juce::String& key, const var& value) { target.getDynamicObject()->setProperty (key, value); }

    var ok (const var& data = var())
    {
        auto result = object();
        put (result, "ok", true);
        if (! data.isVoid())
            put (result, "data", data);
        return result;
    }

    var fail (const juce::String& message)
    {
        auto result = object();
        put (result, "ok", false);
        put (result, "error", message);
        return result;
    }

    double round4 (double v) { return std::round (v * 10000.0) / 10000.0; }

    juce::RangedAudioParameter* findParameter (PluginBase& p, const juce::String& id) { return dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id)); }

    var valueOf (juce::RangedAudioParameter& parameter)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&parameter))
            return choice->getCurrentChoiceName();
        if (dynamic_cast<juce::AudioParameterBool*> (&parameter) != nullptr)
            return parameter.getValue() > 0.5f;
        return round4 (parameter.convertFrom0to1 (parameter.getValue()));
    }

    // Sätter en parameter från ett JSON-värde i parameterns egna enheter (valnamn eller index för val, true/false för på/av).
    bool assign (juce::RangedAudioParameter& parameter, const var& value, juce::String& problem)
    {
        float real = 0.0f;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&parameter))
        {
            auto index = 0;
            if (value.isString() && ! value.toString().containsOnly ("0123456789"))
            {
                index = choice->choices.indexOf (value.toString().trim(), true);
                if (index < 0)
                {
                    problem = parameter.getParameterID() + ": unknown choice '" + value.toString() + "'. Options: " + choice->choices.joinIntoString (" | ");
                    return false;
                }
            }
            else
                index = (int) value;
            real = (float) juce::jlimit (0, choice->choices.size() - 1, index);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (&parameter) != nullptr)
        {
            const auto text = value.toString().toLowerCase();
            real = value.isString() ? (text == "true" || text == "on" || text == "1" ? 1.0f : 0.0f) : ((double) value > 0.5 ? 1.0f : 0.0f);
        }
        else
            real = (float) (double) value;

        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (parameter.convertTo0to1 (real));
        parameter.endChangeGesture();
        return true;
    }

    bool isEffectSlotParameter (const juce::String& id)
    {
        // "fxA3Type", "fxA3P2": platsernas typ och rattar redovisas under "effects" i stället.
        return Effects::isEffectParameter (id) && id.length() >= 6 && juce::CharacterFunctions::isDigit (id[3]) && (id.substring (4) == "Type" || (id[4] == 'P' && juce::CharacterFunctions::isDigit (id[5])));
    }

    bool parseSection (const var& value, Presets::Section& section)
    {
        if (value.isVoid())
            return true;
        for (int s = 0; s < Presets::numSections; ++s)
            if (Presets::sectionName ((Presets::Section) s).equalsIgnoreCase (value.toString().trim()))
            {
                section = (Presets::Section) s;
                return true;
            }
        return false;
    }

    int channelIndex (const var& value)
    {
        const auto text = value.toString().trim().toUpperCase();
        return text.length() == 1 && text[0] >= 'A' && text[0] < 'A' + numEffectChannels ? (int) (text[0] - 'A') : (juce::isPositiveAndBelow ((int) value - 1, numEffectChannels) ? (int) value - 1 : -1);
    }

    juce::String unitName (Effects::Knob::Unit unit)
    {
        using U = Effects::Knob::Unit;
        switch (unit)
        {
            case U::percent:  return "0..1 (shown as %)";
            case U::hz:       return "Hz";
            case U::ms:       return "ms";
            case U::db:       return "dB";
            case U::ratio:    return "ratio";
            case U::bits:     return "bits";
            case U::times:    return "times";
            case U::octaves:  return "octaves";
            case U::plain:    break;
        }
        return "";
    }

    var pointsToVar (const std::vector<NodeEnvelope::Point>& points)
    {
        juce::Array<var> list;
        for (const auto& point : points)
            list.add (var (juce::Array<var> { round4 (point.x), round4 (point.y), round4 (point.tension) }));
        return list;
    }

    std::vector<NodeEnvelope::Point> pointsFromVar (const var& value)
    {
        std::vector<NodeEnvelope::Point> points;
        if (auto* list = value.getArray())
            for (const auto& item : *list)
                if (auto* triple = item.getArray(); triple != nullptr && triple->size() >= 2)
                    points.push_back ({ (float) (double) (*triple)[0], (float) (double) (*triple)[1], triple->size() > 2 ? (float) (double) (*triple)[2] : 0.0f });
        return points;
    }

    juce::File previewFolder() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kickbase").getChildFile ("Previews"); }

    int gridIndex (const juce::String& name)
    {
        return Pattern::gridNames().indexOf (name.trim(), true);
    }
}

ToolLayer::ToolLayer (PluginBase& p) : processor (p) {}

//==============================================================================
juce::String ToolLayer::guide (const juce::String& topic)
{
    if (topic.equalsIgnoreCase ("psytrance"))
        return juce::String::fromUTF8 (BinaryData::psytrancekickbassreference_md, BinaryData::psytrancekickbassreference_mdSize);
    return juce::String::fromUTF8 (BinaryData::AGENT_GUIDE_md, BinaryData::AGENT_GUIDE_mdSize);
}

std::vector<ToolLayer::ToolInfo> ToolLayer::tools()
{
    return {
        { "get_guide", "READ THIS FIRST. Returns the guide for working with Kickbase: how the synth is built, units, the pattern language, workflow and pitfalls. topic 'agent' (default) is the manual; topic 'psytrance' is the sound design reference for psytrance kick and bass.",
          R"({"type":"object","properties":{"topic":{"type":"string","enum":["agent","psytrance"]}}})" },
        { "list_parameters", "Lists the synth's parameters with id, name, type (float/choice/bool), min, max, default, current value and a display text. Filter with prefix (e.g. 'kick', 'bass1', 'fxA', 'sc', 'lfo1', 'master', 'mix') or contains. Effect slot knobs are not listed here, see list_effects and set_effect.",
          R"({"type":"object","properties":{"prefix":{"type":"string"},"contains":{"type":"string"},"limit":{"type":"integer"}}})" },
        { "list_effects", "Lists the 13 effect types and their knobs (name, range, default, unit, choices) for use with set_effect.", R"({"type":"object","properties":{}})" },
        { "get_state", "Reads the current sound: parameters that differ from their defaults (or all), the effect chains, the pattern (notes per bass layer and kick steps), the sidechain curve, the kick envelopes and the LFO routes.",
          R"({"type":"object","properties":{"all_parameters":{"type":"boolean"}}})" },
        { "set_parameters", "Sets parameters in their own units (Hz, ms, dB, semitones, 0..1 for percentages). Choices accept the option name or index (e.g. \"bass1ATable\": \"Classic Saw\"), switches accept true/false. Example: {\"values\":{\"bass1Cutoff\":250,\"kickDrive\":0.3,\"bass1FilterType\":\"LP 24\"}}.",
          R"({"type":"object","properties":{"values":{"type":"object","additionalProperties":true}},"required":["values"]})" },
        { "set_effect", "Puts an effect in a slot. channel A-E, slot 1-5, type e.g. 'Echo', 'Reverb', 'Distortion' ('Off' clears the slot), knobs by name in real units, e.g. {\"TIME\":\"3/16\",\"FEEDBACK\":0.4}. Unmentioned knobs get the effect's defaults. Bass layers and the kick send to a channel with bassNFxChannel / bassNFxAmount (1 = A) via set_parameters.",
          R"({"type":"object","properties":{"channel":{"type":"string"},"slot":{"type":"integer"},"type":{"type":"string"},"knobs":{"type":"object","additionalProperties":true}},"required":["channel","slot","type"]})" },
        { "set_kick_envelopes", "Sets the kick's free envelopes as arrays of [time_seconds, value, tension] nodes. pitch: semitones above the tail pitch (0..72, max 0.5 s). amp: volume 0..1 (max 2 s; the kick ends at the last node, which should be 0). Tension -1..1: negative falls/rises fast first, positive holds then moves late. Omit one to leave it unchanged.",
          R"({"type":"object","properties":{"pitch":{"type":"array","items":{"type":"array","items":{"type":"number"}}},"amp":{"type":"array","items":{"type":"array","items":{"type":"number"}}}}})" },
        { "set_pattern", "Writes notes. layer 0-2 = bass layer 1-3, or 'kick'. For a bass layer give 'bars' (one text per bar, repeated over four bars) in the pattern language, with a 'grid' such as '1/16', '1/8 T', '1/32'. Step words separated by spaces: '.' rest, g/m/x/X = velocity ghost/soft/normal/accent, then optional pitch (+7, -5), '~' glide note, ':50' length in percent of the step. A GLIDE note (flag '~' / slide:true) works like an FL Studio slide note: it does not play a new note, it glides the note that is sounding to its pitch during its own length and the pitch stays there until that note ends. Example 1/16 rolling bass: [\". m x x . m x x . m x x . m x x\"]. Or give 'notes': [{\"tick\":0,\"length\":24,\"pitch\":0,\"velocity\":0.8,\"slide\":false}] (96 ticks per quarter note, 384 per bar). For 'kick' give 'bars' of 16 characters 'x' or '.', e.g. [\"x...x...x...x...\"]. Set the pattern length with set_parameters patternBars.",
          R"({"type":"object","properties":{"layer":{},"bars":{"type":"array","items":{"type":"string"}},"grid":{"type":"string"},"gate":{"type":"number"},"notes":{"type":"array","items":{"type":"object"}},"clear":{"type":"boolean"}},"required":["layer"]})" },
        { "set_gate", "Sets a bass layer's GATE: a separate on/off rhythm that chops the layer's sound, with its own grid, independent of the notes. layer 0-2; on (true/false), grid ('1/16' ...), depth 0..1 (1 = fully closed in the off steps), smooth_ms (edge softness), and the steps as 'bars' (one text per bar, repeated over four bars): 'x' = open, '.' = closed, e.g. [\"x.x.x.x.x.x.x.x.\"] on a 1/16 grid. all:'open' or 'closed' fills every step.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"on":{"type":"boolean"},"grid":{"type":"string"},"depth":{"type":"number"},"smooth_ms":{"type":"number"},"bars":{"type":"array","items":{"type":"string"}},"all":{"type":"string","enum":["open","closed"]}},"required":["layer"]})" },
        { "set_sidechain_curve", "Sets how the kick ducks the bass. mode 'Kick envelope' follows the kick; mode 'Curve' uses 'points' [[x,y,tension]...] where x is 0..1 across the length (1/16, 1/8, 3/16, 1/4, 1/2, or Kick) and y 0..1 is the ducking (1 = fully ducked). The depth per bass layer is bass1Duck/bass2Duck/bass3Duck.",
          R"({"type":"object","properties":{"mode":{"type":"string"},"length":{"type":"string"},"points":{"type":"array","items":{"type":"array","items":{"type":"number"}}}}})" },
        { "set_modulation", "Sets up LFOs and routes. lfos: [{\"lfo\":1,\"shape\":\"Sine\",\"rate\":\"1 bar\",\"on\":true}] (shapes: Sine, Triangle, Saw Up, Saw Down, Square, Random; rates 1/32 ... 4 bars). routes: [{\"lfo\":1,\"param\":\"bass1Cutoff\",\"amount\":0.3}] (amount -1..1 of the knob's travel, only sound parameters: kick*, bass*, fx*). clear_routes removes all routes first.",
          R"({"type":"object","properties":{"lfos":{"type":"array","items":{"type":"object"}},"routes":{"type":"array","items":{"type":"object"}},"clear_routes":{"type":"boolean"}}})" },
        { "list_presets", "Lists factory and user presets. section: Full, Kick, Bass, Pattern, Effects, Sidechain, Master or LFO (default: all).",
          R"({"type":"object","properties":{"section":{"type":"string"}}})" },
        { "load_preset", "Loads a factory preset (by name or index) or a user preset (by name or file path) into a section. layer 0-2 for Bass. Full replaces everything.",
          R"({"type":"object","properties":{"section":{"type":"string"},"name":{"type":"string"},"index":{"type":"integer"},"file":{"type":"string"},"layer":{"type":"integer"}},"required":["section"]})" },
        { "save_preset", "Saves the current sound as a user preset file (Documents\\Kickbase\\Presets\\<Section>), which then shows up in the plugin's preset menus. section Full saves everything.",
          R"({"type":"object","properties":{"section":{"type":"string"},"name":{"type":"string"},"layer":{"type":"integer"}},"required":["section","name"]})" },
        { "render_preview", "Renders the current sound offline (note held for 'bars' bars) and measures it. Returns metrics (peak, RMS, DC, spectral centroid, energy per band, stereo, kick length and tail frequency, checks against the psytrance rules) and a spectrogram image; the WAV is saved under Documents\\Kickbase\\Previews. part 'kick' or 'bass' renders one alone (kick part gets a close-up spectrogram). note is the MIDI key (52 = E4: bass root E1 at 41 Hz, kick tail near E1).",
          R"({"type":"object","properties":{"bpm":{"type":"number"},"bars":{"type":"integer"},"note":{"type":"integer"},"part":{"type":"string","enum":["full","kick","bass"]},"name":{"type":"string"},"spectrogram":{"type":"boolean"}}})" },
        { "save_piano_roll", "Saves a bass layer's notes (and pattern length and grid) as a piano roll preset file (Documents\\Kickbase\\Presets\\Piano Roll), which the plugin's piano roll PRESET menu lists and can load into any layer.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"name":{"type":"string"}},"required":["layer","name"]})" },
        { "load_piano_roll", "Loads a saved piano roll preset (by name or file path) into a bass layer, replacing its notes and setting the pattern length and grid it was saved with.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"name":{"type":"string"},"file":{"type":"string"}},"required":["layer"]})" },
        { "export_midi", "Exports a bass layer's notes as a MIDI file (96 ticks per quarter, tempo, 4/4). note is the MIDI key that sets the bass root (default 52 = E4: root E1); glide notes are written as ordinary notes. Default folder Documents\\Kickbase\\Exports.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"file":{"type":"string"},"bpm":{"type":"number"},"note":{"type":"integer"}},"required":["layer"]})" },
        { "import_midi", "Imports a MIDI file into a bass layer (replacing its notes; pitches are placed relative to the bass root of 'note', default 52, and moved by octaves to fit within 24 semitones) and sets the pattern length to 1, 2 or 4 bars as needed.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"file":{"type":"string"},"note":{"type":"integer"}},"required":["layer","file"]})" },
        { "analyze_audio", "Measures an audio file (WAV, AIFF, FLAC or MP3) the same way render_preview measures the synth, so you can compare a reference (a kick sample, a snippet of a track) with your sound: metrics, spectrogram image and, for kind 'kick', the kick length and tail frequency. start/seconds select a part (default: the first 20 s). Use bpm for the beat lines.",
          R"({"type":"object","properties":{"file":{"type":"string"},"kind":{"type":"string","enum":["kick","music"]},"start":{"type":"number"},"seconds":{"type":"number"},"bpm":{"type":"number"}},"required":["file"]})" },
        { "load_kick_sample", "Loads a WAV/AIFF/FLAC file (max 2 s, mono-summed) as the kick's click layer: it plays from the start of every kick on top of the synthesized body, scaled by kickClick. Empty or missing file clears it.",
          R"({"type":"object","properties":{"file":{"type":"string"}}})" },
        { "undo", "Undoes the last change made through these tools.", R"({"type":"object","properties":{}})" },
        { "redo", "Redoes what undo undid.", R"({"type":"object","properties":{}})" },
        { "reset", "Resets everything to the Init sound (all parameters at their defaults, default pattern, no routes).", R"({"type":"object","properties":{}})" },
    };
}

//==============================================================================
void ToolLayer::snapshot()
{
    juce::MemoryBlock block;
    processor.getStateInformation (block);
    undoStack.push_back (std::move (block));
    if (undoStack.size() > 60)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

int ToolLayer::parameterIndex (const juce::String& id) const
{
    const auto& parameters = processor.getParameters();
    for (int i = 0; i < parameters.size(); ++i)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == id)
            return i;
    return -1;
}

var ToolLayer::call (const juce::String& tool, const var& args)
{
    if (tool == "get_guide")        return ok (guide (args["topic"].toString()));
    if (tool == "list_parameters")  return listParameters (args);
    if (tool == "list_effects")     return listEffects();
    if (tool == "get_state")        return getState (args);
    if (tool == "list_presets")     return listPresets (args);
    if (tool == "render_preview")   return renderPreview (args);
    if (tool == "analyze_audio")    return analyzeAudio (args);
    if (tool == "export_midi" || tool == "save_piano_roll")
        return pianoRollFiles (tool, args);
    if (tool == "undo")             return undo (false);
    if (tool == "redo")             return undo (true);

    // Resten ändrar ljudet: ett ångra-steg först.
    if (tool == "set_parameters" || tool == "set_effect" || tool == "set_kick_envelopes" || tool == "set_pattern" || tool == "set_sidechain_curve" || tool == "set_gate"
        || tool == "set_modulation" || tool == "load_preset" || tool == "reset" || tool == "load_kick_sample" || tool == "load_piano_roll" || tool == "import_midi")
        snapshot();

    if (tool == "set_parameters")      return setParameters (args);
    if (tool == "set_effect")          return setEffect (args);
    if (tool == "set_kick_envelopes")  return setKickEnvelopes (args);
    if (tool == "set_pattern")         return setPattern (args);
    if (tool == "set_sidechain_curve") return setSidechainCurve (args);
    if (tool == "set_gate")            return setGate (args);
    if (tool == "set_modulation")      return setModulation (args);
    if (tool == "load_preset")         return loadPreset (args);
    if (tool == "load_kick_sample")    return loadKickSample (args);
    if (tool == "load_piano_roll" || tool == "import_midi")
        return pianoRollFiles (tool, args);
    if (tool == "save_preset")         return savePreset (args);
    if (tool == "reset")
    {
        processor.loadFactoryPreset (Presets::Section::full, 0);
        return ok ("Reset to the Init sound.");
    }
    return fail ("Unknown tool: " + tool);
}

//==============================================================================
var ToolLayer::listParameters (const var& args) const
{
    const auto prefix = args["prefix"].toString(), contains = args["contains"].toString();
    const auto limit = args.hasProperty ("limit") ? (int) args["limit"] : 150;

    juce::Array<var> list;
    auto total = 0;
    for (auto* p : processor.getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (parameter == nullptr)
            continue;
        const auto id = parameter->getParameterID();
        if (isEffectSlotParameter (id) || (prefix.isNotEmpty() && ! id.startsWithIgnoreCase (prefix)) || (contains.isNotEmpty() && ! id.containsIgnoreCase (contains)))
            continue;

        ++total;
        if (list.size() >= limit)
            continue;

        auto entry = object();
        put (entry, "id", id);
        put (entry, "name", parameter->getName (64));
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        {
            put (entry, "type", "choice");
            juce::Array<var> options;
            for (const auto& option : choice->choices)
                options.add (option);
            put (entry, "choices", options);
            put (entry, "default", choice->choices[(int) std::round (parameter->convertFrom0to1 (parameter->getDefaultValue()))]);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        {
            put (entry, "type", "bool");
            put (entry, "default", parameter->getDefaultValue() > 0.5f);
        }
        else
        {
            const auto range = parameter->getNormalisableRange();
            put (entry, "type", "float");
            put (entry, "min", round4 (range.start));
            put (entry, "max", round4 (range.end));
            put (entry, "default", round4 (parameter->convertFrom0to1 (parameter->getDefaultValue())));
        }
        put (entry, "value", valueOf (*parameter));
        put (entry, "display", parameter->getText (parameter->getValue(), 32));
        list.add (entry);
    }

    auto data = object();
    put (data, "count", total);
    put (data, "truncated", total > list.size());
    put (data, "parameters", list);
    return ok (data);
}

var ToolLayer::listEffects() const
{
    juce::Array<var> types;
    for (int t = 1; t < numEffectTypes; ++t)
    {
        auto type = object();
        put (type, "type", Effects::typeNames()[t]);
        juce::Array<var> knobs;
        for (const auto& knob : Effects::knobs ((EffectType) t))
        {
            auto entry = object();
            put (entry, "name", knob.name);
            if (knob.isChoice())
            {
                juce::Array<var> choices;
                for (const auto& c : knob.choices)
                    choices.add (c);
                put (entry, "choices", choices);
                put (entry, "default", knob.choices[(int) knob.defaultValue]);
            }
            else
            {
                put (entry, "min", round4 (knob.min));
                put (entry, "max", round4 (knob.max));
                put (entry, "default", round4 (knob.defaultValue));
                put (entry, "unit", unitName (knob.unit));
            }
            knobs.add (entry);
        }
        put (type, "knobs", knobs);
        types.add (type);
    }
    return ok (types);
}

var ToolLayer::getState (const var& args) const
{
    const auto all = (bool) args["all_parameters"];
    auto data = object();

    auto transport = object();
    put (transport, "bpm", round4 (processor.transportStatus.bpm.load()));
    put (transport, "key_note", processor.transportStatus.keyNote.load());
    put (data, "transport", transport);

    auto parameters = object();
    for (auto* p : processor.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p); parameter != nullptr && ! isEffectSlotParameter (parameter->getParameterID()))
            if (all || std::abs (parameter->getValue() - parameter->getDefaultValue()) > 1.0e-4f)
                put (parameters, parameter->getParameterID(), valueOf (*parameter));
    put (data, "parameters", parameters);

    // Effektkedjorna: bara platser med en effekt.
    juce::Array<var> effects;
    for (int c = 0; c < numEffectChannels; ++c)
        for (int s = 0; s < slotsPerChannel; ++s)
        {
            const auto type = Effects::slotType (processor.apvts, c, s);
            if (type == EffectType::off)
                continue;
            auto slot = object();
            put (slot, "channel", Effects::channelName (c));
            put (slot, "slot", s + 1);
            put (slot, "type", Effects::typeNames()[(int) type]);
            auto knobs = object();
            const auto& list = Effects::knobs (type);
            for (size_t k = 0; k < list.size(); ++k)
            {
                const auto real = list[k].fromNormalised (processor.apvts.getRawParameterValue (Effects::knobParamID (c, s, (int) k))->load());
                put (knobs, list[k].name, list[k].isChoice() ? var (list[k].choices[(int) std::round (real)]) : var (round4 (real)));
            }
            put (slot, "knobs", knobs);
            effects.add (slot);
        }
    put (data, "effects", effects);

    // Mönstret.
    auto pattern = object();
    put (pattern, "ticks_per_bar", Pattern::ticksPerBar);
    put (pattern, "bars", Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load()));
    juce::String kick;
    for (int i = 0; i < Pattern::maxKickSteps; ++i)
        kick << (processor.pattern.isKickOn (i) ? 'x' : '.');
    put (pattern, "kick_steps_64", kick);
    juce::Array<var> layers;
    for (int layer = 0; layer < Pattern::numLayers; ++layer)
    {
        juce::Array<var> notes;
        for (int tick = 0; tick < Pattern::maxTicks; ++tick)
            if (const auto& n = processor.pattern.note (layer, tick); n.on.load())
                notes.add (var (juce::Array<var> { tick, n.length.load(), n.pitch.load(), round4 (n.velocity.load()), n.slide.load() }));
        auto entry = object();
        put (entry, "layer", layer);
        put (entry, "grid", Pattern::gridNames()[(int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::grid))->load()]);
        put (entry, "notes [tick, length, pitch, velocity, glide]", notes);
        put (entry, "piano_roll_layer", processor.pattern.isCustom (layer));

        auto gateInfo = object();
        const auto gateGrid = juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateGrid))->load());
        put (gateInfo, "on", processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateOn))->load() >= 0.5f);
        put (gateInfo, "grid", Pattern::gridNames()[gateGrid]);
        put (gateInfo, "depth", round4 (processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateDepth))->load()));
        juce::String steps;
        const auto barCount = Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
        for (int i = 0; i < barCount * Pattern::bassStepsPerBar ((BassGrid) gateGrid); ++i)
            steps << (processor.pattern.isGateStepOn (layer, i) ? 'x' : '.');
        put (gateInfo, "steps (x open, . closed)", steps);
        put (entry, "gate", gateInfo);
        layers.add (entry);
    }
    put (pattern, "bass_layers", layers);
    put (data, "pattern", pattern);

    put (data, "kick_pitch_envelope", pointsToVar (processor.kickPitchEnv.getPoints()));
    put (data, "kick_amp_envelope", pointsToVar (processor.kickAmpEnv.getPoints()));

    juce::Array<var> curve;
    for (const auto& point : processor.sidechainCurve.getPoints())
        curve.add (var (juce::Array<var> { round4 (point.x), round4 (point.y), round4 (point.tension) }));
    put (data, "sidechain_curve", curve);

    juce::Array<var> routes;
    for (int r = 0; r < Modulation::maxRoutes; ++r)
    {
        const auto& route = processor.modRoutes.routes[(size_t) r];
        const auto source = route.source.load(), target = route.target.load();
        if (source < 0 || ! juce::isPositiveAndBelow (target, processor.getParameters().size()))
            continue;
        auto entry = object();
        put (entry, "lfo", source + 1);
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (processor.getParameters()[target]))
            put (entry, "param", withId->paramID);
        put (entry, "amount", round4 (route.amount.load()));
        routes.add (entry);
    }
    put (data, "lfo_routes", routes);
    return ok (data);
}

var ToolLayer::setParameters (const var& args)
{
    auto* values = args["values"].getDynamicObject();
    if (values == nullptr)
        return fail ("Give 'values': an object with parameter ids and values.");

    auto applied = object();
    juce::Array<var> problems;
    for (const auto& property : values->getProperties())
    {
        const auto id = property.name.toString();
        auto* parameter = findParameter (processor, id);
        if (parameter == nullptr)
        {
            problems.add ("Unknown parameter: " + id + " (use list_parameters)");
            continue;
        }
        juce::String problem;
        if (assign (*parameter, property.value, problem))
            put (applied, id, valueOf (*parameter));
        else
            problems.add (problem);
    }
    auto data = object();
    put (data, "applied", applied);
    if (! problems.isEmpty())
        put (data, "problems", problems);
    return ok (data);
}

var ToolLayer::setEffect (const var& args)
{
    const auto channel = channelIndex (args["channel"]);
    const auto slot = (int) args["slot"] - 1;
    if (channel < 0 || ! juce::isPositiveAndBelow (slot, slotsPerChannel))
        return fail ("channel must be A-E and slot 1-5.");

    const auto typeIndex = Effects::typeNames().indexOf (args["type"].toString().trim(), true);
    if (typeIndex < 0)
        return fail ("Unknown effect type. Options: " + Effects::typeNames().joinIntoString (" | "));

    const auto type = (EffectType) typeIndex;
    Effects::setSlotType (processor.apvts, channel, slot, type); // sätter också rattarnas standardvärden

    juce::Array<var> problems;
    if (auto* knobs = args["knobs"].getDynamicObject())
    {
        const auto& list = Effects::knobs (type);
        for (const auto& property : knobs->getProperties())
        {
            auto found = -1;
            for (size_t k = 0; k < list.size(); ++k)
                if (list[k].name.replaceCharacter ('_', ' ').equalsIgnoreCase (property.name.toString().replaceCharacter ('_', ' ')))
                    found = (int) k;
            if (found < 0)
            {
                problems.add ("No knob '" + property.name.toString() + "' on " + Effects::typeNames()[typeIndex]);
                continue;
            }

            const auto& knob = list[(size_t) found];
            auto real = 0.0f;
            if (knob.isChoice())
            {
                const auto index = property.value.isString() && ! property.value.toString().containsOnly ("0123456789") ? knob.choices.indexOf (property.value.toString().trim(), true) : (int) property.value;
                if (index < 0 || index >= knob.choices.size())
                {
                    problems.add (knob.name + ": unknown choice. Options: " + knob.choices.joinIntoString (" | "));
                    continue;
                }
                real = (float) index;
            }
            else
                real = (float) (double) property.value;

            if (auto* parameter = findParameter (processor, Effects::knobParamID (channel, slot, found)))
                Presets::setParameter (*parameter, knob.toNormalised (real));
        }
    }

    auto data = object();
    put (data, "slot", Effects::channelName (channel) + juce::String (slot + 1) + " = " + Effects::typeNames()[typeIndex]);
    if (! problems.isEmpty())
        put (data, "problems", problems);
    return ok (data);
}

var ToolLayer::setKickEnvelopes (const var& args)
{
    if (args.hasProperty ("pitch"))
    {
        auto points = pointsFromVar (args["pitch"]);
        if (points.size() < 2)
            return fail ("pitch needs at least two nodes [time_seconds, semitones, tension].");
        processor.kickPitchEnv.setPoints (std::move (points));
    }
    if (args.hasProperty ("amp"))
    {
        auto points = pointsFromVar (args["amp"]);
        if (points.size() < 2)
            return fail ("amp needs at least two nodes [time_seconds, volume, tension].");
        processor.kickAmpEnv.setPoints (std::move (points));
    }
    processor.markStateChanged();

    auto data = object();
    put (data, "kick_pitch_envelope", pointsToVar (processor.kickPitchEnv.getPoints()));
    put (data, "kick_amp_envelope", pointsToVar (processor.kickAmpEnv.getPoints()));
    return ok (data);
}

var ToolLayer::setPattern (const var& args)
{
    auto& pattern = processor.pattern;
    const auto layerArg = args["layer"];

    if (layerArg.toString().equalsIgnoreCase ("kick"))
    {
        for (int i = 0; i < Pattern::maxKickSteps; ++i)
            pattern.kickStep (i) = false;
        if (auto* bars = args["bars"].getArray())
            for (int bar = 0; bar < Pattern::maxBars; ++bar)
            {
                if (bars->isEmpty())
                    break;
                const auto text = (*bars)[bar % bars->size()].toString().retainCharacters ("x.X");
                for (int step = 0; step < Pattern::kickStepsPerBar && step < text.length(); ++step)
                    pattern.kickStep (bar * Pattern::kickStepsPerBar + step) = text[step] != '.';
            }
        processor.patternChanged();
        return ok ("Kick pattern set.");
    }

    const auto layer = (int) layerArg;
    if (! juce::isPositiveAndBelow (layer, Pattern::numLayers))
        return fail ("layer must be 0, 1, 2 or 'kick'.");

    auto grid = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::grid))->load();
    if (args.hasProperty ("grid"))
    {
        grid = gridIndex (args["grid"].toString());
        if (grid < 0)
            return fail ("Unknown grid. Options: " + Pattern::gridNames().joinIntoString (" | "));
    }

    if (auto* notes = args["notes"].getArray())
    {
        pattern.clearNotes (layer);
        for (const auto& note : *notes)
        {
            const auto tick = (int) note["tick"];
            if (! juce::isPositiveAndBelow (tick, Pattern::maxTicks))
                continue;
            pattern.addNote (layer, tick, note.hasProperty ("length") ? (int) note["length"] : Pattern::ticksPerStep ((BassGrid) grid),
                             (int) note["pitch"], note.hasProperty ("velocity") ? (float) (double) note["velocity"] : 0.8f, (bool) note["slide"]);
        }
        pattern.setCustom (layer, true);
    }
    else if (auto* bars = args["bars"].getArray())
    {
        Presets::Extras::PatternLine line;
        line.layer = layer;
        line.grid = (BassGrid) grid;
        line.gate = args.hasProperty ("gate") ? (float) (double) args["gate"] : 0.7f;
        for (const auto& bar : *bars)
            line.bars.push_back (bar.toString().toStdString());
        Presets::writePatternLine (pattern, line);
    }
    else if ((bool) args["clear"])
        pattern.clearNotes (layer);
    else
        return fail ("Give 'bars' (pattern language), 'notes', or clear:true.");

    if (auto* gridParameter = findParameter (processor, ParamID::bass (layer, BassField::grid)))
        Presets::setParameter (*gridParameter, gridParameter->convertTo0to1 ((float) grid));
    processor.patternChanged();

    auto count = 0;
    for (int tick = 0; tick < Pattern::maxTicks; ++tick)
        count += pattern.note (layer, tick).on.load() ? 1 : 0;
    auto data = object();
    put (data, "layer", layer);
    put (data, "notes", count);
    put (data, "grid", Pattern::gridNames()[grid]);
    return ok (data);
}

var ToolLayer::setGate (const var& args)
{
    const auto layer = (int) args["layer"];
    if (! juce::isPositiveAndBelow (layer, Pattern::numLayers))
        return fail ("layer must be 0, 1 or 2.");

    juce::String problem;
    for (const auto& [key, field] : { std::pair<const char*, BassField> { "on", BassField::gateOn }, { "grid", BassField::gateGrid },
                                      { "depth", BassField::gateDepth }, { "smooth_ms", BassField::gateSmooth } })
        if (args.hasProperty (key))
        {
            auto* parameter = findParameter (processor, ParamID::bass (layer, field));
            if (parameter == nullptr || ! assign (*parameter, args[key], problem))
                return fail (problem.isEmpty() ? juce::String ("No such gate parameter.") : problem);
        }

    const auto gridIndexNow = juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateGrid))->load());
    const auto stepsPerBar = Pattern::bassStepsPerBar ((BassGrid) gridIndexNow);

    if (auto* bars = args["bars"].getArray(); bars != nullptr && ! bars->isEmpty())
    {
        for (int bar = 0; bar < Pattern::maxBars; ++bar)
        {
            const auto text = (*bars)[bar % bars->size()].toString().retainCharacters ("xX.");
            for (int step = 0; step < stepsPerBar; ++step)
                processor.pattern.gateStep (layer, bar * stepsPerBar + step) = step < text.length() ? text[step] != '.' : true;
        }
    }
    else if (args.hasProperty ("all"))
        processor.pattern.setGate (layer, args["all"].toString().equalsIgnoreCase ("open"));

    processor.patternChanged();
    auto data = object();
    put (data, "layer", layer);
    put (data, "grid", Pattern::gridNames()[gridIndexNow]);
    put (data, "steps_per_bar", stepsPerBar);
    return ok (data);
}

var ToolLayer::setSidechainCurve (const var& args)
{
    juce::String problem;
    if (args.hasProperty ("mode"))
        if (auto* mode = findParameter (processor, "scMode"); mode == nullptr || ! assign (*mode, args["mode"], problem))
            return fail (problem);
    if (args.hasProperty ("length"))
        if (auto* length = findParameter (processor, "scLength"); length == nullptr || ! assign (*length, args["length"], problem))
            return fail (problem);

    if (args.hasProperty ("points"))
    {
        std::vector<SidechainCurve::Point> points;
        if (auto* list = args["points"].getArray())
            for (const auto& item : *list)
                if (auto* triple = item.getArray(); triple != nullptr && triple->size() >= 2)
                    points.push_back ({ (float) (double) (*triple)[0], (float) (double) (*triple)[1], triple->size() > 2 ? (float) (double) (*triple)[2] : 0.0f });
        if (points.size() < 2)
            return fail ("points needs at least two [x, y, tension] nodes.");
        processor.sidechainCurve.setPoints (points);
    }
    processor.markStateChanged();
    return ok ("Sidechain updated.");
}

var ToolLayer::setModulation (const var& args)
{
    juce::Array<var> problems;
    if ((bool) args["clear_routes"])
        processor.modRoutes.clear();

    if (auto* lfos = args["lfos"].getArray())
        for (const auto& lfo : *lfos)
        {
            const auto index = (int) lfo["lfo"] - 1;
            if (! juce::isPositiveAndBelow (index, Modulation::numLfos))
            {
                problems.add ("lfo must be 1-3");
                continue;
            }
            for (const auto& [key, suffix] : { std::pair<const char*, const char*> { "shape", "Shape" }, { "rate", "Rate" }, { "on", "On" } })
                if (lfo.hasProperty (key))
                    if (auto* parameter = findParameter (processor, Modulation::paramID (index, suffix)))
                    {
                        juce::String problem;
                        if (! assign (*parameter, lfo[key], problem))
                            problems.add (problem);
                    }
        }

    if (auto* routes = args["routes"].getArray())
        for (const auto& route : *routes)
        {
            const auto source = (int) route["lfo"] - 1;
            const auto id = route["param"].toString();
            const auto index = parameterIndex (id);
            auto* parameter = findParameter (processor, id);
            if (! juce::isPositiveAndBelow (source, Modulation::numLfos) || parameter == nullptr || index < 0)
                problems.add ("Bad route: lfo 1-3 and a parameter id are needed (" + id + ")");
            else if (! Modulation::isModulatable (*parameter))
                problems.add (id + " can't be modulated (only kick*, bass*, fx* sound parameters)");
            else if (processor.modRoutes.add (source, index, juce::jlimit (-1.0f, 1.0f, (float) (double) route["amount"])) < 0)
                problems.add ("All 24 routes are in use");
        }

    processor.markStateChanged();
    auto data = object();
    data.getDynamicObject()->setProperty ("note", "LFO routes updated.");
    if (! problems.isEmpty())
        put (data, "problems", problems);
    return ok (data);
}

var ToolLayer::listPresets (const var& args) const
{
    auto only = Presets::Section::full;
    const auto filtered = args.hasProperty ("section");
    if (! parseSection (args["section"], only))
        return fail ("Unknown section.");

    juce::Array<var> sections;
    for (int s = 0; s < Presets::numSections; ++s)
    {
        const auto section = (Presets::Section) s;
        if (filtered && section != only)
            continue;
        auto entry = object();
        put (entry, "section", Presets::sectionName (section));
        juce::Array<var> factory, user;
        for (int i = 0; i < Presets::factoryCount (section, processor.isEffectVersion()); ++i)
            factory.add (var (juce::Array<var> { i, Presets::factoryName (section, processor.isEffectVersion(), i) }));
        for (const auto& file : processor.getUserPresets (section))
            user.add (file.getFileNameWithoutExtension());
        put (entry, "factory [index, name]", factory);
        put (entry, "user", user);
        if (section == Presets::Section::pattern)
        {
            juce::Array<var> rolls;
            for (const auto& file : RollFiles::list())
                rolls.add (file.getFileNameWithoutExtension());
            put (entry, "piano_rolls (save_piano_roll / load_piano_roll)", rolls);
        }
        sections.add (entry);
    }
    return ok (sections);
}

var ToolLayer::loadPreset (const var& args)
{
    auto section = Presets::Section::full;
    if (! parseSection (args["section"], section))
        return fail ("Unknown section.");
    const auto layer = juce::jlimit (0, Pattern::numLayers - 1, (int) args["layer"]);
    const auto effectVersion = processor.isEffectVersion();

    if (args.hasProperty ("index"))
    {
        const auto index = (int) args["index"];
        if (! juce::isPositiveAndBelow (index, Presets::factoryCount (section, effectVersion)))
            return fail ("No factory preset with that index.");
        processor.loadFactoryPreset (section, index, layer);
        return ok ("Loaded " + Presets::factoryName (section, effectVersion, index));
    }

    const auto name = args["name"].toString().trim();
    for (int i = 0; i < Presets::factoryCount (section, effectVersion); ++i)
        if (Presets::factoryName (section, effectVersion, i).equalsIgnoreCase (name))
        {
            processor.loadFactoryPreset (section, i, layer);
            return ok ("Loaded factory preset " + Presets::factoryName (section, effectVersion, i));
        }

    juce::File file;
    if (args.hasProperty ("file"))
        file = juce::File (args["file"].toString());
    else
        for (const auto& candidate : processor.getUserPresets (section))
            if (candidate.getFileNameWithoutExtension().equalsIgnoreCase (name))
                file = candidate;

    if (file.existsAsFile() && processor.loadUserPreset (section, file, layer))
        return ok ("Loaded user preset " + file.getFileNameWithoutExtension());
    return fail ("No factory or user preset matches. Use list_presets.");
}

var ToolLayer::savePreset (const var& args)
{
    auto section = Presets::Section::full;
    if (! parseSection (args["section"], section))
        return fail ("Unknown section.");
    const auto name = args["name"].toString().trim();
    const auto layer = juce::jlimit (0, Pattern::numLayers - 1, (int) args["layer"]);
    if (name.isEmpty() || ! processor.saveUserPreset (section, name, layer))
        return fail ("Could not save the preset (empty name?).");

    auto data = object();
    put (data, "saved", Presets::folder (section, processor.isEffectVersion()).getChildFile (juce::File::createLegalFileName (name) + ".mofp").getFullPathName());
    return ok (data);
}

var ToolLayer::renderPreview (const var& args)
{
    const auto bpm = args.hasProperty ("bpm") ? juce::jlimit (60.0, 220.0, (double) args["bpm"]) : 145.0;
    const auto bars = args.hasProperty ("bars") ? juce::jlimit (1, 16, (int) args["bars"]) : 4;
    const auto note = args.hasProperty ("note") ? juce::jlimit (0, 127, (int) args["note"]) : 52;
    const auto partName = args["part"].toString().toLowerCase();
    const auto part = partName == "kick" ? PluginBase::ExportPart::kick : partName == "bass" ? PluginBase::ExportPart::bass : PluginBase::ExportPart::full;
    const auto wantImage = ! args.hasProperty ("spectrogram") || (bool) args["spectrogram"];
    constexpr double rate = 44100.0;

    OfflineRender::Settings settings;
    settings.sampleRate = rate;
    settings.bpm = bpm;
    settings.bars = bars;
    settings.notes = { note };
    settings.extraBars = 1;
    const auto audio = processor.renderOffline (settings, part);
    const auto m = AudioAnalysis::measure (audio, rate);

    auto name = juce::File::createLegalFileName (args["name"].toString().trim());
    if (name.isEmpty())
        name = "preview";
    const auto folder = previewFolder();
    folder.createDirectory();
    const auto wav = folder.getChildFile (name + ".wav");
    OfflineRender::writeWav (audio, rate, wav);

    auto metrics = object();
    put (metrics, "peak_dbfs", round4 (m.peakDb));
    put (metrics, "rms_db", round4 (m.rmsDb));
    put (metrics, "dc_offset", round4 (m.dc));
    put (metrics, "spectral_centroid_hz", juce::roundToInt (m.centroid));
    auto bands = object();
    const char* bandNames[] { "below_60hz", "60_250hz", "250_1000hz", "1k_5khz", "above_5khz" };
    for (size_t i = 0; i < m.bandsDb.size(); ++i)
        put (bands, bandNames[i], round4 (m.bandsDb[i]));
    put (metrics, "band_energy_db_relative_to_total", bands);
    put (metrics, "side_vs_mid_db", round4 (m.sideDb));
    put (metrics, "side_vs_mid_below_150hz_db", round4 (m.lowSideDb));

    juce::Array<var> checks;
    auto check = [&checks] (const juce::String& what, bool passed, const juce::String& detail)
    {
        auto entry = object();
        put (entry, "check", what);
        put (entry, "pass", passed);
        put (entry, "detail", detail);
        checks.add (entry);
    };
    check ("finite_audio", m.finite, m.finite ? "no NaN or infinity" : "NaN or infinity in the output");
    check ("no_clipping", m.peakDb <= -0.3f, "peak " + juce::String (m.peakDb, 1) + " dBFS (should stay at or below -0.3)");
    check ("dc_offset_small", std::abs (m.dc) < 0.01f, "dc " + juce::String (m.dc, 4));
    check ("bass_mono_below_150hz", m.lowSideDb < -20.0f, "side vs mid below 150 Hz: " + juce::String (m.lowSideDb, 1) + " dB (should be below -20)");

    if (part == PluginBase::ExportPart::kick)
    {
        const auto length = AudioAnalysis::kickLengthMs (audio, rate, bpm);
        const auto tail = AudioAnalysis::kickTailHz (audio, rate, bpm);
        const auto eighth = 60000.0 / bpm * 0.5;
        put (metrics, "kick_length_ms", juce::roundToInt (length));
        put (metrics, "kick_tail_hz", round4 (tail));
        check ("kick_at_most_an_eighth_note", length <= eighth, juce::String (juce::roundToInt (length)) + " ms of " + juce::String (juce::roundToInt (eighth)) + " ms (1/8 note)");

        // Svansen mot basens grundton (lager 1).
        const auto bassRoot = Params::midiToHz ((float) Params::bassRootMidiNote (note)
                                                + (float) processor.apvts.getRawParameterValue (ParamID::bass (0, BassField::octave))->load() * 12.0f
                                                + processor.apvts.getRawParameterValue (ParamID::bass (0, BassField::tune))->load());
        put (metrics, "expected_bass1_root_hz", round4 (bassRoot));
        if (tail > 0.0f)
        {
            const auto semitones = 12.0 * std::log2 (tail / bassRoot);
            check ("kick_tail_matches_bass_root", std::abs (semitones) <= 2.5, "tail " + juce::String (tail, 1) + " Hz vs bass root " + juce::String (bassRoot, 1) + " Hz: "
                                                                                 + juce::String (semitones, 1) + " semitones (within about 2 is normal; exactly 0 is safest)");
        }
    }

    auto data = object();
    put (data, "part", partName.isEmpty() ? juce::String ("full") : partName);
    put (data, "bpm", bpm);
    put (data, "bars", bars);
    put (data, "midi_note", note);
    put (data, "metrics", metrics);
    put (data, "checks", checks);
    put (data, "wav_file", wav.getFullPathName());

    auto result = ok (data);
    if (wantImage)
    {
        const auto kickCloseUp = part == PluginBase::ExportPart::kick;
        const auto image = kickCloseUp ? AudioAnalysis::spectrogram (audio, rate, bpm, 11, 0.4) : AudioAnalysis::spectrogram (audio, rate, bpm);
        AudioAnalysis::writePng (image, folder.getChildFile (name + ".png"));
        auto entry = object();
        put (entry, "name", name + ".png");
        put (entry, "data", AudioAnalysis::pngBase64 (image));
        put (result, "images", juce::Array<var> { entry });
        put (data, "spectrogram_file", folder.getChildFile (name + ".png").getFullPathName());
    }
    return result;
}

var ToolLayer::analyzeAudio (const var& args)
{
    const juce::File file (args["file"].toString());
    if (! file.existsAsFile())
        return fail ("File not found: " + file.getFullPathName());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return fail ("Cannot read that file (WAV, AIFF, FLAC and MP3 are supported).");

    const auto rate = reader->sampleRate;
    const auto start = juce::jlimit<juce::int64> (0, reader->lengthInSamples, (juce::int64) (std::max (0.0, (double) args["start"]) * rate));
    const auto wanted = args.hasProperty ("seconds") ? (double) args["seconds"] : 20.0;
    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples - start, (juce::int64) (juce::jlimit (0.05, 60.0, wanted) * rate));
    if (length < 256)
        return fail ("Nothing to analyse in that range.");

    juce::AudioBuffer<float> audio (2, length);
    juce::AudioBuffer<float> raw ((int) reader->numChannels, length);
    reader->read (&raw, 0, length, start, true, true);
    for (int ch = 0; ch < 2; ++ch)
        audio.copyFrom (ch, 0, raw, std::min (ch, raw.getNumChannels() - 1), 0, length);

    const auto bpm = args.hasProperty ("bpm") ? juce::jlimit (60.0, 220.0, (double) args["bpm"]) : 145.0;
    const auto kick = args["kind"].toString().equalsIgnoreCase ("kick");
    const auto m = AudioAnalysis::measure (audio, rate);

    auto metrics = object();
    put (metrics, "duration_s", round4 ((double) length / rate));
    put (metrics, "sample_rate", rate);
    put (metrics, "peak_dbfs", round4 (m.peakDb));
    put (metrics, "rms_db", round4 (m.rmsDb));
    put (metrics, "dc_offset", round4 (m.dc));
    put (metrics, "spectral_centroid_hz", juce::roundToInt (m.centroid));
    auto bands = object();
    const char* bandNames[] { "below_60hz", "60_250hz", "250_1000hz", "1k_5khz", "above_5khz" };
    for (size_t i = 0; i < m.bandsDb.size(); ++i)
        put (bands, bandNames[i], round4 (m.bandsDb[i]));
    put (metrics, "band_energy_db_relative_to_total", bands);
    put (metrics, "side_vs_mid_db", round4 (m.sideDb));
    put (metrics, "side_vs_mid_below_150hz_db", round4 (m.lowSideDb));
    if (kick)
    {
        put (metrics, "kick_length_ms", juce::roundToInt (AudioAnalysis::kickLengthMs (audio, rate, bpm)));
        put (metrics, "kick_tail_hz", round4 (AudioAnalysis::kickTailHz (audio, rate, bpm)));
    }

    auto data = object();
    put (data, "file", file.getFullPathName());
    put (data, "metrics", metrics);

    auto result = ok (data);
    const auto image = kick ? AudioAnalysis::spectrogram (audio, rate, bpm, 11, 0.4) : AudioAnalysis::spectrogram (audio, rate, bpm);
    auto entry = object();
    put (entry, "name", file.getFileNameWithoutExtension() + ".png");
    put (entry, "data", AudioAnalysis::pngBase64 (image));
    put (result, "images", juce::Array<var> { entry });
    return result;
}

var ToolLayer::pianoRollFiles (const juce::String& tool, const var& args)
{
    const auto layer = (int) args["layer"];
    if (! juce::isPositiveAndBelow (layer, Pattern::numLayers))
        return fail ("layer must be 0, 1 or 2.");

    auto& pattern = processor.pattern;
    const auto bars = Params::barsFromChoice ((int) processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
    const auto gridIndex = juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::grid))->load());
    const auto note = args.hasProperty ("note") ? juce::jlimit (0, 127, (int) args["note"]) : 52;
    const auto root = Params::bassRootMidiNote (note) + 12 * (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::octave))->load();
    const auto documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kickbase");

    auto applyLoaded = [this, layer] (int newBars, BassGrid grid)
    {
        if (auto* gridParameter = findParameter (processor, ParamID::bass (layer, BassField::grid)))
            Presets::setParameter (*gridParameter, gridParameter->convertTo0to1 ((float) grid));
        if (auto* barsParameter = findParameter (processor, ParamID::patternBars))
            Presets::setParameter (*barsParameter, barsParameter->convertTo0to1 ((float) RollFiles::barsToChoice (newBars)));
        processor.patternChanged();
    };

    if (tool == "save_piano_roll")
    {
        const auto name = juce::File::createLegalFileName (args["name"].toString().trim());
        if (name.isEmpty())
            return fail ("Give a name.");
        const auto file = RollFiles::folder().getChildFile (name + ".roll");
        if (! RollFiles::save (pattern, layer, bars, (BassGrid) gridIndex, file))
            return fail ("Could not write " + file.getFullPathName());
        auto data = object();
        put (data, "saved", file.getFullPathName());
        return ok (data);
    }

    if (tool == "load_piano_roll")
    {
        juce::File file = args.hasProperty ("file") ? juce::File (args["file"].toString()) : juce::File();
        if (! file.existsAsFile())
            for (const auto& candidate : RollFiles::list())
                if (candidate.getFileNameWithoutExtension().equalsIgnoreCase (args["name"].toString().trim()))
                    file = candidate;
        int newBars = 1;
        auto grid = BassGrid::sixteenth;
        if (! file.existsAsFile() || ! RollFiles::load (pattern, layer, file, newBars, grid))
            return fail ("No such piano roll preset. The plugin's PRESET menu lists the saved ones; save one first with save_piano_roll.");
        applyLoaded (newBars, grid);
        return ok ("Loaded " + file.getFileNameWithoutExtension() + " into layer " + juce::String (layer) + ".");
    }

    if (tool == "import_midi")
    {
        int newBars = 1;
        const juce::File file (args["file"].toString());
        if (! file.existsAsFile() || ! RollFiles::importMidi (pattern, layer, file, root, newBars))
            return fail ("Could not read notes from that MIDI file.");
        applyLoaded (newBars, (BassGrid) gridIndex);
        return ok ("Imported into layer " + juce::String (layer) + "; pattern length set to " + juce::String (newBars) + " bar(s).");
    }

    // export_midi
    auto file = args.hasProperty ("file") ? juce::File (args["file"].toString()) : documents.getChildFile ("Exports").getChildFile ("Kickbase BASS " + juce::String (layer + 1) + ".mid");
    file = file.withFileExtension ("mid");
    const auto bpm = args.hasProperty ("bpm") ? juce::jlimit (20.0, 300.0, (double) args["bpm"]) : (double) processor.transportStatus.bpm.load();
    if (! RollFiles::exportMidi (pattern, layer, bars, bpm, root, file))
        return fail ("Could not write " + file.getFullPathName());
    auto data = object();
    put (data, "saved", file.getFullPathName());
    put (data, "bass_root_midi_note", root);
    return ok (data);
}

var ToolLayer::loadKickSample (const var& args)
{
    const auto path = args["file"].toString().trim();
    if (path.isEmpty())
    {
        processor.clearClickSample();
        return ok ("Kick sample cleared (the synthesized click is used).");
    }
    if (! processor.loadClickSample (juce::File (path)))
        return fail ("Could not load that file as a sample (WAV, AIFF or FLAC, readable path).");
    return ok ("Loaded " + processor.getClickSampleName() + " as the kick's click layer. kickClick sets its level.");
}

var ToolLayer::undo (bool redo)
{
    auto& from = redo ? redoStack : undoStack;
    auto& to = redo ? undoStack : redoStack;
    if (from.empty())
        return fail (redo ? "Nothing to redo." : "Nothing to undo.");

    juce::MemoryBlock current;
    processor.getStateInformation (current);
    to.push_back (std::move (current));

    const auto block = std::move (from.back());
    from.pop_back();
    processor.setStateInformation (block.getData(), (int) block.getSize());
    processor.patternChanged();
    return ok (redo ? "Redone." : "Undone.");
}
