#pragma once

#include "../Core/PluginBase.h"

// Ctrl+Z = ångra, Ctrl+Shift+Z och Ctrl+Y = gör om. Anropas från editorns keyPressed (tangenter som ingen annan komponent tog, t.ex. en ratt med fokus).
// Textfält har egen ångra och tar tangenterna före editorn.
inline bool handleUndoKey (PluginBase& processor, const juce::KeyPress& key)
{
    const auto c = key.getKeyCode();
    const auto mods = key.getModifiers();
    if (! mods.isCommandDown())
        return false;
    if ((c == 'Z' || c == 'z') && ! mods.isShiftDown())
        return processor.undoManager.undo() || true;
    if (((c == 'Z' || c == 'z') && mods.isShiftDown()) || c == 'Y' || c == 'y')
        return processor.undoManager.redo() || true;
    return false;
}
