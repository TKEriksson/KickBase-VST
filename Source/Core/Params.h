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
    inline constexpr auto kickComp        = "kickComp";        // kompressor efter drive: 0 = av, ju mer desto tätare kick (släpp = kickens längd, auto-makeup)
    inline constexpr auto kickFxSend      = "kickFxSend";      // FX-kanalen som SEND: av = som förut (amount delar ljudet torrt/blött), på = torrt orört + amount till kanalen
    inline constexpr const char* kickFxSendToIds[] { "kickFxSendTo1", "kickFxSendTo2", "kickFxSendTo3", "kickFxSendTo4", "kickFxSendTo5", "kickFxSendTo6", "kickFxSendTo7", "kickFxSendTo8", "kickFxSendTo9", "kickFxSendTo10", "kickFxSendTo11", "kickFxSendTo12" }; // FX-matrisen (kickens send-belopp per kanal)
    inline constexpr const char* fxMuteIds[] { "fxMute1", "fxMute2", "fxMute3", "fxMute4", "fxMute5", "fxMute6", "fxMute7", "fxMute8", "fxMute9", "fxMute10", "fxMute11", "fxMute12" }; // M-knapp per FX-kanal (tystar kanalens retur, sends ligger kvar)

    // Baslagren: se ParamID::bass (lager, fält) nedan.

    // FX
    inline constexpr auto fxKey       = "fxKey";
    inline constexpr auto gateAttack  = "gateAttack";
    inline constexpr auto gateRelease = "gateRelease";

    // Master
    inline constexpr auto gain = "gain";

    // JOG (D3b): hastighet för den egna klockan i förhandsspelning, 1 = normalt, 0 = står still, 4 = fyra gånger snabbare.
    inline constexpr auto jogSpeed = "jogSpeed";

    // Kickens SYNTH och SAMPLE på/av (båda på = som förut). Sist i layouten, så att gamla projekt är orörda.
    inline constexpr auto kickSynthOn  = "kickSynthOn";
    inline constexpr auto kickSampleOn = "kickSampleOn";
    inline constexpr auto kickMix = "kickMix"; // 0 = bara synten, 0.5 = båda (som förut), 1 = bara samplingen

    // Klick-lager 2 och 3 (nivå 0 = av) och sampleverktyg (klipp, baklänges, utfasning). Allra sist i kickens block.
    inline constexpr auto kickClick2Level = "kickClick2Level";
    inline constexpr auto kickClick2Length = "kickClick2Length";
    inline constexpr auto kickClick2Tone = "kickClick2Tone";
    inline constexpr auto kickClick2Delay = "kickClick2Delay";
    inline constexpr auto kickClick2Mode = "kickClick2Mode";
    inline constexpr auto kickClick3Level = "kickClick3Level";
    inline constexpr auto kickClick3Length = "kickClick3Length";
    inline constexpr auto kickClick3Tone = "kickClick3Tone";
    inline constexpr auto kickClick3Delay = "kickClick3Delay";
    inline constexpr auto kickClick3Mode = "kickClick3Mode";
    inline constexpr auto kickSampleStart = "kickSampleStart";
    inline constexpr auto kickSampleReverse = "kickSampleReverse";
    inline constexpr auto kickSampleFade = "kickSampleFade";

    // FUNDAMENTAL DUCK för hela instrumentet (alla motorer som ärver). Sist i layouten (addFundamentalDuckParams).
    inline constexpr auto fundamentalDuckOn     = "fundamentalDuckOn";
    inline constexpr auto fundamentalDuckDepth  = "fundamentalDuckDepth";
    inline constexpr auto fundamentalDuckWindow = "fundamentalDuckWindow";
    inline constexpr auto fundamentalDuckSource = "fundamentalDuckSource";
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
    filterModel,                                                  // filtermodell: 0 Kickbase (SVF, standard), 1 TB-303 (Open303:s TeeBeeFilter); ny parameter, filterType rörs inte
    aMorph, bMorph, cMorph,                                       // wavetable-position: på = tonar över mellan vågformerna (standard), av = hoppar mellan dem (oscMorphField)
    env2Mode, env2AttackSync, env2DecaySync, env2ReleaseSync,     // ENV 2 och ENV 3: ms (standard) eller Sync (notvärden följer tempot); envModeField / envSyncField
    env3Mode, env3AttackSync, env3DecaySync, env3ReleaseSync,
    filterP1, filterP2,                                           // filtermodellens två ratter (0..1): Ladder DRIVE; Formant MORPH och BLEND; Comb DAMPING och BLEND
    filter2On, filter2Model, filter2Type, filter2Cutoff, filter2Reso, filter2P1, filter2P2, // Filter 2 (fas D3): av som standard; ingen egen envelope eller keytrack, men modulerbar
    filterRouting, filterMix,                                     // routing: Serial (Filter 1 -> Filter 2) eller Parallel; MIX: Serial = Filter 2 torr/våt, Parallel = (1-MIX) F1 + MIX F2
    voices,                                                       // fas P: röster per instrument 1..8 (1 = monofoniskt som förut); kan inte moduleras
    env1On, filterEnvOn, env2On, env3On,                          // fas L: ENV-flikens ON/OFF (envOnField); alla på = som förut
    env1Mode, env1AttackSync, env1DecaySync, env1ReleaseSync,     // fas L: volym-envelopen i ms (standard) eller Sync, som ENV 2/3
    filterEnvMode, filterEnvAttackSync, filterEnvDecaySync,       // fas L: filter-envelopen i ms (standard) eller Sync
    filterOn, filterOsc1, filterOsc2, filterOsc3, filterSample, filterNoise, // fas S: FILTER-flikens av/på (hela Filter 1+2) och av/på per källa (av = källan går förbi båda filtren)
    noiseType,                                                    // fas S: brustyp: White (standard = som förut), Pink, Brown, Vinyl
    warpEnvSource, warpEnvDecay,                                  // fas S: WARP ENV-källa (Filter Env = som förut, Direct, ENV 2, ENV 3) och Directs egen decay i ms
    polarity, phaseDeg, nudgeMs,                                  // fas PH: polaritet (vänd), startfas 0..360 grader för OSC 1-3 och sub, nudge -2..+2 ms (tillägg till DELAY; negativt tar bort av DELAY)
    fmOn, fmLevel, fmRatio, fmIndex, fmEnvAmount, fmEnvTime, fmSemis, fmPreset, fmFilter, // fas S2: FM-källa (av som standard); fmPreset 0 = Custom (läses inte av DSP, bara av UI)
    slicerOn, slicerMapTo, slicerReverse, // fas F2; slicerOn och slicerMapTo är oanvända (ID kvar, E2: skivor spelas via raderna "Slice N")
    fxSend,                                                       // FX-kanalen som SEND (av som standard = som förut: amount delar ljudet torrt/blött)
    sampleMode,                                                   // fas R9b: lagrets SAMPLE-rad: 0 BLEND (blandas med syntrösten, som förut), 1 INDEPENDENT (egen rad), 2 OFF; sist, så gamla projekt är orörda
    allpassFreq, allpassAmount,                                   // allpass efter filtren: 4 första ordningens allpass vid FREQ (20..2000 Hz); AMOUNT 0 (standard) = av och ljudet oförändrat
    samplePan, noisePan, fmPan, noiseWidth, subPhaseDeg, fmPhaseDeg, aWidth, bWidth, cWidth, sampleWidth, fmWidth, // bredd per källa: 100 % = som förut (OSC 0..200 %: 0 = mono; SAMPLE och FM 100..200 %: okorrelerat); INSTR-MIX: pan för SAMPLE, NOISE och FM (0 = mitten = som förut) och brusets bredd (100 % = mono som förut, upp till 200 %); sub är alltid mono i mitten
    ringSource,                                                  // RING (B) på OSC 1: vad OSC 1 multipliceras med (0 = OSC 2, 1 = OSC 3, 2 = OSC 2 x OSC 3)
    fundamentalDuckMode, fundamentalDuckDepth, fundamentalDuckWindow, fundamentalDuckSource, // FUNDAMENTAL DUCK per motor: 0 ÄRV (instrumentets värden, standard), 1 AV, 2 EGEN; registreras sist (addFundamentalDuckParams)
    sampleSource,                                                // multisampling: 0 = Enkel (lagrets en sampling, som förut), 1 = Multi (zoner på tangenter och anslag)
    sampleAmpOn, sampleAmpMs, sampleAmpShape,                    // sample-editor: AMP-kurva över tid (av = som förut)
    samplePitchOn, samplePitchSemis, samplePitchMs, samplePitchShape, // sample-editor: PITCH-kurva (startavvikelse i halvtoner som glider mot 0)
    sampleReverse,                                               // sample-editor: spelar samplingen baklänges per not (av = som förut)
    // 5b: tid per motor (POS DELAY, SYNC, LENGTH, CURVE), motorerna i ordningen OSC 1, OSC 2, OSC 3, SAMPLE, NOISE, SUB, FM; timingField (motor, vilken). Registreras sist (addEngineTimingParams).
    aDelayMs, aDelaySync, aLenMs, aCurve, bDelayMs, bDelaySync, bLenMs, bCurve, cDelayMs, cDelaySync, cLenMs, cCurve,
    sampleDelayMs, sampleDelaySync, sampleLenMs, sampleCurve, noiseDelayMs, noiseDelaySync, noiseLenMs, noiseCurve,
    subDelayMs, subDelaySync, subLenMs, subCurve, fmDelayMs, fmDelaySync, fmLenMs, fmCurve,
    // TIMING/LEVELS: per motor DELAY MODE (GATE/SHIFT), LENGTH SYNC, DELAY COUNT, LENGTH COUNT; timingExtraField (motor, vilken). Registreras allra sist (addEngineTimingExtraParams).
    aDelayMode, aLenSync, aDelayCount, aLenCount, bDelayMode, bLenSync, bDelayCount, bLenCount, cDelayMode, cLenSync, cDelayCount, cLenCount,
    sampleDelayMode, sampleLenSync, sampleDelayCount, sampleLenCount, noiseDelayMode, noiseLenSync, noiseDelayCount, noiseLenCount,
    subDelayMode, subLenSync, subDelayCount, subLenCount, fmDelayMode, fmLenSync, fmDelayCount, fmLenCount,
    // FX-matrisen: instrumentets send-belopp (0..1) till FX1..FX12, utöver fxChannel/fxAmount (läggs ihop). Registreras allra sist (addFxMatrixParams); fxSendToField (kanal).
    fxSendTo1, fxSendTo2, fxSendTo3, fxSendTo4, fxSendTo5, fxSendTo6, fxSendTo7, fxSendTo8, fxSendTo9, fxSendTo10, fxSendTo11, fxSendTo12,
    // MAX TIME: gemensamt tak för tidsknobbarna (volym ATTACK/DECAY/RELEASE, filter-DECAY, pitch-DECAY) och ENV-diagrammets tidsaxel. Val 0-5 (250 ms..10 s), standard 1 s = dagens tak. Registreras allra sist (addMaxTimeParam).
    maxTime,
    count
};

