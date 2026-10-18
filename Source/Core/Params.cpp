#include "Params.h"
#include "Modulation.h"
#include "Effects.h"
#include <map>
#include <string>

namespace
{
    using namespace juce;

    String formatMs (float ms)
    {
        if (ms >= 1000.0f) return String (ms / 1000.0f, 2) + " s";
        if (ms < 10.0f)    return String (ms, 1) + " ms";
        return String (roundToInt (ms)) + " ms";
    }

    String formatHz (float hz)
    {
        if (hz >= 1000.0f) return String (hz / 1000.0f, 1) + " kHz";
        return String (roundToInt (hz)) + " Hz";
    }

    String formatSemis (float st, int decimals)
    {
        return (st > 0.0f ? "+" : "") + String (st, decimals) + " st";
    }

    String formatPercent (float value) { return String (roundToInt (value * 100.0f)) + " %"; }


    // Omfång där `centre` hamnar mitt på ratten, så att små värden får mer av vandringen.
    NormalisableRange<float> skewed (float min, float max, float centre, float interval = 0.0f)
    {
        NormalisableRange<float> range (min, max, interval);
        range.setSkewForCentre (centre);
        return range;
    }

    template <typename Formatter>
    std::unique_ptr<AudioParameterFloat> floatParam (const String& id, const String& name, NormalisableRange<float> range,
                                                     float defaultValue, Formatter format)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, defaultValue,
            AudioParameterFloatAttributes().withStringFromValueFunction ([format] (float v, int) { return format (v); }));
    }

    auto ms (float min, float max, float centre, float def, const String& id, const String& name)
    {
        return floatParam (id, name, skewed (min, max, centre, 0.01f), def, formatMs);
    }

    auto percent (const String& id, const String& name, float def)
    {
        return floatParam (id, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def, formatPercent);
    }

    auto curve (const String& id, const String& name, float def)
    {
        return floatParam (id, name, skewed (0.2f, 8.0f, 1.5f, 0.01f), def, [] (float v) { return String (v, 2); });
    }
}

namespace
{
    // "None", "FX1" ... "FX11": platsen är den gamla bokstaven A-E och sedan fler (sparade värden är index).
    juce::StringArray fxChannelChoices()
    {
        juce::StringArray choices { "None" };
        for (int channel = 0; channel < numEffectChannels; ++channel)
            choices.add ("FX" + juce::String (channel + 1));
        return choices;
    }
}

namespace ParamID
{
    const char* instrumentFieldName (InstrumentField field)
    {
        static const char* names[] { "On", "Level", "Gate", "Duck", "Grid", "FxChannel", "FxAmount",
                                     "Octave", "Tune",
                                     "ATable", "APos", "ALevel", "ASemi", "AFine", "AUnison", "ADetune", "ABlend", "AWarp", "AWarpAmount", "APhase", "ARandom", "APan", "AStereo", "AOn",
                                     "BTable", "BPos", "BLevel", "BSemi", "BFine", "BUnison", "BDetune", "BBlend", "BWarp", "BWarpAmount", "BPhase", "BRandom", "BPan", "BStereo", "BOn",
                                     "CTable", "CPos", "CLevel", "CSemi", "CFine", "CUnison", "CDetune", "CBlend", "CWarp", "CWarpAmount", "CPhase", "CRandom", "CPan", "CStereo", "COn",
                                     "WarpEnv", "Sub", "SubOctave", "Noise",
                                     "FilterType", "KeyTrack", "Cutoff", "Resonance", "EnvAmount", "EnvDecay", "VelToEnv",
                                     "Attack", "Decay", "Sustain", "Release", "Glide", "Drive",
                                     "GateOn", "GateGrid", "GateDepth", "GateSmooth",
                                     "SampleLevel", "SampleRoot", "SampleLoop", "SampleKey", "SampleStart", "SampleOn",
                                     "Env2Attack", "Env2Decay", "Env2Sustain", "Env2Release", "Env3Attack", "Env3Decay", "Env3Sustain", "Env3Release",
                                     "SubWave", "NoiseTone", "PitchEnvAmount", "PitchEnvDecay", "DriveMode", "Delay", "Swing", "Humanize", "EnvAttack", "FilterModel", "AMorph", "BMorph", "CMorph",
                                     "Env2Mode", "Env2AttackSync", "Env2DecaySync", "Env2ReleaseSync", "Env3Mode", "Env3AttackSync", "Env3DecaySync", "Env3ReleaseSync" };
        static_assert (std::size (names) == (size_t) InstrumentField::count);
        return names[(size_t) field];
    }

