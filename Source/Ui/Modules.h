#pragma once

#include "VisualTimer.h"

#include "LogoPainter.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "ListenButton.h"
#include "HoldAudition.h"
#include "Cursors.h"
#include "KickDisplay.h"
#include "KickLibraryPanel.h"
#include "LfoPage.h"
#include "OscPanel.h"
#include "FmPanel.h"
#include "InstrMixPanel.h"
#include "EnvelopeView.h"
#include "FilterPanel.h"
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
    ~HeaderBar() override { processor.transportHold = false; } // ett scrubbhopp som inte hunnit släppas får inte frysa klockan
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
    void showProjectMenu();
    void saveProject();
    void openProject();
    void chooseExportTarget (int part, int bars); // part: 0-2 = full, kick, alla instrument; 3 = tre filer; 4 + n = instrument n
    bool exportWithoutEffects = false;            // rutan "Without effects" i EXPORT-menyn (gränssnittets val, sparas inte)

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
    ListenButton listenAll, listenRoll; // listenRoll syns bara medan en pianorulle är öppen
    bool rollShown = false;
    void syncRollButton(); // visar/döljer LISTEN PIANO ROLL när en pianorulle öppnas eller stängs
    // Egen fast takt (10 Hz): topbarens takt kan sänkas av OPTIMIZATIONS, och knappen ska komma direkt när rullen öppnas.
    struct RollWatch : juce::Timer
    {
        explicit RollWatch (std::function<void()> f) : callback (std::move (f)) { startTimerHz (10); }
        void timerCallback() override { callback(); }
        std::function<void()> callback;
    };
    std::unique_ptr<RollWatch> rollWatch;
    juce::ComboBox noteBox;
    juce::TextButton restartButton { "" };       // börjar om räknaren och mönstret vid 1.1 (egen klocka)
    juce::Rectangle<int> previewGroup, bpmPill, keyBlock;

    // Transport i egen klocka (D3a): dubbelklicka tempot eller takten för att skriva ett värde, hjulet över takten scrubbar (musiken står still tills hjulet vilat).
    // Spelar värden är de låsta ("styrs av DAW").
    juce::Label bpmLabel, barLabel;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void updateTransportLabels (bool following);
    struct ReleaseTimer : juce::Timer
    {
        void timerCallback() override { stopTimer(); if (callback) callback(); }
        std::function<void()> callback;
    };
    ReleaseTimer scrubRelease;
    int scrubBar = -1;          // takten (0-räknad) hjulet pekar på medan man scrubbar, -1 = ingen scrubbning
    int lastFollowing = -1;
    bool bridgeShown = false;
    juce::TextButton projectButton { "PROJECT" }; // meny: spara/öppna projekt, exportera, AI-server av/på (statuspunkten visar AI-länken)
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
    juce::TextButton loadButton { "LOAD" }, clearButton { "X" }, listenSampleButton { "HEAR SAMPLE" };
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
    void mouseUp (const juce::MouseEvent&) override { hold.stop(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    void timerCallback() override;

    PluginBase& processor;
    HoldAudition hold { processor }; // klick och håll: samplingen spelas så länge musen är nere
    const int layer;
    juce::Colour accent;
    const ClickSample* shown = nullptr;
    bool shownSlicer = false;
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
    void applySlicing();      // ComboBox/Slider -> samplingens skivinställningar
    void syncSlicerControls(); // samplingen -> ComboBox/Slider (efter laddning av projekt)

    PluginBase& processor;
    const int layer;
    SampleView view;
    juce::ToggleButton slicerToggle { "SLICER" }, reverseToggle { "REVERSE" };
    juce::ToggleButton filterSendToggle { "SEND THROUGH FILTER" }; // samma parameter som i FILTER-fliken
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slicerAttachment, reverseAttachment, filterSendAttachment;
    std::unique_ptr<KnobComponent> mapToKnob;
    juce::ComboBox sliceModeBox;
    juce::Slider gridAmount, sensitivityAmount; // en per läge, så att inget område byts under användaren
    bool syncing = false;
    juce::Label label;
    juce::TextButton loadButton { "LOAD" }, clearButton { "X" }, hearButton { "HEAR" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<juce::Component>> controls; // LEVEL, START, ROOT, LOOP, KEY
};

// Liten oscilloskop i SUB-gruppens rubrikrad: ritar subbens vågform (WAVE, höjd efter LEVEL) och rör sig, med glöd, bara när lagret
// låter. Subben spelar även när alla oscillatorer är av, och då syns det här.
class SubScope : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    SubScope (PluginBase& processor, int layer);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    PluginBase& processor;
    const int layer;
    float level = 0.0f, phase = 0.0f, glow = 0.0f;
    int wave = 0;
    bool sounding = false;
};

// Ett baslager (bara instrumentet): källorna i flikar (OSC 1-3 och SAMPLE, var och en med av/på), filter- och
// amplitud-envelope och rattar. En WAV kan släppas på sidan (eller laddas med LOAD på SAMPLE-fliken).
class InstrumentModule : public ModulePanel, public juce::FileDragAndDropTarget
{
public:
    InstrumentModule (PluginBase& processor, int layer);
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    PluginBase& processor;
    bool dragHover = false;

    const int layer;
    ParamRefs refs;
    InstrumentParams current;
    float ampValueAt (double seconds) const;
    float pitchValueAt (double seconds) const;
    EnvelopeView::NoteOff noteOffNow() const;

    // INSTR-fliken (fas L): envelopegrafen och rattarna under den, i signalens ordning. Rattarna ligger på sidan och ritas av den.
    struct InstrPage : public juce::Component
    {
        explicit InstrPage (InstrumentModule& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.groups.paint (g, owner.getAccent()); }
        void resized() override { owner.layoutInstrPage(); }
        InstrumentModule& owner;
    };
    InstrPage instrPage { *this };
    void layoutInstrPage();
    SubScope subScope { processor, layer };
    juce::ToggleButton noiseFilterButton { "FILTER" }; // NOISE-gruppens skicka-genom-filter (samma parameter som i FILTER-fliken)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseFilterAttachment;

    EnvelopeView envelope { 0.5 };
    HoldAudition hold { processor };      // klick och håll på LISTEN: instrumentet spelas på förhandsvisningstonen
    ListenButton envelopeListen;          // högtalaren i hörnet av envelope-grafen
    TabbedPanel sources;
    std::array<std::unique_ptr<OscPanel>, ParamID::oscsPerLayer> oscs;
    std::unique_ptr<SamplePage> samplePage;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<FmPanel> fmPanel;                                     // FM-fliken (sist, så att fliknumren före den inte förskjuts)
    std::unique_ptr<InstrMixPanel> mixPanel;                              // MIX-fliken: nivå, pan och bredd per källa + sammanlagd vågform (efter FM av samma skäl)
    std::unique_ptr<FilterPage> filterPage;                             // FILTER-fliken: kurvan, Filter 1 och Filter 2
    ControlGroups groups; // PITCH, SUB, NOISE, FILTER ENV, VOLUME ENV, WARP ENV, DRIVE, OUT (ENV 2/3, VELOCITY, RANDOM: ENV-fliken)
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
    int getCurrent() const { return current; }
    void mouseDown (const juce::MouseEvent& event) override;

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
    void resetPanels(); // river alla paneler (färg/klistra in/ta bort): de byggs om när instrumentet visas
    InstrumentModule& moduleFor (int layer);

    PluginBase& processor;
    Header header;
    std::array<std::unique_ptr<InstrumentModule>, ParamID::numInstrumentLayers> modules;
    std::array<juce::Component*, ParamID::numInstrumentLayers> extras {};
    int current = 0, lastCount = -1, lastNamesVersion = -1, lastStructureVersion = 0;
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
// Mönstrets globala inställningar (NOTE END, SWING, HUMANIZE) kompakt i PATTERN-flikens flikrad, så att vänsterkolumnen
// bara visar det valda spåret. Bara i instrumentpluginet.
class PatternGrooveBar : public juce::Component
{
public:
    explicit PatternGrooveBar (juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    static constexpr int width = 450;

private:
    juce::Label noteEndLabel, swingLabel, humanizeLabel;
    ChoiceComponent::Box noteEnd;
    FxSendControl::AmountSlider swing, humanize;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noteEndAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment, humanizeAttachment;
};

class PatternModule : public juce::Component, private juce::Timer
{
public:
    PatternModule (PluginBase& processor, bool instrument);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Component* getGrooveBar() { return grooveBar.get(); } // till flikraden (nullptr i kickpluginet)

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
    void togglePopOut(); // D5: pianorullen i eget fönster / tillbaka

    PluginBase& processor;
    const bool instrument;
    std::unique_ptr<PatternGrooveBar> grooveBar; // NOTE END, SWING, HUMANIZE för hela mönstret (instrumenten)
    std::vector<std::unique_ptr<KnobComponent>> delays, swings, humanizes; // instrumentets egna SWING, HUMAN och DELAY, under GRID
    PresetButton presets;
    // EDIT: vad lanet under det valda lagrets rad visar och ändrar (velocity, tonlängd, tonhöjd), som en rullgardinslista.
    juce::Label laneLabel;
    juce::ComboBox laneBox;
    bool lastLaneEditable = true; // det valda lagret har inte pianorulle: EDIT IN LANE syns och tar plats
    std::vector<StepSequencer::Lane> laneOrder;
    void updateLengthKnobs();
    StepSequencer sequencer;
    PianoRoll pianoRoll;
    struct PopWindow : juce::DocumentWindow // deklareras efter pianoRoll så att fönstret förstörs först
    {
        PopWindow() : juce::DocumentWindow ("Kickbase Piano Roll", Theme::panel, juce::DocumentWindow::allButtons) {}
        void closeButtonPressed() override { if (onClose) onClose(); }
        std::function<void()> onClose;
    };
    std::unique_ptr<PopWindow> popWindow;
    juce::TextButton patternTab { "PATTERN" }, gateTab { "GATE" };
    PianoRollButton pianoButton;
    void updatePianoButton();
    std::unique_ptr<ChoiceComponent> kickGrid;                   // kickens rutnät
    int lastNamesVersion = -1;
    int lastActive = -1;
    bool confirmOpen = false;
    juce::String rowName (int row) const;

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
