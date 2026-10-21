#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// FM-presets (fas S2c): ett val skriver värden i vanliga parametrar (FM-fälten, tonhöjdsenvelopen och vid behov Filter 2). Index 0 är Custom
// och rör ingenting. Konstanterna ligger i FmPresets.cpp; testet använder samma tabell.
namespace FmPresets
{
    struct Preset
    {
        const char* name;
        float level, ratio, index, envAmount, envMs, semis;
        float pitchEnvSemis, pitchEnvDecayMs;
        bool filter2Hp; // slår på Filter 2 som högpass ~120 Hz (leadpresets, så att det låga hålls rent)
    };

    int count();                                   // inklusive Custom (index 0)
    const Preset& get (int index);                 // index 1..count()-1
    juce::StringArray names();                     // Custom först

    // Skriver presetens värden till lagret (som en gest, så att DAW:en ser det). Sätter inte FM Preset-parametern själv.
    void apply (juce::AudioProcessorValueTreeState& apvts, int layer, int index);
}
