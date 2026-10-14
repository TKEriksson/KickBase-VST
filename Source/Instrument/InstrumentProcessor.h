#pragma once

#include <array>
#include "../Core/BassSynth.h"
#include "../Core/KickEngine.h"
#include "../Core/PluginBase.h"
#include "../Core/StepScheduler.h"

// MyOwnFruit (instrument): spelar kick och bas efter mönstret så länge en ton hålls i DAW:en.
// Tonen bestämmer tonarten; DAW:ens taktstreck bestämmer var stegen hamnar.
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
    struct Output
    {
        std::array<float*, 2> channels {};
        int numChannels = 0;

        void add (int sample, float value) const
        {
            for (int ch = 0; ch < numChannels; ++ch)
                channels[(size_t) ch][sample] += value;
        }
    };

    void readBlockParams();
    void refreshEngineParams();
    void handleMidi (const juce::MidiMessage& message, int sampleOffset);
    void startPattern (int sampleOffset);
    void stopPattern (int sampleOffset);
    void renderSegment (int startSample, int endSample);
    void renderSamples (int startSample, int endSample);
    void handleEvent (const StepEvent& event);
    int keyPitchClass() const { return currentNote >= 0 ? currentNote % 12 : 0; }

    StepScheduler scheduler;
    KickEngine kick;
    BassSynth bass;

    std::array<bool, 128> heldNotes {};
    int numHeldNotes = 0;
    int currentNote = -1; // senast anslagna ton; bestämmer tonarten

    bool patternActive = false;
    double stopAtPpq = std::numeric_limits<double>::infinity();

    // Läses en gång per block.
    int patternBars = 1;
    bool kickOn = true, bassOn = true;
    float duckAmount = 0.8f, bassGateScale = 1.0f;
    NoteEnd noteEnd = NoteEnd::cut;

    Output mainOut, kickOut, bassOut;
    juce::SmoothedValue<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
