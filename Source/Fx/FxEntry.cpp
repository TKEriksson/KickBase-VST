#include "FxProcessor.h"

// JUCE anropar den här för att skapa plugin-instansen. Ligger i en egen fil så att
// verktygen (Tools/) kan länka processorn utan att få två createPluginFilter.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FxProcessor();
}
