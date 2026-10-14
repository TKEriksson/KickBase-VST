#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "EnvelopeView.h"
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
    void timerCallback() override;
    void updatePresetText();
    void showPresetMenu();

    PluginBase& processor;
    juce::String subtitle;
    juce::TextButton presetButton;
};

// Kicken: kurvor för pitch- och amplitud-envelope, rattar och plats för en klicksampling.
class KickModule : public ModulePanel, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit KickModule (PluginBase& processor);
    void resized() override;
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

private:
    ParamRefs refs;
    BassParams current;
    float ampValueAt (double seconds) const;

    EnvelopeView envelope { 0.5 };
    ChoiceComponent wave;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
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
    juce::Label help;
};

// Mönstret: längd, rutnät, mallar och stegsekvenseraren.
class PatternModule : public ModulePanel
{
public:
    PatternModule (PluginBase& processor, juce::Colour bassAccent, const juce::String& bassName, bool showPitchLane);
    void resized() override;

private:
    void showTemplateMenu();
    void selectLane (StepSequencer::Lane lane);

    PluginBase& processor;
    ChoiceComponent length, grid;
    juce::TextButton templateButton { "Templates" };
    std::vector<std::pair<std::unique_ptr<juce::TextButton>, StepSequencer::Lane>> laneButtons;
    StepSequencer sequencer;
};

// Mixen: ducking, gate-längd, master och (för instrumentet) vad som händer när tonen släpps.
class MixModule : public ModulePanel
{
public:
    MixModule (PluginBase& processor, bool showNoteEnd);
    void resized() override;

private:
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    std::unique_ptr<ChoiceComponent> noteEnd;
};
