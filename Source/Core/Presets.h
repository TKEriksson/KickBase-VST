#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects.h"
#include "Pattern.h"

// Presets: fabrikspresets i koden och användarens egna som filer.
//
// Ett preset gäller en del av pluginet. Full sparar allt; övriga bara sin del, så att man
// t.ex. kan byta kick utan att röra basen, eller ladda ett echo utan att röra reverbet.
namespace Presets
{
    // Effects är hela effektkedjan (inklusive ordningen); filter ... reverb är en effekt var.
    enum class Section { full, kick, bass, pattern, effects, filter, distortion, eq, chorus, phaser, echo, reverb };
    constexpr int numSections = 12;

    inline Section effectSection (EffectType type) { return (Section) ((int) Section::filter + (int) type); }
    inline bool isSingleEffect (Section section)   { return (int) section >= (int) Section::filter; }
    inline EffectType effectOf (Section section)   { return (EffectType) ((int) section - (int) Section::filter); }

    juce::String sectionName (Section section);

    // Hör parametern till den här delen? (Full innehåller alla.)
    bool belongsTo (Section section, const juce::String& paramID);

    // Mappen där användarens presets för delen sparas. Full och Bass har en mapp per plugin,
    // eftersom instrumentet och FX-versionen har olika parametrar där.
    juce::File folder (Section section, bool effectVersion);

    int factoryCount (Section section, bool effectVersion);
    juce::String factoryName (Section section, bool effectVersion, int index);

    // Sätter delens parametrar till fabrikspresetets värden (övriga i delen till standard) och,
    // för Full och Pattern, fyller mönstret. Rutnätet sätts via parametern.
    void applyFactory (Section section, bool effectVersion, int index,
                       juce::AudioProcessorValueTreeState& apvts, Pattern& pattern);

    // Sätter en parameter som en användare skulle: med gest, så att DAW:en kan spela in den.
    void setParameter (juce::RangedAudioParameter& parameter, float normalisedValue);
}
