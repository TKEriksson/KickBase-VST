#pragma once

#include "../Core/PluginBase.h"
#include "../Core/LibraryIndex.h"
#include "Cursors.h"
#include "Theme.h"

// Delarna av kickens SAMPLE-vy (fas A i docs/content-plan.md): vyflikarna SYNTH | SAMPLE, samplingens vågform och biblioteket.

// SYNTH | SAMPLE: två knappar som bara byter vy. De ändrar ingen parameter: att byta flik ändrar aldrig ljudet.
class KickTabs : public juce::Component
{
public:
    KickTabs (juce::Colour accent);
    void resized() override;
    void select (int tab); // 0 SYNTH, 1 SAMPLE (utan återanrop)

    std::function<void (int)> onChange;

private:
    juce::TextButton synthButton { "SYNTH" }, sampleButton { "SAMPLE" };
};

// Den valda samplingens vågform (statisk: ritas om bara när samplingen byts, ingen timer). Ett klick spelar bara samplingen.
class KickSampleView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KickSampleView (PluginBase& processor);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh(); // ritar om om det är en annan sampling än sist (anropas av modulens långsamma timer)

private:
    PluginBase& processor;
    const ClickSample* shown = nullptr; // bara för jämförelse, läses aldrig
    juce::MouseCursor speakerCursor = Cursors::speaker();
};

// Biblioteket (kategorin Kicks): sök, lista och val. Ett klick på en rad laddar kicken som kickens sampling (som LOAD) och sätter MODE
// till REPLACE om den stod på CLICK. Listan läser bara den lista som indexet senast skickade (aldrig indexet i paint), och skanningen
// görs på indexets bakgrundstråd: vid start, när vyn visas och med RESCAN.
class KickLibraryPanel : public juce::Component, public juce::SettableTooltipClient,
                         private juce::ChangeListener, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    KickLibraryPanel (PluginBase& processor, juce::Colour accent);
    ~KickLibraryPanel() override;

    void paintOverChildren (juce::Graphics&) override; // texten när listan är tom
    void resized() override;
    void visibilityChanged() override;
    void setLoadedName (const juce::String& name); // namnet på samplingen som är laddad (märks i listan)
    void loadRow (int row);                         // laddar kicken på en rad i listan (ett klick); MODE blir REPLACE om den stod på CLICK

    std::function<void()> onLoaded; // efter att en kick laddats

private:
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { loadRow (row); }
    void returnKeyPressed (int row) override { loadRow (row); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void textEditorTextChanged (juce::TextEditor&) override { applyFilter(); }

    void applyFilter();
    void updateStatus();

    juce::SharedResourcePointer<LibraryIndex> index; // först: förstörs sist (lyssnaren tas bort i destruktorn)
    PluginBase& processor;
    juce::Colour accent;

    LibraryIndex::Snapshot snapshot;  // listan som indexet senast skickade
    std::vector<int> shown;           // index i snapshot som matchar sökningen
    int hiddenPackages = 0;           // paket som inte visas (fel i manifestet)
    juce::String loadedName, message;

    juce::TextEditor searchBox;
    juce::Label statusLabel;
    juce::TextButton rescanButton { "RESCAN" };
    juce::ListBox list { "kick library", this };
};