namespace ParamID
{
    constexpr int numInstrumentLayers = maxInstruments; // så många instrument finns som mest (instrumentCount väljer hur många som används)
    constexpr int oscFieldCount = 15; // fält per oscillator (aTable ... aOn)
    constexpr int oscsPerLayer = 3;   // OSC 1, 2 och 3

    // Samma fält för OSC 1 (osc 0), OSC 2 (osc 1) eller OSC 3 (osc 2): oscField (1, InstrumentField::aPos) = bPos.
    inline InstrumentField oscField (int osc, InstrumentField aField) { return (InstrumentField) ((int) aField + osc * oscFieldCount); }
    // Morph-fälten ligger sist i listan (så att gamla projekt är orörda) och följer inte oscField: OSC 1 (osc 0) = aMorph, OSC 2 = bMorph, OSC 3 = cMorph.
    inline InstrumentField oscMorphField (int osc) { return (InstrumentField) ((int) InstrumentField::aMorph + osc); }
    // ENV 2 (env 0) och ENV 3 (env 1): Mode-fältet och tidens Sync-fält (which 0 = attack, 1 = decay, 2 = release).
    inline InstrumentField envModeField (int env) { return (InstrumentField) ((int) InstrumentField::env2Mode + env * 4); }
    inline InstrumentField envSyncField (int env, int which) { return (InstrumentField) ((int) envModeField (env) + 1 + which); }
    // ENV-flikens fyra rutor (0 = ENV 1, 1 = FILTER ENV, 2 = ENV 2, 3 = ENV 3): ON/OFF och tidsläget ms/Sync.
    inline InstrumentField envOnField (int tile) { return (InstrumentField) ((int) InstrumentField::env1On + tile); }
    inline InstrumentField envTileModeField (int tile) { return tile == 0 ? InstrumentField::env1Mode : tile == 1 ? InstrumentField::filterEnvMode : envModeField (tile - 2); }
    // 5b: motorernas tid. engine 0..6 = OSC 1, OSC 2, OSC 3, SAMPLE, NOISE, SUB, FM; which 0 = DelayMs, 1 = DelaySync, 2 = LenMs, 3 = Curve.
    constexpr int numTimingEngines = 7;
    inline InstrumentField timingField (int engine, int which) { return (InstrumentField) ((int) InstrumentField::aDelayMs + engine * 4 + which); }
    // which 0 = DelayMode, 1 = LenSync, 2 = DelayCount, 3 = LenCount.
    inline InstrumentField timingExtraField (int engine, int which) { return (InstrumentField) ((int) InstrumentField::aDelayMode + engine * 4 + which); }
    // FX-matrisen: kanal 0..11 = FX1..FX12. Kickens motsvarighet är kickFxSendToIds (samma ordning).
    constexpr int numFxSendTargets = 12;
    inline InstrumentField fxSendToField (int channel) { return (InstrumentField) ((int) InstrumentField::fxSendTo1 + channel); }
    // MAX TIME: de fem tidsknobbarnas tak = legacyMax * maxTimeMs / 1000. Standard (1 s) ger exakt dagens tak. APVTS-intervallet är alltid 10x legacyMax; knobbarna och modulationen begränsas till taket.
    constexpr int numMaxTimeSteps = 6;
    constexpr int maxTimeDefault = 2; // 1 s
    inline float maxTimeMs (int step)
    {
        constexpr float steps[numMaxTimeSteps] { 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        return steps[step < 0 ? 0 : step >= numMaxTimeSteps ? numMaxTimeSteps - 1 : step];
    }
    struct TimeCapSpec { InstrumentField field; float legacyMax, min, centre; };
    constexpr int numTimeCaps = 6;
    inline const TimeCapSpec& timeCapSpec (int i)
    {
        static const TimeCapSpec specs[numTimeCaps] {
            { InstrumentField::attack, 50.0f, 0.0f, 5.0f }, { InstrumentField::decay, 2000.0f, 5.0f, 200.0f },
            { InstrumentField::release, 300.0f, 1.0f, 30.0f }, { InstrumentField::pitchEnvDecay, 1000.0f, 5.0f, 80.0f },
            { InstrumentField::envDecay, 1000.0f, 5.0f, 150.0f },
            { InstrumentField::fmEnvTime, 400.0f, 1.0f, 60.0f } };
        return specs[i];
    }
    inline float timeCapMs (const TimeCapSpec& spec, int step) { return spec.legacyMax * maxTimeMs (step) / 1000.0f; }
    inline const TimeCapSpec* timeCapFor (InstrumentField field)
    {
        for (int i = 0; i < numTimeCaps; ++i)
            if (timeCapSpec (i).field == field)
                return &timeCapSpec (i);
        return nullptr;
    }
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
    void addJogParams (Layout& layout); // D3b: sist i layouten
    void addFundamentalDuckParams (Layout& layout); // FUNDAMENTAL DUCK (instrument + per motor): allra sist i instrumentets layout
    void addEngineTimingParams (Layout& layout); // 5b: POS DELAY / SYNC / LENGTH / CURVE per motor och lager; sist i instrumentets layout
    void addEngineTimingExtraParams (Layout& layout); // TIMING/LEVELS: DELAY MODE / LENGTH SYNC / antal per motor och lager; allra sist i instrumentets layout
    void addKickMixParam (Layout& layout); // kickens MIX (synt/sampling); allra sist i båda layouterna
    void addKickToggleParams (Layout& layout); // kickens SYNTH/SAMPLE på/av; läses av readKick, så FX-pluginet behöver dem också
    void addMaxTimeParam (Layout& layout); // MAX TIME per instrument; allra sist i instrumentets layout
    void addFxMatrixParams (Layout& layout); // FX-matrisen (instrument + kick) och fxMute1..12: allra sist i layouten

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

    // Mönsterlängderna (R9): varje rads längd kommer ur mönstret (AUTO = innehållet, eller ett valt antal takter), loop på/av ur parametrarna
    // (rowLoop). Mönstret är lika långt som den längsta raden (kicken och de `activeInstruments` första instrumenten): det börjar inte om förrän
    // den är klar, och kortare rader loopar. Parametrarna patternBars och rowBars används inte längre (de ligger kvar så att ordningen är oförändrad).
    // `get` ger ett parameters värde från dess ID (ParamRefs eller apvts).
    template <typename Getter>
    PatternLengths readPatternLengthsWith (Getter&& get, const Pattern& pattern, int activeInstruments)
    {
        PatternLengths lengths;
        lengths.total = 1;
        for (int row = 0; row < numPatternRows; ++row)
        {
            const auto layer = row == 0 ? Pattern::kickLayer : row - 1;
            lengths.rows[(size_t) row] = pattern.lengthBars (layer);
            lengths.loops[(size_t) row] = get (ParamID::rowLoop (row)) >= 0.5f;
            if (row <= activeInstruments)
                lengths.total = std::max (lengths.total, lengths.rows[(size_t) row]);
        }
        // R9b: underraderna (SAMPLE, "Slice N") räknas med när deras instrument är aktivt; längd och loop ur mönstret, inte ur parametrar. Tom pool = ingen skillnad.
        for (int id = RowRegistry::firstSubRow; id < RowRegistry::maxRows; ++id)
        {
            if (! pattern.rows.isActive (id))
                continue;
            const auto slot = (size_t) Pattern::lengthRowOf (id);
            lengths.rows[slot] = pattern.lengthBars (id);
            lengths.loops[slot] = pattern.loopsRow (id);
            if (pattern.rows.instrumentOf (id) < activeInstruments)
                lengths.total = std::max (lengths.total, lengths.rows[slot]);
        }
        return lengths;
    }

    inline PatternLengths readPatternLengths (const juce::AudioProcessorValueTreeState& apvts, const Pattern& pattern, int activeInstruments)
    {
        return readPatternLengthsWith ([&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); }, pattern, activeInstruments);
    }

