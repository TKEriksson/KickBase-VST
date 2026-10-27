#pragma once

#include "../Core/PluginBase.h"
#include "../Core/RollFiles.h"
#include "Theme.h"

// Instrumentmenyn (L4): högerklick på radkortet i mönstret eller på instrumentväljaren. Namn och färg sparas i projektet (inte som parametrar).
// Duplicate, Move och Delete flyttar innehållet mellan de fasta platserna (PluginBase::copyTrack/pasteTrack), se PluginBase.h.
namespace InstrumentMenu
{
    // Kopierat instrument (Copy track). Delas av alla editorer i processen, så att man kan klistra in i ett annat projekt.
    inline std::unique_ptr<juce::XmlElement>& clipboard()
    {
        static std::unique_ptr<juce::XmlElement> clip;
        return clip;
    }

    // Egna färgval -> temat. Anropas av editorn vid start och av InstrumentHost när färgerna ändrats.
    inline void applyColours (PluginBase& processor)
    {
        for (int i = 0; i < ParamID::numInstrumentLayers; ++i)
            Theme::layerColourOverrides()[(size_t) i] = processor.getLayerColour (i);
    }

    inline const std::array<juce::uint32, 10>& palette()
    {
        static const std::array<juce::uint32, 10> colours { 0xffff9a3c, 0xff8f9bff, 0xff5fffb0, 0xffff7ac8, 0xffc3ff6b,
                                                           0xff6bb8ff, 0xffd49bff, 0xff5ff0e0, 0xffffd66b, 0xffff8a8a };
        return colours;
    }

