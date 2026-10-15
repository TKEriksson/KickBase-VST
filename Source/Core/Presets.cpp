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

    // Ett parametervärde. Konstruktorn tar float, så att heltal som 1 blir float vid anropet.
    struct Value
    {
        Value (const char* idIn, float valueIn) : id (idIn), value (valueIn) {}
        const char* id;
        float value;
    };

    struct Preset
    {
        const char* name;
        std::vector<Value> values;
        PatternTemplate pattern = PatternTemplate::rolling; // bara Full och Pattern
        std::vector<StepTweak> tweaks;                      // bara sextondelsmönster
        std::vector<SidechainCurve::Point> curve;           // bara Sidechain; tom = standardkurvan
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

    // Effekternas parameter-ID skrivs ut i sin helhet här (se Effects::addParameters).
    const std::vector<Preset>& effectPresets (EffectType type)
    {
        static const std::vector<Preset> filter {
            { "Default", {} },
            { "Warm Low Pass",   { { "fxFilterOn", 1 }, { "fxFilterCutoff", 900.0f }, { "fxFilterReso", 0.3f } } },
            { "Thin High Pass",  { { "fxFilterOn", 1 }, { "fxFilterType", 1 }, { "fxFilterCutoff", 250.0f }, { "fxFilterReso", 0.1f } } },
            { "Band Pass Talk",  { { "fxFilterOn", 1 }, { "fxFilterType", 2 }, { "fxFilterCutoff", 1200.0f }, { "fxFilterReso", 0.6f } } },
            { "Acid Resonance",  { { "fxFilterOn", 1 }, { "fxFilterCutoff", 450.0f }, { "fxFilterReso", 0.8f } } },
        };
        static const std::vector<Preset> distortion {
            { "Default", {} },
            { "Warm Saturation", { { "fxDistOn", 1 }, { "fxDistDrive", 0.25f }, { "fxDistTone", 6000.0f } } },
            { "Hard Clip",       { { "fxDistOn", 1 }, { "fxDistMode", 1 }, { "fxDistDrive", 0.6f }, { "fxDistTone", 7000.0f } } },
            { "Folded",          { { "fxDistOn", 1 }, { "fxDistMode", 2 }, { "fxDistDrive", 0.5f }, { "fxDistTone", 5000.0f }, { "fxDistMix", 0.6f } } },
            { "Lo-Fi Crush",     { { "fxDistOn", 1 }, { "fxDistMode", 3 }, { "fxDistDrive", 0.6f }, { "fxDistTone", 7000.0f }, { "fxDistMix", 0.7f } } },
        };
        static const std::vector<Preset> eq {
            { "Default", {} },
            { "Psy Bass", { { "fxEqOn", 1 }, { "fxEqLow", 2.0f }, { "fxEqMidFreq", 300.0f }, { "fxEqMid", -3.0f }, { "fxEqHigh", 2.0f } } },
            { "Scooped",  { { "fxEqOn", 1 }, { "fxEqMidFreq", 600.0f }, { "fxEqMid", -6.0f } } },
            { "Bright",   { { "fxEqOn", 1 }, { "fxEqHigh", 6.0f } } },
            { "Thin",     { { "fxEqOn", 1 }, { "fxEqLow", -8.0f } } },
        };
        static const std::vector<Preset> chorus {
            { "Default", {} },
            { "Subtle Width", { { "fxChorusOn", 1 }, { "fxChorusRate", 0.5f }, { "fxChorusDepth", 0.2f }, { "fxChorusMix", 0.25f } } },
            { "Wide",         { { "fxChorusOn", 1 }, { "fxChorusRate", 0.9f }, { "fxChorusDepth", 0.5f }, { "fxChorusMix", 0.5f } } },
            { "Flanger",      { { "fxChorusOn", 1 }, { "fxChorusDelay", 2.0f }, { "fxChorusFeedback", 0.7f }, { "fxChorusDepth", 0.6f }, { "fxChorusMix", 0.5f } } },
        };
        static const std::vector<Preset> phaser {
            { "Default", {} },
            { "Slow Sweep",  { { "fxPhaserOn", 1 }, { "fxPhaserRate", 0.1f }, { "fxPhaserDepth", 0.8f }, { "fxPhaserFeedback", 0.5f }, { "fxPhaserMix", 0.5f } } },
            { "Fast Wobble", { { "fxPhaserOn", 1 }, { "fxPhaserRate", 3.0f }, { "fxPhaserDepth", 0.5f }, { "fxPhaserMix", 0.4f } } },
            { "Psychedelic", { { "fxPhaserOn", 1 }, { "fxPhaserRate", 0.25f }, { "fxPhaserDepth", 1.0f }, { "fxPhaserFeedback", 0.85f },
                               { "fxPhaserFreq", 600.0f }, { "fxPhaserMix", 0.6f } } },
        };
        static const std::vector<Preset> echo {
            { "Default", {} },
            { "Ping Pong 3/16", { { "fxEchoOn", 1 }, { "fxEchoTime", 5 }, { "fxEchoFeedback", 0.45f }, { "fxEchoPingPong", 1 }, { "fxEchoMix", 0.3f } } },
            { "Straight 1/8",   { { "fxEchoOn", 1 }, { "fxEchoTime", 4 }, { "fxEchoFeedback", 0.35f }, { "fxEchoPingPong", 0 }, { "fxEchoMix", 0.25f } } },
            { "Dub 1/4",        { { "fxEchoOn", 1 }, { "fxEchoTime", 6 }, { "fxEchoFeedback", 0.6f }, { "fxEchoHighCut", 2500.0f }, { "fxEchoMix", 0.35f } } },
            { "Triplet Bounce", { { "fxEchoOn", 1 }, { "fxEchoTime", 3 }, { "fxEchoFeedback", 0.4f }, { "fxEchoPingPong", 1 }, { "fxEchoMix", 0.3f } } },
        };
        static const std::vector<Preset> reverb {
            { "Default", {} },
            { "Small Room",   { { "fxReverbOn", 1 }, { "fxReverbSize", 0.25f }, { "fxReverbDamping", 0.6f }, { "fxReverbMix", 0.15f } } },
            { "Big Hall",     { { "fxReverbOn", 1 }, { "fxReverbSize", 0.9f }, { "fxReverbDamping", 0.4f }, { "fxReverbLowCut", 400.0f }, { "fxReverbMix", 0.3f } } },
            { "Gated Reverb", { { "fxReverbOn", 1 }, { "fxReverbPost", 0 }, { "fxReverbSize", 0.7f }, { "fxReverbMix", 0.45f } } },
        };

        switch (type)
        {
            case EffectType::filter:     return filter;
            case EffectType::distortion: return distortion;
            case EffectType::eq:         return eq;
            case EffectType::chorus:     return chorus;
            case EffectType::phaser:     return phaser;
            case EffectType::echo:       return echo;
            case EffectType::reverb:     return reverb;
        }
        return filter;
    }

    const std::vector<Preset>& effectChainPresets()
    {
        static const std::vector<Preset> presets {
            { "Default (all off)", {} },
            { "Psy Glue",     { { "fxEqOn", 1 }, { "fxEqLow", 2.0f }, { "fxEqMidFreq", 300.0f }, { "fxEqMid", -3.0f }, { "fxEqHigh", 2.0f },
                                { "fxDistOn", 1 }, { "fxDistDrive", 0.2f }, { "fxDistTone", 7000.0f } } },
            { "Spacey",       { { "fxEchoOn", 1 }, { "fxEchoTime", 5 }, { "fxEchoFeedback", 0.45f }, { "fxEchoMix", 0.25f },
                                { "fxReverbOn", 1 }, { "fxReverbSize", 0.8f }, { "fxReverbLowCut", 400.0f }, { "fxReverbMix", 0.2f } } },
            { "Gated Psy",    { { "fxReverbOn", 1 }, { "fxReverbPost", 0 }, { "fxReverbSize", 0.7f }, { "fxReverbMix", 0.4f },
                                { "fxFilterOn", 1 }, { "fxFilterCutoff", 1500.0f }, { "fxFilterReso", 0.3f } } },
            { "Acid Squelch", { { "fxFilterOn", 1 }, { "fxFilterCutoff", 500.0f }, { "fxFilterReso", 0.75f },
                                { "fxDistOn", 1 }, { "fxDistMode", 1 }, { "fxDistDrive", 0.45f } } },
            { "Wide Phase",   { { "fxPhaserOn", 1 }, { "fxPhaserRate", 0.15f }, { "fxPhaserDepth", 0.7f }, { "fxPhaserMix", 0.4f },
                                { "fxChorusOn", 1 }, { "fxChorusDepth", 0.25f }, { "fxChorusMix", 0.3f } } },
        };
        return presets;
    }

    const std::vector<Preset>& sidechainPresets()
    {
        static const std::vector<Preset> presets {
            { "Follow Kick", { { "scMode", 0 } } },
            { "Classic Pump", { { "scMode", 1 }, { "scLength", 4 }, { "duckAmount", 0.9f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.05f, 1.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } } },
            { "Tight 1/16", { { "scMode", 1 }, { "scLength", 1 }, { "duckAmount", 0.85f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.4f, 0.9f, -0.4f }, { 1.0f, 0.0f, 0.0f } } },
            { "Soft 1/8", { { "scMode", 1 }, { "scLength", 2 }, { "duckAmount", 0.7f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.3f }, { 1.0f, 0.0f, 0.0f } } },
            { "Hold Then Release", { { "scMode", 1 }, { "scLength", 3 }, { "duckAmount", 1.0f } }, PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.45f, 1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } } },
        };
        return presets;
    }

    const std::vector<Preset>& masterPresets()
    {
        static const std::vector<Preset> presets {
            { "Clean", {} },
            { "Gentle Glue", { { "masterDrive", 2.0f }, { "masterClip", 0.15f }, { "masterRelease", 150.0f } } },
            { "Loud",        { { "masterDrive", 6.0f }, { "masterClip", 0.3f } } },
            { "Smash",       { { "masterDrive", 12.0f }, { "masterClip", 0.6f }, { "masterRelease", 40.0f } } },
            { "Bypass",      { { "masterOn", 0 } } },
        };
        return presets;
    }

    // Destinationernas index: se Modulation::Destination.
    const std::vector<Preset>& lfoPresets()
    {
        static const std::vector<Preset> presets {
            { "Off", {} },
            { "Filter Wobble 1/8", { { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 4 }, { "lfo1DestA", 1 }, { "lfo1AmountA", 0.25f } } },
            { "Slow Sweep 4 Bars", { { "lfo1On", 1 }, { "lfo1Shape", 1 }, { "lfo1Rate", 10 }, { "lfo1DestA", 1 }, { "lfo1AmountA", 0.35f },
                                     { "lfo1DestB", 3 }, { "lfo1AmountB", 0.2f } } },
            { "Echo Throws", { { "lfo1On", 1 }, { "lfo1Shape", 4 }, { "lfo1Rate", 8 }, { "lfo1DestA", 12 }, { "lfo1AmountA", 0.4f } } },
            { "Random Grit 1/16", { { "lfo1On", 1 }, { "lfo1Shape", 5 }, { "lfo1Rate", 2 }, { "lfo1DestA", 4 }, { "lfo1AmountA", 0.3f },
                                    { "lfo2On", 1 }, { "lfo2Shape", 5 }, { "lfo2Rate", 2 }, { "lfo2DestA", 2 }, { "lfo2AmountA", 0.2f } } },
            { "Pumping Level", { { "lfo1On", 1 }, { "lfo1Shape", 2 }, { "lfo1Rate", 6 }, { "lfo1DestA", 5 }, { "lfo1AmountA", 0.3f } } },
        };
        return presets;
    }

    const std::vector<Preset>& listFor (Section section, bool effectVersion)
    {
        if (Presets::isSingleEffect (section))
            return effectPresets (Presets::effectOf (section));

        switch (section)
        {
            case Section::full:    return fullPresets();
            case Section::kick:    return kickPresets();
            case Section::bass:    return effectVersion ? gatePresets() : bassPresets();
            case Section::pattern: return patternPresets();
            case Section::effects:   return effectChainPresets();
            case Section::sidechain: return sidechainPresets();
            case Section::master:    return masterPresets();
            case Section::lfo:       return lfoPresets();
            default:                 break;
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
            case Section::effects:   return "Effects";
            case Section::sidechain: return "Sidechain";
            case Section::master:    return "Master";
            case Section::lfo:       return "LFO";
            default:                 break;
        }
        if (isSingleEffect (section))
        {
            const auto name = Effects::name (effectOf (section));
            return name.substring (0, 1) + name.substring (1).toLowerCase();
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
            case Section::effects:
                return Effects::isEffectParameter (id);
            case Section::sidechain:
                return id.startsWith ("sc") || id == ParamID::duckAmount;
            case Section::master:
                return id.startsWith ("master");
            case Section::lfo:
                return id.startsWith ("lfo");
            default:
                break;
        }
        return isSingleEffect (section) && id.startsWith (Effects::parameterPrefix (effectOf (section)));
    }

    juce::File folder (Section section, bool effectVersion)
    {
        auto name = sectionName (section);
        if (section == Section::full)
            name = effectVersion ? "Full FX" : "Full Instrument";
        else if (section == Section::bass)
            name = effectVersion ? "Gate" : "Bass";

        auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("MyOwnFruit").getChildFile ("Presets");
        if (isSingleEffect (section))
            root = root.getChildFile ("Effects");
        return root.getChildFile (name);
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
                       juce::AudioProcessorValueTreeState& apvts, Pattern& pattern, SidechainCurve& curve)
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

        if (section == Section::full || section == Section::sidechain)
        {
            if (preset.curve.empty())
                curve.reset();
            else
                curve.setPoints (preset.curve);
        }

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
