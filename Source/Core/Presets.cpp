#include "Presets.h"
#include "Params.h"

namespace
{
    using Section = Presets::Section;

    struct StepTweak
    {
        int step;      // steg i takten (upprepas i alla takter)
        int pitch;     // halvtoner
        bool slide;
    };

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
        PatternTemplate pattern = PatternTemplate::rolling; // bara Full och Pattern
        std::vector<StepTweak> tweaks;                      // bara sextondelsmönster
    };

    using namespace ParamID;

    const std::vector<Preset>& fullPresets()
    {
        static const std::vector<Preset> presets {
            { "Init", {} },

            { "Full On Roller",
              { { kickPitch, 48.0f }, { kickPitchTime, 35.0f }, { kickDecay, 120.0f }, { kickDrive, 0.35f }, { kickClick, 0.4f },
                { bassCutoff, 220.0f }, { bassEnvAmount, 4.5f }, { bassEnvDecay, 90.0f }, { bassResonance, 0.35f },
                { bassDecay, 120.0f }, { bassSustain, 0.4f }, { bassRelease, 10.0f }, { duckAmount, 0.9f }, { bassGate, 0.85f } } },

            { "Dark Night",
              { { kickTune, -1.0f }, { kickPitch, 36.0f }, { kickDecay, 160.0f }, { kickDrive, 0.5f }, { kickClick, 0.2f },
                { kickClickTone, 3000.0f },
                { bassWave, 1.0f }, { bassCutoff, 150.0f }, { bassResonance, 0.55f }, { bassEnvAmount, 5.5f },
                { bassEnvDecay, 70.0f }, { bassSub, 0.2f }, { bassDrive, 0.45f }, { bassSustain, 0.3f } },
              PatternTemplate::rolling, { { 15, 12, false } } },

            { "Galloping Goa",
              { { kickPitch, 44.0f }, { kickDecay, 130.0f },
                { bassCutoff, 400.0f }, { bassEnvAmount, 3.0f }, { bassEnvDecay, 150.0f }, { bassGlide, 40.0f },
                { bassSustain, 0.7f }, { bassGate, 0.9f } },
              PatternTemplate::galloping, { { 6, 0, true }, { 7, 7, false }, { 14, 0, true }, { 15, 12, false } } },

            { "Triplet Prog",
              { { kickPitch, 40.0f }, { kickDecay, 180.0f }, { kickClick, 0.25f },
                { bassCutoff, 260.0f }, { bassEnvDecay, 140.0f }, { bassSustain, 0.6f }, { bassRelease, 25.0f } },
              PatternTemplate::triplet },

            { "Offbeat Bounce",
              { { kickDecay, 200.0f }, { kickPitch, 38.0f },
                { bassWave, 2.0f }, { bassCutoff, 900.0f }, { bassEnvAmount, 2.0f }, { bassSustain, 0.8f },
                { bassSub, 0.5f }, { duckAmount, 0.6f } },
              PatternTemplate::offbeat },

            { "Squelch Roller",
              { { kickPitch, 46.0f }, { kickPitchTime, 32.0f }, { kickDecay, 110.0f }, { kickDrive, 0.4f },
                { bassCutoff, 180.0f }, { bassResonance, 0.75f }, { bassEnvAmount, 6.0f }, { bassEnvDecay, 110.0f },
                { bassVelToEnv, 0.8f }, { bassDrive, 0.3f }, { bassSustain, 0.35f } },
              PatternTemplate::rolling, { { 3, 12, false }, { 11, -12, false } } },
        };
        return presets;
    }

    const std::vector<Preset>& kickPresets()
    {
        static const std::vector<Preset> presets {
            { "Default", {} },
            { "Punchy Full On",
              { { kickPitch, 48.0f }, { kickPitchTime, 35.0f }, { kickPitchCurve, 3.5f }, { kickHold, 10.0f }, { kickDecay, 120.0f },
                { kickDecayCurve, 2.2f }, { kickClick, 0.45f }, { kickClickTone, 6000.0f }, { kickDrive, 0.35f } } },
            { "Deep Prog",
              { { kickTune, -2.0f }, { kickPitch, 34.0f }, { kickPitchTime, 60.0f }, { kickPitchCurve, 2.5f }, { kickHold, 25.0f },
                { kickDecay, 200.0f }, { kickDecayCurve, 1.6f }, { kickClick, 0.15f }, { kickClickTone, 2500.0f }, { kickDrive, 0.15f } } },
            { "Hard Click",
              { { kickPitch, 52.0f }, { kickPitchTime, 25.0f }, { kickClick, 0.8f }, { kickClickLength, 3.0f },
                { kickClickTone, 9000.0f }, { kickDecay, 110.0f }, { kickDrive, 0.5f } } },
            { "Long Tail Goa",
              { { kickPitch, 40.0f }, { kickPitchTime, 50.0f }, { kickHold, 30.0f }, { kickDecay, 320.0f },
                { kickDecayCurve, 1.4f }, { kickDrive, 0.25f } } },
            { "Dark Distorted",
              { { kickPitch, 44.0f }, { kickPitchTime, 40.0f }, { kickDecay, 150.0f }, { kickDrive, 0.85f },
                { kickClick, 0.3f }, { kickClickTone, 3500.0f } } },
            { "Tight Short",
              { { kickPitch, 46.0f }, { kickPitchTime, 28.0f }, { kickHold, 5.0f }, { kickDecay, 85.0f },
                { kickDecayCurve, 2.8f }, { kickClick, 0.35f } } },
        };
        return presets;
    }

    const std::vector<Preset>& bassPresets()
    {
        static const std::vector<Preset> presets {
            { "Default", {} },
            { "Rolling Saw",
              { { bassCutoff, 220.0f }, { bassEnvAmount, 4.5f }, { bassEnvDecay, 90.0f }, { bassResonance, 0.35f },
                { bassDecay, 120.0f }, { bassSustain, 0.4f }, { bassRelease, 10.0f } } },
            { "Dark Square",
              { { bassWave, 1.0f }, { bassCutoff, 150.0f }, { bassResonance, 0.55f }, { bassEnvAmount, 5.5f },
                { bassEnvDecay, 70.0f }, { bassSub, 0.2f }, { bassDrive, 0.45f }, { bassSustain, 0.3f } } },
            { "Squelchy",
              { { bassCutoff, 180.0f }, { bassResonance, 0.75f }, { bassEnvAmount, 6.0f }, { bassEnvDecay, 110.0f },
                { bassVelToEnv, 0.8f }, { bassDrive, 0.3f } } },
            { "Soft Sub",
              { { bassWave, 3.0f }, { bassSub, 0.6f }, { bassCutoff, 1200.0f }, { bassEnvAmount, 1.0f },
                { bassSustain, 0.8f }, { bassRelease, 25.0f } } },
            { "Plucky Goa",
              { { bassCutoff, 300.0f }, { bassEnvAmount, 5.0f }, { bassEnvDecay, 60.0f }, { bassDecay, 80.0f },
                { bassSustain, 0.15f }, { bassRelease, 8.0f }, { bassGlide, 40.0f } } },
            { "Fat Triangle",
              { { bassWave, 2.0f }, { bassSub, 0.4f }, { bassCutoff, 600.0f }, { bassEnvAmount, 3.0f }, { bassDrive, 0.5f } } },
        };
        return presets;
    }

    // FX-versionens "Bass" är gaten.
    const std::vector<Preset>& gatePresets()
    {
        static const std::vector<Preset> presets {
            { "Default", {} },
            { "Tight",  { { gateAttack, 0.5f }, { gateRelease, 5.0f } } },
            { "Plucky", { { gateAttack, 0.3f }, { gateRelease, 15.0f } } },
            { "Soft",   { { gateAttack, 5.0f }, { gateRelease, 40.0f } } },
            { "Smooth", { { gateAttack, 12.0f }, { gateRelease, 80.0f } } },
        };
        return presets;
    }

    const std::vector<Preset>& patternPresets()
    {
        static const std::vector<Preset> presets {
            { "Rolling", {}, PatternTemplate::rolling },
            { "Galloping", {}, PatternTemplate::galloping },
            { "Triplet", {}, PatternTemplate::triplet },
            { "Offbeat", {}, PatternTemplate::offbeat },
            { "Rolling + Octaves", {}, PatternTemplate::rolling, { { 3, 12, false }, { 7, 12, false }, { 11, 12, false }, { 15, 12, false } } },
            { "Galloping Slide", {}, PatternTemplate::galloping, { { 6, 0, true }, { 7, 7, false }, { 14, 0, true }, { 15, 12, false } } },
            { "Clear", {}, PatternTemplate::clear },
        };
        return presets;
    }

    const std::vector<Preset>& listFor (Section section, bool effectVersion)
    {
        switch (section)
        {
            case Section::full:    return fullPresets();
            case Section::kick:    return kickPresets();
            case Section::bass:    return effectVersion ? gatePresets() : bassPresets();
            case Section::pattern: return patternPresets();
        }
        return fullPresets();
    }
}

