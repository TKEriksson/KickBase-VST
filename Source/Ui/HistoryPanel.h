#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// MENY > History...: ångra-historiken som en lista, nyast överst. Ångrade steg (redo-delen) är gråa ovanför raden "Now", utförda steg ligger under.
// Undo/Redo tar ett steg, Restore (eller dubbelklick) går till markerat steg. Listan följer UndoManager (ChangeBroadcaster).
class HistoryPanel : public juce::Component, private juce::ListBoxModel, private juce::ChangeListener
{
public:
    explicit HistoryPanel (PluginBase& p) : processor (p)
    {
        list.setModel (this);
        list.setRowHeight (20);
        list.setColour (juce::ListBox::backgroundColourId, Theme::panel);
        addAndMakeVisible (list);

        undoButton.onClick = [this] { processor.undoManager.undo(); };
        redoButton.onClick = [this] { processor.undoManager.redo(); };
        restoreButton.onClick = [this] { restoreSelected(); };
        for (auto* b : { &undoButton, &redoButton, &restoreButton })
            addAndMakeVisible (*b);

        undoButton.setTooltip ("Undo the step just below NOW (Ctrl+Z).");
        redoButton.setTooltip ("Redo the step just above NOW (Ctrl+Shift+Z or Ctrl+Y).");
        restoreButton.setTooltip ("Go to the selected step: undoes or redoes everything between it and NOW. Double-click a row does the same.");

        processor.undoManager.addChangeListener (this);
        refresh();
        setSize (420, 360);
    }

    ~HistoryPanel() override { processor.undoManager.removeChangeListener (this); }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        auto buttons = area.removeFromBottom (28);
        area.removeFromBottom (6);
        const auto w = buttons.getWidth() / 3;
        undoButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
        redoButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
        restoreButton.setBounds (buttons.reduced (2, 0));
        list.setBounds (area);
    }

    void paint (juce::Graphics& g) override { g.fillAll (Theme::panel); }

private:
    struct Row
    {
        juce::String text;
        enum Kind { redo, now, done, note } kind;
    };

    void refresh()
    {
        auto& um = processor.undoManager;
        rows.clear();
        auto redos = um.getRedoDescriptions(); // [0] = nästa att göra om; längst bort i tiden hamnar överst
        for (int i = redos.size(); --i >= 0;)
            rows.push_back ({ redos[i].isEmpty() ? "Parameter change" : redos[i], Row::redo });
        rows.push_back ({ "Now", Row::now });
        for (const auto& s : um.getUndoDescriptions())
            rows.push_back ({ s.isEmpty() ? "Parameter change" : s, Row::done });
        if (processor.historyClearedNote.isNotEmpty())
            rows.push_back ({ processor.historyClearedNote, Row::note });

        redoCount = redos.size();
        undoButton.setEnabled (um.canUndo());
        redoButton.setEnabled (um.canRedo());
        list.updateContent();
        list.repaint();
    }

    void restoreSelected()
    {
        restoreRow (list.getSelectedRow());
    }

    void restoreRow (int row)
    {
        if (row < 0 || row >= (int) rows.size())
            return;
        auto& um = processor.undoManager;
        if (rows[(size_t) row].kind == Row::redo)
            for (int n = redoCount - row; --n >= 0;)
                um.redo();
        else if (rows[(size_t) row].kind == Row::done)
            for (int n = row - redoCount; --n >= 0;) // raden direkt under "Now" är ett steg bakåt
                um.undo();
    }

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row >= (int) rows.size())
            return;
        const auto& r = rows[(size_t) row];
        if (selected)
            g.fillAll (Theme::text.withAlpha (0.15f));
        if (r.kind == Row::now)
        {
            g.setColour (Theme::text.withAlpha (0.6f));
            g.drawHorizontalLine (height / 2, 6.0f, (float) width - 6.0f);
            g.setColour (Theme::panel);
            g.fillRect (juce::Rectangle<int> (0, 0, 60, height).withCentre ({ width / 2, height / 2 }));
        }
        g.setColour (r.kind == Row::redo ? Theme::text.withAlpha (0.4f) : r.kind == Row::note ? Theme::text.withAlpha (0.55f) : Theme::text);
        g.setFont (juce::FontOptions (13.0f, r.kind == Row::now ? juce::Font::bold : juce::Font::plain));
        g.drawText (r.text, 8, 0, width - 16, height, r.kind == Row::now ? juce::Justification::centred : juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { restoreRow (row); }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }

    PluginBase& processor;
    juce::ListBox list;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, restoreButton { "Restore" };
    std::vector<Row> rows;
    int redoCount = 0;
};
