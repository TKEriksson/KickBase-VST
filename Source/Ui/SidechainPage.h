#pragma once

#include "VisualTimer.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "MixerPage.h"
#include "PresetButton.h"
#include "Scopes.h"

// Redigerar duckkurvan. Visar basens volym efter en kick: överst = full volym, nederst = tyst.
// Dra punkter, dubbelklicka för att lägga till, högerklicka för att ta bort, och dra i
// segmentets mitt (den lilla romben) för att böja det.
class DuckCurveView : public juce::Component, private VisualTimer
{
public:
    explicit DuckCurveView (PluginBase& processor);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    bool curveMode() const;
    float depth (int layer) const;
    int numLayers() const;
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float duck) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const;
    juce::Point<float> tensionHandle (size_t segment) const;
    int pointAt (juce::Point<float> p) const;
    int tensionHandleAt (juce::Point<float> p) const;
    void commit (std::vector<SidechainCurve::Point> points);

    PluginBase& processor;
    std::vector<SidechainCurve::Point> editing; // kopia under drag
    int draggedPoint = -1, draggedTension = -1, hovered = -1;
    float tensionStart = 0.0f, dragStartY = 0.0f;
    int lastVersion = -1;
    bool lastMode = false;
    float lastDepths = -1.0f; // summan av lagrens djup, för att se när något ändrats
};

// En rad i listan över hur djupt kicken ducker varje instrument: namn, ett reglage och en liten mätare som visar hur mycket som ducks nu.
class DuckRow : public juce::Component, private VisualTimer
{
public:
    DuckRow (PluginBase& processor, int layer);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginBase& processor;
    const int layer;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float reduction = 0.0f;
    juce::String shownName;
};

// Fliken SIDECHAIN.
class SidechainPage : public juce::Component, private juce::Timer
{
public:
    explicit SidechainPage (PluginBase& processor);
    void resized() override;

private:
    void timerCallback() override;

    PluginBase& processor;
    int lastActive = -1;
    PresetButton presets;
    ChoiceComponent mode, length;
    juce::Viewport depthViewport;                       // instrumentens rader rullar lodrätt när de är många
    juce::Component depthContent;
    std::vector<std::unique_ptr<DuckRow>> depthRows;    // en per instrument
    std::unique_ptr<KnobComponent> depthKnob;           // FX-versionen: ett enda djup
    DuckCurveView curve;
    ScopeView scope;
    juce::TextButton halftimeButton { "HALFTIME" }; // scopet visar 8 slag i stället för 4
    std::unique_ptr<SourceToggles> sources; // vilka källor som ducks (samma som SC i mixern)
};