namespace Presets
{
    juce::String sectionName (Section section)
    {
        switch (section)
        {
            case Section::full:    return "Full";
            case Section::kick:    return "Kick";
            case Section::bass:    return "Bass";
            case Section::pattern: return "Pattern";
        }
        return {};
    }

    bool belongsTo (Section section, const juce::String& id)
    {
        switch (section)
        {
            case Section::full:
                return true;
            case Section::kick:
                return id.startsWith ("kick");
            case Section::bass:
                return (id.startsWith ("bass") && id != ParamID::bassOn && id != ParamID::bassGate && id != ParamID::bassGrid)
                    || id == ParamID::gateAttack || id == ParamID::gateRelease;
            case Section::pattern:
                return id == ParamID::patternBars || id == ParamID::bassGrid || id == ParamID::bassGate || id == ParamID::noteEnd;
        }
        return false;
    }

    juce::File folder (Section section, bool effectVersion)
    {
        auto name = sectionName (section);
        if (section == Section::full)
            name = effectVersion ? "Full FX" : "Full Instrument";
        else if (section == Section::bass)
            name = effectVersion ? "Gate" : "Bass";

        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("MyOwnFruit").getChildFile ("Presets").getChildFile (name);
    }

    int factoryCount (Section section, bool effectVersion)
    {
        return (int) listFor (section, effectVersion).size();
    }

