#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <string>
#include "Effects.h"
#include "NodeEnvelope.h"
#include "Pattern.h"
#include "Sidechain.h"

// Presets: fabrikspresets i koden och användarens egna som filer.
//
// Ett preset gäller en del av pluginet. Full sparar allt; övriga bara sin del, så att man
// t.ex. kan byta kick utan att röra basen, eller ladda ett echo utan att röra reverbet.
namespace Presets
{
    // Effects är hela effektracket: alla kanaler och vilken kanal varje baslager skickar till.
    enum class Section { full, kick, bass, pattern, sidechain, master, lfo, effects };
    constexpr int numSections = 8;

    juce::String sectionName (Section section);

    // Hör parametern till den här delen? (Full innehåller alla.) `layer` gäller Bass: ett
    // baslagers ljud (inte dess på/av, gate, duck och rutnät, som hör till mönster/sidechain).
    bool belongsTo (Section section, const juce::String& paramID, int layer = 0);

    // Vilket baslager ett parameter-ID hör till ("bass2Cutoff" -> 1), eller -1.
    int bassLayerOf (const juce::String& paramID);

    // Samma parameter i ett annat baslager ("bass1Cutoff", 2 -> "bass3Cutoff").
    juce::String toBassLayer (const juce::String& paramID, int layer);

    // Mappen där användarens presets för delen sparas. Full och Bass har en mapp per plugin,
    // eftersom instrumentet och FX-versionen har olika parametrar där.
    juce::File folder (Section section, bool effectVersion);

    int factoryCount (Section section, bool effectVersion);
    juce::String factoryName (Section section, bool effectVersion, int index);

    // Sätter delens parametrar till fabrikspresetets värden (övriga i delen till standard) och,
    // för Full och Pattern, fyller mönstret. Rutnätet sätts via parametern.
    void applyFactory (Section section, bool effectVersion, int index,
                       juce::AudioProcessorValueTreeState& apvts, Pattern& pattern, SidechainCurve& curve, int layer = 0);

    // Det ett fabrikspreset har utöver parametrar och mönster: kickens fria envelopes (pitch och volym,
    // tomma = standardkurvorna) och LFO-kopplingar (LFO 0-2 till en parameter, mängd -1..1).
    struct Extras
    {
        struct Route
        {
            int source = 0;
            juce::String paramID;
            float amount = 0.0f;
        };

        // En bas-rad i fabrikspresetets mönster. `bars` är en text per takt (upprepas över fyra takter), i lagrets
        // rutnät. Ett steg skrivs som ett ord: "." = paus, annars en bokstav för velocity (g = spöke 42 %,
        // m = mjuk 62 %, x = normal 80 %, X = accent 100 %), sedan valfritt tonhöjd (+7, -5), "~" för slide och
        // ":50" för längd i procent av steget. Mellanslag och | skiljer stegen. Exempel: ". m x x"
        struct PatternLine
        {
            int layer = 0;
            BassGrid grid = BassGrid::sixteenth;
            std::vector<std::string> bars;
            float gate = 0.7f; // längd på toner utan ":procent"
        };

        std::vector<NodeEnvelope::Point> pitchEnv, ampEnv;
        std::vector<Route> routes;
        std::vector<PatternLine> pattern; // tomt = mallens mönster
    };

    // Skriver en bas-rad (se PatternLine) till mönstret: rensar lagret och lägger tonerna.
    void writePatternLine (Pattern& pattern, const Extras::PatternLine& line);

    // Nullptr om presetet saknas.
    const Extras* factoryExtras (Section section, bool effectVersion, int index);

    // Sätter en parameter som en användare skulle: med gest, så att DAW:en kan spela in den.
    void setParameter (juce::RangedAudioParameter& parameter, float normalisedValue);
}
