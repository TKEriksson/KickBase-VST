#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <set>
#include <vector>

// Autokomplettering i Live Code (Ctrl+Space). Ren funktion av texten före markören, så den kan testas utan UI.
// Efter punkt: barnen till objektet till vänster (instr1. -> parametrar, fx1.slot2. -> setEffect och rattar, meter. -> mätare),
// eller Strudels metoder när vänstersidan är ett anrop, t.ex. s("i1"). Annars: globala namn.
namespace LiveCodeComplete
{
    struct Item { juce::String label, insert, detail; };
    struct Result { int replaceFrom = 0; std::vector<Item> items; };

    struct Source
    {
        std::vector<juce::String> paramIds; // t.ex. bass1Cutoff (list_parameters)
        juce::var effectCatalog;            // ToolLayer::liveCodeEffects
    };

    inline juce::String key (const juce::String& s) { return s.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789"); }

    inline const std::vector<Item>& methods()
    {
        static const std::vector<Item> list {
            { "s", "s(\"\")", "s(\"i1\") picks the instrument: bd/kick = kick row, i1..i10 = instrument 1-10" },
            { "note", "note(\"\")", "note(\"c2 eb2 g1\") notes; C2 = 0, integers = semitones" },
            { "n", "n(\"\")", "n(\"0 3 5\") notes as numbers (semitones above C2)" },
            { "gain", "gain()", "gain(0.8) velocity 0.05-1" },
            { "velocity", "velocity()", "velocity(0.8) som gain" },
            { "chord", "chord(\"major\", \"c2\", 0, \"1/4\")", "chord(CHORD, BASETONE, START, LENGTH) stacks the chord tones, e.g. .chord(\"major7\", \"c2\", \"0\", [\"1/4\",\"4/4\",\"2/4\"]). START and LENGTH are fractions of a cycle; a LENGTH array gives each tone its own length; default LENGTH 1/4" },
            { "lfo", "lfo(\"bass1Cutoff\", 1, 0.5, \"sine\")", "lfo(id, hz, depth -1..1, shape sine|tri|saw|isaw|square|rand)" },
            { "env", "env(\"bass1Cutoff\", 10, 200, 0.5)", "env(id, attackMs, decayMs, depth -1..1)" },
            { "cutoff", "cutoff()", "cutoff(0.5) filter cutoff (shortcut to instr.Cutoff, same value range as the parameter)" },
            { "lpf", "lpf()", "lpf(0.5) same as cutoff" },
            { "resonance", "resonance()", "resonance(0.3) filter resonance" },
            { "lpq", "lpq()", "lpq(0.3) same as resonance" },
            { "attack", "attack()", "attack(0.01) seconds" },
            { "decay", "decay()", "decay(0.2) seconds" },
            { "sustain", "sustain()", "sustain(0.7) level 0..1" },
            { "release", "release()", "release(0.1) seconds" },
            { "distort", "distort()", "distort(0.4) drive" },
            { "shape", "shape()", "shape(0.4) drive" },
            { "pan", "pan()", "pan(0.5) 0 = left, 1 = right" },
            { "fast", "fast(2)", "fast(2) double speed" },
            { "slow", "slow(2)", "slow(2) half speed" },
            { "rev", "rev()", "plays the pattern backwards" },
            { "every", "every(4, x => x.rev())", "every(n, fn) apply fn every nth cycle" },
            { "off", "off(0.125, x => x.add(7))", "off(time, fn) adds a shifted copy" },
            { "add", "add()", "add(7) add semitones" },
            { "sub", "sub()", "sub(12) subtract" },
            { "mul", "mul()", "mul(2) multiply" },
            { "segment", "segment(16)", "segment(n) n steps per cycle" },
            { "euclid", "euclid(3, 8)", "euclid(pulses, steps)" },
            { "struct", "struct(\"x ~ x x\")", "struct(pattern) rhythm from x/~" },
            { "late", "late(0.125)", "late(cykler) shift later" },
            { "early", "early(0.125)", "early(cykler) shift earlier" },
            { "stack", "stack()", "stack(a, b) play together" },
            { "hurry", "hurry(2)", "hurry(2) faster" },
            { "degradeBy", "degradeBy(0.3)", "degradeBy(p) randomly drops notes" },
            { "sometimes", "sometimes(x => x.add(12))", "sometimes(fn) now and then" }
        };
        return list;
    }

    inline const std::vector<Item>& globals()
    {
        static const std::vector<Item> list {
            { "$:", "$: ", "new block (plays together with the others)" },
            { "$: s(\"i1\")", "$: s(\"i1\").note(\"c2*4\")", "template: a block for instrument 1 with a note pattern" },
            { "s", "s(\"\")", "s(\"i1\") instrument, or bd = kick" },
            { "note", "note(\"\")", "note(\"c2 eb2\") notes" },
            { "n", "n(\"\")", "n(\"0 3\") notes as numbers" },
            { "stack", "stack()", "stack(a, b) together" },
            { "cat", "cat()", "cat(a, b) one after another" },
            { "slider", "slider(0.5, 0, 1)", "slider(value, min, max) slider in the code" },
            { "silence", "silence", "silent pattern" },
            { "kick", "kick", "kick parameters: kick.Param.set(v)" },
            { "meter", "meter", "meter: meter.outputPeak.get()" },
            { "console", "console.log()", "console.log(x) writes to the CONSOLE tab" },
        };
        return list;
    }

    // Skapar träd: grupp -> barn (namn -> beskrivning). Speglar glue.js buildParams / buildEffects / buildMeters.
    inline std::map<juce::String, std::vector<Item>> tree (const Source& src)
    {
        std::map<juce::String, std::vector<Item>> t;
        auto add = [&t] (const juce::String& group, const juce::String& name, const juce::String& detail)
        {
            for (auto& i : t[group]) if (i.label == name) return;
            t[group].push_back ({ name, name, detail });
        };
        for (const auto& id : src.paramIds)
        {
            auto head = id.initialSectionContainingOnly ("abcdefghijklmnopqrstuvwxyz0123456789");
            const auto rest = id.substring (head.length());
            if (head.isEmpty() || rest.isEmpty() || ! juce::CharacterFunctions::isUpperCase (rest[0])) continue;
            const auto detail = "parameter " + id + ": " + rest + ".set(v) / .get()";
            add (head, rest, detail);
            add (head + "." + rest, "set", "set(v) writes " + id + " after the run");
            add (head + "." + rest, "get", "get() the value from before the run");
            if (head.startsWith ("bass") && head.substring (4).isNotEmpty() && head.substring (4).containsOnly ("0123456789"))
                { add ("instr" + head.substring (4), rest, detail); add ("instr" + head.substring (4) + "." + rest, "set", "set(v) writes " + id + " after the run"); add ("instr" + head.substring (4) + "." + rest, "get", "get() the value from before the run"); }
        }
        for (int g = 1; g <= 10; ++g)
        {
            const auto name = "instr" + juce::String (g);
            add (name, "lock", "lock() locks the instrument against manual changes");
        }
        for (const auto* m : { "outputPeak", "gainReduction", "solo" })
            add ("meter", m, juce::String ("meter.") + m + ".get() reads the value (read-only)");
        for (int d = 1; d <= 10; ++d)
            add ("meter", "duck" + juce::String (d), "meter.duck" + juce::String (d) + ".get() ducking in dB");
        if (const auto* slots = src.effectCatalog["slots"].getArray())
        {
            for (int c = 0; c < slots->size(); ++c)
            {
                const auto fx = "fx" + juce::String (c + 1);
                const auto* row = (*slots)[c].getArray();
                const auto count = row != nullptr ? row->size() : 0;
                for (int s = 0; s < count; ++s)
                {
                    add (fx, "slot" + juce::String (s + 1), "effect slot " + juce::String (s + 1) + " in " + fx.toUpperCase());
                    const auto slot = fx + ".slot" + juce::String (s + 1);
                    add (slot, "setEffect", "setEffect(\"reverb\") swaps the effect in the slot (off clears it)");
                    if (const auto* names = src.effectCatalog["knobs"].getProperty ((*row)[s].toString(), juce::var()).getArray())
                        for (const auto& k : *names)
                            if (key (k.toString()) != "seteffect")
                                add (slot, key (k.toString()), "knob " + k.toString() + ": " + key (k.toString()) + ".set(v)");
                }
            }
        }
        return t;
    }

    // Inuti en öppen sträng: returnerar funktionsnamnet före strängen (s, note, n, notes) eller tomt.
    inline juce::String stringContext (const juce::String& text, int caret)
    {
        auto lineStart = caret;
        while (lineStart > 0 && text[lineStart - 1] != '\n') --lineStart;
        int open = -1;
        juce::juce_wchar quote = 0;
        for (int i = lineStart; i < caret; ++i)
        {
            const auto c = text[i];
            if (quote == 0 && (c == '"' || c == '\'')) { quote = c; open = i; }
            else if (quote != 0 && c == quote) quote = 0;
        }
        if (quote == 0 || open < 1 || text[open - 1] != '(') return {};
        auto b = open - 1;
        while (b > 0 && (juce::CharacterFunctions::isLetterOrDigit (text[b - 1]) || text[b - 1] == '_')) --b;
        return text.substring (b, open - 1);
    }

    inline std::vector<Item> sampleNames()
    {
        std::vector<Item> list { { "bd", "bd", "kick row (same as kick)" }, { "kick", "kick", "kick row" } };
        for (int i = 1; i <= 10; ++i)
            list.push_back ({ "i" + juce::String (i), "i" + juce::String (i), "instrument " + juce::String (i) });
        return list;
    }

    inline std::vector<Item> noteNames()
    {
        std::vector<Item> list {
            { "c2 eb2 g2", "c2 eb2 g2", "several notes after each other, one step each" },
            { "[c2,e2,g2]", "[c2,e2,g2]", "notes at the same time (chord)" },
            { "c2*4", "c2*4", "repeat 4 times per cycle" },
            { "c2 ~ c2", "c2 ~ c2", "~ is a rest" },
            { "<c2 g1>", "<c2 g1>", "one note per cycle, alternating" },
            { "c2@3 g1", "c2@3 g1", "@3 makes the note 3 times as long" },
            { "[c2 g1]*2", "[c2 g1]*2", "[ ] squeezes steps into one step; *2 repeats" }
        };
        for (const auto* n : { "c", "d", "e", "f", "g", "a", "b", "eb", "bb", "ab", "db", "gb" })
            for (int o = 1; o <= 3; ++o)
                list.push_back ({ juce::String (n) + juce::String (o), juce::String (n) + juce::String (o), "note " + juce::String (n).toUpperCase() + juce::String (o) });
        return list;
    }

    inline Result complete (const juce::String& text, int caret, const Source& src)
    {
        Result r;
        caret = juce::jlimit (0, text.length(), caret);
        auto isId = [] (juce::juce_wchar c) { return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_' || c == '$'; };
        auto start = caret;
        while (start > 0 && isId (text[start - 1])) --start;
        r.replaceFrom = start;
        const auto prefix = text.substring (start, caret).toLowerCase();

        std::vector<Item> pool;
        const auto inString = stringContext (text, caret);
        if (inString.isNotEmpty())
        {
            if (inString == "s")
                pool = sampleNames();
            else if (inString == "note" || inString == "n" || inString == "notes")
                pool = noteNames();
            else if (inString == "chord")
                for (const char* name : { "major", "minor", "dim", "aug", "sus2", "sus4", "power", "major7", "minor7", "dom7", "dim7",
                                          "minor7b5", "major9", "minor9", "dom9", "add9", "six", "minor6" })
                    pool.push_back ({ name, name, juce::String ("chord ") + name });
            else if (inString == "lfo" || inString == "env")
                for (const auto& id : src.paramIds)
                    pool.push_back ({ id, id, "parameter " + id + " (modulation target)" });
        }
        else if (start > 0 && text[start - 1] == '.')
        {
            auto end = start - 1, b = end;
            if (end > 0 && text[end - 1] == ')')
                pool = methods();
            else
            {
                while (b > 0 && (isId (text[b - 1]) || text[b - 1] == '.')) --b;
                const auto path = text.substring (b, end);
                const auto t = tree (src);
                if (auto it = t.find (path); it != t.end())
                    pool = it->second;
                else if (path.endsWithChar ('"') || path.isEmpty())
                    pool = methods();
            }
        }
        else
        {
            pool = globals();
            const auto t = tree (src);
            for (const auto& [group, kids] : t)
            {
                if (group.contains (".") || kids.empty()) continue;
                pool.push_back ({ group, group, group.startsWith ("fx") ? "effect chain: " + group + ".slot1.setEffect(...)" : "object with " + juce::String ((int) kids.size()) + " members, type . after it" });
            }
        }
        for (auto& i : pool)
            if (prefix.isEmpty() || i.label.toLowerCase().startsWith (prefix))
                r.items.push_back (i);
        for (auto& i : pool)
            if (prefix.isNotEmpty() && ! i.label.toLowerCase().startsWith (prefix) && i.label.toLowerCase().contains (prefix))
                r.items.push_back (i);
        return r;
    }
}
