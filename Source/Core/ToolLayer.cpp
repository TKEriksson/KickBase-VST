#include "ToolLayer.h"
#include "LiveCode.h"
#include "Settings.h"
#include "AudioAnalysis.h"
#include "RollFiles.h"
#include "LibraryIndex.h"
#include "MixAnalysis.h"
#include <BinaryData.h>
#include "../Ui/Help.h"

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
            if (Presets::sectionName ((Presets::Section) s).equalsIgnoreCase (value.toString().trim())
                || ((Presets::Section) s == Presets::Section::bass && value.toString().trim().equalsIgnoreCase ("Instrument")))
            {
                section = (Presets::Section) s;
                return true;
            }
        return false;
    }

    int channelIndex (const var& value)
    {
        // "FX3", "3" (nummer 1-11) eller en gammal bokstav A-K.
        auto text = value.toString().trim().toUpperCase();
        if (text.startsWith ("FX"))
            text = text.substring (2).trim();
        if (text.length() == 1 && text[0] >= 'A' && text[0] < 'A' + numEffectChannels)
            return (int) (text[0] - 'A');
        const auto number = text.getIntValue();
        return text.containsOnly ("0123456789") && juce::isPositiveAndBelow (number - 1, numEffectChannels) ? number - 1 : -1;
    }

    // Källan i en koppling: "lfo" 1-10 (FX 1-3, kollas av anroparen), "macro" 1-4, eller "instrument" 1-10 med "source" ENV 2 / ENV 3 / VELOCITY / RANDOM /
    // AMP ENV (även "ENV 1") / FILT ENV (även "FILTER ENV"). -1 = ogiltig.
    int modulationSource (const var& route)
    {
        if (route.hasProperty ("lfo"))
            return juce::isPositiveAndBelow ((int) route["lfo"] - 1, Modulation::maxLfos) ? Modulation::lfoSource ((int) route["lfo"] - 1) : -1;
        if (route.hasProperty ("macro"))
            return juce::isPositiveAndBelow ((int) route["macro"] - 1, Modulation::numMacros) ? Modulation::firstMacro + (int) route["macro"] - 1 : -1;
        if (route.hasProperty ("instrument"))
        {
            const auto layer = (int) route["instrument"] - 1;
            if (! juce::isPositiveAndBelow (layer, maxInstruments))
                return -1;
            const auto kind = route["source"].toString().toUpperCase().removeCharacters (" ");
            const auto ampEnv = kind.startsWith ("AMP") || kind == "ENV1", filterEnv = kind.startsWith ("FILT");
            return Modulation::instrumentSource (layer, filterEnv ? Modulation::instrumentSources + 1 : ampEnv ? Modulation::instrumentSources
                                                      : kind.contains ("ENV3") ? 1 : kind.startsWith ("VEL") ? 2 : kind.startsWith ("RAND") ? 3 : 0);
        }
        return -1;
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

    juce::File previewFolder() { return Settings::rootFolder().getChildFile ("Previews"); }

    int gridIndex (const juce::String& name)
    {
        return Pattern::gridNames().indexOf (name.trim(), true);
    }
}

ToolLayer::ToolLayer (PluginBase& p) : processor (p) {}

//==============================================================================
juce::String ToolLayer::guide (const juce::String& topic)
{
    if (topic.equalsIgnoreCase ("school"))
        return juce::String::fromUTF8 (BinaryData::kickbassschool_md, BinaryData::kickbassschool_mdSize);
    if (topic.equalsIgnoreCase ("psytrance"))
        return juce::String::fromUTF8 (BinaryData::psytrancekickbassreference_md, BinaryData::psytrancekickbassreference_mdSize);
    return juce::String::fromUTF8 (BinaryData::AGENT_GUIDE_md, BinaryData::AGENT_GUIDE_mdSize);
}

