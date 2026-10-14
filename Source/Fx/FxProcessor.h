#pragma once

#include "../Core/Ducker.h"
#include "../Core/KickEngine.h"
#include "../Core/PluginBase.h"
#include "../Core/StepGate.h"
#include "../Core/StepScheduler.h"

// MyOwnFruit FX: läggs på mixerkanalen där en annan synth hamnar. Synthens ljud gate:as av
// basmönstret och duckas under en egen kick, som läggs till i utgången.
// När DAW:en står still släpps ljudet igenom orört och ingen kick spelas.
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
    void processSamples (juce::AudioBuffer<float>& buffer, int startSample, int endSample);
    void handleEvent (const StepEvent& event);

    StepScheduler scheduler;
    KickEngine kick;
    StepGate gate;
    Ducker ducker;

    // Läses en gång per block.
    bool active = false, kickOn = true, gateOn = true;
    float duckAmount = 0.8f, gateScale = 1.0f, inputLevel = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxProcessor)
};
