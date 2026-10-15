#pragma once

#include <functional>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Spelar en processor offline med en låtsad DAW (4/4, fast tempo, en ton per takt), för
// WAV-exporten och utvecklingsverktyget.
namespace OfflineRender
{
    struct Settings
    {
        double sampleRate = 48000.0;
        double bpm = 145.0;
        int bars = 4;                  // så länge tonerna hålls
        juce::Array<int> notes { 60 }; // en per takt (upprepas); samma ton i rad hålls kvar
        int extraBars = 0;             // takter efter sista tonen, för svansar
        int blockSize = 512;

        // Fyller insignalen (FX-versionen) för blocket som börjar på given sample; annars tyst.
        std::function<void (juce::AudioBuffer<float>&, int startSample)> fillInput;
    };

    juce::AudioBuffer<float> render (juce::AudioProcessor& processor, const Settings& settings);

    bool writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& file);
}
