#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Projektfil (.kbproj): hela plugin-läget som XML-text i en fil (spara, öppna, skicka vidare, lägg i git). Innehållet är samma tillstånd som
// DAW-projektet bär (`getStateInformation`), med två extra läsbara attribut på rotelementet. Ingen parameter, ingen ljudväg.
namespace ProjectFile
{
    constexpr int formatVersion = 1;
    constexpr juce::int64 maxFileBytes = 256ll * 1024 * 1024; // större filer läses inte (de är inte projekt, eller trasiga)

    // Skriver tillståndet till `file` via en temporär fil, så att en avbruten skrivning inte förstör den gamla. `pluginVersion` skrivs som attribut.
    bool save (juce::AudioProcessor& processor, const juce::File& file, const juce::String& pluginVersion, juce::String& error);

    // Läser `file` och sätter tillståndet. `rootType` är parameterträdets rotnamn (apvts.state.getType()); filer med annan rot avvisas. Vid fel (saknas, för stor, inte XML, fel rot, okänd eller nyare version) ändras ingenting och `error` förklarar.
    bool load (juce::AudioProcessor& processor, const juce::Identifier& rootType, const juce::File& file, juce::String& error);
}
