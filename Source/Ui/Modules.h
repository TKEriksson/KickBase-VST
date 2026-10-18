#pragma once

#include "VisualTimer.h"

#include "LogoPainter.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "ListenButton.h"
#include "Cursors.h"
#include "KickDisplay.h"
#include "KickLibraryPanel.h"
#include "LfoPage.h"
#include "OscPanel.h"
#include "EnvelopeView.h"
#include "PianoRoll.h"
#include "PresetButton.h"
#include "StepSequencer.h"
#include "TabbedPanel.h"

// Rubrikraden: logotyp, presetmeny och uppspelningsstatus (DAW eller egen klocka, tempo,
// takt, tonart).
class HeaderBar : public juce::Component, private VisualTimer
{
public:
    HeaderBar (PluginBase& processor, const juce::String& subtitle);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showExportMenu();
    static constexpr int meterWidth = 72;
    int toolsEnd() const // (i Kickbase FX finns ingen EXPORT: då hamnade mätaren över tempot)
    {
        return ! previewGroup.isEmpty() ? previewGroup.getRight() : exportButton.isVisible() ? exportButton.getRight() : presets.getRight();
    }
    juce::Rectangle<int> meterBounds() const { return { toolsEnd() + 16, (getHeight() - 28) / 2, meterWidth, 28 }; }
    void updateAiTooltip();
    void chooseExportTarget (int part, int bars); // part: 0-2 = PluginBase::ExportPart, 3 = alla tre

    // Loggans glöd: en våg från vänster till höger för varje kick och en från höger till vänster för varje bastoner, i färger som sakta
    // glider mellan varandra. Tiderna är sekunder (väggklocka).
    LogoPainter logo;
    double lastKickTime = -100.0, lastInstrumentTime = -100.0, instrumentStrength = 0.0;
    int lastKickTriggers = -1;
    float lastInstrumentPeak = 0.0f;
    bool logoWasActive = true;
    float glow = 0.0f, glowShown = 0.0f; // neonets styrka: följer ljudets nivå (släckt utan ljud)
    int lastStatusHash = 0; // tempo, takt, tonart och synk: rubriken ritas bara om när något av det ändras

    PluginBase& processor;
    juce::String subtitle;
    PresetButton presets;
    juce::TextButton exportButton { "EXPORT" }; // bara i instrumentet
    // PREVIEWS (bara i instrumentet): högtalaren spelar hela synthen på den valda tonen (standard C5) så länge man håller ner den,
    // och tonen gäller alla provlyssningar.
    ListenButton listenAll;
    juce::ComboBox noteBox;
    juce::TextButton restartButton { "" };       // börjar om räknaren och mönstret vid 1.1 (egen klocka)
    juce::Rectangle<int> previewGroup, bpmPill, keyBlock;
    juce::TextButton aiButton { "AI" };         // slår på länken till MCP-bryggan (se BridgeLink)
    float peakL = 0.0f, peakR = 0.0f, holdL = 0.0f, holdR = 0.0f; // utgångsmätaren
    std::vector<float> meterLeft, meterRight;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Kicken: vågformen med pitch- och volymnoder, rattar för kroppen, klicket och drive, och plats för en klicksampling.
class KickModule : public ModulePanel, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit KickModule (PluginBase& processor);
    void resized() override;
    void paint (juce::Graphics& g) override { ModulePanel::paint (g); (sampleTabShown() ? sampleGroups : groups).paint (g, getAccent()); }
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void loadSample (const juce::File& file);
    void updateSampleLabel();
    void applyTab(); // SYNTH eller SAMPLE (processor.kickTab): visar och döljer vyernas delar
    bool sampleTabShown() const { return processor.kickTab.load() == 1; }

    PluginBase& processor;

    KickDisplay display;
    std::vector<std::unique_ptr<juce::Component>> controls;
    ControlGroups groups; // KEY, PITCH, LENGTH, BODY, CLICK, DRIVE, OUT
    ChoiceComponent keyMode;
    PresetButton presets;
    KickTabs viewTabs { Theme::kick };         // SYNTH | SAMPLE i rubrikraden
    KickSampleView sampleView;                 // SAMPLE: samplingens vågform
    KickLibraryPanel library;                  // SAMPLE: biblioteket
    ControlGroups sampleGroups;                // SAMPLE: MODE, LEVEL, TUNE, OFFSET
    std::vector<juce::Component*> sampleControls;
    juce::Label sampleHelp;
    std::unique_ptr<FxSendControl> fxSend;