    // Gamla projekt och verktyg som sätter de gamla längdparametrarna: översätt dem till radernas längd (se Pattern::applyLegacyLengths).
    inline void applyLegacyLengthParams (const juce::AudioProcessorValueTreeState& apvts, Pattern& pattern, int onlyLayer = -1)
    {
        std::array<int, numPatternRows> rows {};
        for (int row = 0; row < numPatternRows; ++row)
            rows[(size_t) row] = rowBarsFromChoice ((int) std::lround (apvts.getRawParameterValue (ParamID::rowBars (row))->load()));
        pattern.applyLegacyLengths (barsFromChoice ((int) std::lround (apvts.getRawParameterValue (ParamID::patternBars)->load())), rows, onlyLayer);
    }

    // Gamla projekt och presets med kickens SYNTH- eller SAMPLE-knapp av: knapparna visas inte längre (MIX styr), så de översätts till MIX.
    // Samma ljud som motorn gav (KickEngine::effectiveSampleMode, MIX-förstärkningarna i KickEngine.cpp):
    //  - synten av = bara samplingen, i läget Replace  ->  MIX 1 (bara samplingen) och läge Replace
    //  - samplingen av = som utan sampling  ->  MIX 0 (bara synten)
    //  - båda av lämnas (tyst kick, som förut).
    // get/set läser och skriver parametrarnas riktiga värden (bool 0/1, valindex, MIX 0-1); get ger andra argumentet när parametern saknas.
    // Returnerar true om något ändrades.
    inline bool migrateKickSources (const std::function<float (const char*, float)>& get, const std::function<void (const char*, float)>& set)
    {
        const auto synthOff = get (ParamID::kickSynthOn, 1.0f) < 0.5f;
        const auto sampleOff = get (ParamID::kickSampleOn, 1.0f) < 0.5f;
        if (synthOff == sampleOff)
            return false;

        if (synthOff)
        {
            set (ParamID::kickMix, 1.0f);
            set (ParamID::kickSampleMode, 2.0f);
            set (ParamID::kickSynthOn, 1.0f);
        }
        else
        {
            set (ParamID::kickMix, 0.0f);
            set (ParamID::kickSampleOn, 1.0f);
        }
        return true;
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
    bool hasBass (int layer, InstrumentField field) const { return instrumentEntries[(size_t) layer][(size_t) field] >= 0; } // FX-pluginet saknar t.ex. fxSend

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

    // get() slår annars upp ID:t som text (hash + jämförelse) varje gång; det gör ljudtråden var 32:a sample.
    static constexpr int cacheSize = 1024, cacheProbes = 8;
    mutable std::array<std::atomic<std::uint64_t>, cacheSize> idCache {};
};