    juce::String factoryName (Section section, bool effectVersion, int index)
    {
        const auto& list = listFor (section, effectVersion);
        return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
    }

    void setParameter (juce::RangedAudioParameter& parameter, float normalisedValue)
    {
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (normalisedValue);
        parameter.endChangeGesture();
    }

    void applyFactory (Section section, bool effectVersion, int index,
                       juce::AudioProcessorValueTreeState& apvts, Pattern& pattern)
    {
        const auto& list = listFor (section, effectVersion);
        if (! juce::isPositiveAndBelow (index, (int) list.size()))
            return;

        const auto& preset = list[(size_t) index];

        // Delens parametrar till standard först, så att presets inte ärver värden från varandra.
        // Mönsterlängden behålls av Full: den hör till låten snarare än ljudet.
        if (section != Section::pattern)
        {
            for (auto* p : apvts.processor.getParameters())
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
                if (ranged == nullptr || ! belongsTo (section, ranged->getParameterID()) || ranged->getParameterID() == ParamID::patternBars)
                    continue;
                setParameter (*ranged, ranged->getDefaultValue());
            }
        }

        for (const auto& [id, value] : preset.values)
            if (auto* parameter = apvts.getParameter (id))
                setParameter (*parameter, parameter->convertTo0to1 (value));

        if (section != Section::full && section != Section::pattern)
            return;

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
            setParameter (*gridParam, gridParam->convertTo0to1 ((float) grid));
    }
}
