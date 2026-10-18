#pragma once

#include <array>
#include "../Core/InstrumentSynth.h"
#include "../Core/Ducker.h"
#include "../Core/Effects.h"
#include "../Core/KickEngine.h"
#include "../Core/Master.h"
#include "../Core/Mixer.h"
#include "../Core/PluginBase.h"
#include "../Core/StepScheduler.h"

// Kickbase (instrument): spelar kick och bas efter mönstret så länge en ton hålls i DAW:en.
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
    const EffectRack& getEffectRack() const override { return effects; }
    const Mixer* getMixer() const override { return &mixer; }
    bool acceptsMidi() const override { return true; }

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardState* getKeyboardState() override { return &keyboardState; }

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
    void resetDspState(); // efter NaN/inf: alla filter, effekter och röster tillbaka till tystnad (allokerar inte)
    int keyPitchClass() const { return currentNote >= 0 ? currentNote % 12 : 0; }
    int instrumentRootNote() const  { return Params::instrumentRootMidiNote (currentNote >= 0 ? currentNote : 60); }

    static constexpr int numLayers = Pattern::numLayers;

    StepScheduler scheduler;
    KickEngine kick;
    std::array<InstrumentSynth, numLayers> basses;
    std::array<Ducker, numLayers> duckers;
    EffectRack effects { apvts };
    DuckCurvePlayer duckCurve;
    MasterSection master { apvts };
    SourceEq kickEq { apvts, "kick" };
    std::vector<SourceEq> layerEqs; // ett per instrument (ANALYZER-fliken)
    Mixer mixer { apvts };

    // Blockets mellanresultat: kick, hur mycket basen ska duckas (före varje lagers mängd),
    // varje baslagers källa (vänster i layerBuffer, höger i layerBufferRight) och amplitud, och basens summa.
    juce::AudioBuffer<float> kickBuffer, duckBuffer, layerBuffer, layerBufferRight, amplitudeBuffer, instrumentBuffer, tapBuffer;

    // Mixerns två stereobussar: det som går genom maximizern, och det som går förbi den.
    juce::AudioBuffer<float> maxBus, bypassBus;

    std::array<int, 128> heldNotes {}; // hur många gånger varje ton slagits an utan att släppas
    int numHeldNotes = 0;
    int currentNote = -1; // senast anslagna ton; bestämmer tonarten

    bool patternActive = false;
    double stopAtPpq = std::numeric_limits<double>::infinity();
    double stoppedAtPpq = -std::numeric_limits<double>::infinity(); // när mönstret senast tystnade

    // Läses en gång per block.
    PatternLengths lengths;
    int soloLayer = 0, soloSource = 0, soloSamplesLeft = 0, kickSoloSamplesLeft = 0;
    int previewLayer = -1; // rad som förhandsspelas utan DAW-ton, -1 = ingen
    void handlePreviewRequest (int row);
    void stopPreview(); // provlyssning av en enda källa (se PluginBase::requestAudition)
    bool kickOn = true;

    // Swing, humanize och instrumentens fördröjning: en ton kan skjutas framåt i tiden (aldrig bakåt: det skulle kräva latens). Toner som hamnar efter
    // slutet av segmentet väntar i `pending` och spelas i ett senare segment. Läses en gång per block.
    float swing = 0.0f, humanize = 0.0f;
    std::array<float, numLayers> delayMs = filledArray<float, numLayers> (0.0f), swingRow = filledArray<float, numLayers> (0.0f), humanizeRow = filledArray<float, numLayers> (0.0f);
    struct PendingEvent { StepEvent event; int remaining = 0; };
    std::array<PendingEvent, 256> pending {};
    int numPending = 0;
    std::array<StepEvent, 768> due {};
    juce::uint32 humanizeState = 0x9e3779b9u;
    int delaySamplesFor (const StepEvent& event);
    int activeLayers = ParamID::defaultInstrumentCount; // instrument som används (instrumentCount); de övriga rörs inte
    std::array<bool, numLayers> layerOn = filledArray<bool, numLayers> (true);
    std::array<float, numLayers> duckAmounts = filledArray<float, numLayers> (0.8f), gateScales = filledArray<float, numLayers> (1.0f);

    // GATE per lager: en egen av/på-rytm (eget rutnät) som hackar ljudet, med mjuka kanter. Läses en gång per block.
    std::array<bool, numLayers> gateOn = filledArray<bool, numLayers> (false);
    std::array<int, numLayers> gateGrid = filledArray<int, numLayers> ((int) InstrumentGrid::sixteenth);
    std::array<float, numLayers> gateDepth = filledArray<float, numLayers> (1.0f), gateCoefficient = filledArray<float, numLayers> (0.01f),
                                 gateGain = filledArray<float, numLayers> (1.0f);
    Sidechain::Mode sidechainMode = Sidechain::Mode::kickEnvelope;
    double duckLengthSamples = 1.0;
    NoteEnd noteEnd = NoteEnd::cut;

    Output mainOut, kickOut;
    std::array<Output, numLayers> layerOuts;
    juce::SmoothedValue<float> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentProcessor)
};
