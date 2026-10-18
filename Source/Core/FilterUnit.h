#pragma once

#include "FilterModels.h"
#include "Tb303Filter.h"
#include <juce_dsp/juce_dsp.h>

// Ett filter för en kanal med alla fem modellerna bakom ett gemensamt gränssnitt (fas D3; fas P behöver det per röst):
// 0 Kickbase (SVF, som alltid), 1 TB-303 (Tb303Filter), 2 Ladder, 3 Formant, 4 Comb (ModelFilter).
//
// Utbrutet ur InstrumentSynth (där Kickbase-modellens SVF-par och TB-303 låg inbakade) utan att ändra ljudet: rendersig* är bit för bit lika.
// Ett modellbyte nollställer filtret som tar över; koefficienterna räknas bara om när något ändrats.
class FilterUnit
{
public:
    void prepare (double sampleRate); // allokerar (Comb) och bygger halvbandsdesignen: aldrig på ljudtråden
    void reset();                     // nollställer allt tillstånd (inte koefficienterna)

    // Anropas före varje process(). `filterType`: 0 LP 24, 1 LP 12, 2 Band, 3 High (gäller Kickbase och Ladder). `p1`, `p2`: se ModelFilter.
    void set (int model, float cutoffHz, float resonance, float p1, float p2, int filterType);

    float process (float input);

private:
    // Kickbase-modellen: LP 24 = två lågpass efter varandra (det första med fast resonans 0,707), övriga typer ett filter (12 dB).
    juce::dsp::StateVariableTPTFilter<float> svf1, svf2;
    Tb303Filter tb303;
    ModelFilter model;

    int appliedModel = 0, activeType = 0;
    float appliedCutoff1 = -1.0f, appliedCutoff2 = -1.0f, appliedResonance2 = -1.0f;
    int appliedType2 = -1;
    float lastResonance = -1.0f, cachedQ = 0.5f; // q = 0,5 * 16^resonans
};
