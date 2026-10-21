#include "FmPresets.h"

#include "../Core/Params.h"

namespace FmPresets
{
    namespace
    {
        const Preset table[] {
            //  namn               nivå  ratio index envAmt envMs  semis  pitchEnv  pitchDec  filter2 HP
            { "Zap",               0.8f, 2.0f, 0.7f, 1.0f,  80.0f, 0.0f,  24.0f,    60.0f,    false },
            { "Acid Squelch",      0.7f, 1.0f, 0.5f, 0.8f, 150.0f, 0.0f,   0.0f,    60.0f,    false },
            { "Metallic Lead",     0.6f, 3.5f, 0.6f, 0.3f, 300.0f, 0.0f,   0.0f,    60.0f,    true  },
            { "FM Pluck",          0.7f, 2.0f, 0.4f, 1.0f, 120.0f, 0.0f,   0.0f,    60.0f,    false },
        };

        void set (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float plain)
        {
            if (auto* parameter = apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain));
                parameter->endChangeGesture();
            }
        }
    }

    int count() { return (int) std::size (table) + 1; }

    const Preset& get (int index) { return table[juce::jlimit (1, count() - 1, index) - 1]; }

    juce::StringArray names()
    {
        juce::StringArray result { "Custom" };
        for (auto& preset : table)
            result.add (preset.name);
        return result;
    }

    void apply (juce::AudioProcessorValueTreeState& apvts, int layer, int index)
    {
        if (index < 1 || index >= count())
            return;
        using F = InstrumentField;
        auto id = [layer] (F field) { return ParamID::bass (layer, field); };
        const auto& p = get (index);
        set (apvts, id (F::fmOn), 1.0f);
        set (apvts, id (F::fmLevel), p.level);
        set (apvts, id (F::fmRatio), p.ratio);
        set (apvts, id (F::fmIndex), p.index);
        set (apvts, id (F::fmEnvAmount), p.envAmount);
        set (apvts, id (F::fmEnvTime), p.envMs);
        set (apvts, id (F::fmSemis), p.semis);
        set (apvts, id (F::pitchEnvAmount), p.pitchEnvSemis);
        set (apvts, id (F::pitchEnvDecay), p.pitchEnvDecayMs);
        if (p.filter2Hp)
        {
            set (apvts, id (F::filter2On), 1.0f);
            set (apvts, id (F::filter2Type), 3.0f);   // High
            set (apvts, id (F::filter2Cutoff), 120.0f);
        }
    }
}
