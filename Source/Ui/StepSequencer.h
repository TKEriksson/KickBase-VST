#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"
#include "ListenButton.h"

// Stegsekvenseraren: en rad för kicken, en per baslager och ett "lane" under där det valda
// lagrets velocity, tonlängd eller tonhöjd ritas och dras i.
//
// Två lägen: PATTERN (tonerna: stegen är en enkel pianorulle; ett lager som redigerats i pianorullen visas som en
// miniatyr i stället för stegknappar) och GATE (en egen av/på-rytm per lager, med eget rutnät, som hackar ljudet).
//
// Raderna är tidsjusterade: kicken har alltid 16 steg per takt, varje baslager sitt eget
// rutnät (16, 12, 24 ...), men en takt tar lika stor bredd i alla.
class StepSequencer : public juce::Component, public juce::SettableTooltipClient, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    enum class Lane { velocity, gate, pitch, slide }; // gate = tonlängd (LENGTH); slide används inte längre
    enum class Mode { notes, gate };

    // `layerNames`: en rad per instrument (högst 10; FX-versionen: bara gaten). Hur många som visas styrs av instrumentCount.
    StepSequencer (PluginBase& processor, juce::StringArray layerNames, std::vector<juce::Colour> layerColours);

    void setLane (Lane newLane) { lane = newLane; repaint(); }
    Lane getLane() const        { return lane; }

    void setMode (Mode newMode);
    Mode getMode() const { return mode; }

    // Anropas när man klickar på ett lagers miniatyr (öppna pianorullen för lagret).
    std::function<void (int)> onOpenPianoRoll;

    void selectLayer (int layer);
    void renameLayer (int layer); // dialog för att döpa om ett baslager (instrumentet)
    int getSelectedLayer() const { return selectedLayer; }
    std::function<void (int)> onLayerSelected;

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
    enum class Target { none, bass, lane, label }; // bass = en lagerrad (också kicken)

    void timerCallback() override;

    int totalLayers() const { return layerNames.size(); }   // alla instrument som finns i gränssnittet
    int numLayers() const { return processor.isEffectVersion() ? 1 : std::min (totalLayers(), processor.activeInstruments()); } // de som används
    int bars() const;
    PatternLengths lengths() const { return Params::readPatternLengths (processor.apvts); }

    // Ett mönster på mer än fyra takter visas en sida (fyra takter) i taget; sidknapparna byter sida.
    static constexpr int barsPerPage = 4;
    int viewBars() const { return std::min (bars(), barsPerPage); }
    int numPages() const { return std::max (1, bars() / barsPerPage); }
    int pageOffset (int visibleSteps) const { return page * visibleSteps; } // första synliga stegets plats i mönstret
    void setPage (int newPage);

    InstrumentGrid grid (int layer) const;
    int instrumentSteps (int layer) const { return viewBars() * Pattern::instrumentStepsPerBar (grid (layer)); }
    InstrumentGrid gateGrid (int layer) const;
    int gateSteps (int layer) const { return viewBars() * Pattern::instrumentStepsPerBar (gateGrid (layer)); }

    // Radens egna längd i steg (kortare än mönstret när raden har en egen längd): efter den loopar eller tystnar raden.
    // Lager 0-9 är instrumenten och lager 10 (Pattern::kickLayer) är kicken. Radens nummer för längderna: kick 0, instrument 1-10.
    static int rowOf (int layer) { return layer == Pattern::kickLayer ? 0 : layer + 1; }
    int instrumentOwnSteps (const PatternLengths& l, int layer) const { return l.barsOf (rowOf (layer)) * Pattern::instrumentStepsPerBar (grid (layer)); }
    int gateOwnSteps (const PatternLengths& l, int layer) const { return l.barsOf (rowOf (layer)) * Pattern::instrumentStepsPerBar (gateGrid (layer)); }

    juce::Rectangle<int> rowBounds (int layer) const { return rowRects[(size_t) rowOf (layer)]; }
    juce::Colour colourOf (int layer) const { return layer == Pattern::kickLayer ? Theme::kick : layerColours[(size_t) layer]; }
    juce::String nameOf (int layer) const;
    bool renameable() const { return totalLayers() > 1; } // instrumentets lager kan döpas om; FX-versionens GATE-rad inte

    int laneStepAt (int x) const { const auto l = lengths(); return stepAt (x, selectedLayer, instrumentSteps (selectedLayer), instrumentOwnSteps (l, selectedLayer)); }
    bool isCustom (int layer) const { return processor.pattern.isCustom (layer); }

    Target targetAt (juce::Point<int> point, int& layer) const;
    // Stegets plats i radens eget mönster (loopade kopior räknas om till originalet), eller -1 i en tyst del av en rad
    // som inte loopar. `numSteps` är de synliga stegen.
    int stepAt (int x, int layer, int numSteps, int ownSteps) const;
    juce::Rectangle<float> stepBounds (juce::Rectangle<int> row, int step, int numSteps) const;

    void paintGrid (juce::Graphics&, juce::Rectangle<int> area) const;
    // `numSteps` är de synliga stegen; `offset` är sidans första steg, som läggs till vid frågor och `playingStep`.
    // `ownSteps`: radens egen längd; efter den visas raden som dämpade kopior (loop) eller tom. `playingStep` är radens eget steg.
    void paintToggleRow (juce::Graphics&, juce::Rectangle<int> row, int numSteps, int offset, int ownSteps, bool loops, juce::Colour colour,
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

    // Uppspelningsmarkören: det steg (eller i GATE-läget det gatesteg) som raden visar som spelande. När bara markörerna flyttas ritas
    // bara de celler om där markören var och hamnar, inte hela rutnätet (som annars ritades om vid varje sextondel).
    int markerStep (int layer) const;
    void repaintMarker (int layer, int oldStep, int newStep);

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
    int page = 0;
    juce::TextButton previousPageButton { "<" }, nextPageButton { ">" }, followButton { "FOLLOW" };
    std::vector<std::unique_ptr<juce::ComboBox>> lengthBoxes; // radens längd: [0] kick, sedan baslagren
    std::vector<std::unique_ptr<ListenButton>> listenButtons; // högtalare: spelar raden
    std::vector<std::unique_ptr<LoopToggle>> loopToggles;     // loopar raden?
    // M (mute: samma som instrumentets på/av) och S (solo: bara de soloade spelar), för kicken och varje instrument. Samma index som listenButtons.
    struct RowSwitches
    {
        juce::TextButton mute { "M" }, solo { "S" };
    };
    std::vector<std::unique_ptr<RowSwitches>> rowSwitches;
    void refreshSwitches();
    bool rowSilent (int layer) const; // raden hörs inte nu: avstängd, eller något annat är soloat
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> loopAttachments;
    juce::Rectangle<int> overviewArea;                        // remsan som visar radernas längder och var man är
    bool overviewDragging = false;
    void setPageFromOverview (int x);
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> lengthAttachments;
    juce::Rectangle<int> pageArea, headerLabels;

    juce::Rectangle<int> labelColumn, stepsColumn, laneArea;
    std::array<juce::Rectangle<int>, numPatternRows> rowRects; // radernas ytor (tom = utanför, bortscrollad eller instrumentet används inte)
    int rowsTop = 0, rowsBottom = 0;
    bool revealSelected = true;
    int rowScroll = 0, visibleRows = numPatternRows; // första synliga rad och hur många som får plats (fler rader än plats: scrolla)
    void scrollRows (int delta);
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    // Hörnet där rullningslisten slutar: dra uppåt/neråt för att zooma radernas höjd.
    struct ZoomCorner : public juce::Component, public juce::SettableTooltipClient
    {
        ZoomCorner();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        std::function<void()> onStart, onReset;
        std::function<void (int)> onDragBy;
    };
    juce::ScrollBar scrollBar { true };
    ZoomCorner zoomCorner;
    int effectiveRowHeight = 50, zoomStartHeight = 50;
    bool compactRows = false;          // låga rader: rattarna och knapparna ligger på en rad
    int labelWidthNow = 132, knobPx = 28;
    juce::Rectangle<int> selectionFrame; // ramen runt det valda lagrets rad och dess lane
    juce::Rectangle<int> selectorBounds (juce::Rectangle<int> row) const;

    Target dragTarget = Target::none;
    int dragLayer = 0;
    bool paintOn = true;        // vad ett drag över av/på-steg sätter
    int dragStartPitch = 0, dragStartY = 0, dragPitch = 0;
    bool edited = false;
    int hoverStep = -1;

    int lastSwitchBits = -1;
    int lastBars = 0, lastVersion = -1, lastCustomBits = -1, lastNamesVersion = -1, lastActive = -1;
    juce::Component::SafePointer<juce::AlertWindow> renameDialog;
    std::array<int, Pattern::numNoteLayers> lastMarkers = filledArray<int, Pattern::numNoteLayers> (-2); // markerStep per lager (kicken sist)
    std::array<int, Pattern::numLayers> lastGrids = filledArray<int, Pattern::numLayers> (-1), lastGateGrids = filledArray<int, Pattern::numLayers> (-1);
};
