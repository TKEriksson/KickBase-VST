#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "PresetButton.h"

// Redigerar duckkurvan. Visar basens volym efter en kick: överst = full volym, nederst = tyst.
// Dra punkter, dubbelklicka för att lägga till, högerklicka för att ta bort, och dra i
// segmentets mitt (den lilla romben) för att böja det.
class DuckCurveView : public juce::Component, private juce::Timer
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
    float depth() const;
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
    float lastDepth = -1.0f;
};

// Fliken SIDECHAIN.
class SidechainPage : public juce::Component
{
public:
    explicit SidechainPage (PluginBase& processor);
    void resized() override;

private:
    PresetButton presets;
    ChoiceComponent mode, length;
    std::vector<std::unique_ptr<KnobComponent>> depths; // en per baslager
    DuckCurveView curve;
};
