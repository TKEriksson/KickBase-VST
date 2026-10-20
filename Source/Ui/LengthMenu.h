#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// Radens LENGTH (R9): AUTO följer innehållet (sista takten med en ton), eller ett valt antal takter (låst). Samma meny från stegvyn, pianorullen
// och radens högerklick. Ett valt antal som är kortare än innehållet kräver en bekräftelse, och noterna efter slutet tas bort (så att minnet frigörs).
namespace LengthMenu
{
    // "AUTO 4" eller "4 LOCKED" (det som står på knappen).
    inline juce::String buttonText (const Pattern& pattern, int layer)
    {
        return pattern.isLengthAuto (layer) ? "AUTO " + juce::String (pattern.lengthBars (layer))
                                            : juce::String (pattern.manualBars (layer)) + juce::String::fromUTF8 (" \xF0\x9F\x94\x92");
    }

    inline juce::String tooltip (const juce::String& rowName)
    {
        return rowName + " LENGTH - AUTO: the row is as long as its notes (the last bar with a note, at least 1 bar), and shrinks again when you delete notes. "
               "A number: the row is locked to that many bars, for example 3 bars of kick and 1 bar of silence. Click to change. "
               "The pattern is as long as the longest row; a shorter row repeats (shown dimmed) while the longest plays out, or plays once if its loop button is off.";
    }

    // Sätter ett valt antal takter (0 = AUTO). Förkortas raden under innehållet frågar den först (`target` är komponenten dialogen hör till).
    inline void setLength (PluginBase& processor, int layer, int bars, std::function<void()> afterwards = {})
    {
        auto& pattern = processor.pattern;
        bars = juce::jlimit (0, Pattern::maxBars, bars);
        if (bars == 0)
        {
            pattern.setManualBars (layer, 0);
            processor.patternChanged();
            if (afterwards)
                afterwards();
            return;
        }

        auto lost = 0;
        pattern.forEachNote (layer, [&] (int tick, const Pattern::Note&) { lost += tick >= bars * Pattern::ticksPerBar ? 1 : 0; });
        auto apply = [&processor, layer, bars, afterwards]
        {
            processor.pattern.trimRow (layer, bars);
            processor.pattern.setManualBars (layer, bars);
            processor.patternChanged();
            if (afterwards)
                afterwards();
        };
        if (lost == 0)
        {
            apply();
            return;
        }
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                          juce::MessageBoxIconType::WarningIcon, "Shorten the row",
                                          "Locking the row to " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " removes " + juce::String (lost)
                                              + (lost == 1 ? " note" : " notes") + " after the end. This can't be undone.",
                                          "Shorten", "Cancel"),
                                      [apply] (int choice)
                                      {
                                          if (choice == 1)
                                              apply();
                                      });
    }

    inline void show (PluginBase& processor, int layer, juce::Component& target, std::function<void()> afterwards = {})
    {
        auto& pattern = processor.pattern;
        juce::PopupMenu menu;
        menu.addItem (1, "AUTO: follow the notes (" + juce::String (pattern.contentBars (layer)) + (pattern.contentBars (layer) == 1 ? " bar" : " bars") + ")",
                      true, pattern.isLengthAuto (layer));
        menu.addItem (2, "Lock at the current length (" + juce::String (pattern.lengthBars (layer)) + (pattern.lengthBars (layer) == 1 ? " bar" : " bars") + ")",
                      pattern.isLengthAuto (layer));
        menu.addItem (3, "Set the number of bars...");

        juce::Component::SafePointer<juce::Component> safeTarget (&target);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [&processor, layer, safeTarget, afterwards] (int result)
        {
            if (result == 1)
                setLength (processor, layer, 0, afterwards);
            else if (result == 2)
                setLength (processor, layer, processor.pattern.lengthBars (layer), afterwards);
            else if (result == 3)
            {
                auto* window = new juce::AlertWindow ("Row length", "Number of bars (1 to " + juce::String (Pattern::maxBars) + "):", juce::MessageBoxIconType::NoIcon);
                window->addTextEditor ("bars", juce::String (processor.pattern.lengthBars (layer)));
                if (auto* editor = window->getTextEditor ("bars"))
                    editor->setInputRestrictions (4, "0123456789");
                window->addButton ("Set", 1, juce::KeyPress (juce::KeyPress::returnKey));
                window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                window->enterModalState (true, juce::ModalCallbackFunction::create ([&processor, layer, window, afterwards] (int choice)
                {
                    if (choice == 1)
                        setLength (processor, layer, std::max (1, window->getTextEditorContents ("bars").getIntValue()), afterwards);
                }), true);
            }
        });
    }
}
