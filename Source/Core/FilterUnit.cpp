#include "FilterUnit.h"

void FilterUnit::prepare (double sampleRate)
{
    for (auto* filter : { &svf1, &svf2 })
    {
        filter->prepare ({ sampleRate, 512, 1 });
        filter->setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
    svf1.setResonance (0.707f); // det första LP-steget i LP 24 har alltid den resonansen (sätts här, inte vid varje ny cutoff)
    appliedCutoff1 = appliedCutoff2 = appliedResonance2 = -1.0f;
    appliedType2 = -1;
    tb303.prepare (sampleRate); // (den funktionsstatiska halvbandsdesignen skapas här, aldrig på ljudtråden)
    model.prepare (sampleRate);
    appliedModel = 0;
    activeType = 0;
    reset();
}

void FilterUnit::reset()
{
    svf1.reset();
    svf2.reset();
    tb303.reset();
    model.reset();
}

void FilterUnit::set (int newModel, float cutoffHz, float resonance, float p1, float p2, int filterType)
{
    // Ett modellbyte nollställer filtret som tar över (cutoff och resonans uppdateras annars på samma ställe och i samma takt som SVF:ns cutoff:
    // per sampling när den ändrats av filter-envelopen).
    if (newModel != appliedModel)
    {
        switch (newModel)
        {
            case 0:  svf1.reset(); svf2.reset(); break;
            case 1:  tb303.reset(); break;
            default: model.reset(); break;
        }
        appliedModel = newModel;
    }
    activeType = filterType;

    if (newModel >= 2)
    {
        model.setParameters ((ModelFilter::Model) juce::jlimit (2, 4, newModel), (double) cutoffHz, (double) resonance, (double) p1, (double) p2, filterType);
        return;
    }
    if (newModel == 1)
    {
        tb303.setParameters ((double) cutoffHz, 100.0 * (double) resonance); // TB-303: resonans i procent, filterType ignoreras
        return;
    }

    // Kickbase (SVF). Bara det som ändrats sätts: varje set-funktion räknar om koefficienterna (tan) ur alla värden, så resultatet är detsamma som att sätta
    // allt, men filter-envelopen ändrar cutoff varje sampling och då räknades de om dubbelt så många gånger som behövs.
    if (resonance != lastResonance)
    {
        lastResonance = resonance;
        cachedQ = 0.5f * std::pow (16.0f, resonance);
    }
    const auto q = cachedQ;

    using Type = juce::dsp::StateVariableTPTFilterType;
    const auto type = filterType == 2 ? Type::bandpass : filterType == 3 ? Type::highpass : Type::lowpass;
    if ((int) type != appliedType2)
    {
        svf2.setType (type);
        appliedType2 = (int) type;
    }
    if (cutoffHz != appliedCutoff2)
    {
        svf2.setCutoffFrequency (cutoffHz);
        appliedCutoff2 = cutoffHz;
    }
    if (q != appliedResonance2)
    {
        svf2.setResonance (q);
        appliedResonance2 = q;
    }
    if (filterType == 0 && cutoffHz != appliedCutoff1)
    {
        svf1.setCutoffFrequency (cutoffHz);
        appliedCutoff1 = cutoffHz;
    }
}

float FilterUnit::process (float input)
{
    switch (appliedModel)
    {
        case 0:
            if (activeType == 0)
                input = svf1.processSample (0, input);
            return svf2.processSample (0, input);
        case 1:
            return tb303.process (input);
        default:
            return model.process (input);
    }
}
