#pragma once

#include "VisualTimer.h"

#include <juce_dsp/juce_dsp.h>
#include "../Core/PluginBase.h"
#include "Theme.h"

// Vågformsvy i realtid: de senaste ~två slagen av en eller flera signaler (kicken, basen före
// och efter duckningen, mastern in och ut), ritade över varandra så att man ser vad de gör.
class ScopeView : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    struct Trace
    {
        const SignalTap* tap;
        int channel = 2;       // 0 vänster, 1 höger, 2 summan
        juce::Colour colour;
        bool filled = true;    // fylld yta, annars bara en linje
        juce::String name;
        bool edge = false;     // förstärkning (0..1) som ritas från över- och underkanten inåt: så mycket som tas bort
        int instrument = -1;   // hör till instrument n (0-9): ritas bara när det instrumentet används
    };

    enum class Mode { wave, gainReduction };

    ScopeView (PluginBase& processor, std::vector<Trace> traces, Mode mode = Mode::wave);

    // Vilken grupp i SETTINGS > OPTIMIZATIONS vyn hör till (takt och fyllning): sätts av ägaren.
    void setVisual (Settings::Visual group) { startVisualTimer (group, 30); }

    // En horisontell hjälplinje (t.ex. masterns tak) som ritas vid ±värdet (vågläge).
    std::function<float()> guideLevel;

    // Rubriken i förstärkningsminskningsläget.
    juce::String gainTitle = "LIMITER GAIN REDUCTION (dB)";

    // Hur många slag fönstret visar (4 som standard; 8 = halvtid, hälften så snabb rullning).
    void setBeats (int newBeats) { beats = juce::jmax (1, newBeats); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    double windowSeconds() const;

    int beats = 4;
    PluginBase& processor;
    std::vector<Trace> traces;
    Mode mode;
    std::vector<std::vector<float>> data;
    int windowSamples = 0;
};

// Spektrum i realtid (logaritmisk frekvensaxel 20 Hz - 20 kHz): kicken, basen och mastern, så
// att man ser vilka frekvenser som spelas och hur starkt.
class SpectrumView : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    explicit SpectrumView (PluginBase& processor);
    void paint (juce::Graphics&) override;

private:
    static constexpr int order = 12;
    static constexpr int size = 1 << order;
    static constexpr int bins = size / 2;

    struct Band
    {
        const SignalTap* tap;
        int channel;
        juce::Colour colour;
        juce::String name;
        std::array<float, bins> level {}; // dB, utjämnad
    };

    void timerCallback() override;
    void analyse (Band& band);
    float xFor (float hz, juce::Rectangle<float> area) const;

    PluginBase& processor;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::vector<Band> bands;
    std::array<float, bins> peakHold {};
    std::vector<float> left, right;
    std::array<float, size * 2> fftData {};
};

// Stereobilden: en vektorskop (mitt uppåt, sidan åt sidorna) och korrelationsmätare. Mono blir
// en lodrät linje, bred stereo en spridd boll, och motfas syns som vågrätt.
class StereoView : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    explicit StereoView (PluginBase& processor);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    PluginBase& processor;
    std::vector<float> left, right;
    float correlation = 1.0f, widthShare = 0.0f;

    // Efterglöd: de senaste bildrutornas punkter (varannan sampel, l/r i par), nyast först i ringen.
    static constexpr int trailFrames = 4;
    std::array<std::vector<float>, trailFrames> trail;
    int trailHead = 0, trailCount = 0;
};
