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
            { "s", "s(\"\")", "s(\"i1\") väljer instrument: bd/kick = kickraden, i1..i10 = instrument 1-10" },
            { "note", "note(\"\")", "note(\"c2 eb2 g1\") toner; C2 = 0, heltal = halvtoner" },
            { "n", "n(\"\")", "n(\"0 3 5\") toner som tal (halvtoner över C2)" },
            { "gain", "gain()", "gain(0.8) velocity 0.05-1" },
            { "velocity", "velocity()", "velocity(0.8) som gain" },
            { "lfo", "lfo(\"bass1Cutoff\", 1, 0.5, \"sine\")", "lfo(id, hz, djup -1..1, form sine|tri|saw|isaw|square|rand)" },
            { "env", "env(\"bass1Cutoff\", 10, 200, 0.5)", "env(id, attackMs, decayMs, djup -1..1)" },
            { "cutoff", "cutoff()", "cutoff(0.5) filterfrekvens (genväg till instr.Cutoff, värde som parametern)" },
            { "lpf", "lpf()", "lpf(0.5) samma som cutoff" },
            { "resonance", "resonance()", "resonance(0.3) filterresonans" },
            { "lpq", "lpq()", "lpq(0.3) samma som resonance" },
            { "attack", "attack()", "attack(0.01) sekunder" },
            { "decay", "decay()", "decay(0.2) sekunder" },
            { "sustain", "sustain()", "sustain(0.7) nivå 0..1" },
            { "release", "release()", "release(0.1) sekunder" },
            { "distort", "distort()", "distort(0.4) drive" },
            { "shape", "shape()", "shape(0.4) drive" },
            { "pan", "pan()", "pan(0.5) 0 = vänster, 1 = höger" },
            { "fast", "fast(2)", "fast(2) dubbelt tempo" },
            { "slow", "slow(2)", "slow(2) halvt tempo" },
            { "rev", "rev()", "spelar mönstret baklänges" },
            { "every", "every(4, x => x.rev())", "every(n, fn) tillämpa fn var n:e cykel" },
            { "off", "off(0.125, x => x.add(7))", "off(tid, fn) lägg en förskjuten kopia" },
            { "add", "add()", "add(7) lägg till halvtoner" },
            { "sub", "sub()", "sub(12) dra ifrån" },
            { "mul", "mul()", "mul(2) multiplicera" },
            { "segment", "segment(16)", "segment(n) n steg per cykel" },
            { "euclid", "euclid(3, 8)", "euclid(pulser, steg)" },
            { "struct", "struct(\"x ~ x x\")", "struct(mönster) rytm från x/~" },
            { "late", "late(0.125)", "late(cykler) skjut framåt" },
            { "early", "early(0.125)", "early(cykler) skjut bakåt" },
            { "stack", "stack()", "stack(a, b) spela samtidigt" },
            { "hurry", "hurry(2)", "hurry(2) snabbare" },
            { "degradeBy", "degradeBy(0.3)", "degradeBy(p) slumpa bort toner" },
            { "sometimes", "sometimes(x => x.add(12))", "sometimes(fn) ibland" }
        };
        return list;
    }

    inline const std::vector<Item>& globals()
    {
        static const std::vector<Item> list {
            { "s", "s(\"\")", "s(\"i1\") instrument eller bd = kick" },
            { "note", "note(\"\")", "note(\"c2 eb2\") toner" },
            { "n", "n(\"\")", "n(\"0 3\") toner som tal" },
            { "stack", "stack()", "stack(a, b) samtidigt" },
            { "cat", "cat()", "cat(a, b) efter varandra" },
            { "slider", "slider(0.5, 0, 1)", "slider(värde, min, max) reglage i koden" },
            { "silence", "silence", "tyst mönster" },
            { "kick", "kick", "kickens parametrar: kick.Param.set(v)" },
            { "meter", "meter", "mätare: meter.outputPeak.get()" },
            { "console", "console.log()", "console.log(x) skriver i fliken KONSOL" },
            { "$:", "$: ", "nytt block (spelas samtidigt med de andra)" }
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
            add (head + "." + rest, "set", "set(v) skriver " + id + " efter körningen");
            add (head + "." + rest, "get", "get() värdet från före körningen");
            if (head.startsWith ("bass") && head.substring (4).isNotEmpty() && head.substring (4).containsOnly ("0123456789"))
                { add ("instr" + head.substring (4), rest, detail); add ("instr" + head.substring (4) + "." + rest, "set", "set(v) skriver " + id + " efter körningen"); add ("instr" + head.substring (4) + "." + rest, "get", "get() värdet från före körningen"); }
        }
        for (int g = 1; g <= 10; ++g)
        {
            const auto name = "instr" + juce::String (g);
            add (name, "lock", "lock() låser instrumentet mot handpåläggning");
        }
        for (const auto* m : { "outputPeak", "gainReduction", "solo" })
            add ("meter", m, juce::String ("meter.") + m + ".get() läser värdet (bara läsning)");
        for (int d = 1; d <= 10; ++d)
            add ("meter", "duck" + juce::String (d), "meter.duck" + juce::String (d) + ".get() ducking i dB");
        if (const auto* slots = src.effectCatalog["slots"].getArray())
        {
            for (int c = 0; c < slots->size(); ++c)
            {
                const auto fx = "fx" + juce::String (c + 1);
                const auto* row = (*slots)[c].getArray();
                const auto count = row != nullptr ? row->size() : 0;
                for (int s = 0; s < count; ++s)
                {
                    add (fx, "slot" + juce::String (s + 1), "effektplats " + juce::String (s + 1) + " i " + fx.toUpperCase());
                    const auto slot = fx + ".slot" + juce::String (s + 1);
                    add (slot, "setEffect", "setEffect(\"reverb\") byter effekt på platsen (off tömmer)");
                    if (const auto* names = src.effectCatalog["knobs"].getProperty ((*row)[s].toString(), juce::var()).getArray())
                        for (const auto& k : *names)
                            if (key (k.toString()) != "seteffect")
                                add (slot, key (k.toString()), "ratten " + k.toString() + ": " + key (k.toString()) + ".set(v)");
                }
            }
        }
        return t;
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
        if (start > 0 && text[start - 1] == '.')
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
                pool.push_back ({ group, group, group.startsWith ("fx") ? "effektkedja: " + group + ".slot1.setEffect(...)" : "objekt med " + juce::String ((int) kids.size()) + " medlemmar, skriv . efter" });
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