    juce::String bass (int layer, InstrumentField field)
    {
        return "bass" + juce::String (layer + 1) + instrumentFieldName (field);
    }

    namespace
    {
        // ID:n byggs en gång och lever hela programmet ut (funktionerna returnerar const char*).
        const std::array<std::string, numPatternRows>& rowNames (const char* prefix, const char* kick)
        {
            static std::map<std::string, std::array<std::string, numPatternRows>> cache;
            auto& names = cache[prefix];
            if (names[0].empty())
            {
                names[0] = std::string (prefix) + kick;
                for (int row = 1; row < numPatternRows; ++row)
                    names[(size_t) row] = std::string (prefix) + "Bass" + std::to_string (row);
            }
            return names;
        }
    }

    const char* rowLoop (int row)
    {
        return rowNames ("rowLoop", "Kick")[(size_t) juce::jlimit (0, numPatternRows - 1, row)].c_str();
    }

    const char* rowBars (int row)
    {
        return rowNames ("rowBars", "Kick")[(size_t) juce::jlimit (0, numPatternRows - 1, row)].c_str();
    }
}

namespace Params
{
    void addPatternParams (Layout& layout, bool includeNoteEnd)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::patternBars, 1 }, "Total Length",
                                                            StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars", "64 bars" }, 0));
        juce::StringArray rowLengths { "Total" };
        for (int bars = 1; bars <= Pattern::maxBars; bars *= 2)
            rowLengths.add (juce::String (bars) + (bars == 1 ? " bar" : " bars"));
        for (int row = 0; row < numPatternRows; ++row)
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::rowBars (row), 1 },
                                                                row == 0 ? juce::String ("Kick Row Length") : "Instr " + juce::String (row) + " Row Length", rowLengths, 0));
        for (int row = 0; row < numPatternRows; ++row)
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::rowLoop (row), 1 },
                                                              row == 0 ? juce::String ("Kick Loop") : "Instr " + juce::String (row) + " Loop", true));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::rowGridKick, 1 }, "Kick Grid", Pattern::gridNames(), (int) InstrumentGrid::sixteenth));
        if (includeNoteEnd)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::noteEnd, 1 }, "Note End",
                                                                StringArray { "Cut", "Finish bar", "Finish pattern" }, 0));
            layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::instrumentCount, 1 }, "Instruments", 1, ParamID::numInstrumentLayers,
                                                             ParamID::defaultInstrumentCount));
            layout.add (percent (ParamID::patSwing, "Swing", 0.0f));
            layout.add (percent (ParamID::patHumanize, "Humanize", 0.0f));
        }
    }

    void addKickParams (Layout& layout)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::kickOn, 1 }, "Kick On", true));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickKeyFollow, 1 }, "Kick Key",
                                                            StringArray { "Fixed", "Follow" }, 1));
        layout.add (floatParam (ParamID::kickTune, "Kick Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 2); }));
        // Pitch- och volym-envelopen ritas i KICK-fliken; rattarna skalar dem.
        layout.add (floatParam (ParamID::kickPitchAmount, "Kick Pitch Amount", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, formatPercent));
        layout.add (floatParam (ParamID::kickLength, "Kick Length", skewed (0.25f, 3.0f, 1.0f, 0.001f), 1.0f, formatPercent));
        layout.add (floatParam (ParamID::kickPhase, "Kick Phase", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                                [] (float v) { return String (roundToInt (v * 360.0f)) + juce::String::fromUTF8 ("\xc2\xb0"); }));
        layout.add (percent (ParamID::kickHarmonics, "Kick Harmonics", 0.0f));
        layout.add (ms (5.0f, 500.0f, 60.0f, 60.0f, ParamID::kickHarmDecay, "Kick Harmonics Decay"));
        layout.add (floatParam (ParamID::kickHarmTilt, "Kick Harmonics Tilt", NormalisableRange<float> (0.3f, 3.0f, 0.01f), 1.0f,
                                [] (float v) { return String (v, 2); }));
        layout.add (percent (ParamID::kickClick, "Kick Click", 0.3f));
        layout.add (ms (1.0f, 30.0f, 6.0f, 6.0f, ParamID::kickClickLength, "Kick Click Length"));
        layout.add (floatParam (ParamID::kickClickTone, "Kick Click Tone", skewed (500.0f, 16000.0f, 4000.0f, 1.0f), 5000.0f, formatHz));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickWave, 1 }, "Kick Wave",
                                                            StringArray { "Sine", "Triangle", "Soft Square", "Soft Saw" }, 0));
        layout.add (percent (ParamID::kickSub, "Kick Sub", 0.0f));
        layout.add (percent (ParamID::kickFm, "Kick FM", 0.0f));
        layout.add (floatParam (ParamID::kickFmRatio, "Kick FM Ratio", skewed (0.5f, 8.0f, 2.0f, 0.01f), 2.0f, [] (float v) { return String (v, 2) + "x"; }));
        layout.add (ms (2.0f, 300.0f, 40.0f, 40.0f, ParamID::kickFmDecay, "Kick FM Decay"));
        layout.add (percent (ParamID::kickNoise, "Kick Noise", 0.0f));
        layout.add (ms (5.0f, 400.0f, 60.0f, 60.0f, ParamID::kickNoiseDecay, "Kick Noise Decay"));
        layout.add (floatParam (ParamID::kickNoiseTone, "Kick Noise Tone", skewed (500.0f, 16000.0f, 4000.0f, 1.0f), 6000.0f, formatHz));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickClipMode, 1 }, "Kick Clip Mode",
                                                            StringArray { "Soft", "Hard", "Tube", "Fold" }, 0));
        layout.add (percent (ParamID::kickCrush, "Kick Crush", 0.0f));
        layout.add (floatParam (ParamID::kickLowCut, "Kick Low Cut", skewed (20.0f, 150.0f, 40.0f, 0.1f), 20.0f,
                                [] (float hz) { return hz <= 21.0f ? String ("Off") : formatHz (hz); })); // under 21 Hz är filtret av (KickEngine)
        layout.add (percent (ParamID::kickVelPitch, "Kick Vel > Pitch", 0.0f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickSampleMode, 1 }, "Kick Sample Mode",
                                                            StringArray { "Click", "Layer", "Replace" }, 0));
        layout.add (percent (ParamID::kickSampleLevel, "Kick Sample Level", 1.0f));
        layout.add (floatParam (ParamID::kickSampleTune, "Kick Sample Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 2); }));
        layout.add (floatParam (ParamID::kickSampleOffset, "Kick Sample Offset", NormalisableRange<float> (-20.0f, 20.0f, 0.05f), 0.0f,
                                [] (float v) { return String (v, 1) + " ms"; }));
        layout.add (percent (ParamID::kickDrive, "Kick Drive", 0.2f));
        layout.add (floatParam (ParamID::kickHighCut, "Kick High Cut", skewed (500.0f, 20000.0f, 4000.0f, 1.0f), 20000.0f, formatHz));
        layout.add (percent (ParamID::kickLevel, "Kick Level", 0.8f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickFxChannel, 1 }, "Kick FX Channel",
                                                            fxChannelChoices(), 0));
        layout.add (percent (ParamID::kickFxAmount, "Kick FX Amount", 1.0f));
    }

    void addInstrumentCommonParams (Layout& layout, int layer, float defaultLevel)
    {
        using F = InstrumentField;
        auto id = [layer] (F f) { return ParamID::bass (layer, f); };
        const auto title = "Instr " + String (layer + 1) + " ";

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id (F::on), 1 }, title + "On", true));
        layout.add (percent (id (F::level), title + "Level", defaultLevel));
        // LENGTH: skalar längden på alla toner i lagret (parameter-ID:t heter fortfarande Gate).
        layout.add (floatParam (id (F::gate), title + "Note Length", NormalisableRange<float> (0.05f, 1.0f, 0.001f), 1.0f, formatPercent));
        layout.add (percent (id (F::duck), title + "Duck", 0.8f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::grid), 1 }, title + "Grid",
                                                            Pattern::gridNames(), (int) InstrumentGrid::sixteenth));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::fxChannel), 1 }, title + "FX Channel",
                                                            fxChannelChoices(), 0));
        layout.add (percent (id (F::fxAmount), title + "FX Amount", 1.0f));

        // GATE: en egen av/på-rytm (eget rutnät) som hackar lagrets ljud; stegen ligger i mönstret.
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id (F::gateOn), 1 }, title + "Gate On", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::gateGrid), 1 }, title + "Gate Grid",
                                                            Pattern::gridNames(), (int) InstrumentGrid::sixteenth));
        layout.add (percent (id (F::gateDepth), title + "Gate Depth", 1.0f));
        layout.add (ms (0.5f, 50.0f, 4.0f, 3.0f, id (F::gateSmooth), title + "Gate Smooth"));
    }

    void addInstrumentSynthParams (Layout& layout, int layer)
    {
        using F = InstrumentField;
        auto id = [layer] (F f) { return ParamID::bass (layer, f); };
        const auto title = "Instr " + String (layer + 1) + " ";

        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id (F::octave), 1 }, title + "Octave", -2, 1, 0));
        layout.add (floatParam (id (F::tune), title + "Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 2); }));
        // OSC A och OSC B: wavetable, position, nivå, stämning, unison, warp och fas.
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
        {
            auto oid = [&] (F f) { return id (ParamID::oscField (osc, f)); };
            const auto oscTitle = title + "Osc " + String (osc + 1) + " "; // som i gränssnittet (OSC 1-3); ID:t har fortfarande A/B/C
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { oid (F::aTable), 1 }, oscTitle + "Table", Wavetables::names(), 0));
            layout.add (floatParam (oid (F::aPos), oscTitle + "Position", NormalisableRange<float> (0.0f, 1.0f, 0.001f), osc == 0 ? 2.0f / 3.0f : 0.0f, formatPercent));
            layout.add (percent (oid (F::aLevel), oscTitle + "Level", osc == 0 ? 1.0f : 0.0f));
            layout.add (floatParam (oid (F::aSemi), oscTitle + "Semi", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
                                    [] (float v) { return formatSemis (v, 0); }));
            layout.add (floatParam (oid (F::aFine), oscTitle + "Fine", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                    [] (float v) { return (v > 0.0f ? "+" : "") + String (v, 1) + " ct"; }));
            layout.add (std::make_unique<AudioParameterInt> (ParameterID { oid (F::aUnison), 1 }, oscTitle + "Unison", 1, 7, 1));
            layout.add (floatParam (oid (F::aDetune), oscTitle + "Detune", skewed (0.0f, 100.0f, 20.0f, 0.1f), 12.0f,
                                    [] (float v) { return String (v, 1) + " ct"; }));
            layout.add (percent (oid (F::aBlend), oscTitle + "Blend", 0.5f));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { oid (F::aWarp), 1 }, oscTitle + "Warp",
                                                                osc == 0 ? StringArray { "Off", "Sync", "Bend", "PWM", "Mirror", "FM (B)", "Flip", "Quantize", "AM" }
                                                                         : StringArray { "Off", "Sync", "Bend", "PWM", "Mirror", "Flip", "Quantize", "AM" }, 0));
            layout.add (percent (oid (F::aWarpAmount), oscTitle + "Warp Amount", 0.0f));
            layout.add (percent (oid (F::aPhase), oscTitle + "Phase", 0.0f));
            layout.add (percent (oid (F::aRandom), oscTitle + "Random Phase", 0.0f));
            layout.add (floatParam (oid (F::aPan), oscTitle + "Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
                                    [] (float v) { const auto p = roundToInt (std::abs (v) * 100.0f);
                                                   return p == 0 ? String ("C") : String (p) + (v < 0.0f ? " L" : " R"); }));
            layout.add (percent (oid (F::aStereo), oscTitle + "Stereo Spread", 0.5f));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { oid (F::aOn), 1 }, oscTitle + "On", osc < 2)); // OSC 3 är av som standard
        }
        layout.add (percent (id (F::warpEnv), title + "Warp Env", 0.0f));
        layout.add (percent (id (F::sub), title + "Sub", 0.3f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::subOctave), 1 }, title + "Sub Octave",
                                                            StringArray { "-1 oct", "0 oct" }, 1));
        layout.add (percent (id (F::noise), title + "Noise", 0.0f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::filterType), 1 }, title + "Filter Type",
                                                            StringArray { "LP 24", "LP 12", "Band", "High" }, 0));
        layout.add (percent (id (F::keyTrack), title + "Key Track", 0.0f));
        layout.add (floatParam (id (F::cutoff), title + "Cutoff", skewed (20.0f, 20000.0f, 600.0f, 0.1f), 300.0f, formatHz));
        layout.add (percent (id (F::resonance), title + "Resonance", 0.3f));
        layout.add (floatParam (id (F::envAmount), title + "Env Amount", NormalisableRange<float> (0.0f, 8.0f, 0.01f), 4.0f,
                                [] (float v) { return String (v, 1) + " oct"; }));
        layout.add (ms (5.0f, 1000.0f, 150.0f, 120.0f, id (F::envDecay), title + "Env Decay"));
        layout.add (ms (0.0f, 300.0f, 10.0f, 1.0f, id (F::envAttack), title + "Env Attack")); // 1 ms = som filtret alltid var före
        layout.add (ms (0.0f, 50.0f, 10.0f, 0.0f, id (F::delay), title + "Delay")); // fördröjer instrumentets toner (bara senare, aldrig tidigare)
        layout.add (percent (id (F::swing), title + "Swing", 0.0f));
        layout.add (percent (id (F::humanize), title + "Humanize", 0.0f));
        layout.add (percent (id (F::velToEnv), title + "Vel > Env", 0.5f));
        layout.add (ms (0.0f, 50.0f, 5.0f, 1.0f, id (F::attack), title + "Attack"));
        layout.add (ms (5.0f, 2000.0f, 200.0f, 200.0f, id (F::decay), title + "Decay"));
        layout.add (percent (id (F::sustain), title + "Sustain", 0.6f));
        layout.add (ms (1.0f, 300.0f, 30.0f, 15.0f, id (F::release), title + "Release"));
        // Används inte längre (en glide-ton glider under sin egen längd), men finns kvar så att gamla projekt och automation pekar rätt.
        layout.add (ms (1.0f, 500.0f, 80.0f, 60.0f, id (F::glide), title + "Glide (unused)"));
        layout.add (percent (id (F::drive), title + "Drive", 0.2f));

        // Samplingen (laddas in på lagrets sida): nivå, samplingens egen ton, loop, om den följer noten, och startpunkt.
        layout.add (percent (id (F::sampleLevel), title + "Sample Level", 1.0f));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id (F::sampleRoot), 1 }, title + "Sample Root", 12, 96, 48,
                                                         AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                         {
                                                             static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                                                             return String (names[((v % 12) + 12) % 12]) + String (v / 12); // C5 = 60, som i FL Studio
                                                         })));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::sampleLoop), 1 }, title + "Sample Loop", StringArray { "Off", "On" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::sampleKey), 1 }, title + "Sample Key Track", StringArray { "Off", "On" }, 0));
        layout.add (percent (id (F::sampleStart), title + "Sample Start", 0.0f));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id (F::sampleOn), 1 }, title + "Sample On", false));

        // ENV 2 och ENV 3: envelopes som inte styr ljudet själva utan används som modulationskällor.
        for (int env = 0; env < 2; ++env)
        {
            const auto envTitle = title + "Env " + String (env + 2) + " ";
            const auto base = (F) ((int) F::env2Attack + env * 4);
            layout.add (ms (0.1f, 3000.0f, 40.0f, 5.0f, id (base), envTitle + "Attack"));
            layout.add (ms (5.0f, 6000.0f, 400.0f, 400.0f, id ((F) ((int) base + 1)), envTitle + "Decay"));
            layout.add (percent (id ((F) ((int) base + 2)), envTitle + "Sustain", 0.0f));
            layout.add (ms (5.0f, 6000.0f, 300.0f, 250.0f, id ((F) ((int) base + 3)), envTitle + "Release"));
        }
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::subWave), 1 }, title + "Sub Wave",
                                                            StringArray { "Sine", "Triangle", "Square", "Saw" }, 0));
        layout.add (percent (id (F::noiseTone), title + "Noise Tone", 1.0f));
        layout.add (floatParam (id (F::pitchEnvAmount), title + "Pitch Env Amount", NormalisableRange<float> (-48.0f, 48.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 1); }));
        layout.add (ms (5.0f, 1000.0f, 80.0f, 60.0f, id (F::pitchEnvDecay), title + "Pitch Env Decay"));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::driveMode), 1 }, title + "Drive Mode",
                                                            StringArray { "Soft", "Hard", "Fold", "Asym", "Crush" }, 0));
        // Ny parameter (filterType rörs inte): Kickbase = dagens SVF, TB-303 = Open303:s TeeBeeFilter. Standardvärdet ger dagens ljud.
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::filterModel), 1 }, title + "Filter Model",
                                                            StringArray { "Kickbase", "TB-303" }, 0));
        // Morph på/av för varje oscillator (på = dagens ljud): sist, i fältens ordning.
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id (ParamID::oscMorphField (osc)), 1 }, title + "Osc " + String (osc + 1) + " Morph", true));
        // ENV 2 och ENV 3: tid i ms (standard) eller Sync (notvärden följer tempot, som LFO:ernas Rate Mode). Sustain är en nivå och berörs inte.
        for (int env = 0; env < 2; ++env)
        {
            const auto envTitle = title + "Env " + String (env + 2) + " ";
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (ParamID::envModeField (env)), 1 }, envTitle + "Time Mode", StringArray { "ms", "Sync" }, 0));
            const char* kinds[] { "Attack", "Decay", "Release" };
            const int defaults[] { 0, 8, 7 }; // 0, 1/8 D, 1/8: ungefär ms-standardvärdena vid 120 BPM
            for (int which = 0; which < 3; ++which)
                layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (ParamID::envSyncField (env, which)), 1 }, envTitle + kinds[which] + " Sync", Modulation::syncTimeNames(), defaults[which]));
        }
    }

    void addFxParams (Layout& layout)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::fxKey, 1 }, "Key", keyNames(), 0));
        layout.add (ms (0.1f, 50.0f, 5.0f, 1.0f, ParamID::gateAttack, "Gate Attack"));
        layout.add (ms (1.0f, 200.0f, 30.0f, 10.0f, ParamID::gateRelease, "Gate Release"));
    }

    void addMasterParams (Layout& layout)
    {
        layout.add (percent (ParamID::gain, "Master", 0.8f));
    }

    juce::StringArray keyNames()
    {
        return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    }

    float midiToHz (float midiNote)
    {
        return 440.0f * std::exp2 ((midiNote - 69.0f) / 12.0f);
    }

    int kickTailMidiNote (int keyPitchClass)
    {
        constexpr int lowest = 22; // A#0
        auto note = 24 + ((keyPitchClass % 12) + 12) % 12;
        if (note > lowest + 11)
            note -= 12;
        return note;
    }

    int instrumentRootMidiNote (int dawNote)
    {
        return dawNote - 24;
    }

    int barsFromChoice (int choiceIndex)
    {
        return 1 << juce::jlimit (0, 6, choiceIndex); // 1, 2, 4 ... 64 takter
    }

    int rowBarsFromChoice (int choiceIndex)
    {
        return choiceIndex <= 0 ? 0 : 1 << juce::jlimit (0, 6, choiceIndex - 1);
    }
}

