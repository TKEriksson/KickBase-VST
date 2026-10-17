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
        int clipBars = 0;              // > 0: som FL:s mönsterklipp, tonen släpps och slås an igen var N:e takt
        bool noteOnFirst = false;      // vid ett klippbyte kommer "ton på" före "ton av" (så skickar FL ibland)
        int blockSize = 512;

        // Fyller insignalen (FX-versionen) för blocket som börjar på given sample; annars tyst.
        std::function<void (juce::AudioBuffer<float>&, int startSample)> fillInput;

        // Anropas efter varje bearbetat block (utsignalen, blockets första sample och längd), t.ex. för analys medan det renderas.
        std::function<void (const juce::AudioBuffer<float>&, int startSample, int numSamples)> onBlock;

        // Avbryter renderingen när den ger true (kollas före varje block); resten av utsignalen blir tyst.
        std::function<bool()> shouldStop;
    };

    juce::AudioBuffer<float> render (juce::AudioProcessor& processor, const Settings& settings);

    bool writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& file);
}
