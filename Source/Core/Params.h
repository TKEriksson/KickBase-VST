#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BassSynth.h"
#include "KickEngine.h"
#include "Pattern.h"

class Modulator;

// Parameter-ID:n samlade på ett ställe. Ändra inte ett ID när det väl använts:
// sparade projekt och automation i DAW:en pekar på det.
namespace ParamID
{
    // Mönster
    inline constexpr auto patternBars = "patternBars";
    inline constexpr auto noteEnd     = "noteEnd";

    // Kick
    inline constexpr auto kickOn          = "kickOn";
    inline constexpr auto kickKeyFollow   = "kickKeyFollow";
    inline constexpr auto kickTune        = "kickTune";
    inline constexpr auto kickPitchAmount = "kickPitchAmount"; // skalar pitch-envelopen
    inline constexpr auto kickLength      = "kickLength";      // sträcker envelopernas tid
    inline constexpr auto kickPhase       = "kickPhase";
    inline constexpr auto kickHarmonics   = "kickHarmonics";
    inline constexpr auto kickHarmDecay   = "kickHarmDecay";
    inline constexpr auto kickHarmTilt    = "kickHarmTilt";
    inline constexpr auto kickClick       = "kickClick";
    inline constexpr auto kickClickLength = "kickClickLength";
    inline constexpr auto kickClickTone   = "kickClickTone";
    inline constexpr auto kickClipMode    = "kickClipMode";
    inline constexpr auto kickDrive       = "kickDrive";
    inline constexpr auto kickHighCut     = "kickHighCut";
    inline constexpr auto kickLevel       = "kickLevel";

    // Baslagren: se ParamID::bass (lager, fält) nedan.

    // FX
    inline constexpr auto fxKey       = "fxKey";
    inline constexpr auto gateAttack  = "gateAttack";
    inline constexpr auto gateRelease = "gateRelease";

    // Master
    inline constexpr auto gain = "gain";
}

// Varje baslagers parametrar. ID:t är "bass" + lagrets nummer + fältets namn, t.ex. "bass2Cutoff".
enum class BassField
{
    on, level, gate, duck, grid,                                  // lagrets utgång och mönster
    octave, tune,
    aTable, aPos, aLevel, aSemi, aFine, aUnison, aDetune, aBlend, aWarp, aWarpAmount, aPhase, aRandom, // OSC A
    bTable, bPos, bLevel, bSemi, bFine, bUnison, bDetune, bBlend, bWarp, bWarpAmount, bPhase, bRandom, // OSC B
    warpEnv, sub, subOctave, noise,
    filterType, keyTrack, cutoff, resonance, envAmount, envDecay, velToEnv,
    attack, decay, sustain, release, glide, drive,
    count
};

namespace ParamID
{
    constexpr int numBassLayers = 3;
    constexpr int oscFieldCount = 12; // fält per oscillator (aTable ... aRandom)

    // Samma fält för OSC A (osc 0) eller OSC B (osc 1): oscField (1, BassField::aPos) = bPos.
    inline BassField oscField (int osc, BassField aField) { return (BassField) ((int) aField + osc * oscFieldCount); }
    juce::String bass (int layer, BassField field);   // "bass1Cutoff" ...
    const char* bassFieldName (BassField field);      // "Cutoff" ...
}

enum class NoteEnd { cut, finishBar, finishPattern };

namespace Params
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    void addPatternParams (Layout& layout, bool includeNoteEnd);
    void addKickParams (Layout& layout);
    void addBassCommonParams (Layout& layout, int layer, float defaultLevel);
    void addBassSynthParams (Layout& layout, int layer);
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

    // Med en modulator läser get() värdena med LFO-modulering (ljudtråden); utan läser den
    // dem orörda (t.ex. gränssnittets kurvor).
    void setModulator (const Modulator* m) { modulator = m; }

    float get (const char* paramID) const;
    bool getBool (const char* paramID) const  { return get (paramID) >= 0.5f; }
    int getChoice (const char* paramID) const { return (int) get (paramID); }

    // Ett baslagers parameter, utan namnuppslag (index räknade i konstruktorn).
    float bass (int layer, BassField field) const;

    KickParams readKick (int keyPitchClass) const;
    BassParams readBass (int layer, int rootMidiNote) const; // rootMidiNote före Octave och Tune

private:
    struct Entry
    {
        juce::String id;
        std::atomic<float>* value;
        int index; // plats i processorns parameterlista (samma som Modulator använder)
    };

    float valueOf (const Entry& entry) const;

    std::vector<Entry> entries; // byggs en gång i konstruktorn, ändras aldrig efter det
    std::array<std::array<int, (size_t) BassField::count>, ParamID::numBassLayers> bassEntries {}; // index i entries, -1 = saknas
    const Modulator* modulator = nullptr;
};
