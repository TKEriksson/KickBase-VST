#pragma once

#include <juce_core/juce_core.h>
#include <juce_javascript/juce_javascript.h>
#include <map>

// Live Code: kör Strudel (@strudel/core + mini, bunten i Resources/strudel) i JUCE:s JavascriptEngine (QuickJS).
// Strudel: Copyright (C) Strudel contributors, AGPL-3.0-or-later, https://codeberg.org/uzu/strudel
//
// Aldrig på ljudtråden. Motorn är inte trådsäker: använd den från en tråd i taget.
// 1 Strudel-cykel = 1 takt = 384 tick. Se docs/work/PROGRESS-lc.md för mappningen.
class LiveCode
{
public:
    LiveCode();

    // Värden användaren dragit fram med slider(v,min,max) (nyckel = anropets ordningsnummer i koden). Sätts före setCode/create.
    void setSliderValues (std::map<int, double> values);

    // Parametrar som koden kan läsa/skriva: id -> värde i egna enheter (ögonblicksbild från före körningen). Sätts före setCode/create.
    // Ger objektträdet instr1.Cutoff.set(v) / .get() (glue.js buildParams). Skrivningarna hämtas med takeParamWrites().
    void setParams (std::map<juce::String, double> values);
    juce::var takeParamWrites(); // lista med {id, value}, i den ordning koden skrev dem (senaste värdet per id vinner)

    // console.log/warn/error i koden (glue/prelude routar till host.log): en rad per anrop, högst maxConsoleLines per körning.
    juce::StringArray takeConsole();
    static constexpr int maxConsoleLines = 200;

    // Läser in och utvärderar koden. Ger fel som text (syntax, oändlig loop efter 500 ms).
    juce::Result setCode (const juce::String& code);

    struct Hap
    {
        double begin = 0, end = 0;  // i cykler
        juce::var value;            // objekt med s, note, n, gain ...
        std::vector<std::pair<int, int>> locs; // teckenpositioner [start, slut) i koden (G3), tom om saknas
    };

    // Notmarkering (G3): var i koden en not står och när den låter. Tick = cykel * 384, inom mönstrets loop.
    // layer: -1 = kicken, annars instrument 0..9 (samma regel som toPatternArgs/collectMods).
    struct Mark
    {
        int layer = 0, beginTick = 0, endTick = 0;
        std::vector<std::pair<int, int>> locs;
    };
    const std::vector<Mark>& getMarks() const { return marks; }

    // Alla händelser som börjar i [from, to) cykler.
    std::vector<Hap> query (double from, double to, juce::Result& result);

    // Mönstret över cycles cykler som en lista med set_pattern-argument (ett per lager).
    juce::var toPatternArgs (const std::vector<Hap>& haps, int cycles) const;

    // Hela vägen: kod -> lista med set_pattern-argument. result bär felet.
    juce::var create (const juce::String& code, int cycles, juce::Result& result);

    // Modulationsorden .lfo/.env från senaste create(): lista med {layer, param, kind ("lfo"/"env"), rate, shape, attack, decay, depth},
    // en post per (lager, param, kind). layer är -1 för kicken. Tom lista = inga modulationsord i koden.
    const juce::var& getMods() const { return mods; }

    // Tonnamn ("c2", "eb3", "f#1") eller tal -> pitch i halvtoner från C2 (MIDI 36). false om det inte går.
    static bool pitchFrom (const juce::var& note, int& pitch);

    static constexpr int ticksPerCycle = 384;

private:
    std::map<juce::String, double> paramValues;
    juce::Array<juce::var> paramWrites;
    juce::StringArray consoleLines;
    std::map<int, double> sliderValues; // före engine: reglagens svar läser den
    juce::JavascriptEngine engine;
    bool loaded = false;
    juce::var mods;
    std::vector<Mark> marks;
    static juce::var collectMods (const std::vector<Hap>& haps);
    juce::Result load();
};
