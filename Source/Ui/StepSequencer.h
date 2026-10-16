#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// Stegsekvenseraren: en rad för kicken, en per baslager och ett "lane" under där det valda
// lagrets velocity, tonlängd eller tonhöjd ritas och dras i.
//
// Två lägen: PATTERN (tonerna: stegen är en enkel pianorulle; ett lager som redigerats i pianorullen visas som en
// miniatyr i stället för stegknappar) och GATE (en egen av/på-rytm per lager, med eget rutnät, som hackar ljudet).
//
// Raderna är tidsjusterade: kicken har alltid 16 steg per takt, varje baslager sitt eget
// rutnät (16, 12, 24 ...), men en takt tar lika stor bredd i alla.
class StepSequencer : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Lane { velocity, gate, pitch, slide }; // gate = tonlängd (LENGTH); slide används inte längre
    enum class Mode { notes, gate };

    // `layerNames`: en rad per baslager (FX-versionen: bara gaten).
    StepSequencer (PluginBase& processor, juce::StringArray layerNames, std::vector<juce::Colour> layerColours);

    void setLane (Lane newLane) { lane = newLane; repaint(); }
    Lane getLane() const        { return lane; }

    void setMode (Mode newMode);
    Mode getMode() const { return mode; }

    // Anropas när man klickar på ett lagers miniatyr (öppna pianorullen för lagret).
    std::function<void (int)> onOpenPianoRoll;

    void selectLayer (int layer);
    int getSelectedLayer() const { return selectedLayer; }
    std::function<void (int)> onLayerSelected;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Target { none, kick, bass, lane, label };

    void timerCallback() override;

    int numLayers() const { return layerNames.size(); }
    int bars() const;
    BassGrid grid (int layer) const;
    int kickSteps() const { return bars() * Pattern::kickStepsPerBar; }
    int bassSteps (int layer) const { return bars() * Pattern::bassStepsPerBar (grid (layer)); }
    BassGrid gateGrid (int layer) const;
    int gateSteps (int layer) const { return bars() * Pattern::bassStepsPerBar (gateGrid (layer)); }
    bool isCustom (int layer) const { return processor.pattern.isCustom (layer); }

    Target targetAt (juce::Point<int> point, int& layer) const;
    int stepAt (int x, int numSteps) const;
    juce::Rectangle<float> stepBounds (juce::Rectangle<int> row, int step, int numSteps) const;

    void paintGrid (juce::Graphics&, juce::Rectangle<int> area) const;
    void paintToggleRow (juce::Graphics&, juce::Rectangle<int> row, int numSteps, juce::Colour colour,
                         int playingStep, const std::function<bool (int)>& isOn, bool dimmed) const;
    void paintLane (juce::Graphics&) const;
    void paintMiniature (juce::Graphics&, juce::Rectangle<int> row, int layer, juce::Colour colour, int playTick, bool dimmed) const;
    void showCustomMenu (int layer);

    void editAt (juce::Point<int> point, bool firstEvent);
    float laneValueAt (int y) const;
    juce::String laneName() const;
    juce::String describeStep (int step) const;

    // Tonerna ligger fritt i tiden; ett steg i rutnätet är de ticks det täcker. Stegets ton är den
    // första som börjar inom det (eller nullptr).
    int ticksPerStep (int layer) const { return Pattern::ticksPerStep (grid (layer)); }
    int noteTickAt (int layer, int step) const;
    Pattern::Note* noteAt (int layer, int step) const;
    int stepOfTick (int layer, int tick) const { return tick < 0 ? -1 : tick / ticksPerStep (layer); }

    // Volym och FX-send för varje rad (kicken och varje lager), som i FL Studios kanalrack.
    struct RowKnobs
    {
        juce::Slider level, fx;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment, fxAttachment;
    };
    void addRowKnobs (const juce::String& name, const juce::String& levelID, const juce::String& fxID, juce::Colour colour);
    juce::Rectangle<int> knobBounds (juce::Rectangle<int> row, int index) const;

    PluginBase& processor;
    std::vector<std::unique_ptr<RowKnobs>> rowKnobs; // [0] = kick, sedan ett per lager
    juce::StringArray layerNames;
    std::vector<juce::Colour> layerColours;
    Lane lane = Lane::velocity;
    Mode mode = Mode::notes;
    int selectedLayer = 0;

    juce::Rectangle<int> labelColumn, kickRow, laneArea;
    std::vector<juce::Rectangle<int>> bassRows;

    Target dragTarget = Target::none;
    int dragLayer = 0;
    bool paintOn = true;        // vad ett drag över av/på-steg sätter
    int dragStartPitch = 0, dragStartY = 0, dragPitch = 0;
    bool edited = false;
    int hoverStep = -1;

    int lastKickStep = -2, lastBars = 0, lastVersion = -1, lastCustomBits = -1;
    std::array<int, Pattern::numLayers> lastBassSteps { -2, -2, -2 }, lastGrids { -1, -1, -1 }, lastGateGrids { -1, -1, -1 };
};