ParamRefs::ParamRefs (juce::AudioProcessorValueTreeState& apvts)
{
    const auto& parameters = apvts.processor.getParameters();
    for (int i = 0; i < parameters.size(); ++i)
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
            entries.push_back ({ withId->paramID, apvts.getRawParameterValue (withId->paramID), i });
    }

    lookup.reserve (entries.size() * 2);
    for (size_t e = 0; e < entries.size(); ++e)
        lookup.emplace (std::string_view (entries[e].id.toRawUTF8()), (int) e);

    // Baslagrens parametrar slås upp en gång här, så att ljudtråden läser dem med index.
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
    {
        for (int f = 0; f < (int) InstrumentField::count; ++f)
        {
            auto& slot = instrumentEntries[(size_t) layer][(size_t) f];
            const auto id = ParamID::bass (layer, (InstrumentField) f);
            const auto found = lookup.find (std::string_view (id.toRawUTF8())); // (förr en jämförelse mot alla ~2000 ID:n per fält)
            slot = found != lookup.end() ? found->second : -1;
        }
    }
}

float ParamRefs::valueOf (const Entry& entry) const
{
    return modulator != nullptr ? modulator->apply (entry.index, entry.value->load()) : entry.value->load();
}

float ParamRefs::bass (int layer, InstrumentField field) const
{
    const auto index = instrumentEntries[(size_t) layer][(size_t) field];
    jassert (index >= 0); // parametern finns inte i det här pluginet
    return index >= 0 ? valueOf (entries[(size_t) index]) : 0.0f;
}

