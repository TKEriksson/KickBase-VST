#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>

// Mått och bilder för att bedöma ljud utan att kunna lyssna (verktygens audit och MCP-bryggans
// render_preview): topp, RMS, tyngdpunkt, band, stereo, kickens längd och svansfrekvens, spektrogram.
namespace AudioAnalysis
{
    struct Metrics
    {
        float peakDb = -100.0f, rmsDb = -100.0f, dc = 0.0f, centroid = 0.0f;
        float sideDb = -100.0f;     // sidan i förhållande till mitten, hela spektrat
        float lowSideDb = -100.0f;  // sidan i förhållande till mitten under 150 Hz (bör vara mycket lågt: basen ska vara mono)
        std::array<float, 5> bandsDb {}; // < 60 Hz, 60-250, 250-1k, 1k-5k, > 5k, i dB relativt summan
        bool finite = true;
    };

    // `audio` är stereo.
    Metrics measure (const juce::AudioBuffer<float>& audio, double rate);

    // Tiden från första slaget tills nivån fallit 40 dB under toppen (ms), för en kick-rendering.
    float kickLengthMs (const juce::AudioBuffer<float>& audio, double rate, double bpm);

    // Kickens svansfrekvens (Hz): spektrumtoppen mellan 25 och 150 Hz i andra halvan av kickens längd.
    float kickTailHz (const juce::AudioBuffer<float>& audio, double rate, double bpm);

    // Spektrogram med logaritmisk frekvensaxel (20 Hz - 20 kHz) och slaglinjer. `order` är FFT-storleken
    // (12 = 4096, 10 = 1024 för tät tidsupplösning); `seconds` > 0 visar bara början (närbild på en kick).
    juce::Image spectrogram (const juce::AudioBuffer<float>& audio, double rate, double bpm, int order = 12, double seconds = 0.0);

    bool writePng (const juce::Image& image, const juce::File& file);
    juce::String pngBase64 (const juce::Image& image);
}
