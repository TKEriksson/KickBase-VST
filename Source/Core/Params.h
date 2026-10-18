#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <string_view>
#include <unordered_map>
#include "InstrumentSynth.h"
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
    inline constexpr auto patSwing    = "patSwing";     // swing: de ojämna sextondelarna fördröjs (bara instrumenten)
    inline constexpr auto patHumanize = "patHumanize";  // slumpad fördröjning av varje instrumentton
    inline constexpr auto rowGridKick = "rowGridKick"; // kickens rutnät (som bassidornas Grid); heter inte kick... så att det hör till mönstret

    // Hur många instrument (baslager 1-10) som används. De övriga finns kvar som parametrar men spelas, visas och mixas inte.
    inline constexpr auto instrumentCount = "instrumentCount";
    inline constexpr int defaultInstrumentCount = 3;

    // Radens egen längd: 0 = kicken, 1-10 = instrument (val 0 = hela mönstret, sedan 1 ... 64 takter).
    const char* rowBars (int row);

    // Loopar raden (när den är kortare än mönstret)? Per rad: 0 = kicken, 1-10 = instrument.
    const char* rowLoop (int row);

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
    inline constexpr auto kickWave        = "kickWave";        // kroppens vågform
    inline constexpr auto kickSub         = "kickSub";         // sinus en oktav ner
    inline constexpr auto kickFm          = "kickFm";          // FM på kroppen
    inline constexpr auto kickFmRatio     = "kickFmRatio";
    inline constexpr auto kickFmDecay     = "kickFmDecay";
    inline constexpr auto kickNoise       = "kickNoise";       // brusig "topp" ovanpå kicken
    inline constexpr auto kickNoiseDecay  = "kickNoiseDecay";
    inline constexpr auto kickNoiseTone   = "kickNoiseTone";
    inline constexpr auto kickCrush       = "kickCrush";       // bitkrossning efter drive
    inline constexpr auto kickLowCut      = "kickLowCut";      // tar bort ljud under kicken (sub-rumlet)
    inline constexpr auto kickVelPitch    = "kickVelPitch";    // hur mycket velocity ändrar pitch-svepet
    inline constexpr auto kickSampleMode  = "kickSampleMode";  // sampling som klick, lager eller hel kick
    inline constexpr auto kickSampleLevel = "kickSampleLevel";
    inline constexpr auto kickSampleTune  = "kickSampleTune";
    inline constexpr auto kickSampleOffset = "kickSampleOffset"; // samplingens startpunkt i förhållande till kicken (ms): plus = senare, minus = börjar inne i samplingen
    inline constexpr auto kickClipMode    = "kickClipMode";
    inline constexpr auto kickDrive       = "kickDrive";
    inline constexpr auto kickHighCut     = "kickHighCut";
    inline constexpr auto kickLevel       = "kickLevel";
    inline constexpr auto kickFxChannel   = "kickFxChannel"; // effektkanal (hör till EFFECTS, inte KICK)
    inline constexpr auto kickFxAmount    = "kickFxAmount";

    // Baslagren: se ParamID::bass (lager, fält) nedan.

    // FX
    inline constexpr auto fxKey       = "fxKey";
    inline constexpr auto gateAttack  = "gateAttack";
    inline constexpr auto gateRelease = "gateRelease";

    // Master
    inline constexpr auto gain = "gain";
}

// Varje baslagers parametrar. ID:t är "bass" + lagrets nummer + fältets namn, t.ex. "bass2Cutoff".
enum class InstrumentField
{
    on, level, gate, duck, grid, fxChannel, fxAmount,             // lagrets utgång, mönster och effektkanal
    octave, tune,
    aTable, aPos, aLevel, aSemi, aFine, aUnison, aDetune, aBlend, aWarp, aWarpAmount, aPhase, aRandom, aPan, aStereo, aOn, // OSC 1 (A)
    bTable, bPos, bLevel, bSemi, bFine, bUnison, bDetune, bBlend, bWarp, bWarpAmount, bPhase, bRandom, bPan, bStereo, bOn, // OSC 2 (B)
    cTable, cPos, cLevel, cSemi, cFine, cUnison, cDetune, cBlend, cWarp, cWarpAmount, cPhase, cRandom, cPan, cStereo, cOn, // OSC 3 (C)
    warpEnv, sub, subOctave, noise,
    filterType, keyTrack, cutoff, resonance, envAmount, envDecay, velToEnv,
    attack, decay, sustain, release, glide, drive,
    gateOn, gateGrid, gateDepth, gateSmooth,                      // lagrets GATE: en egen av/på-rytm som hackar ljudet
    sampleLevel, sampleRoot, sampleLoop, sampleKey, sampleStart, sampleOn, // lagrets sampling som ljudkälla
    env2Attack, env2Decay, env2Sustain, env2Release,              // ENV 2 och ENV 3: modulationsenveloper
    env3Attack, env3Decay, env3Sustain, env3Release,
    subWave, noiseTone, pitchEnvAmount, pitchEnvDecay, driveMode,
    delay,                                                        // instrumentets fördröjning (ms): läggs efter kicken
    swing, humanize,                                              // instrumentets egen swing och humanize (läggs till patternets)
    envAttack,                                                    // filterenvelopens attack (före envDecay i signalen, men sist här så att gamla projekt är orörda)
    count
};

