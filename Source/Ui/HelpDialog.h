#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Gemensam hjälpdialog: markdown-lik text (rubriker, punktlistor, **fet**, `kod`) i en vy med rullningslist.
// En flik per ämne; HELP-knappen på en flik öppnar dialogen på sitt ämne. Icke-modal, en enda instans.
namespace HelpDialog
{
    enum class Topic { pianoRoll, liveCode, soundstage, osc, filter, fm, mix, sample };

    // Öppnar dialogen på `topic` (eller byter flik om den redan är öppen).
    void show (juce::Component* centreAround, Topic topic);

    // Ställer in en HELP-knapp: text, tooltip och klick som öppnar rätt flik.
    void setupButton (juce::TextButton& button, juce::Component& owner, Topic topic, const juce::String& what);
}
