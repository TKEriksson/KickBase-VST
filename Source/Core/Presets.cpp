#include "Presets.h"
#include "Params.h"

namespace
{
    using Section = Presets::Section;

    struct StepTweak
    {
        int step;          // steg i takten (upprepas i alla takter), i mallens rutnät
        int pitch;         // halvtoner
        bool slide;
        float gate = -1.0f; // gate-längd 0..1, eller -1 = mallens
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
        std::vector<StepTweak> tweaks;                      // i mallens rutnät
        std::vector<SidechainCurve::Point> curve;           // bara Sidechain; tom = standardkurvan
    };

    using namespace ParamID;

    std::vector<Value> join (std::initializer_list<std::vector<Value>> parts)
    {
        std::vector<Value> all;
        for (const auto& part : parts)
            all.insert (all.end(), part.begin(), part.end());
        return all;
    }

    // Kickar och basar som används både i helpresets och som egna kick- och baspresets.
    // Kickens svans ligger oftast 1-2 halvtoner över tonarten (som i referenslåtarna), och
    // suben ligger i grundtonens oktav, så att inget hamnar under ~35 Hz.
    namespace Sound
    {
        // Varianter av de anpassade referensljuden (se "(... style)"-presetsen).
        const std::vector<Value> astrixKick { { kickTune, 3.0f }, { kickPitch, 28.0f }, { kickPitchTime, 3.0f }, { kickPitchCurve, 4.0f },
            { kickGlide, 4.0f }, { kickGlideTime, 90.0f }, { kickAttack, 0.0f }, { kickHold, 50.0f }, { kickDecay, 125.0f },
            { kickDecayCurve, 1.3f }, { kickClick, 0.12f }, { kickClickLength, 1.0f }, { kickClickTone, 6000.0f }, { kickDrive, 0.1f } };
        const std::vector<Value> gorovichKick { { kickTune, 2.5f }, { kickPitch, 42.0f }, { kickPitchTime, 4.5f }, { kickPitchCurve, 3.0f },
            { kickAttack, 0.0f }, { kickHold, 5.0f }, { kickDecay, 250.0f }, { kickDecayCurve, 2.7f }, { kickClick, 0.12f },
            { kickClickLength, 1.0f }, { kickClickTone, 6000.0f }, { kickDrive, 0.3f } };
        const std::vector<Value> dekelKick { { kickTune, 2.5f }, { kickPitch, 20.0f }, { kickPitchTime, 14.0f }, { kickPitchCurve, 1.6f },
            { kickGlide, 4.0f }, { kickGlideTime, 14.0f }, { kickAttack, 1.0f }, { kickHold, 45.0f }, { kickDecay, 225.0f },
            { kickDecayCurve, 2.6f }, { kickClick, 0.22f }, { kickClickLength, 1.0f }, { kickClickTone, 4800.0f }, { kickDrive, 0.1f } };
        const std::vector<Value> hookKick { { kickTune, 1.5f }, { kickPitch, 28.0f }, { kickPitchTime, 9.0f }, { kickPitchCurve, 2.2f },
            { kickGlide, 4.0f }, { kickGlideTime, 70.0f }, { kickHold, 30.0f }, { kickDecay, 175.0f }, { kickDecayCurve, 1.6f },
            { kickClick, 0.18f }, { kickClickLength, 1.5f }, { kickClickTone, 5500.0f }, { kickDrive, 0.3f } };
        const std::vector<Value> fullOnKick { { kickTune, 1.0f }, { kickPitch, 36.0f }, { kickPitchTime, 14.0f }, { kickPitchCurve, 3.0f },
            { kickGlide, 2.0f }, { kickGlideTime, 45.0f }, { kickHold, 16.0f }, { kickDecay, 150.0f }, { kickDecayCurve, 1.9f },
            { kickClick, 0.3f }, { kickClickLength, 2.0f }, { kickClickTone, 7000.0f }, { kickDrive, 0.35f } };
        const std::vector<Value> snapKick { { kickTune, 1.0f }, { kickPitch, 42.0f }, { kickPitchTime, 8.0f }, { kickPitchCurve, 4.0f },
            { kickHold, 8.0f }, { kickDecay, 110.0f }, { kickDecayCurve, 2.6f }, { kickClick, 0.6f }, { kickClickLength, 2.5f },
            { kickClickTone, 9500.0f }, { kickDrive, 0.4f } };
        const std::vector<Value> nightKick { { kickTune, -1.0f }, { kickPitch, 28.0f }, { kickPitchTime, 20.0f }, { kickPitchCurve, 2.0f },
            { kickHold, 20.0f }, { kickDecay, 160.0f }, { kickDecayCurve, 1.8f }, { kickClick, 0.15f }, { kickClickLength, 2.0f },
            { kickClickTone, 2800.0f }, { kickDrive, 0.65f } };
        const std::vector<Value> progKick { { kickTune, -2.0f }, { kickPitch, 22.0f }, { kickPitchTime, 25.0f }, { kickPitchCurve, 1.8f },
            { kickGlide, 3.0f }, { kickGlideTime, 90.0f }, { kickHold, 35.0f }, { kickDecay, 240.0f }, { kickDecayCurve, 1.3f },
            { kickClick, 0.1f }, { kickClickLength, 3.0f }, { kickClickTone, 2000.0f }, { kickDrive, 0.15f } };
        const std::vector<Value> gritKick { { kickPitch, 44.0f }, { kickPitchTime, 22.0f }, { kickPitchCurve, 3.5f }, { kickHold, 12.0f },
            { kickDecay, 135.0f }, { kickDecayCurve, 2.2f }, { kickClick, 0.35f }, { kickClickTone, 4000.0f }, { kickDrive, 0.85f } };
        const std::vector<Value> longKick { { kickTune, 1.0f }, { kickPitch, 24.0f }, { kickPitchTime, 15.0f }, { kickPitchCurve, 2.0f },
            { kickHold, 50.0f }, { kickDecay, 320.0f }, { kickDecayCurve, 1.2f }, { kickClick, 0.1f }, { kickClickLength, 1.0f },
            { kickDrive, 0.15f } };

