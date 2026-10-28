#pragma once

#include "../Core/InstrumentSynth.h"
#include "../Core/PluginBase.h"
#include "Components.h"
#include <array>

// MIX-fliken i källpanelen: en strip per källa (OSC 1-3, SAMPLE, NOISE, SUB, FM) med nivå och pan (NOISE också bredd; SUB är alltid mono i mitten),
// dB-mätare per källa (källan ensam), zoom i vågformen (hjul = tid, Shift + hjul = höjd, mittknapp eller rullningslist = panorera, dubbelklick = återställ, WIDE = hela bredden, tidsaxel i ms) och till vänster den sammanlagda vågformen av det instrumentet spelar (alla källor genom filter och envelope, några cykler efter anslaget).
class InstrMixPanel : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    InstrMixPanel (PluginBase& processor, int layer, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    struct Strip
    {
        juce::String name;
        std::unique_ptr<KnobComponent> level, pan, width, phase; // pan, width och phase kan saknas
        std::unique_ptr<KnobComponent> delay, length, curve;     // 5b: TIMING-vyn (POS DELAY, LENGTH, CURVE)
        std::unique_ptr<ChoiceComponent> sync;                   // 5b: POS DELAY i notvärden
        juce::Rectangle<int> frame, meter;
    };

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    double samplesPerColumn() const { return 5.2 / zoomX; }
    void updateScrollBar();
    void setWide (bool on);
    void setTiming (bool on); // TIMING: ratterna byter till POS DELAY / SYNC / LENGTH / CURVE per motor
    void paintTimeAxis (juce::Graphics&, juce::Rectangle<float> wave) const;
    void addStrip (const char* name, InstrumentField level, const InstrumentField* pan, const InstrumentField* width, int colourIndex, const InstrumentField* phase = nullptr);
    void renderWave();
    void renderMeters();

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    ParamRefs refs;
    std::vector<std::unique_ptr<Strip>> strips;
    std::vector<float> peaks;      // min/max-par per kolumn
    float waveGain = 1.0f;
    float zoomX = 1.0f, zoomY = 1.0f;    // vågformens zoom: tid (hjul) och höjd (Shift + hjul)
    int panSamples = 0, panAtDragStart = 0;  // vågens startpunkt (samples efter den startade tonen): mittknapp-drag eller rullningslist
    static constexpr int maxPanSamples = 96000;
    bool wide = false;                      // WIDE: vågen får hela panelens bredd, ratterna göms
    bool timing = false;
    juce::TextButton wideButton { "WIDE" }, timingButton { "TIMING" }, helpButton;
    juce::ScrollBar scrollBar { false };
    std::array<float, 7> peakDb {};       // varje källas toppnivå (dB) ensam, efter pan och bredd
    juce::Rectangle<int> waveArea;
    juce::String lastKey;
    InstrumentSynth synth, meterSynth;
};