float ParamRefs::get (const char* paramID) const
{
    // Samma ID-sträng (samma adress) hittas i cachen; annars slås texten upp och läggs in på en tom plats.
    const auto key = (std::uint64_t) reinterpret_cast<std::uintptr_t> (paramID);
    const auto home = (int) (((key >> 3) * 0x9E3779B97F4A7C15ull) >> 54) & (cacheSize - 1);
    auto freeSlot = -1;
    if (key < (1ull << 48))
    {
        for (int probe = 0; probe < cacheProbes; ++probe)
        {
            const auto slot = (home + probe) & (cacheSize - 1);
            const auto packed = idCache[(size_t) slot].load (std::memory_order_acquire);
            if (packed == 0)
            {
                freeSlot = slot;
                break;
            }
            if ((packed >> 16) == key)
                return valueOf (entries[(size_t) (packed & 0xffff)]);
        }
    }

    if (const auto found = lookup.find (std::string_view (paramID)); found != lookup.end())
    {
        if (freeSlot >= 0 && found->second < 0x10000)
        {
            auto expected = std::uint64_t (0);
            idCache[(size_t) freeSlot].compare_exchange_strong (expected, (key << 16) | (std::uint64_t) found->second, std::memory_order_release);
        }
        return valueOf (entries[(size_t) found->second]);
    }

    jassertfalse; // okänt parameter-ID
    return 0.0f;
}