namespace ParamID
{
    constexpr int numInstrumentLayers = maxInstruments; // så många instrument finns som mest (instrumentCount väljer hur många som används)
    constexpr int oscFieldCount = 15; // fält per oscillator (aTable ... aOn)
    constexpr int oscsPerLayer = 3;   // OSC 1, 2 och 3

    // Samma fält för OSC 1 (osc 0), OSC 2 (osc 1) eller OSC 3 (osc 2): oscField (1, InstrumentField::aPos) = bPos.
    inline InstrumentField oscField (int osc, InstrumentField aField) { return (InstrumentField) ((int) aField + osc * oscFieldCount); }
    juce::String bass (int layer, InstrumentField field);   // "bass1Cutoff" ...
    const char* instrumentFieldName (InstrumentField field);      // "Cutoff" ...
}

enum class NoteEnd { cut, finishBar, finishPattern };

namespace Params
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    void addPatternParams (Layout& layout, bool includeNoteEnd);
    void addKickParams (Layout& layout);
    void addInstrumentCommonParams (Layout& layout, int layer, float defaultLevel);
    void addInstrumentSynthParams (Layout& layout, int layer);
    void addFxParams (Layout& layout);
    void addMasterParams (Layout& layout);

    juce::StringArray keyNames();
    float midiToHz (float midiNote);

    // Kickens svans för en tonart: tonens namn i registret A#0..A1 (29-55 Hz), så att
    // kicken aldrig hamnar orimligt högt eller lågt oavsett tonart.
    int kickTailMidiNote (int keyPitchClass);

    // Basens grundton för en ton från DAW:en: samma tonnamn två oktaver ner (FL:s C5 -> C2),
    // så att en basgång som går nedåt i pianorullen också går nedåt i basen.
    int instrumentRootMidiNote (int dawNote);
    int barsFromChoice (int choiceIndex); // 0 ... 6 -> 1, 2, 4, 8, 16, 32, 64 takter
    int rowBarsFromChoice (int choiceIndex); // 0 = hela mönstret (0), 1 ... 7 -> 1, 2 ... 64 takter

    // Mönsterlängderna ur parametrarna. `get` ger ett parameters värde från dess ID (ParamRefs eller apvts).
    template <typename Getter>
    PatternLengths readPatternLengthsWith (Getter&& get)
    {
        PatternLengths lengths;
        lengths.total = barsFromChoice ((int) get (ParamID::patternBars));
        for (int row = 0; row < numPatternRows; ++row)
        {
            lengths.rows[(size_t) row] = rowBarsFromChoice ((int) get (ParamID::rowBars (row)));
            lengths.loops[(size_t) row] = get (ParamID::rowLoop (row)) >= 0.5f;
        }

        // Totalen är minst så lång som den längsta raden: mönstret börjar inte om förrän den är klar.
        for (const auto rowBars : lengths.rows)
            lengths.total = std::max (lengths.total, rowBars);
        return lengths;
    }

    inline PatternLengths readPatternLengths (const juce::AudioProcessorValueTreeState& apvts)
    {
        return readPatternLengthsWith ([&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); });
    }
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
    float bass (int layer, InstrumentField field) const;

    // Antal använda instrument (1-10).
    int instruments() const { return juce::jlimit (1, ParamID::numInstrumentLayers, (int) std::lround (get (ParamID::instrumentCount))); }

    KickParams readKick (int keyPitchClass) const;
    InstrumentParams readInstrument (int layer, int rootMidiNote) const; // rootMidiNote före Octave och Tune

private:
    struct Entry
    {
        juce::String id;
        std::atomic<float>* value;
        int index; // plats i processorns parameterlista (samma som Modulator använder)
    };

    float valueOf (const Entry& entry) const;

    std::vector<Entry> entries; // byggs en gång i konstruktorn, ändras aldrig efter det
    std::unordered_map<std::string_view, int> lookup; // parameter-ID -> plats i entries (nycklarna pekar in i entries; ljudtråden slår upp utan att allokera)
    std::array<std::array<int, (size_t) InstrumentField::count>, ParamID::numInstrumentLayers> instrumentEntries {}; // index i entries, -1 = saknas
    const Modulator* modulator = nullptr;
};
