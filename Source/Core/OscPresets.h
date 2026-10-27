#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Oscillator-presets (.oscpreset): en enda oscillator (OSC 1, 2 eller 3) i ett baslager, som kan laddas i vilken oscillator som helst.
// Filen har tabellen (med namn), position, warp, tonhöjd (SEMI/FINE), unison/detune, fas och de övriga OSC-rattarna. Den rör aldrig
// andra oscillatorer, SAMPLE, filter eller effekter, och inte heller oscillatorns av/på.
namespace OscPresets
{
    // Dokument\Kickbase\Presets\Oscillators (kategorier = undermappar).
    juce::File folder();
    // Skriver ett par färdiga presets första gången (bara om mappen inte finns). Anropas av OSC-panelen, inte av list().
    void ensureFactory();
    // Alla filer, kategori för kategori (mapp före mapp, sorterade).
    juce::Array<juce::File> list();

    bool save (juce::AudioProcessorValueTreeState& apvts, int layer, int osc, const juce::File& file);
    bool load (juce::AudioProcessorValueTreeState& apvts, int layer, int osc, const juce::File& file);

    // Oscillatorn tillbaka till standardvärdena (som en ny oscillator), utom av/på.
    void reset (juce::AudioProcessorValueTreeState& apvts, int layer, int osc);
}