    inline void rename (PluginBase& processor, int layer, juce::Component::SafePointer<juce::Component> target)
    {
        auto* window = new juce::AlertWindow ("Rename", "Name for this instrument (it is shown in the pattern, the mixer and the piano roll):", juce::MessageBoxIconType::NoIcon);
        if (target != nullptr)
            window->setLookAndFeel (&target->getLookAndFeel());
        window->addTextEditor ("name", processor.getLayerName (layer));
        window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Default name", 2);
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([&processor, layer, window, target] (int result)
        {
            if (target == nullptr || result == 0)
                return;
            processor.setLayerName (layer, result == 2 ? juce::String() : window->getTextEditorContents ("name"));
            processor.markStateChanged();
        }), true);
    }

    inline void savePattern (PluginBase& processor, int layer, juce::Component::SafePointer<juce::Component> target)
    {
        auto* window = new juce::AlertWindow ("Save pattern preset", "Name for the pattern preset of " + processor.getLayerName (layer) + ":", juce::MessageBoxIconType::NoIcon);
        if (target != nullptr)
            window->setLookAndFeel (&target->getLookAndFeel());
        window->addTextEditor ("name", processor.getLayerName (layer));
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([&processor, layer, window, target] (int result)
        {
            const auto name = window->getTextEditorContents ("name").trim();
            if (target == nullptr || result == 0 || name.isEmpty())
                return;
            if (! processor.savePatternPreset (layer, name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed", "Could not save the pattern preset.");
        }), true);
    }

    // FileChooser måste leva tills dialogen stängts.
    inline std::unique_ptr<juce::FileChooser>& chooser()
    {
        static std::unique_ptr<juce::FileChooser> c;
        return c;
    }

    inline void exportMidi (PluginBase& processor, int layer, juce::Component::SafePointer<juce::Component> target)
    {
        const auto folder = Settings::exportsFolder();
        folder.createDirectory();
        chooser() = std::make_unique<juce::FileChooser> ("Export the pattern as MIDI", folder.getChildFile ("Kickbase Instr " + juce::String (layer + 1) + ".mid"), "*.mid");
        chooser()->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                [&processor, layer, target] (const juce::FileChooser& fc)
                                {
                                    if (target == nullptr || fc.getResult() == juce::File())
                                        return;
                                    const auto key = processor.transportStatus.keyNote.load();
                                    const auto root = Params::instrumentRootMidiNote (key >= 0 ? key : 60)
                                                      + 12 * (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::octave))->load();
                                    const auto file = fc.getResult().withFileExtension ("mid");
                                    if (! RollFiles::exportMidi (processor.pattern, layer, processor.pattern.lengthBars (layer), processor.transportStatus.bpm.load(), root, file))
                                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", "Could not write " + file.getFullPathName());
                                    else if (const auto slides = RollFiles::countSlideNotes (processor.pattern, layer); slides > 0)
                                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MIDI exported",
                                                                                juce::String (slides) + " glide note(s) were written as plain notes, because MIDI has no glide notes.");
                                });
    }

    inline void deleteTrack (PluginBase& processor, int layer, juce::Component::SafePointer<juce::Component> target)
    {
        auto doDelete = [&processor, layer, target]
        {
            if (target == nullptr)
                return;
            processor.deleteTrack (layer);
            processor.selectedInstrument = juce::jlimit (0, processor.activeInstruments() - 1, layer);
        };
        juce::String what;
        if (! processor.instrumentHasContent (layer, &what))
        {
            doDelete();
            return;
        }
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                          juce::MessageBoxIconType::WarningIcon, "Delete instrument",
                                          "Delete " + processor.getLayerName (layer) + "?\n\nIt has " + what + ".\n\nThe instruments after it move up one step. It can't be undone.",
                                          "Delete", "Cancel", target.getComponent()),
                                      [doDelete] (int choice)
                                      {
                                          if (choice == 1)
                                              doDelete();
                                      });
    }

    // Visar menyn för ett instrument (0-9) vid `target`.
    inline void show (PluginBase& processor, int layer, juce::Component& target, juce::Rectangle<int> area = {}, std::function<void()> onEdit = {})
    {
        const auto count = processor.activeInstruments();
        if (processor.isEffectVersion() || ! juce::isPositiveAndBelow (layer, count))
            return;

        juce::PopupMenu colours;
        colours.addItem (50, "Default colour", true, processor.getLayerColour (layer) == 0);
        for (size_t i = 0; i < palette().size(); ++i)
            colours.addColouredItem (51 + (int) i, "Colour " + juce::String ((int) i + 1), juce::Colour (palette()[i]), true, processor.getLayerColour (layer) == palette()[i]);

        juce::PopupMenu menu;
        menu.addSectionHeader (processor.getLayerName (layer));
        if (onEdit)
            menu.addItem (10, "Edit instrument");
        menu.addItem (1, "Rename...");
        menu.addSubMenu ("Color", colours);
        menu.addSeparator();
        menu.addItem (2, "Duplicate", count < ParamID::numInstrumentLayers);
        menu.addItem (3, "Move up", layer > 0);
        menu.addItem (4, "Move down", layer < count - 1);
        menu.addItem (5, "Delete", count > 1);
        menu.addSeparator();
        menu.addItem (6, "Copy track");
        menu.addItem (7, "Paste track (replaces this one)", clipboard() != nullptr);
        menu.addSeparator();
        menu.addItem (8, "Export MIDI...");
        menu.addItem (9, "Save pattern preset...");

        juce::Component::SafePointer<juce::Component> safeTarget (&target);
        auto options = juce::PopupMenu::Options().withTargetComponent (&target);
        if (! area.isEmpty())
            options = options.withTargetScreenArea (target.localAreaToGlobal (area));
        menu.showMenuAsync (options, [&processor, layer, safeTarget, onEdit] (int result)
        {
            if (safeTarget == nullptr || result == 0)
                return;
            if (result >= 50)
            {
                const auto index = result - 51;
                processor.setLayerColour (layer, index < 0 ? 0 : palette()[(size_t) index]);
                processor.markStateChanged();
                return;
            }
            switch (result)
            {
                case 1: rename (processor, layer, safeTarget); break;
                case 2: if (processor.duplicateTrack (layer)) processor.selectedInstrument = layer + 1; break;
                case 3: if (processor.moveTrack (layer, -1)) processor.selectedInstrument = layer - 1; break;
                case 4: if (processor.moveTrack (layer, 1)) processor.selectedInstrument = layer + 1; break;
                case 5: deleteTrack (processor, layer, safeTarget); break;
                case 6: clipboard() = processor.copyTrack (layer); break;
                case 7: if (clipboard() != nullptr) processor.pasteTrack (layer, *clipboard()); break;
                case 8: exportMidi (processor, layer, safeTarget); break;
                case 9: savePattern (processor, layer, safeTarget); break;
                case 10: if (onEdit) onEdit(); break;
                default: break;
            }
        });
    }
}
