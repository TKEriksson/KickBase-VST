#pragma once

#include "Wavetable.h"
#include <vector>

// Övertons- och fasredigering av wavetable-frames (PH2). Arbetar på frames som en vanlig lista (numFrames * frameSize float, som
// Wavetables::fromFrames), aldrig på ljudtråden. Resultatet blir oscillatorns User-tabell, se PluginBase::editWavetable.
//
// Konvention: en frame är summan av A_k * sin (2 pi k x + phase_k), k = 1..frameSize / 2 (samma som Wavetable::Additive).
namespace WavetableEdit
{
    struct Edit
    {
        int harmonic = 1;       // 1 = grundtonen
        float gain = 1.0f;      // linjär faktor på amplituden (0 = ta bort överton)
        float phaseShift = 0.0f; // radianer som läggs till fasen
    };

    // Alla numFrames frames ur en tabell (obandbegränsade, som gränssnittets bild).
    std::vector<float> framesOf (const Wavetable& table);

    // Amplitud och fas (radianer) för övertonerna 1..count i en frame (frameSize samples).
    void analyse (const float* frame, int count, std::vector<float>& amp, std::vector<float>& phase);

    // Tillämpar ändringarna på en frame (frame < 0: alla). Ger varje ändrad frame samma toppvärde som före ändringen när normalise är på.
    void apply (std::vector<float>& frames, int frame, const std::vector<Edit>& edits, bool normalise = true);
}