KickParams ParamRefs::readKick (int keyPitchClass) const
{
    KickParams p;
    // Fixed: kicken stannar på C1 + Tune oavsett ton (som en kick i en riktig låt brukar).
    const auto follow = getChoice (ParamID::kickKeyFollow) == 1;
    const auto tail = follow ? Params::kickTailMidiNote (keyPitchClass) : 24;
    p.tailHz        = Params::midiToHz ((float) tail + get (ParamID::kickTune));
    // Envelopernas punkter ägs av processorn och kopplas in av PluginBase.
    p.pitchAmount   = get (ParamID::kickPitchAmount);
    p.lengthScale   = get (ParamID::kickLength);
    p.startPhase    = get (ParamID::kickPhase);
    p.harmonics     = get (ParamID::kickHarmonics);
    p.harmonicsDecayMs = get (ParamID::kickHarmDecay);
    p.harmonicsTilt = get (ParamID::kickHarmTilt);
    p.clickLevel    = get (ParamID::kickClick);
    p.clickLengthMs = get (ParamID::kickClickLength);
    p.clickToneHz   = get (ParamID::kickClickTone);
    p.clipMode      = getChoice (ParamID::kickClipMode);
    p.wave          = getChoice (ParamID::kickWave);
    p.sub           = get (ParamID::kickSub);
    p.fm            = get (ParamID::kickFm);
    p.fmRatio       = get (ParamID::kickFmRatio);
    p.fmDecayMs     = get (ParamID::kickFmDecay);
    p.noise         = get (ParamID::kickNoise);
    p.noiseDecayMs  = get (ParamID::kickNoiseDecay);
    p.noiseToneHz   = get (ParamID::kickNoiseTone);
    p.crush         = get (ParamID::kickCrush);
    p.lowCutHz      = get (ParamID::kickLowCut);
    p.velPitch      = get (ParamID::kickVelPitch);
    p.sampleMode    = getChoice (ParamID::kickSampleMode);
    p.sampleLevel   = get (ParamID::kickSampleLevel);
    p.sampleTuneSemis = get (ParamID::kickSampleTune);
    p.sampleOffsetMs = get (ParamID::kickSampleOffset);
    p.drive         = get (ParamID::kickDrive);
    p.highCutHz     = get (ParamID::kickHighCut);
    p.level         = get (ParamID::kickLevel);
    return p;
}