    juce::Label sampleLabel;
    juce::TextButton loadButton { "LOAD" }, clearButton { "X" }, listenButton { "LISTEN" }, listenSampleButton { "HEAR SAMPLE" };
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragHover = false;
};

// Samplingens vågform med startpunkten markerad.
class SampleView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    SampleView (PluginBase& processor, int layer, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    void timerCallback() override;

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    const ClickSample* shown = nullptr;
    float lastStart = -1.0f;
    bool hover = false;
    juce::MouseCursor speakerCursor = Cursors::speaker();
    float marker = -1.0f;                 // var i samplingen lagret spelar nu (0..1), -1 = inget
    double flashWallTime = -1000.0;       // när lagret senast började spela samplingen (ms), för blixten
};

// Samplingen som ljudkälla i ett baslager (en flik bredvid OSC 1-3): vågformen, filen (LOAD, ta bort) och rattarna.
class SamplePage : public juce::Component, private juce::Timer
{
public:
    SamplePage (PluginBase& processor, int layer, juce::Colour accent);
    void resized() override;
    void loadSample (const juce::File& file);

private:
    void timerCallback() override;
    void updateLabel();

    PluginBase& processor;
    const int layer;
    SampleView view;
    juce::Label label;
    juce::TextButton loadButton { "LOAD" }, clearButton { "X" }, hearButton { "HEAR" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<juce::Component>> controls; // LEVEL, START, ROOT, LOOP, KEY
};

// Ett baslager (bara instrumentet): källorna i flikar (OSC 1-3 och SAMPLE, var och en med av/på), filter- och
// amplitud-envelope och rattar. En WAV kan släppas på sidan (eller laddas med LOAD på SAMPLE-fliken).
class InstrumentModule : public ModulePanel, public juce::FileDragAndDropTarget
{
public:
    InstrumentModule (PluginBase& processor, int layer);
    void resized() override;
    void paint (juce::Graphics& g) override
    {
        ModulePanel::paint (g);
        groups.paint (g, getAccent());
        if (modOpen)
            modGroups.paint (g, getAccent());
    }
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    PluginBase& processor;
    bool dragHover = false;
    juce::TextButton listenButton { "LISTEN" }; // spelar lagrets ton

    const int layer;
    ParamRefs refs;
    InstrumentParams current;
    float ampValueAt (double seconds) const;
    float pitchValueAt (double seconds) const;
    EnvelopeView::NoteOff noteOffNow() const;

    EnvelopeView envelope { 0.5 };
    TabbedPanel sources;
    std::array<std::unique_ptr<OscPanel>, ParamID::oscsPerLayer> oscs;
    std::unique_ptr<SamplePage> samplePage;
    std::vector<std::unique_ptr<juce::Component>> controls;
    ControlGroups groups; // PITCH, SUB, NOISE, FILTER, FILTER ENV, VOLUME ENV, WARP ENV, DRIVE, OUT

    // MODULATION (dragspel, stängt som standard): ENV 2, ENV 3 och instrumentets övriga källor, med handtag att dra till rattar.
    juce::TextButton modToggle;
    std::vector<juce::Component*> modControls;
    ControlGroups modGroups;
    bool modOpen = false;
    void updateModToggle();
    PresetButton presets;
    std::unique_ptr<FxSendControl> fxSend;
};

// INSTRUMENT-fliken: ett instrument i taget (1-10), valt med en väljare i flikraden, med + och − för att lägga till och ta bort. Panelerna
// (InstrumentModule) skapas när ett instrument först visas och rivs när det tas bort.
class InstrumentHost : public juce::Component, private juce::Timer
{
public:
    static constexpr int headerWidth = 650;

    explicit InstrumentHost (PluginBase& processor);
    ~InstrumentHost() override;

    juce::Component* getHeader() { return &header; }
    void resized() override;
    void select (int layer);

private:
    struct Header : public juce::Component
    {
        void resized() override;
        juce::TextButton selector, addButton { "+" }, removeButton { "-" };
        juce::Component* extras = nullptr;
    };

    void timerCallback() override;
    void showSelector();
    void rename (int layer);
    void addInstrument();
    void removeInstrument();
    void refresh();
    InstrumentModule& moduleFor (int layer);

