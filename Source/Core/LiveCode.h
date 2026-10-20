#pragma once

#include <juce_core/juce_core.h>
#include <juce_javascript/juce_javascript.h>

// Live Code: kör Strudel (@strudel/core + mini, bunten i Resources/strudel) i JUCE:s JavascriptEngine (QuickJS).
// Strudel: Copyright (C) Strudel contributors, AGPL-3.0-or-later, https://codeberg.org/uzu/strudel
//
// Aldrig på ljudtråden. Motorn är inte trådsäker: använd den från en tråd i taget.
// 1 Strudel-cykel = 1 takt = 384 tick. Se docs/work/PROGRESS-lc.md för mappningen.
class LiveCode
{
public:
    LiveCode();

    // Läser in och utvärderar koden. Ger fel som text (syntax, oändlig loop efter 500 ms).
    juce::Result setCode (const juce::String& code);

    struct Hap
    {
        double begin = 0, end = 0;  // i cykler
        juce::var value;            // objekt med s, note, n, gain ...
    };

    // Alla händelser som börjar i [from, to) cykler.
    std::vector<Hap> query (double from, double to, juce::Result& result);

    // Mönstret över cycles cykler som en lista med set_pattern-argument (ett per lager).
    juce::var toPatternArgs (const std::vector<Hap>& haps, int cycles) const;

    // Hela vägen: kod -> lista med set_pattern-argument. result bär felet.
    juce::var create (const juce::String& code, int cycles, juce::Result& result);

    // Tonnamn ("c2", "eb3", "f#1") eller tal -> pitch i halvtoner från C2 (MIDI 36). false om det inte går.
    static bool pitchFrom (const juce::var& note, int& pitch);

    static constexpr int ticksPerCycle = 384;

private:
    juce::JavascriptEngine engine;
    bool loaded = false;
    juce::Result load();
};
