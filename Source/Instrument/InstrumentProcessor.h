#pragma once

#include <array>
#include "../Core/PluginBase.h"

// Ett kort sinusklick. Platshållare tills kickmotorn finns (steg 2).
struct Click
{
    void prepare (double sampleRate)
    {
        twoPiOverSampleRate = juce::MathConstants<double>::twoPi / sampleRate;
        decay = (float) std::exp (-1.0 / (0.03 * sampleRate)); // ~30 ms
    }

    void trigger (double frequency, float newLevel)
    {
        phase = 0.0;
        phaseIncrement = frequency * twoPiOverSampleRate;
        level = newLevel;
    }

    float nextSample()
    {
        if (level < 1.0e-4f)
            return 0.0f;

        const auto sample = (float) std::sin (phase) * level;
        phase += phaseIncrement;
        level *= decay;
        return sample;
    }

    double twoPiOverSampleRate = 0.0, phase = 0.0, phaseIncrement = 0.0;
    float level = 0.0f, decay = 0.0f;
};

// MyOwnFruit (instrument): spelar mönstret så länge en ton hålls i DAW:en.
// Steg 1: ett klick på varje fjärdedel, stämt efter tonen och högre på ettan.
class InstrumentProcessor : public PluginBase
{
public:
    InstrumentProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return true; }

    juce::MidiKeyboardState keyboardState;

private:
    void handleMidi (const juce::MidiMessage& message, int sampleOffset);
    void renderSegment (juce::AudioBuffer<float>& buffer, int startSample, int endSample);
    void renderClick (juce::AudioBuffer<float>& buffer, int startSample, int endSample);
    void triggerClick (int stepInBar);

    StepCursor quarterCursor { 1.0 };
    Click click;

    std::array<bool, 128> heldNotes {};
    int numHeldNotes = 0;
    int currentNote = -1; // senast anslagna ton som fortfarande hålls; bestämmer tonarten

    juce::SmoothedValue<float> gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
