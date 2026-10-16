#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// Pianorullen för ett baslager, som i FL Studio: tonerna ligger fritt i tiden (1/4 ... 1/64 och
// trioler, valt med lagrets GRID som också är "snap") och i höjd, ±24 halvtoner från tonarten.
//
// - Klicka i tomt: ny ton (dra för att flytta den). Dra i en tons högerkant: längd.
// - Högerklicka på en ton: ta bort. Dubbelklicka på en ton: slide på/av (triangeln i hörnet).
// - Skroll: tonhöjd. Ctrl + skroll: zooma i tid. Shift + skroll: flytta i tid.
// - Remsan längst ner är velocity: dra i en tons stapel.
class PianoRoll : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    PianoRoll (PluginBase& processor, std::vector<juce::Colour> layerColours);

    void setLayer (int newLayer);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Drag { none, move, resize, velocity };

    void timerCallback() override;

    int bars() const;
    int totalTicks() const { return bars() * Pattern::ticksPerBar; }
    BassGrid grid() const;
    int snapTicks() const { return Pattern::ticksPerStep (grid()); }
    int snapped (float tick) const;           // närmaste steg (nedåt) i ticks
    int rootNote() const;                     // basens grundton (midi)
    int rootPitchClass() const;

    float ticksToX (float tick) const;
    float xToTicks (float x) const;
    float pitchToY (int pitch) const;         // överkanten av tonens rad
    int yToPitch (float y) const;
    float rowHeight() const { return 13.0f; }
    int visibleRows() const { return std::max (1, (int) (gridArea.getHeight() / rowHeight())); }
    void clampView();
    void fitToWidth();

    juce::Rectangle<float> noteBounds (int tick) const;
    int noteAtPoint (juce::Point<float> p, bool& onRightEdge) const;
    int noteForVelocity (float x) const;
    juce::String describe (int tick) const;
    void setHover (int tick);
    void removeAt (int tick);

    PluginBase& processor;
    std::vector<juce::Colour> layerColours;
    int layer = 0;

    juce::Rectangle<float> keyArea, rulerArea, gridArea, velocityArea;

    float pixelsPerTick = 1.0f;
    float scrollTicks = 0.0f;   // tiden längst till vänster
    float topPitch = 12.0f;     // översta synliga raden (halvtoner från tonarten)
    int justCreatedTick = -1;   // tonen som det första klicket i ett dubbelklick skapade
    int lastLength = 0;         // längden på den senast ritade tonen: nästa ton får samma

    Drag drag = Drag::none;
    int dragTick = -1;
    float grabOffsetTicks = 0.0f;
    bool edited = false;
    int hoverTick = -1;
    bool hoverEdge = false;

    int lastVersion = -1, lastPlayTick = -2, lastBars = 0, lastGrid = -1;
};
