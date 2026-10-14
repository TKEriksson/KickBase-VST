#pragma once

#include "../Core/PluginBase.h"

// MyOwnFruit FX: läggs på mixerkanalen där en annan synth hamnar och hackar upp dess ljud
// efter mönstret. Steg 1: gate:ar på varje fjärdedel.
class FxProcessor : public PluginBase
{
public:
    FxProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return false; }

private:
    void applyGate (juce::AudioBuffer<float>& buffer, int startSample, int endSample);

    StepCursor quarterCursor { 1.0 };

    int gateSamplesLeft = 0; // hur länge gaten är öppen till
    float gateLevel = 0.0f;  // 0..1, glider mot målet för att undvika knäppar
    float gateRampStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxProcessor)
};
