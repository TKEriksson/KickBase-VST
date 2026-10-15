#include "Params.h"

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
    std::unique_ptr<AudioParameterFloat> floatParam (const char* id, const String& name, NormalisableRange<float> range,
                                                     float defaultValue, Formatter format)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, defaultValue,
            AudioParameterFloatAttributes().withStringFromValueFunction ([format] (float v, int) { return format (v); }));
    }

    auto ms (float min, float max, float centre, float def, const char* id, const String& name)
    {
        return floatParam (id, name, skewed (min, max, centre, 0.01f), def, formatMs);
    }

    auto percent (const char* id, const String& name, float def)
    {
        return floatParam (id, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def, formatPercent);
    }

    auto curve (const char* id, const String& name, float def)
    {
        return floatParam (id, name, skewed (0.2f, 8.0f, 1.5f, 0.01f), def, [] (float v) { return String (v, 2); });
    }
}

namespace Params
{
    void addPatternParams (Layout& layout, bool includeNoteEnd)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::patternBars, 1 }, "Pattern Length",
                                                            StringArray { "1 bar", "2 bars", "4 bars" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bassGrid, 1 }, "Bass Grid",
                                                            Pattern::gridNames(), (int) BassGrid::sixteenth));
        if (includeNoteEnd)
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::noteEnd, 1 }, "Note End",
                                                                StringArray { "Cut", "Finish bar", "Finish pattern" }, 0));
    }

    void addKickParams (Layout& layout)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::kickOn, 1 }, "Kick On", true));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickKeyFollow, 1 }, "Kick Key",
                                                            StringArray { "Fixed", "Follow" }, 1));
        layout.add (floatParam (ParamID::kickTune, "Kick Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 2); }));
        layout.add (floatParam (ParamID::kickPitch, "Kick Pitch", NormalisableRange<float> (0.0f, 72.0f, 0.1f), 42.0f,
                                [] (float v) { return formatSemis (v, 1); }));
        layout.add (ms (2.0f, 300.0f, 50.0f, 45.0f, ParamID::kickPitchTime, "Kick Pitch Time"));
        layout.add (curve (ParamID::kickPitchCurve, "Kick Pitch Curve", 3.0f));
        layout.add (floatParam (ParamID::kickGlide, "Kick Glide", NormalisableRange<float> (0.0f, 24.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 1); }));
        layout.add (ms (5.0f, 400.0f, 80.0f, 80.0f, ParamID::kickGlideTime, "Kick Glide Time"));
        layout.add (ms (0.0f, 10.0f, 1.0f, 0.3f, ParamID::kickAttack, "Kick Attack"));
        layout.add (ms (0.0f, 200.0f, 40.0f, 15.0f, ParamID::kickHold, "Kick Hold"));
        layout.add (ms (10.0f, 1500.0f, 250.0f, 140.0f, ParamID::kickDecay, "Kick Decay"));
        layout.add (curve (ParamID::kickDecayCurve, "Kick Decay Curve", 2.0f));
        layout.add (percent (ParamID::kickClick, "Kick Click", 0.3f));
        layout.add (ms (1.0f, 30.0f, 6.0f, 6.0f, ParamID::kickClickLength, "Kick Click Length"));
        layout.add (floatParam (ParamID::kickClickTone, "Kick Click Tone", skewed (500.0f, 16000.0f, 4000.0f, 1.0f), 5000.0f, formatHz));
        layout.add (percent (ParamID::kickDrive, "Kick Drive", 0.2f));
        layout.add (percent (ParamID::kickLevel, "Kick Level", 0.8f));
    }

    void addBassCommonParams (Layout& layout, float defaultLevel)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::bassOn, 1 }, "Bass On", true));
        layout.add (percent (ParamID::bassLevel, "Bass Level", defaultLevel));
        layout.add (floatParam (ParamID::bassGate, "Bass Gate", NormalisableRange<float> (0.05f, 1.0f, 0.001f), 1.0f, formatPercent));
        layout.add (percent (ParamID::duckAmount, "Duck Amount", 0.8f));
    }

    void addBassSynthParams (Layout& layout)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::bassOctave, 1 }, "Bass Octave", -2, 1, 0));
        layout.add (floatParam (ParamID::bassTune, "Bass Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 2); }));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bassWave, 1 }, "Bass Wave",
                                                            StringArray { "Saw", "Square", "Triangle", "Sine" }, 0));
        layout.add (percent (ParamID::bassSub, "Bass Sub", 0.3f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bassSubOctave, 1 }, "Bass Sub Octave",
                                                            StringArray { "-1 oct", "0 oct" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bassOsc2Wave, 1 }, "Bass Osc2 Wave",
                                                            StringArray { "Saw", "Square", "Triangle", "Sine" }, 0));
        layout.add (percent (ParamID::bassOsc2Level, "Bass Osc2 Level", 0.0f));
        layout.add (floatParam (ParamID::bassOsc2Semi, "Bass Osc2 Semi", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
                                [] (float v) { return formatSemis (v, 0); }));
        layout.add (floatParam (ParamID::bassOsc2Detune, "Bass Osc2 Detune", NormalisableRange<float> (0.0f, 50.0f, 0.1f), 0.0f,
                                [] (float v) { return String (v, 1) + " ct"; }));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bassFilterType, 1 }, "Bass Filter Type",
                                                            StringArray { "LP 24", "LP 12", "Band", "High" }, 0));
        layout.add (percent (ParamID::bassKeyTrack, "Bass Key Track", 0.0f));
        layout.add (floatParam (ParamID::bassCutoff, "Bass Cutoff", skewed (20.0f, 20000.0f, 600.0f, 0.1f), 300.0f, formatHz));
        layout.add (percent (ParamID::bassResonance, "Bass Resonance", 0.3f));
        layout.add (floatParam (ParamID::bassEnvAmount, "Bass Env Amount", NormalisableRange<float> (0.0f, 8.0f, 0.01f), 4.0f,
                                [] (float v) { return String (v, 1) + " oct"; }));
        layout.add (ms (5.0f, 1000.0f, 150.0f, 120.0f, ParamID::bassEnvDecay, "Bass Env Decay"));
        layout.add (percent (ParamID::bassVelToEnv, "Bass Vel > Env", 0.5f));
        layout.add (ms (0.0f, 50.0f, 5.0f, 1.0f, ParamID::bassAttack, "Bass Attack"));
        layout.add (ms (5.0f, 2000.0f, 200.0f, 200.0f, ParamID::bassDecay, "Bass Decay"));
        layout.add (percent (ParamID::bassSustain, "Bass Sustain", 0.6f));
        layout.add (ms (1.0f, 300.0f, 30.0f, 15.0f, ParamID::bassRelease, "Bass Release"));
        layout.add (ms (1.0f, 500.0f, 80.0f, 60.0f, ParamID::bassGlide, "Bass Glide"));
        layout.add (percent (ParamID::bassDrive, "Bass Drive", 0.2f));
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

    int bassRootMidiNote (int dawNote)
    {
        return dawNote - 24;
    }

    int barsFromChoice (int choiceIndex)
    {
        return choiceIndex <= 0 ? 1 : (choiceIndex == 1 ? 2 : 4);
    }
}