std::vector<ToolLayer::ToolInfo> ToolLayer::tools()
{
    return {
        { "get_guide", "READ THIS FIRST. Returns the guide for working with Kickbase: how the synth is built, units, the pattern language, workflow and pitfalls. topic 'agent' (default) is the manual; topic 'psytrance' is the sound design reference for psytrance kick and bass; topic 'school' is the kick and bass school (how to build and judge the sound).",
          R"({"type":"object","properties":{"topic":{"type":"string","enum":["agent","psytrance","school"]}}})" },
        { "list_parameters", "Lists the synth's parameters with id, name, type (float/choice/bool), min, max, default, current value, a display text and help (what the knob does, the same text as its tooltip in the plugin). Filter with prefix (e.g. 'kick', 'bass1', 'fxA', 'sc', 'lfo1', 'master', 'mix') or contains. Effect slot knobs are not listed here, see list_effects and set_effect.",
          R"({"type":"object","properties":{"prefix":{"type":"string"},"contains":{"type":"string"},"limit":{"type":"integer"}}})" },
        { "list_effects", "Lists the 13 effect types and their knobs (name, range, default, unit, choices) for use with set_effect.", R"({"type":"object","properties":{}})" },
        { "get_state", "Reads the current sound: parameters that differ from their defaults (or all), the effect chains, the pattern (notes per bass layer and kick steps), the sidechain curve, the kick envelopes and the LFO routes.",
          R"({"type":"object","properties":{"all_parameters":{"type":"boolean"}}})" },
        { "set_parameters", "Sets parameters in their own units (Hz, ms, dB, semitones, 0..1 for percentages). Choices accept the option name or index (e.g. \"bass1ATable\": \"Classic Saw\"), switches accept true/false. Example: {\"values\":{\"bass1Cutoff\":250,\"kickDrive\":0.3,\"bass1FilterType\":\"LP 24\"}}.",
          R"({"type":"object","properties":{"values":{"type":"object","additionalProperties":true}},"required":["values"]})" },
        { "set_effect", "Puts an effect in a slot. channel FX1-FX11 (or the old letters A-K), slot 1-5, type e.g. 'Echo', 'Reverb', 'Distortion' ('Off' clears the slot), knobs by name in real units, e.g. {\"TIME\":\"3/16\",\"FEEDBACK\":0.4}. Unmentioned knobs get the effect's defaults. Bass layers and the kick send to a channel with bassNFxChannel / bassNFxAmount (1 = A) via set_parameters. Type 'Sidechain' ducks the channel by another source: knobs SOURCE ('Kick', 'Instr 1' ... 'Instr 10'), THRESHOLD (dB), DEPTH (0-1), ATTACK (ms), RELEASE (ms). To duck instrument B under instrument A: put a Sidechain effect with SOURCE 'Instr A' in an FX channel and send B 100 % to that channel (bassBFxChannel = that channel number, bassBFxAmount = 1, so nothing of B stays dry). See the guide (get_guide) section 9 for recipes.",
          R"({"type":"object","properties":{"channel":{"type":"string"},"slot":{"type":"integer"},"type":{"type":"string"},"knobs":{"type":"object","additionalProperties":true}},"required":["channel","slot","type"]})" },
        { "set_kick_envelopes", "Sets the kick's free envelopes as arrays of [time_seconds, value, tension] nodes. pitch: semitones above the tail pitch (0..72, max 0.5 s). amp: volume 0..1 (max 2 s; the kick ends at the last node, which should be 0). Tension -1..1: negative falls/rises fast first, positive holds then moves late. Omit one to leave it unchanged.",
          R"({"type":"object","properties":{"pitch":{"type":"array","items":{"type":"array","items":{"type":"number"}}},"amp":{"type":"array","items":{"type":"array","items":{"type":"number"}}}}})" },
        { "set_pattern", "Writes notes. layer 0-9 = instrument 1-10 (only the first instrumentCount play, see set_instruments), or 'kick'. For an instrument give 'bars' (one text per bar; the row becomes as many bars as you give, so give every distinct bar you need, and use repeat_to to repeat them further) in the pattern language, with a 'grid' such as '1/16', '1/8 T', '1/32'. Step words separated by spaces: '.' rest, g/m/x/X = velocity ghost/soft/normal/accent, then optional pitch (+7, -5), '~' glide note, ':50' length in percent of the step. A GLIDE note (flag '~' / slide:true) works like an FL Studio slide note: it does not play a new note, it glides the note that is sounding to its pitch during its own length and the pitch stays there until that note ends. Example 1/16 rolling bass: [\". m x x . m x x . m x x . m x x\"]. Or give 'notes': [{\"tick\":0,\"length\":24,\"pitch\":0,\"velocity\":0.8,\"slide\":false}] (96 ticks per quarter note, 384 per bar). CHORDS: add \"chord\":[3,7] to a note (up to 3 extra notes, semitones from the note, -24 to +24); the instrument plays them only when its Voices parameter is 2 or more (at most Voices - 1 extra notes), and get_state shows them as a sixth element of the note array. For 'kick' give 'bars' of 16 characters 'x' or '.', e.g. [\"x...x...x...x...\"]. ROW LENGTH: a row is as long as its notes (length 'auto', the default: the last bar that has a note, at least 1 bar; get_state shows bars, length_mode, content_bars). length:\"auto\" or length:N (1-1024) locks the row to N bars (for example 3 bars of notes and a bar of silence); shortening below the notes needs trim:true, otherwise you get an error saying how many notes would go. repeat_to:N repeats the row's content until it is N bars. In a locked row, writing a note beyond the end returns the error 'row is locked to N bars'. The pattern is as long as the longest row. The old patternBars and rowBars parameters are not used any more.",
          R"({"type":"object","properties":{"layer":{},"bars":{"type":"array","items":{"type":"string"}},"grid":{"type":"string"},"gate":{"type":"number"},"notes":{"type":"array","items":{"type":"object"}},"clear":{"type":"boolean"},"length":{},"trim":{"type":"boolean"},"repeat_to":{"type":"integer"}},"required":["layer"]})" },
        { "live_code_create", "Runs Strudel code (Strudel mini-notation and patterns, https://strudel.cc) and writes the result into the piano roll rows: 1 Strudel cycle = 1 bar. s('bd') or s('kick') = the kick row (16 steps per bar); s('i1')..s('i10') = instrument 1-10; note(...) or n(...) without s = instrument 1. Pitch: note names (c2 = pitch 0, the lowest bass note) or numbers (semitones from C2). gain = velocity (default 0.8). Only one note per tick for now. Example: note('c2 [e2 g2]*2').s('i1'). Rows not mentioned are left as they are. Runs in a sandbox with a 500 ms limit.",
          R"({"type":"object","properties":{"code":{"type":"string"},"cycles":{"type":"integer","minimum":1,"maximum":64,"description":"how many bars to write, default 4"}},"required":["code"]})" },
        { "set_gate", "Sets a bass layer's GATE: a separate on/off rhythm that chops the layer's sound, with its own grid, independent of the notes. layer 0-9; on (true/false), grid ('1/16' ...), depth 0..1 (1 = fully closed in the off steps), smooth_ms (edge softness), and the steps as 'bars' (one text per bar, repeated over four bars): 'x' = open, '.' = closed, e.g. [\"x.x.x.x.x.x.x.x.\"] on a 1/16 grid. all:'open' or 'closed' fills every step.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"on":{"type":"boolean"},"grid":{"type":"string"},"depth":{"type":"number"},"smooth_ms":{"type":"number"},"bars":{"type":"array","items":{"type":"string"}},"all":{"type":"string","enum":["open","closed"]}},"required":["layer"]})" },
        { "set_sidechain_curve", "Sets how the kick ducks the bass. mode 'Kick envelope' follows the kick; mode 'Curve' uses 'points' [[x,y,tension]...] where x is 0..1 across the length (1/16, 1/8, 3/16, 1/4, 1/2, or Kick) and y 0..1 is the ducking (1 = fully ducked). The depth per instrument is bass1Duck, bass2Duck ... bass10Duck.",
          R"({"type":"object","properties":{"mode":{"type":"string"},"length":{"type":"string"},"points":{"type":"array","items":{"type":"array","items":{"type":"number"}}}}})" },
        { "set_instruments", "Sets how many instruments are used (1-10; each has its own pattern, oscillators, filter, envelopes, effect send and sidechain depth; the default is 3). Removing instruments deletes the last ones completely (pattern, sound, sample, wavetables, name, mixer and modulation routes). Adding gives instruments with default settings and an empty pattern.",
          R"({"type":"object","properties":{"count":{"type":"integer","minimum":1,"maximum":10}},"required":["count"]})" },
        { "edit_instrument", "The instrument menu of the UI. action: 'rename' (give 'name'; empty = default name), 'colour' (give 'colour' as '#rrggbb', or empty for the default colour), 'duplicate' (a copy right after, the later instruments move down; needs a free instrument), 'move_up' / 'move_down' (swaps places with the neighbour), 'delete' (the later instruments move up; at least one stays). layer 0-9 = instrument 1-10. Name and colour are saved in the project (not parameters). The whole instrument moves with it: sound, pattern, mixer, FX send and modulation routes.",
          R"({"type":"object","properties":{"action":{"type":"string","enum":["rename","colour","duplicate","move_up","move_down","delete"]},"layer":{"type":"integer","minimum":0,"maximum":9},"name":{"type":"string"},"colour":{"type":"string"}},"required":["action","layer"]})" },
        { "set_modulation", "Sets up LFOs and routes (the matrix). lfos: [{\"lfo\":1,\"shape\":\"Sine\",\"rate\":\"1 bar\",\"on\":true}] (lfo 1-10 in the instrument, 1-3 in the FX plugin; setting an inactive LFO or routing from it adds it, \"remove\":true removes it with its routes; get_state lists active_lfos; shapes: Sine, Triangle, Saw Up, Saw Down, Square, Random, Custom; synced rates 1/32 ... 16 bars; \"hz\":0.02-40 runs the LFO at its own speed instead, \"mode\":\"Sync\"|\"Hz\"; \"points\":[[phase 0..1, value -1..1, tension -1..1],...] draws a custom curve, at least two points, and selects the Custom shape). routes: [{\"lfo\":1,\"param\":\"bass1Cutoff\",\"amount\":0.3}] (amount -1..1 of the knob's travel, only sound parameters: kick*, bass*, fx*). The source is \"lfo\" 1-10, or \"macro\" 1-4 (turn macro1..macro4 with set_parameters), or \"instrument\" 1-10 with \"source\":\"ENV 2\" | \"ENV 3\" | \"VELOCITY\" | \"RANDOM\" | \"AMP ENV\" | \"FILT ENV\" (ENV 2/3 are set with bassNEnv2Attack ... bassNEnv3Release; AMP ENV is the volume envelope ENV 1, FILT ENV the filter envelope's shape 0..1 without its amount, both 0 when switched off). Up to 64 routes. clear_routes removes all routes first.",
          R"({"type":"object","properties":{"lfos":{"type":"array","items":{"type":"object"}},"routes":{"type":"array","items":{"type":"object"}},"clear_routes":{"type":"boolean"}}})" },
        { "list_presets", "Lists factory and user presets. section: Full, Kick, Bass, Pattern, Effects, Sidechain, Master (the maximizer), Soundstage or LFO (default: all).",
          R"({"type":"object","properties":{"section":{"type":"string"}}})" },
        { "load_preset", "Loads a factory preset (by name or index) or a user preset (by name or file path) into a section. layer 0-9 (the instrument) for Bass. Full replaces everything.",
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
        { "import_midi", "Imports a MIDI file into a bass layer (replacing its notes; pitches are placed relative to the bass root of 'note', default 52, and moved by octaves to fit within 96 semitones) and sets the pattern length (1, 2, 4 ... 64 bars) as needed.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"file":{"type":"string"},"note":{"type":"integer"}},"required":["layer","file"]})" },
        { "analyze_audio", "Measures an audio file (WAV, AIFF, FLAC or MP3) the same way render_preview measures the synth, so you can compare a reference (a kick sample, a snippet of a track) with your sound: metrics, spectrogram image and, for kind 'kick', the kick length and tail frequency. start/seconds select a part (default: the first 20 s). Use bpm for the beat lines.",
          R"({"type":"object","properties":{"file":{"type":"string"},"kind":{"type":"string","enum":["kick","music"]},"start":{"type":"number"},"seconds":{"type":"number"},"bpm":{"type":"number"}},"required":["file"]})" },
        { "load_bass_sample", "Loads a WAV/AIFF/FLAC file (max 12 s, mono-summed) as an instrument's sample source: it plays alongside the oscillators (through the filter and envelope), scaled by bassNSampleLevel (set here with 'level', default 1); it switches bassNSampleOn on. layer 0-9. key_track true makes it follow the notes (root_note = the MIDI note the sample was recorded at, C5 = 60), loop repeats it while the note is held, start 0..1. Empty or missing file clears the sample. Turn the oscillators off (bassNAOn, bassNBOn, bassNCOn = false) and bassNSub/bassNNoise down to hear the sample alone.",
          R"({"type":"object","properties":{"layer":{"type":"integer"},"file":{"type":"string"},"level":{"type":"number"},"key_track":{"type":"boolean"},"root_note":{"type":"integer"},"loop":{"type":"boolean"},"start":{"type":"number"}},"required":["layer"]})" },
        { "load_kick_sample", "Loads a WAV/AIFF/FLAC file (max 2 s, mono-summed) as the kick's sample. kickSampleMode (Click | Layer | Replace) decides how it is used: Click = the first moments of every kick on top of the synthesized body, scaled by kickClick; Layer = the whole sample on top of the synthesized kick (kickSampleLevel); Replace = the sample is the kick (the synthesized body is silent). kickSampleTune plays it faster or slower. Empty or missing file clears it.",
          R"({"type":"object","properties":{"file":{"type":"string"}}})" },
        { "list_library", "Lists the sounds in the user's library (Documents\\Kickbase\\Library: kicks, bass sounds, synth sounds, pads and loops from free packs and the user's own recordings). Each item has id, name, category, package, tags, license, author and, if known, bpm and root_note. PERSONAL items are the user's own sounds (personal: true): they may be used but are never included in anything that is shared. Filter with category (Kicks, Bass, Synth, Pads, BassLoops, SynthLoops, Multi) and search (all words must match name, package, tags, author or license). Packages with a broken manifest are not listed; their problems are returned.",
          R"({"type":"object","properties":{"category":{"type":"string"},"search":{"type":"string"},"limit":{"type":"integer"}}})" },
        { "load_library_kick", "Loads a kick from the library (a Kicks item from list_library, by id or by exact name) as the kick's sample, like clicking it in the plugin's KICK > SAMPLE view. If kickSampleMode is Click it is switched to Replace (the sample becomes the kick), as in the plugin; pass mode (click, layer or replace) to choose another. Undo restores the previous sample and mode.",
          R"({"type":"object","properties":{"id":{"type":"string"},"name":{"type":"string"},"mode":{"type":"string","enum":["click","layer","replace"]}}})" },
        { "analyse_mix", "STARTS an analysis of the mix and returns at once (it runs in the background, a few seconds up to half a minute depending on the project; the plugin keeps working). It plays the pattern through once offline (all bars of the pattern, at most 16) and analyses how the kick, each instrument and the master share the frequencies, bar by bar. Then call get_mix_report (repeat while it says status running) to get the findings. Findings have severity (problem, warning, info), the sources involved, the frequency range and the bars where it happens: collisions between sources at the same loudness and frequency, an instrument covering the kick, a kick tail tuned too low or high, instruments that are out of tune with each other or the kick (octaves and unison count as in tune), rumble under 30 Hz, a master limiter that works too hard, and (with reference_file) where the master has more or less energy than a reference track. Measured on the real signals after ducking and EQ, so a collision the sidechain already solved does not show. Kick and instrument sharing low frequencies is only a warning because that is normal when the sidechain gives the kick the hit. sensitivity: low (clear problems only), normal (default) or high (smaller overlaps too). Use it after a change to check that you fixed what you meant to; the user sees the same report in the ANALYZER tab. The analysis uses the pattern and sound as they are when you start it: do not change anything until the report is done.",
          R"({"type":"object","properties":{"sensitivity":{"type":"string","enum":["low","normal","high"]},"bpm":{"type":"number"},"note":{"type":"integer"},"max_bars":{"type":"integer"},"reference_file":{"type":"string"}}})" },
        { "get_mix_report", "Returns the result of analyse_mix: {status:'running', progress} while it works (call again in a few seconds), or the report when it is done (status 'done'). By default the findings and a short overview. For MORE DATA pass detail:'bars' (adds, for every source and every bar of the loop: the level in each frequency band, the fundamental and the harmonic series, plus the kick hits and how much each instrument is ducked at them), or choose exactly what you want with include (any of: findings, sources = overview per source, bars = band levels per source and bar, harmonics = fundamental and harmonic series per source and bar, spectra = 1/12-octave spectrum per source and bar, dynamics = rms/peak/crest/brightness/energy in six frequency ranges per source and bar, kicks = every kick hit with the ducking, plus the field lowband = the kick/bass phase measurements (correlation per 16th at 30-150 Hz or null where one of them is silent, sum_diff_db, sum_vs_max_db, kick_hits with peak_db/polarity/end_db/click, kick_peak_spread_db, click_hits, stereo_per_bar), presets, overview = ALL settings arranged by section (kick, every instrument with oscillators A/B/C and wavetables, filter, envelopes, EQ, sends; the effect channels with their slots; master, mixer, modulation), synth = the same settings as the raw flat parameter list plus the pattern notes (also gives pattern_timeline = the notes by bar and beat), about = an explanation for a reader who knows nothing about Kickbase). To keep the answer small, limit it: sources (parts of names, e.g. ['kick','instr 2']), bars (e.g. [3,4] or '1-4'), min_hz and max_hz (a frequency range: band arrays then only hold those bands, see meta.bands_included). The data is large with many sources and bars, so ask for what you need. For everything including every frame, use save_mix_report (a file). wait_seconds (0-60) lets the call wait for the result when this is the headless instance; against the live plugin it never waits (so the link stays responsive).",
          R"({"type":"object","properties":{"detail":{"type":"string","enum":["summary","bars"]},"include":{"type":"array","items":{"type":"string"}},"sources":{"type":"array","items":{"type":"string"}},"bars":{"type":["array","string"],"items":{"type":"integer"}},"min_hz":{"type":"number"},"max_hz":{"type":"number"},"wait_seconds":{"type":"number"}}})" },
        { "save_mix_report", "Saves the finished analysis as a JSON file with ALL the data (so it can be read by hand or by another AI): findings, an overview per source, band levels, the fundamental and harmonic series and a 1/12-octave spectrum for every source and every bar, the kick hits with the ducking, the level of every frequency band in every frame through the whole loop (about every 43 ms), and all synth settings (every parameter, wavetables, effects, modulation, pattern and notes). include limits what is saved (same names as get_mix_report, plus frames = the level of every band in every frame = the spectrogram, with rms/peak/brightness per frame and the ducking per frame; default everything), and sources, bars, min_hz, max_hz and frame_step (every n-th frame) limit it further (e.g. only the kick and Instr 1, bars 3-4, 30-300 Hz) to keep the file small. file is optional (default Documents\\Kickbase\\Analysis\\kickbase-analysis-<time>.json). Returns the path.",
          R"({"type":"object","properties":{"file":{"type":"string"},"include":{"type":"array","items":{"type":"string"}},"sources":{"type":"array","items":{"type":"string"}},"bars":{"type":["array","string"],"items":{"type":"integer"}},"min_hz":{"type":"number"},"max_hz":{"type":"number"},"frame_step":{"type":"integer"}}})" },
        { "add_report_note", "Writes a note in the report box of the plugin's ANALYZER tab, under the findings, where the user reads it: your suggested FIX for a finding, or what you changed. title is short (e.g. 'Duck Instr 2 under Instr 1'), text explains what to do or what you did (parameters and values), finding is the 1-based number of the finding it answers (the findings are listed in order in the report; 0 = general), applied:true if you have already made the change (shown as APPLIED), otherwise it is a SUGGESTION. Notes are cleared when a new analysis starts. Use this after analyse_mix so the user can see in the plugin what you found and fixed.",
          R"({"type":"object","properties":{"title":{"type":"string"},"text":{"type":"string"},"finding":{"type":"integer"},"applied":{"type":"boolean"}},"required":["title","text"]})" },
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
    if (tool == "list_library")     return listLibrary (args);
    if (tool == "render_preview")   return renderPreview (args);
    if (tool == "analyse_mix")      return analyseMix (args);
    if (tool == "get_mix_report")   return getMixReport (args);
    if (tool == "save_mix_report")  return saveMixReport (args);
    if (tool == "add_report_note")  return addReportNote (args);
    if (tool == "analyze_audio")    return analyzeAudio (args);
    if (tool == "export_midi" || tool == "save_piano_roll")
        return pianoRollFiles (tool, args);
    if (tool == "undo")             return undo (false);
    if (tool == "redo")             return undo (true);

    // Resten ändrar ljudet: ett ångra-steg först.
    if (tool == "set_instruments")
    {
        snapshot();
        const auto before = processor.activeInstruments();
        processor.setInstrumentCount ((int) args["count"]);
        auto data = object();
        put (data, "instruments", processor.activeInstruments());
        put (data, "removed", std::max (0, before - processor.activeInstruments()));
        return ok (data);
    }

    if (tool == "edit_instrument")
    {
        const auto action = args["action"].toString();
        const int layer = (int) args["layer"];
        const auto count = processor.activeInstruments();
        if (! juce::isPositiveAndBelow (layer, count))
            return fail ("layer must be 0-" + juce::String (count - 1) + " (only the first instrumentCount instruments exist).");
        snapshot();
        bool done = true;
        if (action == "rename")         processor.setLayerName (layer, args["name"].toString().trim());
        else if (action == "colour")
        {
            const auto text = args["colour"].toString().trim().trimCharactersAtStart ("#");
            if (text.isNotEmpty() && (text.length() != 6 || ! text.containsOnly ("0123456789abcdefABCDEF")))
                return fail ("colour must be '#rrggbb' or empty.");
            processor.setLayerColour (layer, text.isEmpty() ? 0 : (juce::uint32) (0xff000000 | (juce::uint32) text.getHexValue32()));
        }
        else if (action == "duplicate") done = processor.duplicateTrack (layer);
        else if (action == "move_up")   done = processor.moveTrack (layer, -1);
        else if (action == "move_down") done = processor.moveTrack (layer, 1);
        else if (action == "delete")    done = processor.deleteTrack (layer);
        else return fail ("action must be rename, colour, duplicate, move_up, move_down or delete.");
        if (! done)
            return fail ("Not possible here (duplicate needs a free instrument, move needs a neighbour, delete keeps at least one).");
        processor.markStateChanged();
        auto data = object();
        put (data, "instruments", processor.activeInstruments());
        return ok (data);
    }

    if (tool == "set_parameters" || tool == "set_effect" || tool == "set_kick_envelopes" || tool == "set_pattern" || tool == "set_sidechain_curve" || tool == "set_gate" || tool == "live_code_create"
        || tool == "set_modulation" || tool == "load_preset" || tool == "reset" || tool == "load_kick_sample" || tool == "load_library_kick" || tool == "load_bass_sample" || tool == "load_piano_roll" || tool == "import_midi")
        snapshot();

    if (tool == "set_parameters")      return setParameters (args);
    if (tool == "set_effect")          return setEffect (args);
    if (tool == "set_kick_envelopes")  return setKickEnvelopes (args);
    if (tool == "set_pattern")         return setPattern (args);
    if (tool == "set_sidechain_curve") return setSidechainCurve (args);
    if (tool == "set_gate")            return setGate (args);
    if (tool == "live_code_create")    return liveCodeCreate (args);
    if (tool == "set_modulation")      return setModulation (args);
    if (tool == "load_preset")         return loadPreset (args);
    if (tool == "load_kick_sample")    return loadKickSample (args);
    if (tool == "load_library_kick")   return loadLibraryKick (args);
    if (tool == "load_bass_sample")    return loadInstrumentSample (args);
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
        if (const auto help = Help::forParameter (id); help.isNotEmpty())
            put (entry, "help", help); // samma förklaring som rattens tooltip i pluginet
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
            put (entry, "help", Help::forEffectKnob (knob.name, Effects::typeNames()[t]));
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
            put (slot, "channel", Effects::channelLabel (c));
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
    const auto lengths = Params::readPatternLengths (processor.apvts, processor.pattern, processor.activeInstruments());
    put (pattern, "bars", lengths.total); // mönstret är lika långt som den längsta raden
    juce::Array<var> layers;
    for (int layer = 0; layer < Pattern::numNoteLayers; ++layer)
    {
        const auto isKick = layer == Pattern::kickLayer;
        juce::Array<var> notes;
        processor.pattern.forEachNote (layer, [&] (int tick, const Pattern::Note& n)
        {
            juce::Array<var> entry { tick, n.length.load(), n.pitch.load(), round4 (n.velocity.load()), n.slide.load() };
            if (n.hasChord()) // fas P: ett sjätte fält med ackordets intervall, bara när tonen har ett ackord
            {
                juce::Array<var> chord;
                for (const auto interval : n.getChord())
                    if (interval != 0)
                        chord.add (interval);
                entry.add (var (chord));
            }
            notes.add (var (entry));
        });
        auto entry = object();
        put (entry, "layer", isKick ? juce::var ("kick") : juce::var (layer));
        if (! isKick)
            put (entry, "name", processor.getLayerName (layer));
        put (entry, "grid", Pattern::gridNames()[isKick ? (int) processor.apvts.getRawParameterValue (ParamID::rowGridKick)->load()
                                                         : (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::grid))->load()]);
        put (entry, "notes [tick, length, pitch, velocity, glide]", notes);
        put (entry, "piano_roll_layer", processor.pattern.isCustom (layer));
        put (entry, "bars", processor.pattern.lengthBars (layer));
        put (entry, "length_mode", processor.pattern.isLengthAuto (layer) ? juce::var ("auto") : juce::var ("locked")); // auto: följer tonerna, locked: ett valt antal takter
        put (entry, "content_bars", processor.pattern.contentBars (layer));
        put (entry, "extra_bars", processor.pattern.extraBars (layer)); // takter som "+ BAR" lagt till efter innehållet
        if (isKick)
        {
            layers.add (entry);
            continue; // kicken har ingen gate
        }

        auto gateInfo = object();
        const auto gateGrid = juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateGrid))->load());
        put (gateInfo, "on", processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateOn))->load() >= 0.5f);
        put (gateInfo, "grid", Pattern::gridNames()[gateGrid]);
        put (gateInfo, "depth", round4 (processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateDepth))->load()));
        juce::String steps;
        const auto barCount = lengths.total;
        for (int i = 0; i < barCount * Pattern::instrumentStepsPerBar ((InstrumentGrid) gateGrid); ++i)
            steps << (processor.pattern.isGateStepOn (layer, i) ? 'x' : '.');
        put (gateInfo, "steps (x open, . closed)", steps);
        put (entry, "gate", gateInfo);
        layers.add (entry);
    }
    put (pattern, "instrument_count", processor.activeInstruments());
    put (pattern, "layers (instruments 0-9 and 'kick')", layers);
    put (data, "pattern", pattern);

    // Aktiva LFO:er (1-10, FX 1-3) och deras egna kurvor (form Custom): [fas 0..1, värde -1..1, spänning].
    {
        juce::Array<var> active, curves;
        for (int lfo = 0; lfo < processor.availableLfos(); ++lfo)
            if (processor.isLfoActive (lfo))
                active.add (lfo + 1);
        put (data, "active_lfos", active);
        for (int lfo = 0; lfo < processor.availableLfos(); ++lfo)
        {
            if (lfo >= Modulation::numLfos && ! processor.isLfoActive (lfo))
                continue;
            juce::Array<var> points;
            for (const auto& point : processor.lfoCurves[(size_t) lfo].getPoints())
                points.add (var (juce::Array<var> { round4 (point.x), round4 (1.0f - 2.0f * point.y), round4 (point.tension) }));
            auto entry = object();
            put (entry, "lfo", lfo + 1);
            put (entry, "points [phase, value, tension]", points);
            curves.add (entry);
        }
        put (data, "lfo_custom_curves", curves);
    }

    // Samplingar (kickens och instrumentens) och egna wavetables: namnen (själva ljuddatan sparas inte).
    {
        auto samples = object();
        if (processor.getClickSampleName().isNotEmpty())
            put (samples, "kick", processor.getClickSampleName());
        auto userWavetables = object();
        for (int layer = 0; layer < processor.activeInstruments(); ++layer)
        {
            if (processor.getInstrumentSampleName (layer).isNotEmpty())
                put (samples, processor.getLayerName (layer), processor.getInstrumentSampleName (layer));
            for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
                if (processor.getUserTableName (layer, osc).isNotEmpty())
                    put (userWavetables, processor.getLayerName (layer) + " osc " + juce::String ("ABC").substring (osc, osc + 1), processor.getUserTableName (layer, osc));
        }
        put (data, "samples_loaded", samples);
        put (data, "user_wavetables_loaded", userWavetables);
    }

    // Vilka presets som är laddade (namn; en ändrad inställning efter inladdningen syns inte här).
    {
        auto presets = object();
        for (int section = 0; section < Presets::numSections; ++section)
        {
            const auto sectionId = (Presets::Section) section;
            if (sectionId == Presets::Section::bass)
                continue;
            put (presets, Presets::sectionName (sectionId), processor.getPresetName (sectionId));
        }
        juce::Array<var> instrumentPresets;
        for (int layer = 0; layer < processor.activeInstruments(); ++layer)
        {
            auto entry = object();
            put (entry, "instrument", processor.getLayerName (layer));
            put (entry, "preset", processor.getPresetName (Presets::Section::bass, layer));
            instrumentPresets.add (entry);
        }
        put (presets, "Instruments", instrumentPresets);
        put (data, "presets", presets);
    }

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
        if (Modulation::isLfoSource (source))
            put (entry, "lfo", Modulation::lfoIndexOfSource (source) + 1);
        else
            put (entry, "source", Modulation::sourceName (source));
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
        return fail ("channel must be FX1-FX11 (or A-K) and slot 1-5.");

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
    put (data, "slot", Effects::channelLabel (channel) + "." + juce::String (slot + 1) + " = " + Effects::typeNames()[typeIndex]);
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

    const auto isKick = layerArg.toString().equalsIgnoreCase ("kick");
    const auto layer = isKick ? Pattern::kickLayer : (int) layerArg;
    if (! juce::isPositiveAndBelow (layer, Pattern::numNoteLayers))
        return fail ("layer must be 0-9 or 'kick'.");

    const auto gridParameterID = isKick ? juce::String (ParamID::rowGridKick) : ParamID::bass (layer, InstrumentField::grid);
    auto grid = (int) processor.apvts.getRawParameterValue (gridParameterID)->load();
    if (args.hasProperty ("grid"))
    {
        grid = gridIndex (args["grid"].toString());
        if (grid < 0)
            return fail ("Unknown grid. Options: " + Pattern::gridNames().joinIntoString (" | "));
    }

    // Radens längd: "length" = "auto" eller 1-1024 takter, "trim" tillåter att toner efter slutet tas bort, "repeat_to" upprepar innehållet till så många takter.
    auto wantedLength = -1; // -1 = oförändrad, 0 = AUTO
    if (args.hasProperty ("length"))
    {
        const auto text = args["length"].toString().trim();
        if (text.equalsIgnoreCase ("auto"))
            wantedLength = 0;
        else if (text.containsOnly ("0123456789") && text.getIntValue() >= 1 && text.getIntValue() <= Pattern::maxBars)
            wantedLength = text.getIntValue();
        else
            return fail ("length must be \"auto\" or a number of bars from 1 to " + juce::String (Pattern::maxBars) + ".");
    }
    const auto repeatTo = args.hasProperty ("repeat_to") ? (int) args["repeat_to"] : 0;
    if (args.hasProperty ("repeat_to") && (repeatTo < 1 || repeatTo > Pattern::maxBars))
        return fail ("repeat_to must be a number of bars from 1 to " + juce::String (Pattern::maxBars) + ".");

    // Gränsen skrivningen måste hålla sig inom: den nya längden, annars en låst rads längd, annars ingen (AUTO växer).
    const auto limitBars = wantedLength > 0 ? wantedLength : (wantedLength < 0 && ! pattern.isLengthAuto (layer) ? pattern.manualBars (layer) : 0);
    auto lockedError = [&] { return fail ("row is locked to " + juce::String (limitBars) + (limitBars == 1 ? " bar" : " bars") + ". Use length:\"auto\" or a larger number first."); };
    if (limitBars > 0)
    {
        if (repeatTo > limitBars)
            return lockedError();
        if (auto* notes = args["notes"].getArray())
            for (const auto& note : *notes)
                if ((int) note["tick"] >= limitBars * Pattern::ticksPerBar && (int) note["tick"] < Pattern::maxTicks)
                    return lockedError();
        if (auto* bars = args["bars"].getArray(); bars != nullptr && (int) bars->size() > limitBars)
            return lockedError();
        if (! args.hasProperty ("notes") && ! args.hasProperty ("bars") && ! (bool) args["clear"] && wantedLength > 0)
        {
            auto lost = 0;
            pattern.forEachNote (layer, [&] (int tick, const Pattern::Note&) { lost += tick >= limitBars * Pattern::ticksPerBar ? 1 : 0; });
            if (lost > 0 && ! (bool) args["trim"])
                return fail ("length " + juce::String (limitBars) + " would remove " + juce::String (lost) + (lost == 1 ? " note" : " notes") + " after the end. Nothing was changed. Give trim:true to remove them.");
        }
    }

    if (auto* notes = args["notes"].getArray())
    {
        pattern.clearNotes (layer);
        for (const auto& note : *notes)
        {
            const auto tick = (int) note["tick"];
            if (! juce::isPositiveAndBelow (tick, Pattern::maxTicks))
                continue;
            auto& added = pattern.addNote (layer, tick, note.hasProperty ("length") ? (int) note["length"] : Pattern::ticksPerStep ((InstrumentGrid) grid),
                                           (int) note["pitch"], note.hasProperty ("velocity") ? (float) (double) note["velocity"] : Pattern::defaultVelocity (layer), (bool) note["slide"]);
            if (const auto* chord = note["chord"].getArray(); chord != nullptr && ! isKick)
            {
                Pattern::Chord intervals {};
                for (int i = 0; i < std::min (chord->size(), Pattern::maxChordNotes); ++i)
                    intervals[(size_t) i] = (int) (*chord)[i];
                added.setChord (intervals);
            }
        }
        pattern.setCustom (layer, true);
    }
    else if (auto* bars = args["bars"].getArray())
    {
        Presets::Extras::PatternLine line;
        line.layer = layer;
        line.grid = (InstrumentGrid) grid;
        line.gate = args.hasProperty ("gate") ? (float) (double) args["gate"] : 0.7f;
        for (const auto& bar : *bars)
        {
            // Äldre kickstil: 16 tecken 'x' och '.' utan mellanslag, ett steg per tecken.
            auto text = bar.toString();
            if (isKick && ! text.containsAnyOf (" |") && text.containsOnly ("xX."))
            {
                juce::StringArray characters;
                for (auto c : text)
                    characters.add (juce::String::charToString (c));
                text = characters.joinIntoString (" ");
            }
            line.bars.push_back (text.toStdString());
        }
        Presets::writePatternLine (pattern, line);
    }
    else if ((bool) args["clear"])
        pattern.clearNotes (layer);
    else if (wantedLength < 0 && repeatTo == 0)
        return fail ("Give 'bars' (pattern language), 'notes', clear:true, length or repeat_to.");

    if (repeatTo > 0)
    {
        pattern.refreshLengths();
        const auto sourceBars = std::max (1, pattern.contentBars (layer));
        struct Copy { int tick, length, pitch; float velocity; bool slide; Pattern::Chord chord; };
        std::vector<Copy> copies;
        pattern.forEachNote (layer, [&] (int tick, const Pattern::Note& n) { copies.push_back ({ tick, n.length.load(), n.pitch.load(), n.velocity.load(), n.slide.load(), n.getChord() }); });
        for (int bar = sourceBars; bar < repeatTo; bar += sourceBars)
            for (const auto& c : copies)
            {
                const auto tick = c.tick + bar * Pattern::ticksPerBar;
                if (tick < repeatTo * Pattern::ticksPerBar)
                    pattern.addNote (layer, tick, std::min (c.length, repeatTo * Pattern::ticksPerBar - tick), c.pitch, c.velocity, c.slide).setChord (c.chord);
            }
        pattern.setCustom (layer, true);
    }

    if (wantedLength == 0)
        pattern.setManualBars (layer, 0);
    else if (wantedLength > 0)
    {
        pattern.trimRow (layer, wantedLength);
        pattern.setManualBars (layer, wantedLength);
    }

    if (auto* gridParameter = findParameter (processor, gridParameterID))
        Presets::setParameter (*gridParameter, gridParameter->convertTo0to1 ((float) grid));
    processor.patternChanged();

    auto count = 0;
    pattern.forEachNote (layer, [&count] (int, const Pattern::Note&) { ++count; });
    auto data = object();
    put (data, "layer", isKick ? juce::var ("kick") : juce::var (layer));
    put (data, "notes", count);
    put (data, "grid", Pattern::gridNames()[grid]);
    put (data, "bars", pattern.lengthBars (layer));
    put (data, "length_mode", pattern.isLengthAuto (layer) ? juce::var ("auto") : juce::var ("locked"));
    return ok (data);
}