        const std::vector<Value> hookBass { { bassShape, 1.8f }, { bassPulseWidth, 0.3f }, { bassSub, 0.1f }, { bassSubOctave, 1 },
            { bassOsc2Shape, 3.0f }, { bassOsc2Pw, 0.25f }, { bassOsc2Level, 0.6f }, { bassOsc2Semi, 24.0f }, { bassOsc2Detune, 6.0f },
            { bassFmEnv, 0.1f }, { bassFilterType, 1 }, { bassCutoff, 95.0f }, { bassResonance, 0.45f }, { bassEnvAmount, 1.0f },
            { bassEnvDecay, 110.0f }, { bassVelToEnv, 0.2f }, { bassAttack, 0.3f }, { bassDecay, 25.0f }, { bassSustain, 0.4f },
            { bassRelease, 14.0f }, { bassDrive, 0.4f } };
        const std::vector<Value> gorovichBass { { bassShape, 1.6f }, { bassSub, 0.35f }, { bassSubOctave, 1 }, { bassOsc2Shape, 3.0f },
            { bassOsc2Pw, 0.6f }, { bassOsc2Level, 0.05f }, { bassFm, 0.05f }, { bassFmEnv, 0.75f }, { bassFilterType, 1 },
            { bassCutoff, 210.0f }, { bassResonance, 0.45f }, { bassEnvAmount, 1.8f }, { bassEnvDecay, 32.0f }, { bassVelToEnv, 0.0f },
            { bassAttack, 3.0f }, { bassDecay, 18.0f }, { bassSustain, 0.55f }, { bassRelease, 5.0f }, { bassDrive, 0.35f } };
        const std::vector<Value> dekelBass { { bassShape, 1.8f }, { bassPulseWidth, 0.4f }, { bassSub, 0.45f }, { bassSubOctave, 1 },
            { bassOsc2Shape, 2.5f }, { bassOsc2Level, 0.25f }, { bassOsc2Semi, 7.0f }, { bassOsc2Detune, 10.0f }, { bassFilterType, 1 },
            { bassCutoff, 850.0f }, { bassResonance, 0.35f }, { bassEnvAmount, 2.2f }, { bassEnvDecay, 160.0f }, { bassKeyTrack, 0.4f },
            { bassAttack, 2.5f }, { bassDecay, 180.0f }, { bassSustain, 0.6f }, { bassRelease, 18.0f }, { bassDrive, 0.15f } };
        const std::vector<Value> rubberBass { { bassShape, 2.0f }, { bassSub, 0.3f }, { bassSubOctave, 1 }, { bassCutoff, 170.0f },
            { bassResonance, 0.78f }, { bassEnvAmount, 5.5f }, { bassEnvDecay, 95.0f }, { bassVelToEnv, 0.7f }, { bassKeyTrack, 0.3f },
            { bassAttack, 1.0f }, { bassDecay, 120.0f }, { bassSustain, 0.35f }, { bassRelease, 10.0f }, { bassDrive, 0.35f } };
        const std::vector<Value> squareBass { { bassShape, 3.0f }, { bassPulseWidth, 0.35f }, { bassSub, 0.25f }, { bassSubOctave, 1 },
            { bassCutoff, 150.0f }, { bassResonance, 0.5f }, { bassEnvAmount, 5.0f }, { bassEnvDecay, 60.0f }, { bassKeyTrack, 0.3f },
            { bassAttack, 0.8f }, { bassDecay, 90.0f }, { bassSustain, 0.3f }, { bassRelease, 8.0f }, { bassDrive, 0.5f } };
        const std::vector<Value> morningBass { { bassShape, 2.0f }, { bassSub, 0.35f }, { bassSubOctave, 1 }, { bassOsc2Shape, 2.0f },
            { bassOsc2Level, 0.45f }, { bassOsc2Detune, 14.0f }, { bassCutoff, 480.0f }, { bassResonance, 0.25f }, { bassEnvAmount, 3.0f },
            { bassEnvDecay, 95.0f }, { bassKeyTrack, 0.5f }, { bassAttack, 1.2f }, { bassDecay, 100.0f }, { bassSustain, 0.4f },
            { bassRelease, 10.0f }, { bassDrive, 0.3f } };
        const std::vector<Value> thunderBass { { bassShape, 2.3f }, { bassSub, 0.4f }, { bassSubOctave, 1 }, { bassFm, 0.08f },
            { bassFmEnv, 0.3f }, { bassOsc2Shape, 0.0f }, { bassOsc2Semi, 12.0f }, { bassCutoff, 260.0f }, { bassResonance, 0.4f },
            { bassEnvAmount, 4.0f }, { bassEnvDecay, 70.0f }, { bassKeyTrack, 0.4f }, { bassAttack, 0.6f }, { bassDecay, 80.0f },
            { bassSustain, 0.3f }, { bassRelease, 6.0f }, { bassDrive, 0.55f } };
        const std::vector<Value> progBass { { bassShape, 0.8f }, { bassSub, 0.5f }, { bassSubOctave, 1 }, { bassOsc2Shape, 1.0f },
            { bassOsc2Level, 0.2f }, { bassOsc2Semi, 12.0f }, { bassCutoff, 480.0f }, { bassResonance, 0.25f }, { bassEnvAmount, 2.0f },
            { bassEnvDecay, 140.0f }, { bassKeyTrack, 0.5f }, { bassAttack, 3.0f }, { bassDecay, 200.0f }, { bassSustain, 0.6f },
            { bassRelease, 30.0f }, { bassDrive, 0.2f } };
        const std::vector<Value> acidBass { { bassShape, 2.0f }, { bassSub, 0.25f }, { bassSubOctave, 1 }, { bassCutoff, 330.0f },
            { bassResonance, 0.72f }, { bassEnvAmount, 4.0f }, { bassEnvDecay, 140.0f }, { bassVelToEnv, 0.5f }, { bassGlide, 45.0f },
            { bassAttack, 1.0f }, { bassDecay, 160.0f }, { bassSustain, 0.45f }, { bassRelease, 12.0f }, { bassDrive, 0.3f } };
        const std::vector<Value> bellBass { { bassShape, 1.0f }, { bassSub, 0.35f }, { bassSubOctave, 1 }, { bassOsc2Shape, 0.0f },
            { bassOsc2Level, 0.1f }, { bassOsc2Semi, 19.0f }, { bassFm, 0.2f }, { bassFmEnv, 0.45f }, { bassCutoff, 900.0f },
            { bassResonance, 0.2f }, { bassEnvAmount, 2.5f }, { bassEnvDecay, 70.0f }, { bassKeyTrack, 0.5f }, { bassAttack, 1.0f },
            { bassDecay, 90.0f }, { bassSustain, 0.35f }, { bassRelease, 10.0f }, { bassDrive, 0.25f } };
    }

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
                { bassShape, 3.0f }, { bassCutoff, 150.0f }, { bassResonance, 0.55f }, { bassEnvAmount, 5.5f },
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
                { bassShape, 1.0f }, { bassCutoff, 900.0f }, { bassEnvAmount, 2.0f }, { bassSustain, 0.8f },
                { bassSub, 0.5f }, { duckAmount, 0.6f } },
              PatternTemplate::offbeat },

            { "Squelch Roller",
              { { kickPitch, 46.0f }, { kickPitchTime, 32.0f }, { kickDecay, 110.0f }, { kickDrive, 0.4f },
                { bassCutoff, 180.0f }, { bassResonance, 0.75f }, { bassEnvAmount, 6.0f }, { bassEnvDecay, 110.0f },
                { bassVelToEnv, 0.8f }, { bassDrive, 0.3f }, { bassSustain, 0.35f } },
              PatternTemplate::rolling, { { 3, 12, false }, { 11, -12, false } } },

            // Presets som använder effekter, duckkurva, LFO och master. Index för val
            // (t.ex. LFO-mål och echo-tid) följer listorna i Modulation, Effects och Sidechain.
            { "Night Rider",
              { { kickPitch, 48.0f }, { kickPitchTime, 33.0f }, { kickDecay, 125.0f }, { kickDrive, 0.4f }, { kickClick, 0.45f },
                { bassShape, 3.0f }, { bassCutoff, 170.0f }, { bassResonance, 0.45f }, { bassEnvAmount, 5.0f }, { bassEnvDecay, 80.0f },
                { bassSustain, 0.3f }, { bassRelease, 10.0f },
                { "fxDistOn", 1 }, { "fxDistDrive", 0.25f }, { "fxDistTone", 6500.0f },
                { "fxReverbOn", 1 }, { "fxReverbSize", 0.3f }, { "fxReverbMix", 0.12f },
                { "scMode", 1 }, { "scLength", 1 }, { duckAmount, 0.85f },
                { "masterDrive", 4.0f }, { "masterClip", 0.25f } },
              PatternTemplate::rolling, { { 15, 12, false } },
              { { 0.0f, 1.0f, 0.0f }, { 0.4f, 0.9f, -0.4f }, { 1.0f, 0.0f, 0.0f } } },

            { "Goa Sunrise",
              { { kickPitch, 44.0f }, { kickDecay, 140.0f },
                { bassCutoff, 420.0f }, { bassResonance, 0.4f }, { bassEnvAmount, 3.5f }, { bassEnvDecay, 140.0f },
                { bassGlide, 45.0f }, { bassSustain, 0.6f }, { bassGate, 0.9f },
                { "fxEchoOn", 1 }, { "fxEchoTime", 5 }, { "fxEchoFeedback", 0.4f }, { "fxEchoMix", 0.2f },
                { "fxPhaserOn", 1 }, { "fxPhaserRate", 0.12f }, { "fxPhaserDepth", 0.6f }, { "fxPhaserMix", 0.3f },
                { "lfo1On", 1 }, { "lfo1Shape", 1 }, { "lfo1Rate", 10 }, { "lfo1DestA", 1 }, { "lfo1AmountA", 0.25f } },
              PatternTemplate::galloping, { { 6, 0, true }, { 7, 7, false }, { 14, 0, true }, { 15, 12, false } } },

            { "Forest Prog",
              { { kickTune, -2.0f }, { kickPitch, 34.0f }, { kickPitchTime, 60.0f }, { kickDecay, 200.0f }, { kickClick, 0.15f },
                { bassShape, 0.0f }, { bassSub, 0.5f }, { bassCutoff, 900.0f }, { bassEnvAmount, 1.5f }, { bassSustain, 0.7f },
                { bassRelease, 30.0f },
                { "fxFilterOn", 1 }, { "fxFilterCutoff", 1200.0f }, { "fxFilterReso", 0.3f },
                { "fxReverbOn", 1 }, { "fxReverbSize", 0.85f }, { "fxReverbLowCut", 400.0f }, { "fxReverbMix", 0.2f },
                { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 9 }, { "lfo1DestA", 9 }, { "lfo1AmountA", 0.3f },
                { "masterDrive", 2.0f }, { "masterClip", 0.15f }, { "masterRelease", 150.0f } },
              PatternTemplate::triplet },

            { "Acid Psy",
              { { kickPitch, 46.0f }, { kickDecay, 115.0f }, { kickDrive, 0.35f },
                { bassCutoff, 260.0f }, { bassEnvAmount, 4.0f }, { bassEnvDecay, 100.0f }, { bassSustain, 0.4f },
                { "fxFilterOn", 1 }, { "fxFilterCutoff", 600.0f }, { "fxFilterReso", 0.75f },
                { "fxDistOn", 1 }, { "fxDistMode", 1 }, { "fxDistDrive", 0.35f },
                { "lfo1On", 1 }, { "lfo1Shape", 5 }, { "lfo1Rate", 2 }, { "lfo1DestA", 9 }, { "lfo1AmountA", 0.3f },
                { "masterDrive", 6.0f }, { "masterClip", 0.3f } },
              PatternTemplate::rolling, { { 3, 12, false }, { 7, 12, false }, { 11, 12, false }, { 15, 12, false } } },

            { "Gated Madness",
              { { kickPitch, 50.0f }, { kickPitchTime, 28.0f }, { kickDecay, 105.0f }, { kickDrive, 0.5f }, { kickClick, 0.5f },
                { bassCutoff, 240.0f }, { bassEnvAmount, 5.0f }, { bassEnvDecay, 80.0f }, { bassSustain, 0.35f },
                { "fxReverbOn", 1 }, { "fxReverbPost", 0 }, { "fxReverbSize", 0.75f }, { "fxReverbMix", 0.45f },
                { "scMode", 1 }, { "scLength", 1 }, { duckAmount, 0.9f },
                { "masterDrive", 10.0f }, { "masterClip", 0.5f }, { "masterRelease", 40.0f } },
              PatternTemplate::rolling, {},
              { { 0.0f, 1.0f, 0.0f }, { 0.3f, 1.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } } },

            { "Dark Forest",
              { { kickTune, -1.0f }, { kickPitch, 44.0f }, { kickDecay, 150.0f }, { kickDrive, 0.85f }, { kickClickTone, 3500.0f },
                { bassShape, 3.0f }, { bassCutoff, 140.0f }, { bassResonance, 0.6f }, { bassEnvAmount, 6.0f }, { bassEnvDecay, 65.0f },
                { bassSustain, 0.25f },
                { "fxDistOn", 1 }, { "fxDistMode", 2 }, { "fxDistDrive", 0.3f }, { "fxDistMix", 0.5f },
                { "fxPhaserOn", 1 }, { "fxPhaserRate", 0.25f }, { "fxPhaserDepth", 0.9f }, { "fxPhaserFeedback", 0.7f }, { "fxPhaserMix", 0.35f },
                { "lfo1On", 1 }, { "lfo1Shape", 5 }, { "lfo1Rate", 2 }, { "lfo1DestA", 4 }, { "lfo1AmountA", 0.25f },
                { "lfo2On", 1 }, { "lfo2Shape", 5 }, { "lfo2Rate", 2 }, { "lfo2DestA", 2 }, { "lfo2AmountA", 0.2f } },
              PatternTemplate::rolling, { { 11, -12, false } } },

            { "Bouncy Offbeat",
              { { kickDecay, 190.0f }, { kickPitch, 40.0f },
                { bassShape, 1.0f }, { bassSub, 0.45f }, { bassCutoff, 800.0f }, { bassEnvAmount, 2.5f }, { bassSustain, 0.75f },
                { duckAmount, 0.65f },
                { "fxChorusOn", 1 }, { "fxChorusRate", 0.9f }, { "fxChorusDepth", 0.4f }, { "fxChorusMix", 0.35f },
                { "fxEchoOn", 1 }, { "fxEchoTime", 6 }, { "fxEchoFeedback", 0.5f }, { "fxEchoHighCut", 3000.0f }, { "fxEchoMix", 0.25f } },
              PatternTemplate::offbeat },

            // Anpassade mot referenslåtarna med MyOwnFruitTool fitbeat (analys genom syntes: ett
            // medelslag per position i takten jämfört med vår kick + bas i tre band, kickens svep
            // och basens övertoner), toner från analyze. Spela i låtens tempo; tonen i pianorullen:
            // E4 (bas E på 41 Hz), G4 för Digital. Klicket mäts inte av fitbeat (över 4,6 kHz)
            // och kommer från analyze.

            // Captain Hook & Astrix - Bungee Jump (138 BPM): kort hårt svep och glid mot svansen
            // 3,6 halvtoner över E; basen (såg + fyrkant två oktaver upp genom LP12) ligger över
            // kicken i nivå. Åttondelstrioler (K-B-B) på E, D# på första bastriolen i slag 3.
            { "Bungee Jump (Astrix style)",
              { { kickKeyFollow, 1 }, { kickTune, 3.6f }, { kickPitch, 30.0f }, { kickPitchTime, 2.3f }, { kickPitchCurve, 4.67f },
                { kickGlide, 3.4f }, { kickGlideTime, 95.0f }, { kickAttack, 0.0f }, { kickHold, 52.0f }, { kickDecay, 112.0f },
                { kickDecayCurve, 1.28f }, { kickClick, 0.12f }, { kickClickLength, 1.0f }, { kickClickTone, 6000.0f }, { kickDrive, 0.0f },
                { kickLevel, 0.77f },
                { bassShape, 1.61f }, { bassPulseWidth, 0.28f }, { bassSub, 0.02f }, { bassSubOctave, 1 }, { bassNoise, 0.05f },
                { bassOsc2Shape, 3.0f }, { bassOsc2Pw, 0.21f }, { bassOsc2Level, 0.74f }, { bassOsc2Semi, 24.0f }, { bassOsc2Detune, 4.7f },
                { bassFmEnv, 0.07f }, { bassFilterType, 1 }, { bassCutoff, 74.0f }, { bassResonance, 0.45f }, { bassEnvAmount, 0.42f },
                { bassEnvDecay, 139.0f }, { bassVelToEnv, 0.0f }, { bassAttack, 0.1f }, { bassDecay, 8.0f }, { bassSustain, 0.34f },
                { bassRelease, 15.0f }, { bassDrive, 0.35f }, { bassLevel, 1.0f }, { duckAmount, 0.2f },
                { "masterDrive", 3.0f }, { "masterClip", 0.15f } },
              PatternTemplate::triplet,
              { { 1, 0, false, 1.0f }, { 2, 0, false, 1.0f }, { 4, 0, false, 1.0f }, { 5, 0, false, 1.0f },
                { 7, -1, false, 1.0f }, { 8, 0, false, 1.0f }, { 10, 0, false, 1.0f }, { 11, 0, false, 1.0f } } },

            // Gorovich - Digital Pussy (145 BPM): kicken sveper mjukt (~20 ms) till svansen två
            // halvtoner under tonarten; mörk FM-bas (FM 0,8 genom LP24 vid 120 Hz) med mjukt anslag.
            // Basgång relativt G: G G G | G C# E | G G G | G C# E. Spela G4 i pianorullen.
            { "Digital (Gorovich style)",
              { { kickKeyFollow, 1 }, { kickTune, -2.0f }, { kickPitch, 12.0f }, { kickPitchTime, 19.0f }, { kickPitchCurve, 0.88f },
                { kickAttack, 0.0f }, { kickHold, 27.0f }, { kickDecay, 126.0f }, { kickDecayCurve, 2.14f }, { kickClick, 0.1f },
                { kickClickLength, 1.0f }, { kickDrive, 0.01f },
                { bassShape, 2.02f }, { bassSub, 0.56f }, { bassSubOctave, 1 }, { bassNoise, 0.09f },
                { bassOsc2Shape, 0.0f }, { bassOsc2Pw, 0.29f }, { bassOsc2Level, 0.05f }, { bassOsc2Semi, 12.0f }, { bassOsc2Detune, 19.3f },
                { bassFm, 0.81f }, { bassFmEnv, 0.02f }, { bassCutoff, 120.0f }, { bassResonance, 0.16f }, { bassEnvAmount, 0.73f },
                { bassEnvDecay, 726.0f }, { bassVelToEnv, 0.0f }, { bassAttack, 26.0f }, { bassDecay, 680.0f }, { bassSustain, 0.39f },
                { bassRelease, 10.3f }, { bassDrive, 0.14f }, { bassLevel, 0.39f }, { duckAmount, 0.15f },
                { "masterDrive", 3.0f }, { "masterClip", 0.15f } },
              PatternTemplate::rolling,
              { { 1, 0, false, 0.99f }, { 2, 0, false, 0.99f }, { 3, 0, false, 0.99f },
                { 5, 0, false, 0.99f }, { 6, 6, false, 0.99f }, { 7, 9, false, 0.99f },
                { 9, 0, false, 0.99f }, { 10, 0, false, 0.99f }, { 11, 0, false, 0.99f },
                { 13, 0, false, 0.99f }, { 14, 6, false, 0.99f }, { 15, 9, false, 0.99f } } },

            // Gorovich - Pinball (145 BPM): kort hårt svep (~4 ms) och lång
            // kick nära F#, mörk rund bas (triangel/såg genom LP12 vid 180 Hz) med FM-anslag.
            // Basgång relativt E: D# F F(oktav) | E E E | E F F | E E E. Spela E4 i pianorullen.
            { "Pinball (Gorovich style)",
              { { kickKeyFollow, 1 }, { kickTune, 2.7f }, { kickPitch, 44.6f }, { kickPitchTime, 4.1f }, { kickPitchCurve, 3.1f },
                { kickAttack, 0.0f }, { kickHold, 0.0f }, { kickDecay, 286.0f }, { kickDecayCurve, 2.88f }, { kickClick, 0.1f },
                { kickClickLength, 1.5f }, { kickDrive, 0.25f },
                { bassShape, 1.35f }, { bassSub, 0.38f }, { bassSubOctave, 1 }, { bassOsc2Shape, 3.0f }, { bassOsc2Pw, 0.61f },
                { bassOsc2Level, 0.0f }, { bassFm, 0.05f }, { bassFmEnv, 0.87f }, { bassFilterType, 1 }, { bassCutoff, 182.0f },
                { bassResonance, 0.4f }, { bassEnvAmount, 1.44f }, { bassEnvDecay, 28.0f }, { bassVelToEnv, 0.0f }, { bassAttack, 5.2f },
                { bassDecay, 11.0f }, { bassSustain, 0.58f }, { bassRelease, 3.7f }, { bassDrive, 0.29f }, { bassLevel, 0.6f },
                { duckAmount, 0.74f },
                { "masterDrive", 3.0f }, { "masterClip", 0.15f } },
              PatternTemplate::rolling,
              { { 1, -1, false, 0.98f }, { 2, 1, false, 0.98f }, { 3, 13, false, 0.98f },
                { 5, 0, false, 0.98f }, { 6, 0, false, 0.98f }, { 7, 0, false, 0.98f },
                { 9, 0, false, 0.98f }, { 10, 1, false, 0.98f }, { 11, 1, false, 0.98f },
                { 13, 0, false, 0.98f }, { 14, 0, false, 0.98f }, { 15, 0, false, 0.98f } } },

            // Dekel - Bear Line (135,5 BPM): lång kick med svep och glid
            // (~23 halvtoner på ~15 ms) nära F#, bas som en nästan ren sinus genom distortion
            // (filter vid 40 Hz, drive 0,74), tyst i mixen. Galopp: C, sedan E. Spela E4.
            { "Bearline (Dekel style)",
              { { kickKeyFollow, 1 }, { kickTune, 2.7f }, { kickPitch, 18.7f }, { kickPitchTime, 14.6f }, { kickPitchCurve, 1.5f },
                { kickGlide, 4.5f }, { kickGlideTime, 11.0f }, { kickAttack, 1.15f }, { kickHold, 48.0f }, { kickDecay, 249.0f },
                { kickDecayCurve, 2.9f }, { kickClick, 0.25f }, { kickClickLength, 1.0f }, { kickClickTone, 5000.0f }, { kickDrive, 0.0f },
                { bassShape, 0.58f }, { bassSub, 0.35f }, { bassSubOctave, 1 }, { bassNoise, 0.04f }, { bassOsc2Shape, 0.45f },
                { bassOsc2Level, 0.0f }, { bassOsc2Semi, 12.0f }, { bassOsc2Detune, 26.3f }, { bassFm, 0.44f }, { bassFmEnv, 0.0f },
                { bassCutoff, 40.0f }, { bassResonance, 0.61f }, { bassEnvAmount, 1.16f }, { bassEnvDecay, 122.0f }, { bassVelToEnv, 0.0f },
                { bassAttack, 5.4f }, { bassDecay, 16.0f }, { bassSustain, 0.69f }, { bassRelease, 70.0f }, { bassDrive, 0.74f },
                { bassLevel, 0.2f }, { duckAmount, 0.09f },
                { "masterDrive", 2.0f }, { "masterClip", 0.15f }, { "masterRelease", 120.0f } },
              PatternTemplate::galloping,
              { { 2, 8, false, 1.0f }, { 6, 8, false, 1.0f }, { 10, 8, false, 1.0f }, { 14, 8, false, 1.0f },
                { 3, 0, false, 1.0f }, { 7, 0, false, 1.0f }, { 11, 0, false, 1.0f }, { 15, 0, false, 1.0f } } },

            // Egna presets i genrens anda. Tonerna är relativa till tonen i pianorullen.

            // Studsande trioler: såg + fyrkant två oktaver upp genom LP12 (som Bungee Jump), en liten
            // ters i slag 3 och oktaven på sista triolen (138-142 BPM).
            { "Hook Groove",
              join ({ Sound::astrixKick, Sound::hookBass,
                      { { bassLevel, 0.85f }, { duckAmount, 0.35f },
                        { "fxEchoOn", 1 }, { "fxEchoTime", 3 }, { "fxEchoFeedback", 0.3f }, { "fxEchoLowCut", 400.0f }, { "fxEchoMix", 0.1f },
                        { "masterDrive", 3.0f }, { "masterClip", 0.2f } } }),
              PatternTemplate::triplet,
              { { 1, 0, false, 0.95f }, { 2, 0, false, 0.95f }, { 4, 0, false, 0.95f }, { 5, 0, false, 0.95f },
                { 7, 0, false, 0.95f }, { 8, 3, false, 0.95f }, { 10, 0, false, 0.95f }, { 11, 12, false, 0.95f } } },

            // Galopp med ihålig kvint-oscillator: kort kvint, lång grundton (135 BPM).
            { "Midnight Gallop",
              join ({ Sound::dekelKick, Sound::dekelBass,
                      { { bassLevel, 0.7f }, { duckAmount, 0.8f },
                        { "fxReverbOn", 1 }, { "fxReverbSize", 0.35f }, { "fxReverbLowCut", 500.0f }, { "fxReverbMix", 0.08f },
                        { "masterDrive", 2.0f }, { "masterClip", 0.15f }, { "masterRelease", 120.0f } } }),
              PatternTemplate::galloping,
              { { 2, 7, false, 0.4f }, { 3, 0, false, 0.9f }, { 6, 7, false, 0.4f }, { 7, 0, false, 0.9f },
                { 10, 7, false, 0.4f }, { 11, 0, false, 0.9f }, { 14, 10, false, 0.4f }, { 15, 12, false, 0.6f } } },

            // Rullande rund bas med FM-anslag (som Pinball, ljusare), oktaver och en liten melodi i slag 4 (145 BPM).
            { "Pixel Storm",
              join ({ Sound::gorovichKick, Sound::gorovichBass,
                      { { bassLevel, 0.62f },
                        { "fxDistOn", 1 }, { "fxDistDrive", 0.2f }, { "fxDistTone", 8000.0f },
                        { "scMode", 1 }, { "scLength", 1 }, { duckAmount, 0.7f },
                        { "masterDrive", 5.0f }, { "masterClip", 0.3f } } }),
              PatternTemplate::rolling,
              { { 1, 0, false, 0.95f }, { 2, 0, false, 0.95f }, { 3, 12, false, 0.95f }, { 5, 0, false, 0.95f },
                { 6, 1, false, 0.95f }, { 7, 0, false, 0.95f }, { 9, 0, false, 0.95f }, { 10, 0, false, 0.95f },
                { 11, 12, false, 0.95f }, { 13, 3, false, 0.95f }, { 14, 1, false, 0.95f }, { 15, 0, false, 0.95f } },
              { { 0.0f, 1.0f, 0.0f }, { 0.4f, 0.9f, -0.4f }, { 1.0f, 0.0f, 0.0f } } },

            // Resonant squelch där filtret slumpas per sextondel och resonansen andas över två takter.
            { "Rubber Band",
              join ({ Sound::fullOnKick, Sound::rubberBass,
                      { { bassLevel, 0.7f }, { duckAmount, 0.85f },
                        { "lfo1On", 1 }, { "lfo1Shape", 5 }, { "lfo1Rate", 2 }, { "lfo1DestA", 1 }, { "lfo1AmountA", 0.15f },
                        { "lfo2On", 1 }, { "lfo2Shape", 0 }, { "lfo2Rate", 9 }, { "lfo2DestA", 2 }, { "lfo2AmountA", 0.12f },
                        { "masterDrive", 4.0f }, { "masterClip", 0.25f } } }),
              PatternTemplate::rolling, { { 7, 12, false }, { 15, 7, false } } },

            // Mörk fyrkant med pulsbredden i en LFO och en phaser efter gaten (148 BPM).
            { "Twilight Square",
              join ({ Sound::nightKick, Sound::squareBass,
                      { { bassLevel, 0.7f }, { duckAmount, 0.85f },
                        { "lfo1On", 1 }, { "lfo1Shape", 1 }, { "lfo1Rate", 8 }, { "lfo1DestA", 22 }, { "lfo1AmountA", 0.3f },
                        { "fxPhaserOn", 1 }, { "fxPhaserRate", 0.2f }, { "fxPhaserDepth", 0.8f }, { "fxPhaserFeedback", 0.6f },
                        { "fxPhaserMix", 0.25f },
                        { "masterDrive", 5.0f }, { "masterClip", 0.3f } } }),
              PatternTemplate::rolling, { { 15, 1, false } } },

            // Morgonljus full on: två detunade sågar och oktaver på slag 2 och 4 (142 BPM).
            { "Morning Glory",
              join ({ Sound::fullOnKick, Sound::morningBass,
                      { { bassLevel, 0.72f }, { duckAmount, 0.85f },
                        { "fxEchoOn", 1 }, { "fxEchoTime", 5 }, { "fxEchoFeedback", 0.3f }, { "fxEchoLowCut", 500.0f }, { "fxEchoMix", 0.08f },
                        { "masterDrive", 4.0f }, { "masterClip", 0.2f } } }),
              PatternTemplate::rolling, { { 7, 12, false }, { 15, 12, false } } },

            // Tung och driven, med hård distortion och kort duckkurva (146 BPM).
            { "Thunder Roll",
              join ({ Sound::snapKick, Sound::thunderBass,
                      { { bassLevel, 0.7f },
                        { "fxDistOn", 1 }, { "fxDistMode", 1 }, { "fxDistDrive", 0.3f }, { "fxDistTone", 6000.0f }, { "fxDistMix", 0.45f },
                        { "scMode", 1 }, { "scLength", 1 }, { duckAmount, 0.9f },
                        { "masterDrive", 9.0f }, { "masterClip", 0.5f }, { "masterRelease", 40.0f } } }),
              PatternTemplate::rolling, { { 3, 12, false }, { 11, 12, false } },
              { { 0.0f, 1.0f, 0.0f }, { 0.35f, 0.95f, -0.4f }, { 1.0f, 0.0f, 0.0f } } },

            // Progressiv triol med rund bas, långsamt filtersvep över fyra takter och rymd (136 BPM).
            { "Hypno Prog",
              join ({ Sound::progKick, Sound::progBass,
                      { { bassLevel, 0.72f }, { duckAmount, 0.75f },
                        { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 10 }, { "lfo1DestA", 1 }, { "lfo1AmountA", 0.3f },
                        { "fxEchoOn", 1 }, { "fxEchoTime", 6 }, { "fxEchoFeedback", 0.35f }, { "fxEchoHighCut", 3000.0f }, { "fxEchoMix", 0.1f },
                        { "fxReverbOn", 1 }, { "fxReverbSize", 0.7f }, { "fxReverbLowCut", 500.0f }, { "fxReverbMix", 0.12f },
                        { "masterDrive", 2.0f }, { "masterClip", 0.15f }, { "masterRelease", 150.0f } } }),
              PatternTemplate::triplet, { { 11, 7, false } } },

            // Acid med glid upp till oktaven i varje slag och slumpat filter per åttondel (145 BPM).
            { "Acid Slides",
              join ({ Sound::fullOnKick, Sound::acidBass,
                      { { bassLevel, 0.7f }, { duckAmount, 0.85f },
                        { "fxDistOn", 1 }, { "fxDistMode", 2 }, { "fxDistDrive", 0.35f }, { "fxDistMix", 0.4f },
                        { "lfo1On", 1 }, { "lfo1Shape", 5 }, { "lfo1Rate", 4 }, { "lfo1DestA", 1 }, { "lfo1AmountA", 0.2f },
                        { "masterDrive", 5.0f }, { "masterClip", 0.3f } } }),
              PatternTemplate::rolling,
              { { 2, 0, true }, { 3, 12, false }, { 6, 0, true }, { 7, 7, false },
                { 10, 0, true }, { 11, 12, false }, { 14, 3, true }, { 15, 0, false } } },

            // Metallisk FM (osc2 en duodecima upp) där FM-mängden faller med en LFO varje fjärdedelstriol (140 BPM).
            { "Fractal Bells",
              join ({ Sound::hookKick, Sound::bellBass,
                      { { bassLevel, 0.72f }, { duckAmount, 0.85f },
                        { "lfo1On", 1 }, { "lfo1Shape", 3 }, { "lfo1Rate", 5 }, { "lfo1DestA", 21 }, { "lfo1AmountA", 0.25f },
                        { "fxEchoOn", 1 }, { "fxEchoTime", 3 }, { "fxEchoFeedback", 0.35f }, { "fxEchoLowCut", 400.0f }, { "fxEchoMix", 0.12f },
                        { "masterDrive", 3.0f }, { "masterClip", 0.2f } } }),
              PatternTemplate::triplet, { { 2, 12, false }, { 8, 12, false }, { 11, 7, false } } },
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
            { "Long Glide (Astrix vibe)", Sound::astrixKick },
            { "Punch (Gorovich vibe)",    Sound::gorovichKick },
            { "Round (Dekel vibe)",       Sound::dekelKick },
            { "Boom (Hook vibe)",         Sound::hookKick },
            { "Modern Full On",           Sound::fullOnKick },
            { "Snap Click",               Sound::snapKick },
            { "Night Thud",               Sound::nightKick },
            { "Woody Prog",               Sound::progKick },
            { "Forest Grit",              Sound::gritKick },
            { "Sub Long Tail",            Sound::longKick },
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
              { { bassShape, 3.0f }, { bassCutoff, 150.0f }, { bassResonance, 0.55f }, { bassEnvAmount, 5.5f },
                { bassEnvDecay, 70.0f }, { bassSub, 0.2f }, { bassDrive, 0.45f }, { bassSustain, 0.3f } } },
            { "Squelchy",
              { { bassCutoff, 180.0f }, { bassResonance, 0.75f }, { bassEnvAmount, 6.0f }, { bassEnvDecay, 110.0f },
                { bassVelToEnv, 0.8f }, { bassDrive, 0.3f } } },
            { "Soft Sub",
              { { bassShape, 0.0f }, { bassSub, 0.6f }, { bassCutoff, 1200.0f }, { bassEnvAmount, 1.0f },
                { bassSustain, 0.8f }, { bassRelease, 25.0f } } },
            { "Plucky Goa",
              { { bassCutoff, 300.0f }, { bassEnvAmount, 5.0f }, { bassEnvDecay, 60.0f }, { bassDecay, 80.0f },
                { bassSustain, 0.15f }, { bassRelease, 8.0f }, { bassGlide, 40.0f } } },
            { "Fat Triangle",
              { { bassShape, 1.0f }, { bassSub, 0.4f }, { bassCutoff, 600.0f }, { bassEnvAmount, 3.0f }, { bassDrive, 0.5f } } },
            { "Saw Pluck (Hook vibe)",    Sound::hookBass },
            { "FM Knock (Gorovich vibe)", Sound::gorovichBass },
            { "Hollow Fifth (Dekel vibe)", Sound::dekelBass },
            { "Rubber Squelch",           Sound::rubberBass },
            { "Hollow Square",            Sound::squareBass },
            { "Detuned Morning",          Sound::morningBass },
            { "Thunder Grit",             Sound::thunderBass },
            { "Deep Prog Round",          Sound::progBass },
            { "Acid Roller",              Sound::acidBass },
            { "Metallic Bell FM",         Sound::bellBass },
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
                auto& step = pattern.bassStep (grid, bar * Pattern::bassStepsPerBar (grid) + tweak.step);
                step.on = true;
                step.pitch = tweak.pitch;
                step.slide = tweak.slide;
                if (tweak.gate >= 0.0f)
                    step.gate = tweak.gate;
            }
        }

        if (auto* gridParam = apvts.getParameter (ParamID::bassGrid))
            setParameter (*gridParam, gridParam->convertTo0to1 ((float) grid));
    }
}
