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

protected:
    std::unique_ptr<PluginBase> createOfflineCopy() const override { return std::make_unique<InstrumentProcessor>(); }

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

    static constexpr int numLayers = Pattern::numLayers;

    StepScheduler scheduler;
    KickEngine kick;
    std::array<BassSynth, numLayers> basses;
    std::array<Ducker, numLayers> duckers;
    EffectChain effects { apvts };
    DuckCurvePlayer duckCurve;
    MasterSection master { apvts };

    // Blockets mellanresultat: kick, hur mycket basen ska duckas (före varje lagers mängd),
    // varje baslagers källa och amplitud (en kanal per lager), och basens summa (stereo).
    juce::AudioBuffer<float> kickBuffer, duckBuffer, layerBuffer, amplitudeBuffer, bassBuffer;

    std::array<bool, 128> heldNotes {};
    int numHeldNotes = 0;
    int currentNote = -1; // senast anslagna ton; bestämmer tonarten

    bool patternActive = false;
    double stopAtPpq = std::numeric_limits<double>::infinity();
    double stoppedAtPpq = -std::numeric_limits<double>::infinity(); // när mönstret senast tystnade

    // Läses en gång per block.
    int patternBars = 1;
    bool kickOn = true;
    std::array<bool, numLayers> layerOn { true, true, true };
    std::array<float, numLayers> duckAmounts { 0.8f, 0.8f, 0.8f }, gateScales { 1.0f, 1.0f, 1.0f };
    Sidechain::Mode sidechainMode = Sidechain::Mode::kickEnvelope;
    double duckLengthSamples = 1.0;
    NoteEnd noteEnd = NoteEnd::cut;

    Output mainOut, kickOut;
    std::array<Output, numLayers> layerOuts;
    juce::SmoothedValue<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
