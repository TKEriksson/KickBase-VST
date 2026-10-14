#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"

// Krävs av juce::Synthesiser: säger vilka toner en röst kan spela. Vi spelar alla.
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// En röst = en ton som spelas. Synthesiser fördelar inkommande MIDI-toner över flera röster.
class SynthVoice : public juce::SynthesiserVoice
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void setParams (const VoiceParams& newParams);

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    float nextOscillatorSample();

    VoiceParams params;
    juce::ADSR adsr;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::AudioBuffer<float> voiceBuffer;

    double phase = 0.0;          // 0..1, en period av vågformen
    double phaseIncrement = 0.0; // hur långt fasen flyttas per sample
    float level = 0.0f;          // från anslagsstyrkan (velocity)
};
