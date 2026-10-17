#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>
#include <array>
#include "SignalTap.h"
#include "SourceEq.h"

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
    std::atomic<float>* enhance;

    // Stereoförstärkaren: höga delar av mitten fördröjs ~9 ms och läggs i sidan (försvinner i mono).
    std::vector<float> enhanceLine;
    int enhanceIndex = 0;
    float enhanceLow = 0.0f;

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
    std::vector<float> boxWindow;

    // Minsta förstärkningen över fönstret (de senaste `lookahead` värdena), som en monoton kö: kön har stigande värden och det äldsta
    // ligger först, så minimum är det första. Samma värde som att gå igenom hela fönstret, men med i snitt ett par jämförelser per sample
    // i stället för `lookahead` (72 vid 48 kHz).
    std::vector<float> minValues;
    std::vector<juce::int64> minIndices;
    int minFront = 0, minCount = 0;
    juce::int64 minCounter = 0;
    void pushMin (float value);
    int writeIndex = 0;
    float released = 1.0f;  // förstärkning efter release (sjunker direkt, stiger långsamt)
    double boxSum = 0.0;

    // Masterns EQ (ANALYZER-fliken), före bredd, drive och limiter.
    SourceEq eq;

    // Extra fördröjningar för andra utgångar (kick och bas).
    std::vector<std::vector<std::vector<float>>> extraDelays;
    std::vector<int> extraIndex;
};