    PluginBase& processor;
    Header header;
    std::array<std::unique_ptr<InstrumentModule>, ParamID::numInstrumentLayers> modules;
    std::array<juce::Component*, ParamID::numInstrumentLayers> extras {};
    int current = 0, lastCount = -1, lastNamesVersion = -1;
    bool confirmOpen = false;
    juce::Component::SafePointer<juce::AlertWindow> renameDialog;
};

// Gaten (bara FX-versionen): formar ljudet från den andra synthen.
class GateModule : public ModulePanel
{
public:
    explicit GateModule (PluginBase& processor);
    void resized() override;

private:
    ParamRefs refs;
    float attackMs = 1.0f, releaseMs = 10.0f;

    EnvelopeView envelope { 0.25 };
    ChoiceComponent key;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    PresetButton presets;
    juce::Label help;
};

// Mönstret (en egen flik). Två lägen per lager, som man väljer med PATTERN | GATE:
// - PATTERN: tonerna. Stegen är en enkel pianorulle (VEL, LENGTH och PITCH per ton i en lane); PIANO ROLL är den avancerade
//   varianten av samma toner, och ett lager som redigerats där visas som en miniatyr i stället för stegknappar.
// - GATE: en egen av/på-rytm som hackar lagrets ljud, med eget rutnät (GRID), djup (DEPTH) och mjukhet (SMOOTH). Den ändrar
//   inte tonerna.
class PatternModule : public juce::Component, private juce::Timer
{
public:
    PatternModule (PluginBase& processor, bool instrument);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void selectLane (StepSequencer::Lane lane);
    void showLayer (int layer);
    void showMode (StepSequencer::Mode newMode);
    void showFillMenu();
    void showKickFillMenu();
    void showGateFillMenu();
    void showPianoRoll (bool open);
    void updateVisibility();

    PluginBase& processor;
    const bool instrument;
    ChoiceComponent length;
    std::unique_ptr<ChoiceComponent> noteEnd;
    std::unique_ptr<KnobComponent> swingKnob, humanizeKnob; // groove för hela mönstret (instrumenten)
    std::vector<std::unique_ptr<KnobComponent>> delays, swings, humanizes; // instrumentets egna SWING, HUMAN och DELAY, under GRID
    PresetButton presets;
    // EDIT: vad lanet under det valda lagrets rad visar och ändrar (velocity, tonlängd, tonhöjd), som en rullgardinslista.
    juce::Label laneLabel;
    juce::ComboBox laneBox;
    std::vector<StepSequencer::Lane> laneOrder;
    void updateLengthKnobs();
    StepSequencer sequencer;
    PianoRoll pianoRoll;
    juce::TextButton patternTab { "PATTERN" }, gateTab { "GATE" };
    PianoRollButton pianoButton;
    void updatePianoButton();
    std::unique_ptr<ChoiceComponent> kickGrid;                   // kickens rutnät
    int lastNamesVersion = -1;
    // Längdändringar: förkortas en rad tas det som inte får plats bort (efter en bekräftelse om något går förlorat), förlängs den
    // upprepas det den spelade. Se checkLengthChanges.
    std::array<int, numPatternRows> lastRowBars = filledArray<int, numPatternRows> (-1);        // radernas längd förra gången (kick, instrument 1-10)
    std::array<int, numPatternRows + 1> lastChoices = filledArray<int, numPatternRows + 1> (-1); // valen för TOTAL LENGTH och radernas längd förra gången
    int lastActive = -1;
    int lastPatternVersion = -1;
    bool confirmOpen = false;
    void checkLengthChanges();
    void extendRow (int row, int beforeBars, int nowBars, bool repeat); // förlängd rad: upprepa det den spelade, eller töm det nya
    void trimRow (int row, int bars);                                   // förkortad rad: ta bort det som inte får plats
    juce::String rowName (int row) const;
    void restoreChoices (const std::array<int, numPatternRows + 1>& choices);

    // Per lager (bara det valda syns): notrutnätet och tonlängden (PATTERN), och GATE: på/av, rutnät, djup och mjukhet.
    std::vector<std::unique_ptr<ChoiceComponent>> grids, gateGrids;
    std::vector<std::unique_ptr<KnobComponent>> lengths, gateDepths, gateSmooths;
    std::vector<std::unique_ptr<juce::TextButton>> gateOnButtons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> gateOnAttachments;
    juce::TextButton fillButton { "FILL..." }, gateFillButton { "FILL..." };
    StepSequencer::Mode mode = StepSequencer::Mode::notes;
    juce::Rectangle<int> headingBounds, blockFrame; // blockFrame: ramen runt det valda instrumentets inställningar
    int shownLayer = 0;
};
