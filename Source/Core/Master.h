#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <array>
#include "SignalTap.h"

// Mastersektionen: stereobredd (med mono under en frekvens), drive, soft clip och en limiter
// med lookahead och tak (ceiling).
//
// Limitern tittar `lookaheadSamples` framåt: ljudet fördröjs så mycket och förstärkningen
// hinner sänkas mjukt innan en topp når utgången. Fördröjningen rapporteras som latens
// till DAW:en, och andra utgångar fördröjs lika mycket med delayOnly.
class MasterSection
{
public:
    static constexpr double lookaheadSeconds = 0.0015;

    explicit MasterSection (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate, int numChannels);
    void reset();
    int getLatencySamples() const { return lookahead - 1; }

    void process (juce::AudioBuffer<float>& buffer, int numSamples);

    // Fördröjer en buffert lika mycket som process, utan att röra ljudet (för kick-/basutgångarna).
    void delayOnly (juce::AudioBuffer<float>& buffer, int numSamples, int delaySlot);

    // För gränssnittets mätare (dB). Skrivs av ljudtråden.
    std::atomic<float> outputPeakDb { -100.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    // Vad gränssnittet ritar: ljudet in i limitern (efter bredd, drive och clip), ut ur den,
    // och limiterns förstärkning (i båda kanalerna) sample för sample.
    SignalTap inputTap, outputTap, gainTap;

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

private:
    std::atomic<float>* on;
    std::atomic<float>* driveDb;
    std::atomic<float>* clip;
    std::atomic<float>* ceilingDb;
    std::atomic<float>* releaseMs;
    std::atomic<float>* width;
    std::atomic<float>* monoBelowHz;

    // Sidosignalens lågpass (två poler) för mono under gränsfrekvensen.
    std::array<float, 4> sideState {};

    double sampleRate = 44100.0;
    int lookahead = 72;
    int channels = 2;

    // DC-filter (högpass ~5 Hz) per kanal: korta bastoner som börjar i samma fas varje gång
    // har ett litet medelvärde som annars äter headroom.
    std::vector<float> dcInput, dcOutput;
    float dcCoefficient = 0.999f;

    // Fördröjning (ringbuffert) per kanal, och limiterns mellansteg.
    std::vector<std::vector<float>> delayLines;
    std::vector<float> minWindow, boxWindow;
    int writeIndex = 0;
    float released = 1.0f;  // förstärkning efter release (sjunker direkt, stiger långsamt)
    double boxSum = 0.0;

    // Extra fördröjningar för andra utgångar (kick och bas).
    std::vector<std::vector<std::vector<float>>> extraDelays;
    std::vector<int> extraIndex;
};
