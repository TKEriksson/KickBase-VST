#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BassSynth.h"
#include "KickEngine.h"
#include "Pattern.h"

// Parameter-ID:n samlade på ett ställe. Ändra inte ett ID när det väl använts:
// sparade projekt och automation i DAW:en pekar på det.
namespace ParamID
{
    // Mönster
    inline constexpr auto patternBars = "patternBars";
    inline constexpr auto bassGrid    = "bassDivision"; // "bassGrid" användes när listan bara hade tre val
    inline constexpr auto noteEnd     = "noteEnd";

    // Kick
    inline constexpr auto kickOn          = "kickOn";
    inline constexpr auto kickKeyFollow   = "kickKeyFollow";
    inline constexpr auto kickTune        = "kickTune";
    inline constexpr auto kickPitch       = "kickPitch";
    inline constexpr auto kickPitchTime   = "kickPitchTime";
    inline constexpr auto kickPitchCurve  = "kickPitchCurve";
    inline constexpr auto kickGlide       = "kickGlide";
    inline constexpr auto kickGlideTime   = "kickGlideTime";
    inline constexpr auto kickAttack      = "kickAttack";
    inline constexpr auto kickHold        = "kickHold";
    inline constexpr auto kickDecay       = "kickDecay";
    inline constexpr auto kickDecayCurve  = "kickDecayCurve";
    inline constexpr auto kickClick       = "kickClick";
    inline constexpr auto kickClickLength = "kickClickLength";
    inline constexpr auto kickClickTone   = "kickClickTone";
    inline constexpr auto kickDrive       = "kickDrive";
    inline constexpr auto kickLevel       = "kickLevel";

    // Bas, gemensamt för instrument och FX
    inline constexpr auto bassOn     = "bassOn";
    inline constexpr auto bassLevel  = "bassLevel";
    inline constexpr auto bassGate   = "bassGate";
    inline constexpr auto duckAmount = "duckAmount";

    // Bassynt (instrument)
    inline constexpr auto bassOctave    = "bassOctave";
    inline constexpr auto bassTune      = "bassTune";
    inline constexpr auto bassShape     = "bassShape";      // ersätter valet "bassWave"
    inline constexpr auto bassPulseWidth = "bassPulseWidth";
    inline constexpr auto bassNoise     = "bassNoise";
    inline constexpr auto bassSub       = "bassSub";
    inline constexpr auto bassSubOctave = "bassSubOctave";
    inline constexpr auto bassOsc2Shape = "bassOsc2Shape";  // ersätter valet "bassOsc2Wave"
    inline constexpr auto bassOsc2Pw    = "bassOsc2Pw";
    inline constexpr auto bassFm        = "bassFm";
    inline constexpr auto bassFmEnv     = "bassFmEnv";
    inline constexpr auto bassOsc2Level = "bassOsc2Level";
    inline constexpr auto bassOsc2Semi  = "bassOsc2Semi";
    inline constexpr auto bassOsc2Detune = "bassOsc2Detune";
    inline constexpr auto bassFilterType = "bassFilterType";
    inline constexpr auto bassKeyTrack  = "bassKeyTrack";
    inline constexpr auto bassCutoff    = "bassCutoff";
    inline constexpr auto bassResonance = "bassResonance";
    inline constexpr auto bassEnvAmount = "bassEnvAmount";
    inline constexpr auto bassEnvDecay  = "bassEnvDecay";
    inline constexpr auto bassVelToEnv  = "bassVelToEnv";
    inline constexpr auto bassAttack    = "bassAttack";
    inline constexpr auto bassDecay     = "bassDecay";
    inline constexpr auto bassSustain   = "bassSustain";
    inline constexpr auto bassRelease   = "bassRelease";
    inline constexpr auto bassGlide     = "bassGlide";
    inline constexpr auto bassDrive     = "bassDrive";

    // FX
    inline constexpr auto fxKey       = "fxKey";
    inline constexpr auto gateAttack  = "gateAttack";
    inline constexpr auto gateRelease = "gateRelease";

    // Master
    inline constexpr auto gain = "gain";
}

enum class NoteEnd { cut, finishBar, finishPattern };

namespace Params
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    void addPatternParams (Layout& layout, bool includeNoteEnd);
    void addKickParams (Layout& layout);
    void addBassCommonParams (Layout& layout, float defaultLevel);
    void addBassSynthParams (Layout& layout);
    void addFxParams (Layout& layout);
    void addMasterParams (Layout& layout);

    juce::StringArray keyNames();
    float midiToHz (float midiNote);

    // Kickens svans för en tonart: tonens namn i registret A#0..A1 (29-55 Hz), så att
    // kicken aldrig hamnar orimligt högt eller lågt oavsett tonart.
    int kickTailMidiNote (int keyPitchClass);

    // Basens grundton för en ton från DAW:en: samma tonnamn två oktaver ner (FL:s C5 -> C2),
    // så att en basgång som går nedåt i pianorullen också går nedåt i basen.
    int bassRootMidiNote (int dawNote);
    int barsFromChoice (int choiceIndex); // 0, 1, 2 -> 1, 2, 4 takter
}

// Cachade pekare till parametrarnas värden, så att ljudtråden kan läsa dem utan att
// slå upp namn (vilket allokerar minne) i varje block.
class ParamRefs
{
public:
    explicit ParamRefs (juce::AudioProcessorValueTreeState& apvts);

    float get (const char* paramID) const;
    bool getBool (const char* paramID) const  { return get (paramID) >= 0.5f; }
    int getChoice (const char* paramID) const { return (int) get (paramID); }

    KickParams readKick (int keyPitchClass) const;
    BassParams readBass (int rootMidiNote) const; // rootMidiNote före Octave och Tune

private:
    struct Entry
    {
        juce::String id;
        std::atomic<float>* value;
    };

    std::vector<Entry> entries; // byggs en gång i konstruktorn, ändras aldrig efter det
};