ParamRefs::ParamRefs (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* parameter : apvts.processor.getParameters())
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            entries.push_back ({ withId->paramID, apvts.getRawParameterValue (withId->paramID) });
    }
}

float ParamRefs::get (const char* paramID) const
{
    for (const auto& entry : entries)
        if (entry.id == paramID) // jämför utan att allokera
            return entry.value->load();

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
    p.pitchSemis    = get (ParamID::kickPitch);
    p.pitchTimeMs   = get (ParamID::kickPitchTime);
    p.pitchCurve    = get (ParamID::kickPitchCurve);
    p.glideSemis    = get (ParamID::kickGlide);
    p.glideTimeMs   = get (ParamID::kickGlideTime);
    p.attackMs      = get (ParamID::kickAttack);
    p.holdMs        = get (ParamID::kickHold);
    p.decayMs       = get (ParamID::kickDecay);
    p.decayCurve    = get (ParamID::kickDecayCurve);
    p.clickLevel    = get (ParamID::kickClick);
    p.clickLengthMs = get (ParamID::kickClickLength);
    p.clickToneHz   = get (ParamID::kickClickTone);
    p.drive         = get (ParamID::kickDrive);
    p.level         = get (ParamID::kickLevel);
    return p;
}

BassParams ParamRefs::readBass (int rootMidiNote) const
{
    BassParams p;
    p.rootHz     = Params::midiToHz ((float) rootMidiNote + 12.0f * get (ParamID::bassOctave) + get (ParamID::bassTune));
    p.wave       = (BassWave) getChoice (ParamID::bassWave);
    p.sub        = get (ParamID::bassSub);
    p.subOctave  = getChoice (ParamID::bassSubOctave) == 1 ? 0 : -1;
    p.osc2Wave   = (BassWave) getChoice (ParamID::bassOsc2Wave);
    p.osc2Level  = get (ParamID::bassOsc2Level);
    p.osc2Semis  = get (ParamID::bassOsc2Semi);
    p.osc2Detune = get (ParamID::bassOsc2Detune);
    p.filterType = (BassFilterType) getChoice (ParamID::bassFilterType);
    p.keyTrack   = get (ParamID::bassKeyTrack);
    p.cutoffHz   = get (ParamID::bassCutoff);
    p.resonance  = get (ParamID::bassResonance);
    p.envAmount  = get (ParamID::bassEnvAmount);
    p.envDecayMs = get (ParamID::bassEnvDecay);
    p.velToEnv   = get (ParamID::bassVelToEnv);
    p.attackMs   = get (ParamID::bassAttack);
    p.decayMs    = get (ParamID::bassDecay);
    p.sustain    = get (ParamID::bassSustain);
    p.releaseMs  = get (ParamID::bassRelease);
    p.glideMs    = get (ParamID::bassGlide);
    p.drive      = get (ParamID::bassDrive);
    p.level      = get (ParamID::bassLevel);
    return p;
}
