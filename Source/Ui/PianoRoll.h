#pragma once

#include <map>
#include <set>
#include "../Core/PluginBase.h"
#include "Theme.h"

// Pianorullen för ett baslager, som i FL Studio: tonerna ligger fritt i tiden (1/4 ... 1/64 och
// trioler) och i höjd, ±96 halvtoner från tonarten.
//
// - Klicka i tomt: ny ton (dra för att flytta den). Dra i en tons högerkant: längd.
// - Skift + klick på en ton: lägg till/ta bort i markeringen. Skift + dra i tomt: markera med en ruta.
//   Ctrl + A markerar alla. Markerade toner flyttas, förlängs och tas bort tillsammans.
// - Högerklicka på en ton: ta bort. GLIDE-knappen: de toner man ritar blir glide-toner (som slide-toner i FL Studio):
//   en glide-ton slår inte an utan glider tonen som klingar till sin tonhöjd under sin egen längd, och tonen blir kvar
//   där tills den slutar. Dubbelklicka på en ton: glide på/av för den.
// - Piltangenter flyttar de markerade (upp/ner = halvton, skift = oktav, vänster/höger = ett snap-steg).
// - Skroll: tonhöjd. Ctrl + skroll: zooma i tid. Shift + skroll: flytta i tid.
// - Verktygsraden: SNAP av/på, rutnätet (snap) 1/4 ... 1/64, radhöjd, markera alla och ta bort, GLIDE, samt
//   PRESET (spara och ladda pianorullar) och MIDI (exportera och importera MIDI-filer).
// - Remsan längst ner är velocity: dra i en tons stapel.
class PianoRoll : public juce::Component, public juce::SettableTooltipClient, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    PianoRoll (PluginBase& processor, std::vector<juce::Colour> layerColours);

    void setLayer (int newLayer);

    // Anropas när pianorullens toner tagits bort (lagret går tillbaka till stegmönstret).
    std::function<void()> onDeleted;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { none, move, resize, velocity, box };

    struct NoteData
    {
        int length = 17, pitch = 0;
        float velocity = 0.8f;
        bool slide = false;
    };
    using NoteMap = std::map<int, NoteData>; // starttick -> ton

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;
    void updateScrollBars();

    int bars() const;
    int totalTicks() const { return bars() * Pattern::ticksPerBar; }
    BassGrid grid() const;
    int snapTicks() const;
    int snapped (float tick) const;           // närmaste steg (nedåt) i ticks
    int rootNote() const;                     // basens grundton (midi)
    int rootPitchClass() const;

    float ticksToX (float tick) const;
    float xToTicks (float x) const;
    float pitchToY (int pitch) const;         // överkanten av tonens rad
    int yToPitch (float y) const;
    float rowHeight() const { return rowH; }
    int visibleRows() const { return std::max (1, (int) (gridArea.getHeight() / rowHeight())); }
    void clampView();
    void fitToWidth();

    juce::Rectangle<float> noteBounds (int tick) const;
    int noteAtPoint (juce::Point<float> p, bool& onRightEdge) const;
    int noteForVelocity (float x) const;
    juce::String describe (int tick) const;
    void setHover (int tick);

    NoteMap readNotes() const;
    void applyNotes (const NoteMap& target); // skriver bara det som ändrats, så att ljudtråden aldrig ser ett tomt mönster
    void pruneSelection();
    void selectAll();
    void deleteSelected();
    void moveSelected (int deltaTicks, int deltaPitch);
    void changeRowHeight (float delta);
    void showPresetMenu();
    void showMidiMenu();
    void askForNameAndSave();
    void afterLoad (int bars, BassGrid newGrid); // sätter mönsterlängd och rutnät efter att en pianorulle laddats
    int soundingRoot() const; // basens grundton med lagrets oktav (tonhöjden 0), som MIDI-tonnummer
    void commit(); // markerar lagret som pianorulle och talar om att mönstret ändrats
    int pitchBefore (int tick) const;
    void attachGridBox();

    PluginBase& processor;
    std::vector<juce::Colour> layerColours;
    int layer = 0;

    juce::Rectangle<float> toolbarArea, keyArea, rulerArea, gridArea, velocityArea;

    // Verktygsraden.
    juce::TextButton snapButton { "SNAP" }, smallerButton { "-" }, biggerButton { "+" }, allButton { "ALL" }, deleteButton { "DEL" },
                     glideButton { "GLIDE" }, presetButton { "PRESET" }, midiButton { "MIDI" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::AlertWindow> saveDialog;
    juce::Rectangle<float> infoArea;
    juce::ComboBox gridBox;
    juce::ScrollBar hBar { false }, vBar { true }; // tid och tonhöjd
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAttachment;
    bool snapEnabled = true;

    float rowH = 13.0f;
    float pixelsPerTick = 1.0f;
    float scrollTicks = 0.0f;   // tiden längst till vänster
    float topPitch = 12.0f;     // översta synliga raden (halvtoner från tonarten)
    int justCreatedTick = -1;   // tonen som det första klicket i ett dubbelklick skapade
    int lastLength = 0;         // längden på den senast ritade tonen: nästa ton får samma

    std::set<int> selection;    // starttick för de markerade tonerna

    Drag drag = Drag::none;
    int dragTick = -1;          // tonen som greps (dess nuvarande starttick)
    int dragStartTick = 0, dragStartPitch = 0;
    float grabOffsetTicks = 0.0f;
    NoteMap dragSnapshot;       // alla toner när draget började
    std::set<int> dragSelection; // de markerade när draget började (starttick)
    juce::Point<float> boxStart, boxEnd;
    std::set<int> boxBase;      // markeringen innan rutan (skift lägger till)
    bool edited = false;
    int hoverTick = -1;
    bool hoverEdge = false;

    int lastVersion = -1, lastPlayTick = -2, lastBars = 0, lastGrid = -1;
};
