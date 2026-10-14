#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Pattern.h"

// Fabrikspresets: startpunkter för ljud och mönster.
namespace Presets
{
    int count();
    juce::String name (int index);

    // Återställer alla parametrar till standard, sätter presetens värden och fyller mönstret.
    // Parametrar som pluginet saknar (t.ex. bassynten i FX-versionen) hoppas över.
    // Returnerar mönstrets rutnät.
    BassGrid apply (int index, juce::AudioProcessorValueTreeState& apvts, Pattern& pattern);
}
