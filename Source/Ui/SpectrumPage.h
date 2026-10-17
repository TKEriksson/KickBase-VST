#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Core/MixAnalysis.h"
#include "../Core/PluginBase.h"
#include "../Core/SourceEq.h"
#include "Theme.h"

// Rapportens rader (en per fynd), i en rullningsbar lista.
class ReportContent : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    int selected = -1;

    void setReport (const MixAnalysis::Report& newReport);
    void setNotes (std::vector<PluginBase::AiNote> newNotes);
    void setWidth (int width);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    const MixAnalysis::Report& getReport() const { return report; }

private:
    struct Row
    {
        juce::TextLayout layout;
        int top = 0, height = 0;
        int kind = 0;       // 0 = text, 1 = ett fynd, 2 = en AI-anteckning
        int finding = -1;   // fyndet raden gäller (kind 1: dess nummer, kind 2: fyndet anteckningen svarar på)
        bool applied = false;
    };

    void layoutRows();

    MixAnalysis::Report report;
    std::vector<PluginBase::AiNote> notes;
    std::vector<Row> rows;
    int width = 300;
};

// Fliken SPECTRUM: ett spektrum i realtid där man väljer vilka ljudkällor som visas samtidigt (kicken, varje instrument, mastern), för att
// jämföra deras övertonsserier och se var de krockar. Där två eller fler källor har energi på samma frekvens blir ytan under dem röd
// (starkare röd = högre nivå och fler källor). Analysen är avstängd som standard och bara igång när fliken är öppen och ANALYSER är på,
// så att den inte tar processorkraft i onödan.
//
// ANALYSE spelar mönstret ett helt varv offline och skriver en rapport (MixAnalysis) med saker att titta på, takt för takt. En
// referenslåt kan läsas in och visas bakom mastern, och rapporten jämför balansen mot den. EQ-knappen visar och redigerar EQ:n
// för vald källa direkt i diagrammet (dra punkterna).
class SpectrumPage : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumPage (PluginBase& processor);
    ~SpectrumPage() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { updateRunning(); }
    void parentHierarchyChanged() override { updateRunning(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int order = 13;
    static constexpr int size = 1 << order; // 8192 punkter: ~5 Hz per steg vid 44,1 kHz, så att de låga övertonerna skiljs åt
    static constexpr int bins = size / 2;
    static constexpr int lowBinCount = 96; // upp till ~560 Hz
    static constexpr int kickSource = 0, firstInstrument = 1, masterSource = 1 + ParamID::numBassLayers, numSources = masterSource + 1;

    struct Source
    {
        juce::Colour colour;
        juce::String name;
        std::vector<float> column;        // dB per bildpunkt som visas: följer `target` mjukt (snabbt upp, faller jämnt), så att det inte hackar mellan analyserna
        std::vector<float> target;        // dB per bildpunkt från den senaste analysen (analysen görs direkt i bildpunkter: mycket billigare än per steg)
        std::array<float, lowBinCount> low {}; // dB per steg för de lägsta stegen, till grundtonen
        float fundamental = 0.0f;         // grundtonens frekvens (Hz), 0 = ingen hittad
        bool selected = true;
        bool present = true;              // instrumentet finns (inom instrumentantalet)
    };

    void timerCallback() override;
    void updateRunning();
    void updateSources();
    void ensureMapping (int width, double rate);
    void analyse (int source);
    void smoothColumns(); // varje bildruta: kurvorna närmar sig det senaste analysresultatet (upp snabbt, ner med jämn fart)
    void findFundamental (Source& source) const;
    float xFor (float hz) const;
    float hzFor (float x) const;
    juce::Rectangle<float> plotArea() const;
    juce::String describe (float hz) const;

    void startAnalysis();
    void takeReport();
    void selectFinding (int index);
    void chooseReference();
    void loadReferenceFile (const juce::File& file);
    void copyReport();
    void showExportMenu();
    void saveExportFile();
    void setView (bool report);
    juce::Rectangle<int> overlayBounds() const;
    void showSourceMenu();
    void updateShowText();

    // EQ
    static constexpr float eqCentreDb = -45.0f; // diagrammets nivå där EQ:ns förstärkning 0 dB ritas
    struct EqNode { juce::Point<float> position; int id = -1; };
    float dbToY (float db) const;
    float yToDb (float y) const;
    std::vector<EqNode> eqNodes() const;
    int eqHit (juce::Point<float> position) const;
    void setEqParameter (const juce::String& suffix, float value);
    float eqParameter (const juce::String& suffix) const;
    juce::String eqKey (int node, const char* field) const;
    void paintEq (juce::Graphics&, juce::Rectangle<float> area);

    PluginBase& processor;
    juce::TextButton analyserButton { "ANALYSER" }, collisionsButton { "COLLISIONS" }, harmonicsButton { "HARMONICS" }, eqButton { "EQ" };
    juce::TextButton analyseButton { "ANALYSE" }, referenceButton { "REFERENCE" }, exportButton { "EXPORT" };
    juce::TextButton viewSpectrumButton { "SPECTRUM" }, viewReportButton { "ANALYSIS + REPORT" }; // flikarna i fliken
    juce::ComboBox sensitivityBox, eqTargetBox, speedBox;
    juce::TextButton showButton { "SHOW" }; // flervalslista över vilka källor som visas
    std::array<Source, numSources> sources;

    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> left, right;
    std::array<float, size * 2> fftData {};

    MixAnalyser& analyser; // delas med verktygslagret: en AI kan starta och hämta samma analys
    juce::Rectangle<int> plotBody; // diagrammets yta (rapporten ligger till höger om den)
    juce::Viewport viewport;
    ReportContent content;
    bool reportShown = false, analysing = false, reportView = false;
    int notesVersionShown = -1;
    MixAnalysis::ExportOptions exportOptions;
    bool exportOnlyShown = false; // bara de källor som visas i SHOW
    int reportVersion = 0;
    float highlightLow = 0.0f, highlightHigh = 0.0f;

    std::shared_ptr<const MixAnalysis::Reference> reference;
    float referenceOffset = 0.0f;
    juce::String referenceError;
    std::unique_ptr<juce::FileChooser> chooser;

    int eqSource = 0, dragNode = -1;
    std::vector<juce::RangedAudioParameter*> gestureParameters;

    int namesVersion = -1, instrumentsShown = -1, tick = 0;
    float hoverX = -1.0f;
    juce::Point<float> hoverPoint;
    bool running = false;

    // Hur ofta analysen körs och ritas (SPEED), och hur snabbt kurvorna faller (anpassat till takten).
    int fps = 15;          // analyser per sekund (SPEED)
    int displayHz = 30;    // bildrutor per sekund: oberoende av analysen, så att kurvorna faller mjukt mellan analyserna
    int analysisEvery = 2; // analysera var n:te bildruta
    int analysisCount = 0;
    float smoothing = 0.3f;

    // Bildpunkt -> steg i FFT:n, beräknat när bredden eller samplingsfrekvensen ändras.
    std::vector<int> pixelFirst, pixelLast;
    std::vector<float> pixelFraction;
    int mappedWidth = 0;
    double mappedRate = 0.0;

    // Referenslåtens kurva i bildpunkter (beräknas bara när den eller bredden ändras).
    std::vector<float> refColumn;
    const MixAnalysis::Reference* refKey = nullptr;
};
