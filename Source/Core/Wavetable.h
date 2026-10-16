#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include <vector>

// En wavetable: en serie vågformer (frames) som WT POS sveper igenom, som i Serum.
//
// Varje frame lagras band-begränsad i en version per oktav (mip-nivåer), så att höga toner
// inte viker ner som falska övertoner: en ton läser den nivå vars övertoner ryms under Nyquist.
class Wavetable
{
public:
    static constexpr int frameSize = 1024; // samples per vågform (upp till 512 övertoner)
    static constexpr int numFrames = 16;
    static constexpr int numLevels = 10;   // 512, 256, ... 1 övertoner

    // Formen som funktion av position (0..1 genom tabellen) och fas (0..1), värde -1..1.
    using Shape = std::function<double (double position, double phase)>;

    Wavetable (juce::String name, const Shape& shape);

    const juce::String& getName() const { return name; }

    // Ett sampel. `position` 0..1 (frames interpoleras), `phase` 0..1, `maxHarmonics` = hur
    // många övertoner som ryms (Nyquist / tonens frekvens).
    float sample (float position, double phase, double maxHarmonics) const;

    // En frame utan band-begränsning (för gränssnittets bild), frameSize samples.
    const float* displayFrame (int frame) const { return frameData (0, frame); }

private:
    const float* frameData (int level, int frame) const
    {
        return data.data() + ((size_t) level * numFrames + (size_t) frame) * (frameSize + 1);
    }

    juce::String name;
    std::vector<float> data; // [nivå][frame][frameSize + 1], sista sampelt = första (för interpolation)
};

// De inbyggda tabellerna. Byggs första gången de används.
namespace Wavetables
{
    int count();                  // antal inbyggda
    int userIndex();              // TABLE-listans sista val, "User WAV": oscillatorns egen inladdade tabell
    juce::StringArray names();    // de inbyggda och sist "User WAV"
    const Wavetable& get (int index);

    // En tabell av ljud (som i Serum): en fil med cykler om 2048 samples blir upp till 16 frames
    // som POS sveper igenom; en kort fil räknas som en enda cykel. `framesOut` får de 16 frames
    // (numFrames * frameSize) som tabellen byggdes av, så att den kan sparas i projektet.
    std::unique_ptr<Wavetable> fromAudio (const std::vector<float>& mono, const juce::String& name, std::vector<float>& framesOut);
    std::unique_ptr<Wavetable> fromFrames (const juce::String& name, const std::vector<float>& frames);
}
