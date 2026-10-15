#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "EnvelopeView.h"
#include "PresetButton.h"
#include "StepSequencer.h"

// Rubrikraden: logotyp, presetmeny och uppspelningsstatus (DAW eller egen klocka, tempo,
// takt, tonart).
class HeaderBar : public juce::Component, private juce::Timer
{
public:
    HeaderBar (PluginBase& processor, const juce::String& subtitle);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void showExportMenu();
    void chooseExportTarget (int part, int bars); // part: 0-2 = PluginBase::ExportPart, 3 = alla tre

    PluginBase& processor;
    juce::String subtitle;
    PresetButton presets;
    juce::TextButton exportButton { "EXPORT" }; // bara i instrumentet
    std::unique_ptr<juce::FileChooser> chooser;
};

// Kicken: kurvor för pitch- och amplitud-envelope, rattar och plats för en klicksampling.
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
    ParamRefs refs;
    KickParams current; // läses före varje omritning av kurvorna

    EnvelopeView envelope { 0.4 };
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    ControlGroups groups; // PITCH, GLIDE, AMP, CLICK ...
    ChoiceComponent keyMode;
    PresetButton presets;

    juce::Label sampleLabel;
    juce::TextButton loadButton { "Load" }, clearButton { "X" };
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragHover = false;
};

// Bassynten (bara instrumentet): filter- och amplitud-envelope och rattar.
class BassModule : public ModulePanel
{
public:
    explicit BassModule (PluginBase& processor);
    void resized() override;
    void paint (juce::Graphics& g) override { ModulePanel::paint (g); groups.paint (g, getAccent()); }

private:
    ParamRefs refs;
    BassParams current;
    float ampValueAt (double seconds) const;

    EnvelopeView envelope { 0.5 };
    std::vector<std::unique_ptr<juce::Component>> controls;
    ControlGroups groups; // OSC 1, SUB, OSC 2, FM, FILTER, FILTER ENV, AMP ...
    PresetButton presets;
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

// Mönstret: längd, rutnät, presets och stegsekvenseraren. En sida i flikpanelen.
class PatternModule : public juce::Component
{
public:
    PatternModule (PluginBase& processor, juce::Colour bassAccent, const juce::String& bassName, bool showPitchLane);
    void resized() override;

private:
    void selectLane (StepSequencer::Lane lane);

    ChoiceComponent length, grid;
    PresetButton presets;
    std::vector<std::pair<std::unique_ptr<juce::TextButton>, StepSequencer::Lane>> laneButtons;
    StepSequencer sequencer;
};

// Mixen: gate-längd, master och (för instrumentet) vad som händer när tonen släpps.
// (Duckningen ställs i SIDECHAIN-fliken.)
class MixModule : public ModulePanel
{
public:
    MixModule (PluginBase& processor, bool showNoteEnd);
    void resized() override;

private:
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    std::unique_ptr<ChoiceComponent> noteEnd;
};
