#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "Cursors.h"
#include "KickDisplay.h"
#include "OscPanel.h"
#include "EnvelopeView.h"
#include "PianoRoll.h"
#include "PresetButton.h"
#include "StepSequencer.h"
#include "TabbedPanel.h"

// Rubrikraden: logotyp, presetmeny och uppspelningsstatus (DAW eller egen klocka, tempo,
// takt, tonart).
class HeaderBar : public juce::Component, private juce::Timer
{
public:
    HeaderBar (PluginBase& processor, const juce::String& subtitle);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showExportMenu();
    static constexpr int meterWidth = 72;
    int toolsEnd() const { return listenButton.isVisible() ? noteButton.getRight() : exportButton.getRight(); }
    void updateAiTooltip();
    void chooseExportTarget (int part, int bars); // part: 0-2 = PluginBase::ExportPart, 3 = alla tre

    PluginBase& processor;
    juce::String subtitle;
    PresetButton presets;
    juce::TextButton exportButton { "EXPORT" }; // bara i instrumentet
    juce::TextButton listenButton { "LISTEN" }; // spelar den valda tonen (standard C5) för hela synthen (bara i instrumentet)
    juce::TextButton noteButton { "C5" };       // väljer tonen som LISTEN och förhandsspelningen spelar
    int listeningNote = -1;                     // tonen som LISTEN håller just nu, -1 = ingen
    void showNoteMenu();
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
    void paint (juce::Graphics& g) override { ModulePanel::paint (g); groups.paint (g, getAccent()); }
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void loadSample (const juce::File& file);
    void updateSampleLabel();

    PluginBase& processor;

    KickDisplay display;
    std::vector<std::unique_ptr<juce::Component>> controls;
    ControlGroups groups; // KEY, PITCH, LENGTH, BODY, CLICK, DRIVE, OUT
    ChoiceComponent keyMode;
    PresetButton presets;
    std::unique_ptr<FxSendControl> fxSend;

    juce::Label sampleLabel;
    juce::TextButton loadButton { "Load" }, clearButton { "X" }, listenButton { "LISTEN" }, listenSampleButton { "HEAR SAMPLE" };
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
class BassModule : public ModulePanel, public juce::FileDragAndDropTarget
{
public:
    BassModule (PluginBase& processor, int layer);
    void resized() override;
    void paint (juce::Graphics& g) override { ModulePanel::paint (g); groups.paint (g, getAccent()); }
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
    BassParams current;
    float ampValueAt (double seconds) const;

    EnvelopeView envelope { 0.5 };
    TabbedPanel sources;
    std::array<std::unique_ptr<OscPanel>, ParamID::oscsPerLayer> oscs;
    std::unique_ptr<SamplePage> samplePage;
    std::vector<std::unique_ptr<juce::Component>> controls;
    ControlGroups groups; // PITCH, SUB, NOISE, FILTER, FILTER ENV, VOLUME ENV, WARP ENV, DRIVE, OUT
    PresetButton presets;
    std::unique_ptr<FxSendControl> fxSend;
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
    void mouseDoubleClick (const juce::MouseEvent&) override; // dubbelklick på ett lagers knapp: döp om

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
    PresetButton presets;
    std::vector<std::pair<std::unique_ptr<juce::TextButton>, StepSequencer::Lane>> laneButtons;
    StepSequencer sequencer;
    PianoRoll pianoRoll;
    juce::TextButton patternTab { "PATTERN" }, gateTab { "GATE" };
    juce::TextButton loopButton { "LOOP ROWS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttachment;
    juce::TextButton pianoButton { "PIANO ROLL" };
    std::vector<std::unique_ptr<juce::TextButton>> layerButtons; // BASS 1-3: vilket lager som redigeras
    juce::TextButton kickLayerButton { "KICK" };                // lager 3: kicken
    std::unique_ptr<ChoiceComponent> kickGrid;                   // kickens rutnät
    int lastNamesVersion = -1;

    // Per lager (bara det valda syns): notrutnätet och tonlängden (PATTERN), och GATE: på/av, rutnät, djup och mjukhet.
    std::vector<std::unique_ptr<ChoiceComponent>> grids, gateGrids;
    std::vector<std::unique_ptr<KnobComponent>> lengths, gateDepths, gateSmooths;
    std::vector<std::unique_ptr<juce::TextButton>> gateOnButtons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> gateOnAttachments;
    juce::TextButton fillButton { "FILL..." }, gateFillButton { "FILL..." };
    StepSequencer::Mode mode = StepSequencer::Mode::notes;
    juce::Rectangle<int> headingBounds;
    int shownLayer = 0;
};
