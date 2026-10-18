#pragma once

#include <juce_events/juce_events.h>
#include "../Core/Settings.h"

// En timer för en vy som ritas om hela tiden (scopes, mätare, spelhuvud ...), vars takt följer SETTINGS > OPTIMIZATIONS.
// Vyn startar den med startVisualTimer (gruppen och vyns vanliga takt vid 30 bilder per sekund) och anropar followVisualRate()
// först i timerCallback: har inställningen ändrats startas timern om med den nya takten direkt (fönstret behöver inte öppnas igen).
// visualFilled() säger om gruppens fyllda ytor ska ritas (läst samtidigt, inte i paint: inställningsfilen ska inte läsas där).
class VisualTimer : public juce::Timer
{
protected:
    void startVisualTimer (Settings::Visual group, int baseHz)
    {
        visual = group;
        base = baseHz;
        seenVersion = Settings::visualVersion();
        drawFilled = Settings::visualFill (group);
        startTimerHz (Settings::visualHz (group, baseHz));
    }

    // true om takten eller fyllningen just ändrades (vyn kan då behöva ritas om).
    bool followVisualRate()
    {
        const auto now = Settings::visualVersion();
        if (now == seenVersion)
            return false;
        startVisualTimer (visual, base);
        return true;
    }

    bool visualFilled() const { return drawFilled; }

private:
    Settings::Visual visual = Settings::Visual::other;
    int base = 30, seenVersion = -1;
    bool drawFilled = true;
};
