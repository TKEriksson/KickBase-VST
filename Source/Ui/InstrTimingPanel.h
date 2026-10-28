#pragma once

#include "../Core/InstrumentSynth.h"
#include "../Core/PluginBase.h"
#include "Components.h"
#include "TimelineChart.h"
#include <array>

// TIMING-fliken i källpanelen (bredvid MIX): en ruta per motor (OSC 1-3, SAMPLE, NOISE, SUB, FM) med ett diagram över motorns nivå i tiden
// (samma gate-formel som rösten), MODE (GATE/SHIFT), POS DELAY, LENGTH och CURVE. SYNC-valrutan ligger under ratten och ratten göms när SYNC är på;
// då visas ett x-fält med antal notvärden. Spelhuvud och släpplinje läses ur atomer som ljudtråden skriver.
class InstrTimingPanel : public juce::Component, private juce::Timer
{
public:
    InstrTimingPanel (PluginBase& processor, int layer, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell
    {
        juce::String name;
        std::unique_ptr<ChoiceComponent> mode, delaySync, lengthSync;
        std::unique_ptr<KnobComponent> delay, length, curve;
        juce::Slider delayCount, lengthCount; // x-fält (1-8), syns bara när respektive SYNC är på
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayCountAttachment, lengthCountAttachment;
        juce::Rectangle<int> frame, plot;
        bool delayIsSync = false, lengthIsSync = false;
    };

    void timerCallback() override;
    void setupCount (juce::Slider&, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>&, const juce::String& paramID);
    void updateVisibility (Cell&, size_t engine);
    void layoutCell (Cell&, bool withPlot = true);
    void refreshParams();
    void applyViewMode();
    void paintPlot (juce::Graphics&, const Cell&, size_t engine) const;

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    ParamRefs refs;
    std::vector<std::unique_ptr<Cell>> cells;
    juce::TextButton helpButton, viewButton; // viewButton: växlar FOCUS (tidsdiagram + vald motor) och OVERVIEW (alla sju rutor)
    TimelineChart timeline;
    int selectedEngine = 0;
    juce::Rectangle<int> legendArea;
    InstrumentParams params;
    double bpm = 120.0;
    double spanSeconds = 1.0;
    int tick = 0;
};
