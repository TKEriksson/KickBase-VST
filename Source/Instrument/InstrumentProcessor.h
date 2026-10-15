#pragma once

#include <array>
#include "../Core/BassSynth.h"
#include "../Core/Ducker.h"
#include "../Core/Effects.h"
#include "../Core/KickEngine.h"
#include "../Core/Master.h"
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
    const MasterSection& getMasterSection() const override { return master; }
    bool acceptsMidi() const override { return true; }

    juce::MidiKeyboardState keyboardState;

private:
    struct Output
    {
        std::array<float*, 2> channels {};
        int numChannels = 0;

        void add (int sample, float left, float right) const
        {
            if (numChannels == 1)
                channels[0][sample] += 0.5f * (left + right);
            else if (numChannels == 2)
            {
                channels[0][sample] += left;
                channels[1][sample] += right;
            }
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
    void finishBlock (int numSamples);
    void ensureScratchSize (int numSamples);
    int keyPitchClass() const { return currentNote >= 0 ? currentNote % 12 : 0; }
    int bassRootNote() const  { return Params::bassRootMidiNote (currentNote >= 0 ? currentNote : 60); }

    StepScheduler scheduler;
    KickEngine kick;
    BassSynth bass;
    Ducker ducker;
    EffectChain effects { apvts };
    DuckCurvePlayer duckCurve;
    MasterSection master { apvts };

    // Blockets mellanresultat: kick, hur mycket basen ska duckas, basens källa (stereo) och amplitud.
    juce::AudioBuffer<float> kickBuffer, duckBuffer, bassBuffer, amplitudeBuffer;

    std::array<bool, 128> heldNotes {};
    int numHeldNotes = 0;
    int currentNote = -1; // senast anslagna ton; bestämmer tonarten

    bool patternActive = false;
    double stopAtPpq = std::numeric_limits<double>::infinity();
    double stoppedAtPpq = -std::numeric_limits<double>::infinity(); // när mönstret senast tystnade

    // Läses en gång per block.
    int patternBars = 1;
    bool kickOn = true, bassOn = true;
    float duckAmount = 0.8f, bassGateScale = 1.0f;
    Sidechain::Mode sidechainMode = Sidechain::Mode::kickEnvelope;
    Sidechain::Position sidechainPosition = Sidechain::Position::afterEffects;
    double duckLengthSamples = 1.0;
    NoteEnd noteEnd = NoteEnd::cut;

    Output mainOut, kickOut, bassOut;
    juce::SmoothedValue<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
