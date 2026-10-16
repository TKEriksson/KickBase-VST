#pragma once

#include "../Core/Ducker.h"
#include "../Core/Effects.h"
#include "../Core/KickEngine.h"
#include "../Core/Master.h"
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
    const MasterSection& getMasterSection() const override { return master; }
    const EffectRack& getEffectRack() const override { return effects; }
    bool acceptsMidi() const override { return false; }

protected:
    std::unique_ptr<PluginBase> createOfflineCopy() const override { return std::make_unique<FxProcessor>(); }

private:
    void renderSamples (int startSample, int endSample);
    void handleEvent (const StepEvent& event);
    void ensureScratchSize (int numSamples);

    StepScheduler scheduler;
    KickEngine kick;
    StepGate gate;
    Ducker ducker;
    EffectRack effects { apvts };
    DuckCurvePlayer duckCurve;
    MasterSection master { apvts };

    // Blockets mellanresultat: kick, hur mycket synthen ska duckas, gatens nivå och synthens ljud (stereo).
    juce::AudioBuffer<float> kickBuffer, duckBuffer, duckRawBuffer, gateBuffer, synthBuffer;

    // Läses en gång per block.
    bool active = false, kickOn = true, gateOn = true;
    float duckAmount = 0.8f, gateScale = 1.0f, inputLevel = 1.0f;
    PatternLengths lengths;
    int kickSoloSamplesLeft = 0; // provlyssning av bara kickens sampling
    bool useCurve = false;
    double duckLengthSamples = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxProcessor)
};
