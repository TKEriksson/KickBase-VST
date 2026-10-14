#include "Presets.h"
#include "Params.h"

namespace
{
    struct StepTweak
    {
        int step;      // steg i takten (upprepas i alla takter)
        int pitch;     // halvtoner
        bool slide;
    };

    struct Preset
    {
        const char* name;
        PatternTemplate pattern;
        std::vector<std::pair<const char*, float>> values;
        std::vector<StepTweak> tweaks; // bara för sextondelsmönster
    };

    const std::vector<Preset>& all()
    {
        using namespace ParamID;

        static const std::vector<Preset> presets {
            { "Init", PatternTemplate::rolling, {}, {} },

            { "Full On Roller", PatternTemplate::rolling,
              { { kickPitch, 48.0f }, { kickPitchTime, 35.0f }, { kickDecay, 120.0f }, { kickDrive, 0.35f }, { kickClick, 0.4f },
                { bassCutoff, 220.0f }, { bassEnvAmount, 4.5f }, { bassEnvDecay, 90.0f }, { bassResonance, 0.35f },
                { bassDecay, 120.0f }, { bassSustain, 0.4f }, { bassRelease, 10.0f }, { duckAmount, 0.9f }, { bassGate, 0.85f } },
              {} },

            { "Dark Night", PatternTemplate::rolling,
              { { kickTune, -1.0f }, { kickPitch, 36.0f }, { kickDecay, 160.0f }, { kickDrive, 0.5f }, { kickClick, 0.2f },
                { kickClickTone, 3000.0f },
                { bassWave, 1.0f }, { bassCutoff, 150.0f }, { bassResonance, 0.55f }, { bassEnvAmount, 5.5f },
                { bassEnvDecay, 70.0f }, { bassSub, 0.2f }, { bassDrive, 0.45f }, { bassSustain, 0.3f } },
              { { 15, 12, false } } },

            { "Galloping Goa", PatternTemplate::galloping,
              { { kickPitch, 44.0f }, { kickDecay, 130.0f },
                { bassCutoff, 400.0f }, { bassEnvAmount, 3.0f }, { bassEnvDecay, 150.0f }, { bassGlide, 40.0f },
                { bassSustain, 0.7f }, { bassGate, 0.9f } },
              { { 6, 0, true }, { 7, 7, false }, { 14, 0, true }, { 15, 12, false } } },

            { "Triplet Prog", PatternTemplate::triplet,
              { { kickPitch, 40.0f }, { kickDecay, 180.0f }, { kickClick, 0.25f },
                { bassCutoff, 260.0f }, { bassEnvDecay, 140.0f }, { bassSustain, 0.6f }, { bassRelease, 25.0f } },
              {} },

            { "Offbeat Bounce", PatternTemplate::offbeat,
              { { kickDecay, 200.0f }, { kickPitch, 38.0f },
                { bassWave, 2.0f }, { bassCutoff, 900.0f }, { bassEnvAmount, 2.0f }, { bassSustain, 0.8f },
                { bassSub, 0.5f }, { duckAmount, 0.6f } },
              {} },
        };

        return presets;
    }
}

namespace Presets
{
    int count() { return (int) all().size(); }

    juce::String name (int index)
    {
        return juce::isPositiveAndBelow (index, count()) ? juce::String (all()[(size_t) index].name) : juce::String();
    }

    BassGrid apply (int index, juce::AudioProcessorValueTreeState& apvts, Pattern& pattern)
    {
        if (! juce::isPositiveAndBelow (index, count()))
            return BassGrid::sixteenth;

        const auto& preset = all()[(size_t) index];

        auto set = [] (juce::RangedAudioParameter& parameter, float normalised)
        {
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost (normalised);
            parameter.endChangeGesture();
        };

        // Allt till standard först, så att presets inte ärver värden från varandra.
        // Mönsterlängden behålls: den hör till låten snarare än ljudet.
        for (auto* p : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && ranged->getParameterID() != ParamID::patternBars)
                set (*ranged, ranged->getDefaultValue());

        for (const auto& [id, value] : preset.values)
            if (auto* parameter = apvts.getParameter (id))
                set (*parameter, parameter->convertTo0to1 (value));

        const auto grid = pattern.applyTemplate (preset.pattern);

        for (const auto& tweak : preset.tweaks)
        {
            for (int bar = 0; bar < Pattern::maxBars; ++bar)
            {
                auto& step = pattern.bassStep (BassGrid::sixteenth, bar * Pattern::bassStepsPerBar (BassGrid::sixteenth) + tweak.step);
                step.on = true;
                step.pitch = tweak.pitch;
                step.slide = tweak.slide;
            }
        }

        if (auto* gridParam = apvts.getParameter (ParamID::bassGrid))
            set (*gridParam, gridParam->convertTo0to1 ((float) grid));

        return grid;
    }
}