var ToolLayer::liveCodeCreate (const var& args)
{
    const auto code = args["code"].toString();
    if (code.trim().isEmpty()) return fail ("code is empty");
    const int cycles = juce::jlimit (1, 64, args.hasProperty ("cycles") ? (int) args["cycles"] : 4);

    LiveCode live;
    juce::Result res = juce::Result::ok();
    auto list = live.create (code, cycles, res);
    if (res.failed()) return fail ("Strudel: " + res.getErrorMessage());

    var written (juce::Array<var>{});
    if (auto* a = list.getArray())
        for (auto& item : *a)
        {
            auto r = setPattern (item);
            if (! (bool) r["ok"]) return r;
            written.append (item["layer"]);
        }
    auto data = object();
    put (data, "layers", written);
    put (data, "cycles", cycles);
    return ok (data);
}

var ToolLayer::setGate (const var& args)
{
    const auto layer = (int) args["layer"];
    if (! juce::isPositiveAndBelow (layer, Pattern::numLayers))
        return fail ("layer must be 0-9.");

    juce::String problem;
    for (const auto& [key, field] : { std::pair<const char*, InstrumentField> { "on", InstrumentField::gateOn }, { "grid", InstrumentField::gateGrid },
                                      { "depth", InstrumentField::gateDepth }, { "smooth_ms", InstrumentField::gateSmooth } })
        if (args.hasProperty (key))
        {
            auto* parameter = findParameter (processor, ParamID::bass (layer, field));
            if (parameter == nullptr || ! assign (*parameter, args[key], problem))
                return fail (problem.isEmpty() ? juce::String ("No such gate parameter.") : problem);
        }

    const auto gridIndexNow = juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gateGrid))->load());
    const auto stepsPerBar = Pattern::instrumentStepsPerBar ((InstrumentGrid) gridIndexNow);

    if (auto* bars = args["bars"].getArray(); bars != nullptr && ! bars->isEmpty())
    {
        for (int bar = 0; bar < bars->size(); ++bar)
        {
            const auto text = (*bars)[bar].toString().retainCharacters ("xX.");
            for (int step = 0; step < stepsPerBar; ++step)
                processor.pattern.setGateStep (layer, bar * stepsPerBar + step, step < text.length() ? text[step] != '.' : true);
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
            if (! juce::isPositiveAndBelow (index, processor.availableLfos()))
            {
                problems.add ("lfo must be 1-" + juce::String (processor.availableLfos()));
                continue;
            }
            // "remove": true tar bort LFO:n (dess kopplingar försvinner och den återställs). Annars aktiveras en inaktiv LFO.
            if ((bool) lfo["remove"])
            {
                processor.setLfoActive (index, false);
                continue;
            }
            processor.setLfoActive (index, true);
            // Egen kurva: "points" = [[fas 0..1, värde -1..1, böjning -1..1] ...] (minst två punkter). Formen blir Custom.
            if (auto* pointList = lfo["points"].getArray())
            {
                std::vector<NodeEnvelope::Point> points;
                for (const auto& item : *pointList)
                    if (auto* triple = item.getArray(); triple != nullptr && triple->size() >= 2)
                        points.push_back ({ juce::jlimit (0.0f, 1.0f, (float) (double) (*triple)[0]), 0.5f - 0.5f * juce::jlimit (-1.0f, 1.0f, (float) (double) (*triple)[1]),
                                            triple->size() > 2 ? juce::jlimit (-1.0f, 1.0f, (float) (double) (*triple)[2]) : 0.0f });
                if (points.size() < 2)
                    problems.add ("An LFO curve needs at least two points [phase, value, tension]");
                else
                {
                    processor.lfoCurves[(size_t) index].setPoints (std::move (points));
                    if (auto* shapeParameter = findParameter (processor, Modulation::paramID (index, "Shape")))
                        Presets::setParameter (*shapeParameter, shapeParameter->convertTo0to1 (6.0f));
                }
            }
            // Fri hastighet: "hz" sätter hastigheten och växlar till Hz.
            if (lfo.hasProperty ("hz"))
            {
                if (auto* hzParameter = findParameter (processor, Modulation::paramID (index, "Hz")))
                {
                    Presets::setParameter (*hzParameter, hzParameter->convertTo0to1 ((float) (double) lfo["hz"]));
                    if (auto* modeParameter = findParameter (processor, Modulation::paramID (index, "Mode")))
                        Presets::setParameter (*modeParameter, modeParameter->convertTo0to1 (1.0f));
                }
            }

            for (const auto& [key, suffix] : { std::pair<const char*, const char*> { "shape", "Shape" }, { "rate", "Rate" }, { "mode", "Mode" }, { "on", "On" } })
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
            const auto source = modulationSource (route);
            const auto id = route["param"].toString();
            const auto index = parameterIndex (id);
            auto* parameter = findParameter (processor, id);
            const auto lfo = Modulation::lfoIndexOfSource (source);
            if (lfo >= processor.availableLfos())
                problems.add ("lfo must be 1-" + juce::String (processor.availableLfos()));
            else if (source < 0 || parameter == nullptr || index < 0)
                problems.add ("Bad route: a source (lfo 1-10, macro 1-4, or instrument 1-10 with source ENV 2 / ENV 3 / VELOCITY / RANDOM / AMP ENV / FILT ENV) and a parameter id are needed (" + id + ")");
            else if (! Modulation::isModulatable (*parameter))
                problems.add (id + " can't be modulated (only kick*, bass*, fx* sound parameters)");
            else if (processor.modRoutes.add (source, index, juce::jlimit (-1.0f, 1.0f, (float) (double) route["amount"])) < 0)
                problems.add ("All " + juce::String (Modulation::maxRoutes) + " routes are in use");
            else if (lfo >= 0)
                processor.setLfoActive (lfo, true); // en koppling från en inaktiv LFO aktiverar den
        }

    processor.markStateChanged();
    auto data = object();
    data.getDynamicObject()->setProperty ("note", "Modulation updated.");
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

    auto loaded = false;
    if (file.existsAsFile())
    {
        if (section == Presets::Section::bass || section == Presets::Section::kick)
            loaded = processor.loadInstrumentPreset (section, layer, file).ok; // en länkad effekt hamnar på första lediga FX-kanal
        else if (section == Presets::Section::pattern)
            loaded = processor.loadPatternPreset (layer, file, false).ok;      // mönstret, inte ett länkat instrument
        else
            loaded = processor.loadUserPreset (section, file, layer);
    }
    if (loaded)
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

var ToolLayer::analyseMix (const var& args)
{
    if (processor.isEffectVersion())
        return fail ("analyse_mix works in the instrument version of Kickbase (the FX version has no instruments to compare).");

    auto& analyser = processor.getMixAnalyser();
    if (analyser.isRunning())
    {
        auto data = object();
        put (data, "status", "running");
        put (data, "progress", round4 (analyser.getProgress()));
        put (data, "next", "An analysis is already running. Call get_mix_report.");
        return ok (data);
    }

    MixAnalysis::Options options;
    const auto sensitivity = args["sensitivity"].toString().toLowerCase();
    options.sensitivity = sensitivity == "low" ? 0 : sensitivity == "high" ? 2 : 1;
    if (args.hasProperty ("max_bars"))
        options.maxBars = juce::jlimit (1, 32, (int) args["max_bars"]);

    std::shared_ptr<const MixAnalysis::Reference> reference;
    if (const auto path = args["reference_file"].toString(); path.isNotEmpty())
    {
        juce::String error;
        reference = MixAnalysis::loadReference (juce::File (path), error);
        if (reference == nullptr)
            return fail (error);
    }

    // Alla inställningar när analysen startar följer med i rapporten (oscillatorer, wavetables, effekter, modulering, mönster, toner).
    auto stateArgs = object();
    put (stateArgs, "all_parameters", true);
    const auto state = getState (stateArgs);
    processor.clearAiNotes();
    if (! analyser.start (processor, options, reference, state["data"]))
        return fail ("Could not start the analysis.");

    auto data = object();
    put (data, "status", "started");
    put (data, "next", "The analysis runs in the background. Call get_mix_report (repeat while status is running).");
    return ok (data);
}

namespace
{
    MixAnalysis::ExportFilter filterFrom (const var& args)
    {
        MixAnalysis::ExportFilter filter;
        if (auto* list = args["sources"].getArray())
            for (const auto& item : *list)
                filter.sources.add (item.toString());
        else if (args["sources"].isString())
            filter.sources.add (args["sources"].toString());
        filter.bars = MixAnalysis::ExportFilter::parseBars (args["bars"]);
        if (args.hasProperty ("min_hz"))
            filter.minHz = (float) (double) args["min_hz"];
        if (args.hasProperty ("max_hz"))
            filter.maxHz = (float) (double) args["max_hz"];
        if (args.hasProperty ("frame_step"))
            filter.frameStep = juce::jlimit (1, 64, (int) args["frame_step"]);
        return filter;
    }

    juce::StringArray includeFrom (const var& args)
    {
        juce::StringArray include;
        if (auto* list = args["include"].getArray())
            for (const auto& item : *list)
                include.add (item.toString());
        return include;
    }
}

var ToolLayer::getMixReport (const var& args)
{
    auto& analyser = processor.getMixAnalyser();
    if (! analyser.isRunning() && ! analyser.hasReport())
        return fail ("No analysis yet. Call analyse_mix first.");

    // Mot den fristående instansen (ett annat tråd än huvudtråden) kan anropet vänta; mot pluginet aldrig (huvudtråden svarar på länkens ping).
    const auto wait = juce::jlimit (0.0, 60.0, (double) args["wait_seconds"]);
    if (wait > 0.0 && ! juce::MessageManager::getInstance()->isThisTheMessageThread())
        for (auto waited = 0.0; analyser.isRunning() && waited < wait; waited += 0.1)
            juce::Thread::sleep (100);

    if (analyser.isRunning())
    {
        auto data = object();
        put (data, "status", "running");
        put (data, "progress", round4 (analyser.getProgress()));
        put (data, "next", "Still working. Call get_mix_report again in a few seconds.");
        return ok (data);
    }

    auto report = analyser.getReport();
    if (! report.valid)
        return fail (report.error);

    // Mer data: en egen lista (include), eller detail:'bars'. Filtren (sources, bars, min_hz, max_hz) begränsar det som tas med.
    const auto include = includeFrom (args);
    if (! include.isEmpty() || args["detail"].toString().equalsIgnoreCase ("bars"))
    {
        auto options = include.isEmpty() ? MixAnalysis::ExportOptions::fromNames ({ "findings", "sources", "bars", "harmonics", "kicks", "dynamics" })
                                         : MixAnalysis::ExportOptions::fromNames (include, false);
        options.frames = false; // varje ram är för mycket för ett verktygssvar: se save_mix_report
        report.aiNotes = processor.getAiNotesVar();
        auto data = report.toExportVar (options, filterFrom (args));
        put (data, "status", "done");
        return ok (data);
    }

    auto data = report.toVar();
    put (data, "status", "done");
    put (data, "more_data", "Pass detail:'bars' or include:[...] (with sources, bars, min_hz, max_hz to limit it) for band levels, harmonics, dynamics and the kick hits per bar, or call save_mix_report for a file with everything.");
    return ok (data);
}

var ToolLayer::saveMixReport (const var& args)
{
    auto& analyser = processor.getMixAnalyser();
    if (! analyser.isRunning() && ! analyser.hasReport())
        return fail ("No analysis yet. Call analyse_mix first.");
    if (analyser.isRunning())
        return fail ("The analysis is still running. Call get_mix_report until the status is done, then save it.");

    auto report = analyser.getReport();
    if (! report.valid)
        return fail (report.error);

    const auto options = MixAnalysis::ExportOptions::fromNames (includeFrom (args));
    report.aiNotes = processor.getAiNotesVar();

    auto file = args["file"].toString().isNotEmpty() ? juce::File (args["file"].toString()) : Settings::rootFolder().getChildFile ("Analysis").getChildFile ("kickbase-analysis-" + juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H%M%S") + ".json");
    if (! report.saveExport (file, options, filterFrom (args)))
        return fail ("Could not write " + file.getFullPathName());

    auto data = object();
    put (data, "file", file.getFullPathName());
    put (data, "bytes", (int) file.getSize());
    put (data, "content", options.toString());
    return ok (data);
}

var ToolLayer::addReportNote (const var& args)
{
    const auto title = args["title"].toString().trim(), text = args["text"].toString().trim();
    if (title.isEmpty() && text.isEmpty())
        return fail ("title and text are needed.");
    PluginBase::AiNote note;
    note.title = title.substring (0, 90);
    note.text = text.substring (0, 1200);
    note.finding = juce::jmax (0, (int) args["finding"]);
    note.applied = (bool) args["applied"];
    processor.addAiNote (std::move (note));
    return ok ("The note is in the report box of the ANALYZER tab.");
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
        const auto instrumentRoot = Params::midiToHz ((float) Params::instrumentRootMidiNote (note)
                                                + (float) processor.apvts.getRawParameterValue (ParamID::bass (0, InstrumentField::octave))->load() * 12.0f
                                                + processor.apvts.getRawParameterValue (ParamID::bass (0, InstrumentField::tune))->load());
        put (metrics, "expected_bass1_root_hz", round4 (instrumentRoot));
        if (tail > 0.0f)
        {
            auto semitones = 12.0 * std::log2 (tail / instrumentRoot);
            semitones -= 12.0 * std::round (semitones / 12.0); // en oktav (eller flera) ifrån rotens ton räknas som i stämning
            check ("kick_tail_matches_bass_root", std::abs (semitones) <= 2.5, "tail " + juce::String (tail, 1) + " Hz vs bass root " + juce::String (instrumentRoot, 1) + " Hz: "
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
    const auto isKick = args["layer"].toString().equalsIgnoreCase ("kick");
    const auto layer = isKick ? Pattern::kickLayer : (int) args["layer"];
    if (! juce::isPositiveAndBelow (layer, Pattern::numNoteLayers))
        return fail ("layer must be 0-9 or 'kick'.");

    auto& pattern = processor.pattern;
    const auto bars = pattern.lengthBars (layer);
    const auto gridParameterID = isKick ? juce::String (ParamID::rowGridKick) : ParamID::bass (layer, InstrumentField::grid);
    const auto gridIndex = juce::jlimit (0, Pattern::numGrids - 1, (int) processor.apvts.getRawParameterValue (gridParameterID)->load());
    const auto note = args.hasProperty ("note") ? juce::jlimit (0, 127, (int) args["note"]) : 52;
    const auto root = isKick ? (int) (processor.apvts.getRawParameterValue (ParamID::kickKeyFollow)->load() >= 0.5f ? Params::kickTailMidiNote (note % 12) : 24)
                             : Params::instrumentRootMidiNote (note) + 12 * (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::octave))->load();

    auto applyLoaded = [this, layer, gridParameterID] (InstrumentGrid grid)
    {
        if (auto* gridParameter = findParameter (processor, gridParameterID))
            Presets::setParameter (*gridParameter, gridParameter->convertTo0to1 ((float) grid));
        processor.patternChanged();
    };

    if (tool == "save_piano_roll")
    {
        const auto name = juce::File::createLegalFileName (args["name"].toString().trim());
        if (name.isEmpty())
            return fail ("Give a name.");
        const auto file = RollFiles::folder().getChildFile (name + ".roll");
        if (! RollFiles::save (pattern, layer, (InstrumentGrid) gridIndex, file))
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
        auto grid = InstrumentGrid::sixteenth;
        if (! file.existsAsFile() || ! RollFiles::load (pattern, layer, file, grid))
            return fail ("No such piano roll preset. The plugin's PRESET menu lists the saved ones; save one first with save_piano_roll.");
        applyLoaded (grid);
        return ok ("Loaded " + file.getFileNameWithoutExtension() + " into layer " + juce::String (layer) + ".");
    }

    if (tool == "import_midi")
    {
        const juce::File file (args["file"].toString());
        if (! file.existsAsFile() || ! RollFiles::importMidi (pattern, layer, file, root))
            return fail ("Could not read notes from that MIDI file.");
        applyLoaded ((InstrumentGrid) gridIndex);
        return ok ("Imported into layer " + juce::String (layer) + "; the row length follows the notes (" + juce::String (pattern.lengthBars (layer)) + " bar(s)).");
    }

    // export_midi
    auto file = args.hasProperty ("file") ? juce::File (args["file"].toString()) : Settings::exportsFolder().getChildFile ("Kickbase Instr " + juce::String (layer + 1) + ".mid");
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
    return ok ("Loaded " + processor.getClickSampleName() + " as the kick's sample (kickSampleMode: Click = click layer, Layer, Replace = the sample is the whole kick).");
}

var ToolLayer::listLibrary (const var& args)
{
    const auto category = args["category"].toString().trim();
    if (category.isNotEmpty() && ! Library::categories().contains (category))
        return fail ("Unknown category \"" + category + "\". Categories: " + Library::categories().joinIntoString (", "));

    juce::SharedResourcePointer<LibraryIndex> index;
    const auto folder = Settings::libraryFolder();
    index->refreshIfChanged (folder); // bara om något ändrats (ändringstiderna); skannar på indexets bakgrundstråd
    index->waitUntilIdle (15000);
    const auto list = index->entries();
    const auto matches = LibraryIndex::filter (*list, category, args["search"].toString());
    const auto limit = args.hasProperty ("limit") ? juce::jlimit (1, 300, (int) args["limit"]) : 50;

    juce::Array<var> items;
    for (size_t i = 0; i < matches.size() && (int) i < limit; ++i)
    {
        const auto& entry = (*list)[(size_t) matches[i]];
        auto item = object();
        put (item, "id", entry.id());
        put (item, "name", entry.name);
        put (item, "category", entry.category);
        put (item, "package", entry.package);
        juce::Array<var> tags;
        for (const auto& tag : entry.tags)
            tags.add (tag);
        put (item, "tags", tags);
        put (item, "license", entry.license);
        put (item, "personal", entry.personal);
        if (entry.author.isNotEmpty())
            put (item, "author", entry.author);
        if (entry.bpm > 0.0)
            put (item, "bpm", entry.bpm);
        if (entry.rootNote >= 0)
            put (item, "root_note", entry.rootNote);
        items.add (item);
    }
    auto data = object();
    put (data, "folder", folder.getFullPathName());
    put (data, "total", (int) list->size());
    put (data, "matched", (int) matches.size());
    put (data, "shown", items.size());
    put (data, "items", items);
    const auto problems = index->problems();
    if (! problems.isEmpty())
    {
        juce::Array<var> shown;
        for (int i = 0; i < problems.size() && i < 10; ++i)
            shown.add (problems[i]);
        put (data, "package_problems", shown);
    }
    return ok (data);
}

var ToolLayer::loadLibraryKick (const var& args)
{
    const auto id = args["id"].toString().trim(), name = args["name"].toString().trim();
    if (id.isEmpty() && name.isEmpty())
        return fail ("Give the id of a Kicks item from list_library (or its exact name).");

    juce::SharedResourcePointer<LibraryIndex> index;
    index->refreshIfChanged (Settings::libraryFolder());
    index->waitUntilIdle (15000);
    const auto list = index->entries();
    std::vector<const LibraryIndex::Entry*> found;
    for (const auto& entry : *list)
        if (entry.category == "Kicks" && (id.isNotEmpty() ? entry.id().equalsIgnoreCase (id) : entry.name.equalsIgnoreCase (name)))
            found.push_back (&entry);
    if (found.empty())
        return fail ("No Kicks item in the library matches that " + juce::String (id.isNotEmpty() ? "id" : "name") + ". Use list_library with category Kicks.");
    if (found.size() > 1)
    {
        juce::StringArray ids;
        for (const auto* entry : found)
            ids.add (entry->id());
        return fail ("Several kicks have that name; use one of these ids: " + ids.joinIntoString (" | "));
    }

    const auto& entry = *found.front();
    if (! processor.loadClickSample (entry.file))
        return fail ("Could not read " + entry.file.getFullPathName());

    if (auto* mode = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (ParamID::kickSampleMode)))
    {
        const auto wanted = args["mode"].toString().toLowerCase();
        const auto target = wanted == "click" ? 0 : wanted == "layer" ? 1 : wanted == "replace" ? 2 : (mode->getIndex() == 0 ? 2 : mode->getIndex());
        if (target != mode->getIndex())
        {
            mode->beginChangeGesture();
            mode->setValueNotifyingHost (mode->convertTo0to1 ((float) target));
            mode->endChangeGesture();
        }
    }
    auto data = object();
    put (data, "loaded", entry.name);
    put (data, "id", entry.id());
    put (data, "personal", entry.personal);
    put (data, "kickSampleMode", processor.apvts.getParameter (ParamID::kickSampleMode)->getCurrentValueAsText());
    return ok (data);
}

var ToolLayer::loadInstrumentSample (const var& args)
{
    const auto layer = (int) args["layer"];
    if (! juce::isPositiveAndBelow (layer, Pattern::numLayers))
        return fail ("layer must be 0-9.");

    const auto path = args["file"].toString().trim();
    if (path.isEmpty())
    {
        processor.clearInstrumentSample (layer);
        return ok ("Sample cleared from layer " + juce::String (layer) + ".");
    }
    if (! processor.loadInstrumentSample (layer, juce::File (path)))
        return fail ("Could not load that file as a sample (WAV, AIFF or FLAC, readable path).");

    auto set = [this, layer] (InstrumentField field, float real)
    {
        if (auto* parameter = findParameter (processor, ParamID::bass (layer, field)))
            Presets::setParameter (*parameter, parameter->convertTo0to1 (real));
    };
    set (InstrumentField::sampleOn, 1.0f);
    set (InstrumentField::sampleLevel, args.hasProperty ("level") ? (float) (double) args["level"] : 1.0f);
    if (args.hasProperty ("key_track"))  set (InstrumentField::sampleKey, (bool) args["key_track"] ? 1.0f : 0.0f);
    if (args.hasProperty ("root_note"))  set (InstrumentField::sampleRoot, (float) (int) args["root_note"]);
    if (args.hasProperty ("loop"))       set (InstrumentField::sampleLoop, (bool) args["loop"] ? 1.0f : 0.0f);
    if (args.hasProperty ("start"))      set (InstrumentField::sampleStart, (float) (double) args["start"]);
    processor.patternChanged();
    return ok ("Loaded " + processor.getInstrumentSampleName (layer) + " into layer " + juce::String (layer) + ".");
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
