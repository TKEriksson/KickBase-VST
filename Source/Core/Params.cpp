#include "Params.h"
#include "Modulation.h"

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

namespace ParamID
{
    const char* bassFieldName (BassField field)
    {
        static const char* names[] { "On", "Level", "Gate", "Duck", "Grid", "FxChannel", "FxAmount",
                                     "Octave", "Tune",
                                     "ATable", "APos", "ALevel", "ASemi", "AFine", "AUnison", "ADetune", "ABlend", "AWarp", "AWarpAmount", "APhase", "ARandom", "APan", "AStereo",
                                     "BTable", "BPos", "BLevel", "BSemi", "BFine", "BUnison", "BDetune", "BBlend", "BWarp", "BWarpAmount", "BPhase", "BRandom", "BPan", "BStereo",
                                     "WarpEnv", "Sub", "SubOctave", "Noise",
                                     "FilterType", "KeyTrack", "Cutoff", "Resonance", "EnvAmount", "EnvDecay", "VelToEnv",
                                     "Attack", "Decay", "Sustain", "Release", "Glide", "Drive" };
        static_assert (std::size (names) == (size_t) BassField::count);
        return names[(size_t) field];
    }

    juce::String bass (int layer, BassField field)
    {
        return "bass" + juce::String (layer + 1) + bassFieldName (field);
    }
}

namespace Params
{
    void addPatternParams (Layout& layout, bool includeNoteEnd)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::patternBars, 1 }, "Pattern Length",
                                                            StringArray { "1 bar", "2 bars", "4 bars" }, 0));
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
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickClipMode, 1 }, "Kick Clip Mode",
                                                            StringArray { "Soft", "Hard", "Tube" }, 0));
        layout.add (percent (ParamID::kickDrive, "Kick Drive", 0.2f));
        layout.add (floatParam (ParamID::kickHighCut, "Kick High Cut", skewed (500.0f, 20000.0f, 4000.0f, 1.0f), 20000.0f, formatHz));
        layout.add (percent (ParamID::kickLevel, "Kick Level", 0.8f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::kickFxChannel, 1 }, "Kick FX Channel",
                                                            StringArray { "None", "A", "B", "C", "D", "E" }, 0));
        layout.add (percent (ParamID::kickFxAmount, "Kick FX Amount", 1.0f));
    }

    void addBassCommonParams (Layout& layout, int layer, float defaultLevel)
    {
        using F = BassField;
        auto id = [layer] (F f) { return ParamID::bass (layer, f); };
        const auto title = "Bass " + String (layer + 1) + " ";

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id (F::on), 1 }, title + "On", true));
        layout.add (percent (id (F::level), title + "Level", defaultLevel));
        layout.add (floatParam (id (F::gate), title + "Gate", NormalisableRange<float> (0.05f, 1.0f, 0.001f), 1.0f, formatPercent));
        layout.add (percent (id (F::duck), title + "Duck", 0.8f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::grid), 1 }, title + "Grid",
                                                            Pattern::gridNames(), (int) BassGrid::sixteenth));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id (F::fxChannel), 1 }, title + "FX Channel",
                                                            StringArray { "None", "A", "B", "C", "D", "E" }, 0));
        layout.add (percent (id (F::fxAmount), title + "FX Amount", 1.0f));
    }

    void addBassSynthParams (Layout& layout, int layer)
    {
        using F = BassField;
        auto id = [layer] (F f) { return ParamID::bass (layer, f); };
        const auto title = "Bass " + String (layer + 1) + " ";

        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id (F::octave), 1 }, title + "Octave", -2, 1, 0));
        layout.add (floatParam (id (F::tune), title + "Tune", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                [] (float v) { return formatSemis (v, 2); }));
        // OSC A och OSC B: wavetable, position, nivå, stämning, unison, warp och fas.
        for (int osc = 0; osc < 2; ++osc)
        {
            auto oid = [&] (F f) { return id (ParamID::oscField (osc, f)); };
            const auto oscTitle = title + (osc == 0 ? "Osc A " : "Osc B ");
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
                                                                osc == 0 ? StringArray { "Off", "Sync", "Bend", "PWM", "Mirror", "FM (B)" }
                                                                         : StringArray { "Off", "Sync", "Bend", "PWM", "Mirror" }, 0));
            layout.add (percent (oid (F::aWarpAmount), oscTitle + "Warp Amount", 0.0f));
            layout.add (percent (oid (F::aPhase), oscTitle + "Phase", 0.0f));
            layout.add (percent (oid (F::aRandom), oscTitle + "Random Phase", 0.0f));
            layout.add (floatParam (oid (F::aPan), oscTitle + "Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
                                    [] (float v) { const auto p = roundToInt (std::abs (v) * 100.0f);
                                                   return p == 0 ? String ("C") : String (p) + (v < 0.0f ? " L" : " R"); }));
            layout.add (percent (oid (F::aStereo), oscTitle + "Stereo Spread", 0.5f));
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
        layout.add (percent (id (F::velToEnv), title + "Vel > Env", 0.5f));
        layout.add (ms (0.0f, 50.0f, 5.0f, 1.0f, id (F::attack), title + "Attack"));
        layout.add (ms (5.0f, 2000.0f, 200.0f, 200.0f, id (F::decay), title + "Decay"));
        layout.add (percent (id (F::sustain), title + "Sustain", 0.6f));
        layout.add (ms (1.0f, 300.0f, 30.0f, 15.0f, id (F::release), title + "Release"));
        layout.add (ms (1.0f, 500.0f, 80.0f, 60.0f, id (F::glide), title + "Glide"));
        layout.add (percent (id (F::drive), title + "Drive", 0.2f));
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
    const auto& parameters = apvts.processor.getParameters();
    for (int i = 0; i < parameters.size(); ++i)
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
            entries.push_back ({ withId->paramID, apvts.getRawParameterValue (withId->paramID), i });
    }

    // Baslagrens parametrar slås upp en gång här, så att ljudtråden läser dem med index.
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
    {
        for (int f = 0; f < (int) BassField::count; ++f)
        {
            auto& slot = bassEntries[(size_t) layer][(size_t) f];
            slot = -1;
            const auto id = ParamID::bass (layer, (BassField) f);
            for (size_t e = 0; e < entries.size(); ++e)
                if (entries[e].id == id)
                    slot = (int) e;
        }
    }
}

