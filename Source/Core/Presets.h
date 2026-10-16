#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
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

        std::vector<NodeEnvelope::Point> pitchEnv, ampEnv;
        std::vector<Route> routes;
    };

    // Nullptr om presetet saknas.
    const Extras* factoryExtras (Section section, bool effectVersion, int index);

    // Sätter en parameter som en användare skulle: med gest, så att DAW:en kan spela in den.
    void setParameter (juce::RangedAudioParameter& parameter, float normalisedValue);
}