InstrumentParams ParamRefs::readInstrument (int layer, int rootMidiNote) const
{
    using F = InstrumentField;
    auto get = [this, layer] (F f) { return bass (layer, f); };

    InstrumentParams p;
    p.rootHz     = Params::midiToHz ((float) rootMidiNote + 12.0f * get (F::octave) + get (F::tune));
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
    {
        auto oget = [&] (F f) { return get (ParamID::oscField (osc, f)); };
        auto& o = osc == 0 ? p.oscA : osc == 1 ? p.oscB : p.oscC;
        o.table       = (int) oget (F::aTable);
        o.position    = oget (F::aPos);
        o.level       = oget (F::aLevel);
        o.semis       = oget (F::aSemi);
        o.fineCents   = oget (F::aFine);
        o.unison      = (int) oget (F::aUnison);
        o.detuneCents = oget (F::aDetune);
        o.blend       = oget (F::aBlend);
        {
            // OSC 2 och 3 har ingen FM (B) i listan: Flip, Quantize och AM ligger en plats tidigare.
            const auto choice = (int) oget (F::aWarp);
            o.warp = (WarpMode) (osc > 0 && choice >= (int) WarpMode::fm ? choice + 1 : choice);
        }
        o.warpAmount  = oget (F::aWarpAmount);
        o.phase       = oget (F::aPhase);
        o.randomPhase = oget (F::aRandom);
        o.pan = oget (F::aPan);
        o.stereo = oget (F::aStereo);
        o.on = oget (F::aOn) >= 0.5f;
        o.morph = get (ParamID::oscMorphField (osc)) >= 0.5f;
    }
    p.warpEnv    = get (F::warpEnv);
    p.noise      = get (F::noise);
    p.sub        = get (F::sub);
    p.subOctave  = (int) get (F::subOctave) == 1 ? 0 : -1;
    p.filterType = (InstrumentFilterType) (int) get (F::filterType);
    p.filterModel = (int) get (F::filterModel);
    p.keyTrack   = get (F::keyTrack);
    p.cutoffHz   = get (F::cutoff);
    p.resonance  = get (F::resonance);
    p.envAmount  = get (F::envAmount);
    p.envDecayMs = get (F::envDecay);
    p.envAttackMs = get (F::envAttack);
    p.velToEnv   = get (F::velToEnv);
    p.attackMs   = get (F::attack);
    p.decayMs    = get (F::decay);
    p.sustain    = get (F::sustain);
    p.releaseMs  = get (F::release);
    p.glideMs    = get (F::glide);
    p.drive      = get (F::drive);
    p.driveMode  = (int) get (F::driveMode);
    p.env2 = { get (F::env2Attack), get (F::env2Decay), get (F::env2Sustain), get (F::env2Release) };
    p.env3 = { get (F::env3Attack), get (F::env3Decay), get (F::env3Sustain), get (F::env3Release) };
    for (int env = 0; env < 2; ++env)
    {
        auto& e = env == 0 ? p.env2 : p.env3;
        e.sync = get (ParamID::envModeField (env)) >= 0.5f;
        e.attackNote = (int) get (ParamID::envSyncField (env, 0));
        e.decayNote = (int) get (ParamID::envSyncField (env, 1));
        e.releaseNote = (int) get (ParamID::envSyncField (env, 2));
    }
    p.subWave = (int) get (F::subWave);
    p.noiseTone = get (F::noiseTone);
    p.pitchEnvSemis = get (F::pitchEnvAmount);
    p.pitchEnvDecayMs = get (F::pitchEnvDecay);
    p.level      = get (F::level);
    p.sampleLevel    = get (F::sampleLevel);
    p.sampleRootHz   = Params::midiToHz (get (F::sampleRoot));
    p.sampleLoop     = (int) get (F::sampleLoop) == 1;
    p.sampleKeyTrack = (int) get (F::sampleKey) == 1;
    p.sampleStart    = get (F::sampleStart);
    p.sampleOn       = get (F::sampleOn) >= 0.5f;
    return p;
}