float ParamRefs::valueOf (const Entry& entry) const
{
    return modulator != nullptr ? modulator->apply (entry.index, entry.value->load()) : entry.value->load();
}

float ParamRefs::bass (int layer, BassField field) const
{
    const auto index = bassEntries[(size_t) layer][(size_t) field];
    jassert (index >= 0); // parametern finns inte i det här pluginet
    return index >= 0 ? valueOf (entries[(size_t) index]) : 0.0f;
}

float ParamRefs::get (const char* paramID) const
{
    for (const auto& entry : entries)
        if (entry.id == paramID) // jämför utan att allokera
            return valueOf (entry);

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
    p.drive         = get (ParamID::kickDrive);
    p.highCutHz     = get (ParamID::kickHighCut);
    p.level         = get (ParamID::kickLevel);
    return p;
}

BassParams ParamRefs::readBass (int layer, int rootMidiNote) const
{
    using F = BassField;
    auto get = [this, layer] (F f) { return bass (layer, f); };

    BassParams p;
    p.rootHz     = Params::midiToHz ((float) rootMidiNote + 12.0f * get (F::octave) + get (F::tune));
    for (int osc = 0; osc < 2; ++osc)
    {
        auto oget = [&] (F f) { return get (ParamID::oscField (osc, f)); };
        auto& o = osc == 0 ? p.oscA : p.oscB;
        o.table       = (int) oget (F::aTable);
        o.position    = oget (F::aPos);
        o.level       = oget (F::aLevel);
        o.semis       = oget (F::aSemi);
        o.fineCents   = oget (F::aFine);
        o.unison      = (int) oget (F::aUnison);
        o.detuneCents = oget (F::aDetune);
        o.blend       = oget (F::aBlend);
        o.warp        = (WarpMode) (int) oget (F::aWarp);
        o.warpAmount  = oget (F::aWarpAmount);
        o.phase       = oget (F::aPhase);
        o.randomPhase = oget (F::aRandom);
        o.pan = oget (F::aPan);
        o.stereo = oget (F::aStereo);
    }
    p.warpEnv    = get (F::warpEnv);
    p.noise      = get (F::noise);
    p.sub        = get (F::sub);
    p.subOctave  = (int) get (F::subOctave) == 1 ? 0 : -1;
    p.filterType = (BassFilterType) (int) get (F::filterType);
    p.keyTrack   = get (F::keyTrack);
    p.cutoffHz   = get (F::cutoff);
    p.resonance  = get (F::resonance);
    p.envAmount  = get (F::envAmount);
    p.envDecayMs = get (F::envDecay);
    p.velToEnv   = get (F::velToEnv);
    p.attackMs   = get (F::attack);
    p.decayMs    = get (F::decay);
    p.sustain    = get (F::sustain);
    p.releaseMs  = get (F::release);
    p.glideMs    = get (F::glide);
    p.drive      = get (F::drive);
    p.level      = get (F::level);
    return p;
}
