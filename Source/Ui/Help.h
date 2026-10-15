#pragma once

#include <juce_core/juce_core.h>

// Förklaringar till rattar och val, som visas när musen hålls över dem (och i inforaden
// längst ner). På engelska, som resten av gränssnittet.
namespace Help
{
    // Tom sträng om parametern saknar förklaring.
    juce::String forParameter (const juce::String& paramID);

    // Hur man använder LFO-ringarna, visas för rattar som kan moduleras.
    juce::String modulationHint();
}
