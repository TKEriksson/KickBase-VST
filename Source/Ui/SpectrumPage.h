#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Core/PluginBase.h"
#include "Theme.h"

// Fliken SPECTRUM: ett spektrum i realtid där man väljer vilka ljudkällor som visas samtidigt (kicken, varje instrument, mastern), för att
// jämföra deras övertonsserier och se var de krockar. Där två eller fler källor har energi på samma frekvens blir ytan under dem röd
// (starkare röd = högre nivå och fler källor). Analysen är avstängd som standard och bara igång när fliken är öppen och ANALYSER är på,
// så att den inte tar processorkraft i onödan.
class SpectrumPage : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumPage (PluginBase& processor);
    ~SpectrumPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { updateRunning(); }
    void parentHierarchyChanged() override { updateRunning(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    static constexpr int order = 13;
    static constexpr int size = 1 << order; // 8192 punkter: ~5 Hz per steg vid 44,1 kHz, så att de låga övertonerna skiljs åt
    static constexpr int bins = size / 2;
    static constexpr int kickSource = 0, firstInstrument = 1, masterSource = 1 + ParamID::numBassLayers, numSources = masterSource + 1;

    struct Source
    {
        juce::Colour colour;
        juce::String name;
        std::array<float, bins> level {}; // dB, utjämnad
        float fundamental = 0.0f;         // grundtonens frekvens (Hz), 0 = ingen hittad
        bool selected = true;
        bool present = true;              // instrumentet finns (inom instrumentantalet)
    };

    void timerCallback() override;
    void updateRunning();
    void updateSources();
    void analyse (int source);
    void findFundamental (Source& source) const;
    float xFor (float hz) const;
    float hzFor (float x) const;
    juce::Rectangle<float> plotArea() const;
    juce::String describe (float hz) const;

    PluginBase& processor;
    juce::TextButton analyserButton { "ANALYSER" }, collisionsButton { "COLLISIONS" }, harmonicsButton { "HARMONICS" };
    std::array<juce::TextButton, numSources> chips;
    std::array<Source, numSources> sources;

    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> left, right;
    std::array<float, size * 2> fftData {};

    int namesVersion = -1, instrumentsShown = -1, tick = 0;
    float hoverX = -1.0f;
    bool running = false;
};
