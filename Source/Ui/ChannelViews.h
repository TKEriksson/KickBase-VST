#pragma once

#include "VisualTimer.h"

#include <juce_dsp/juce_dsp.h>
#include "../Core/PluginBase.h"
#include "Components.h"
#include "Scopes.h"

// Kanalens EQ: frekvenskurvan med noder att dra i (som i en parametrisk EQ) över ett live-spektrum av
// ljudet som når EQ:n, så att man ser vilka frekvenser som behöver fixas. Noderna I-IV är band
// (hylla, klocka, klocka, hylla), HP/LP är högpass och lågpass. Skrolla på en klocka för Q;
// dubbelklicka på en nod för att nollställa den.
class EqView : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    explicit EqView (PluginBase& processor);
    void setChannel (int newChannel);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int order = 12, size = 1 << order, bins = size / 2;
    static constexpr int numNodes = Effects::eqBands + 2; // 0 = HP, 1-4 = band, 5 = LP

    void timerCallback() override;
    juce::RangedAudioParameter* freqParam (int node) const;
    juce::RangedAudioParameter* gainParam (int node) const;
    juce::RangedAudioParameter* qParam (int node) const;
    juce::Rectangle<float> plot() const;
    float xForHz (float hz) const;
    float hzForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    juce::Point<float> nodePosition (int node, const Effects::EqSettings& s) const;
    int nodeAt (juce::Point<float> p) const;
    juce::String describe (int node, const Effects::EqSettings& s) const;
    static void setValue (juce::RangedAudioParameter* parameter, float value);

    PluginBase& processor;
    int channel = 0;
    int hovered = -1, dragged = -1;

    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, bins> level {}, hold {};
    std::vector<float> left, right;
    std::array<float, size * 2> fftData {};
};

// Kanalens kompressor: rattarna och live-vågformer av ljudet in (vit kontur) och ut (fylld), med
// förstärkningsminskningen under, så att man ser vad kompressorn gör.
class CompView : public juce::Component
{
public:
    CompView (PluginBase& processor, int channel);
    void resized() override;

private:
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    ScopeView scope, gainScope;
};
