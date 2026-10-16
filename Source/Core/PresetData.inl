// Fabrikspresets (inkluderas av Presets.cpp, i samma namnrymd som Preset och Value).
//
// Ljuden är formade efter docs/psytrance-kick-bass-reference.md och mätta med `MyOwnFruitTool audit`
// (topp, RMS, band, stereo, kickens längd) och spektrogram. Tonerna i helpresets är relativa till tonen i
// pianorullen (E4 ger basgrundton E1 på 41 Hz; spela i låtens tempo). Värden är i parameterns egna enheter.

namespace
{
    using F = BassField;
    using Env = std::vector<NodeEnvelope::Point>;

    std::vector<Value> join (std::initializer_list<std::vector<Value>> parts)
    {
        std::vector<Value> all;
        for (const auto& part : parts)
            all.insert (all.end(), part.begin(), part.end());
        return all;
    }

    // Ett baslagers fält, och en oscillators fält (osc 0 = A, 1 = B).
    Value b (int layer, F field, float value) { return { ParamID::bass (layer, field).toStdString(), value }; }
    Value osc (int layer, int index, F aField, float value) { return b (layer, ParamID::oscField (index, aField), value); }

    // En effektplats: typ och rattarna i riktiga enheter, i tabellens ordning (saknade får standardvärdet).
    std::vector<Value> fx (int channel, int slot, EffectType type, std::initializer_list<float> real)
    {
        std::vector<Value> values;
        values.emplace_back (Effects::slotParamID (channel, slot, "Type").toStdString(), (float) (int) type);
        const auto& knobs = Effects::knobs (type);
        auto it = real.begin();
        for (size_t k = 0; k < knobs.size(); ++k)
        {
            const auto value = it != real.end() ? *it++ : knobs[k].defaultValue;
            values.emplace_back (Effects::knobParamID (channel, slot, (int) k).toStdString(), knobs[k].toNormalised (value));
        }
        return values;
    }

    // En effektkanals egna parametrar (Level, Duck, EqHp, EqLp, Eq1F ... CompThr ...).
    Value ch (int channel, const char* suffix, float value) { return { Effects::channelParamID (channel, suffix).toStdString(), value }; }

    // Kanalnummer för "skicka till": 0 = ingen, 1 = A ... 5 = E.
    constexpr float none = 0, chA = 1, chB = 2, chC = 3, chD = 4, chE = 5;

    //==========================================================================================
    // Kickar: parametrar och de två fria envelopena (pitch i halvtoner över svansen, volym 0..1).
    struct KickSound
    {
        std::vector<Value> values;
        Env pitch, amp;
    };

    Preset kickPreset (const char* name, const KickSound& sound)
    {
        Preset p { name, sound.values };
        p.extras.pitchEnv = sound.pitch;
        p.extras.ampEnv = sound.amp;
        return p;
    }

    // Klassisk full-on: tydligt klick, snabbt svep, tät kropp (~190 ms).
    KickSound kickFullOn()
    {
        return { { { kickTune, 0.5f }, { kickHarmonics, 0.12f }, { kickHarmDecay, 35.0f }, { kickClick, 0.36f }, { kickClickLength, 4.5f },
                   { kickClickTone, 6500.0f }, { kickDrive, 0.22f }, { kickClipMode, 0 }, { kickLevel, 0.85f } },
                 { { 0.0f, 38.0f, -0.65f }, { 0.010f, 17.0f, -0.45f }, { 0.035f, 4.0f, -0.3f }, { 0.085f, 0.4f, -0.2f }, { 0.120f, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0005f, 1.0f, 0.0f }, { 0.030f, 1.0f, 0.0f }, { 0.075f, 0.78f, 0.15f }, { 0.190f, 0.0f, 0.35f } } };
    }

    // Mörk skogskick: lägre svep, mer kropp och tub-mättnad, dämpat klick, lång svans (~225 ms).
    KickSound kickForest()
    {
        return { { { kickTune, -1.0f }, { kickHarmonics, 0.30f }, { kickHarmDecay, 70.0f }, { kickHarmTilt, 1.2f }, { kickClick, 0.12f },
                   { kickClickLength, 7.0f }, { kickClickTone, 2800.0f }, { kickDrive, 0.45f }, { kickClipMode, 2 }, { kickHighCut, 5500.0f },
                   { kickLevel, 0.85f } },
                 { { 0.0f, 28.0f, -0.5f }, { 0.020f, 12.0f, -0.4f }, { 0.060f, 3.0f, -0.3f }, { 0.130f, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0008f, 1.0f, 0.0f }, { 0.045f, 0.95f, 0.1f }, { 0.120f, 0.70f, 0.25f }, { 0.225f, 0.0f, 0.4f } } };
    }

    // Progressiv rund kick: långsammare svep, nästan inget klick, mjuk topp (~215 ms).
    KickSound kickProgressive()
    {
        return { { { kickTune, 0.0f }, { kickHarmonics, 0.08f }, { kickHarmDecay, 50.0f }, { kickClick, 0.10f }, { kickClickLength, 8.0f },
                   { kickClickTone, 3500.0f }, { kickDrive, 0.12f }, { kickClipMode, 0 }, { kickHighCut, 7500.0f }, { kickLevel, 0.85f } },
                 { { 0.0f, 26.0f, -0.4f }, { 0.030f, 10.0f, -0.3f }, { 0.090f, 2.5f, -0.2f }, { 0.160f, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0010f, 1.0f, 0.0f }, { 0.060f, 0.98f, 0.0f }, { 0.130f, 0.72f, 0.2f }, { 0.215f, 0.0f, 0.35f } } };
    }

    // Hi-tech: kort, högt och snabbt svep, tydligt klick (~135 ms).
    KickSound kickHiTech()
    {
        return { { { kickTune, 2.0f }, { kickHarmonics, 0.18f }, { kickHarmDecay, 25.0f }, { kickClick, 0.62f }, { kickClickLength, 3.0f },
                   { kickClickTone, 9000.0f }, { kickDrive, 0.30f }, { kickClipMode, 1 }, { kickLevel, 0.82f } },
                 { { 0.0f, 56.0f, -0.7f }, { 0.006f, 28.0f, -0.5f }, { 0.020f, 8.0f, -0.4f }, { 0.055f, 0.5f, -0.2f }, { 0.080f, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0003f, 1.0f, 0.0f }, { 0.018f, 0.9f, 0.0f }, { 0.055f, 0.62f, 0.1f }, { 0.135f, 0.0f, 0.4f } } };
    }

    // Zenonesque: punch i mellanregistret (platå i svepet), övertoner och hård mättnad (~180 ms).
    KickSound kickZenon()
    {
        return { { { kickTune, 0.0f }, { kickHarmonics, 0.45f }, { kickHarmDecay, 90.0f }, { kickHarmTilt, 0.8f }, { kickClick, 0.30f },
                   { kickClickLength, 6.0f }, { kickClickTone, 4000.0f }, { kickDrive, 0.38f }, { kickClipMode, 1 }, { kickLevel, 0.80f } },
                 { { 0.0f, 34.0f, -0.2f }, { 0.015f, 26.0f, -0.2f }, { 0.040f, 10.0f, -0.3f }, { 0.090f, 1.5f, -0.2f }, { 0.130f, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0004f, 1.0f, 0.0f }, { 0.035f, 1.0f, 0.0f }, { 0.090f, 0.80f, 0.2f }, { 0.180f, 0.0f, 0.35f } } };
    }

    // Hård psy: tung klippning, filtrerat klick, kort och aggressiv (~170 ms).
    KickSound kickHardPsy()
    {
        return { { { kickTune, 0.0f }, { kickHarmonics, 0.25f }, { kickHarmDecay, 45.0f }, { kickClick, 0.40f }, { kickClickLength, 4.0f },
                   { kickClickTone, 5000.0f }, { kickDrive, 0.75f }, { kickClipMode, 1 }, { kickHighCut, 7000.0f }, { kickLevel, 0.78f } },
                 { { 0.0f, 42.0f, -0.7f }, { 0.012f, 18.0f, -0.5f }, { 0.040f, 5.0f, -0.3f }, { 0.090f, 0.4f, -0.2f }, { 0.120f, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0004f, 1.0f, 0.0f }, { 0.025f, 1.0f, 0.0f }, { 0.070f, 0.85f, 0.1f }, { 0.170f, 0.0f, 0.4f } } };
    }

    //==========================================================================================
    // Basar (för lager `l`; bas-presets skrivs för lager 0 och flyttas dit de laddas).

    // A. Klassisk full-on-roll: såg, omedelbar amp, snabb filterenvelope (receptkort A).
    std::vector<Value> bassRolling (int l)
    {
        return { osc (l, 0, F::aTable, 12), osc (l, 0, F::aLevel, 1.0f), osc (l, 1, F::aLevel, 0.0f), b (l, F::sub, 0.18f),
                 b (l, F::filterType, 0), b (l, F::cutoff, 250.0f), b (l, F::resonance, 0.22f), b (l, F::envAmount, 3.0f),
                 b (l, F::envDecay, 45.0f), b (l, F::velToEnv, 0.5f), b (l, F::attack, 0.5f), b (l, F::decay, 120.0f),
                 b (l, F::sustain, 0.85f), b (l, F::release, 12.0f), b (l, F::drive, 0.22f), b (l, F::level, 0.72f) };
    }

    // Full-on pluck: såg och fyrkant, ljusare och med mer klick i filtret.
    std::vector<Value> bassPluck (int l)
    {
        return { osc (l, 0, F::aTable, 12), osc (l, 1, F::aTable, 13), osc (l, 1, F::aLevel, 0.45f), b (l, F::sub, 0.12f),
                 b (l, F::filterType, 0), b (l, F::cutoff, 330.0f), b (l, F::resonance, 0.28f), b (l, F::envAmount, 3.6f),
                 b (l, F::envDecay, 55.0f), b (l, F::velToEnv, 0.55f), b (l, F::attack, 0.4f), b (l, F::decay, 100.0f),
                 b (l, F::sustain, 0.75f), b (l, F::release, 10.0f), b (l, F::drive, 0.30f), b (l, F::level, 0.70f) };
    }

    // Mörk skogsbas: tabell mellan triangel och såg, fri fas, en oktav ner sinus, lågt filter och brus.
    std::vector<Value> bassForest (int l)
    {
        return { osc (l, 0, F::aTable, 0), osc (l, 0, F::aPos, 0.45f), osc (l, 0, F::aRandom, 0.6f), osc (l, 1, F::aTable, 10),
                 osc (l, 1, F::aSemi, -12.0f), osc (l, 1, F::aLevel, 0.55f), osc (l, 1, F::aRandom, 0.6f), b (l, F::sub, 0.0f),
                 b (l, F::noise, 0.04f), b (l, F::filterType, 0), b (l, F::cutoff, 150.0f), b (l, F::resonance, 0.35f),
                 b (l, F::envAmount, 2.6f), b (l, F::envDecay, 90.0f), b (l, F::velToEnv, 0.5f), b (l, F::attack, 1.0f),
                 b (l, F::decay, 160.0f), b (l, F::sustain, 0.7f), b (l, F::release, 20.0f), b (l, F::drive, 0.40f), b (l, F::level, 0.72f) };
    }

    // Hi-tech zap: FM (OSC B frekvensmodulerar OSC A) med extra FM i början av varje ton, kort filter.
    std::vector<Value> bassZap (int l)
    {
        return { osc (l, 0, F::aTable, 10), osc (l, 0, F::aWarp, 5), osc (l, 0, F::aWarpAmount, 0.55f), osc (l, 1, F::aTable, 10),
                 osc (l, 1, F::aSemi, 12.0f), osc (l, 1, F::aLevel, 0.0f), b (l, F::warpEnv, 0.35f), b (l, F::sub, 0.18f),
                 b (l, F::filterType, 1), b (l, F::cutoff, 600.0f), b (l, F::resonance, 0.25f), b (l, F::envAmount, 3.5f),
                 b (l, F::envDecay, 28.0f), b (l, F::velToEnv, 0.5f), b (l, F::attack, 0.3f), b (l, F::decay, 90.0f),
                 b (l, F::sustain, 0.6f), b (l, F::release, 8.0f), b (l, F::drive, 0.25f), b (l, F::level, 0.72f) };
    }

    // Acid: resonant såg med glid och accent (velocity öppnar filtret mer), 90-talsgoa.
    std::vector<Value> bassAcid (int l)
    {
        return { osc (l, 0, F::aTable, 12), osc (l, 1, F::aTable, 13), osc (l, 1, F::aLevel, 0.25f), osc (l, 0, F::aRandom, 0.4f),
                 b (l, F::sub, 0.1f), b (l, F::filterType, 0), b (l, F::cutoff, 210.0f), b (l, F::resonance, 0.62f), b (l, F::envAmount, 4.8f),
                 b (l, F::envDecay, 120.0f), b (l, F::velToEnv, 0.85f), b (l, F::keyTrack, 0.2f), b (l, F::attack, 0.6f),
                 b (l, F::decay, 160.0f), b (l, F::sustain, 0.9f), b (l, F::release, 30.0f), b (l, F::glide, 80.0f), b (l, F::drive, 0.38f),
                 b (l, F::level, 0.68f) };
    }

    // Bred reese: två detunade sågar med unison, mono sub i mitten, långsam filterenvelope.
    std::vector<Value> bassReese (int l)
    {
        return { osc (l, 0, F::aTable, 12), osc (l, 0, F::aUnison, 5), osc (l, 0, F::aDetune, 16.0f), osc (l, 0, F::aBlend, 0.6f),
                 osc (l, 0, F::aStereo, 0.45f), osc (l, 1, F::aTable, 12), osc (l, 1, F::aLevel, 0.6f), osc (l, 1, F::aFine, 9.0f),
                 osc (l, 1, F::aUnison, 3), osc (l, 1, F::aDetune, 10.0f), osc (l, 1, F::aStereo, 0.4f), b (l, F::sub, 0.25f),
                 b (l, F::filterType, 0), b (l, F::cutoff, 260.0f), b (l, F::resonance, 0.15f), b (l, F::envAmount, 1.8f),
                 b (l, F::envDecay, 140.0f), b (l, F::velToEnv, 0.4f), b (l, F::attack, 1.5f), b (l, F::decay, 200.0f),
                 b (l, F::sustain, 0.85f), b (l, F::release, 45.0f), b (l, F::drive, 0.25f), b (l, F::level, 0.62f) };
    }

    // En mjuk sinus-sub (lager 2 under en huvudbas), ren och mono.
    std::vector<Value> bassSub (int l)
    {
        return { osc (l, 0, F::aTable, 10), osc (l, 1, F::aLevel, 0.0f), b (l, F::sub, 0.0f), b (l, F::filterType, 1),
                 b (l, F::cutoff, 700.0f), b (l, F::resonance, 0.1f), b (l, F::envAmount, 0.0f), b (l, F::attack, 1.5f),
                 b (l, F::decay, 100.0f), b (l, F::sustain, 1.0f), b (l, F::release, 25.0f), b (l, F::drive, 0.08f), b (l, F::level, 0.6f) };
    }

    //==========================================================================================
    // Init: tre lager som låter bra ihop. Lager 1 en djup sinus-sub på offbeat, lager 2 en mellanbas i såg som rullar,
    // lager 3 en acid-linje med resonant filter där LFO 1 sveper filtret.

    // Djup sub: ren sinus med ett strå övertoner (så att den hörs i små högtalare), lågt filter, mjuk drive.
    std::vector<Value> initSub (int l)
    {
        return { osc (l, 0, F::aTable, 10), osc (l, 0, F::aLevel, 1.0f), osc (l, 1, F::aTable, 12), osc (l, 1, F::aLevel, 0.10f),
                 b (l, F::sub, 0.0f), b (l, F::filterType, 0), b (l, F::cutoff, 190.0f), b (l, F::resonance, 0.10f), b (l, F::envAmount, 1.0f),
                 b (l, F::envDecay, 70.0f), b (l, F::velToEnv, 0.3f), b (l, F::attack, 1.5f), b (l, F::decay, 200.0f), b (l, F::sustain, 0.92f),
                 b (l, F::release, 28.0f), b (l, F::drive, 0.18f), b (l, F::duck, 0.9f), b (l, F::level, 0.74f) };
    }

    // Mellanbas: en oktav upp, såg med lite unison, filter som öppnar sig vid varje ton.
    std::vector<Value> initMid (int l)
    {
        return { b (l, F::octave, 1), osc (l, 0, F::aTable, 12), osc (l, 0, F::aLevel, 1.0f), osc (l, 0, F::aUnison, 2), osc (l, 0, F::aDetune, 9.0f),
                 osc (l, 0, F::aStereo, 0.3f), osc (l, 1, F::aLevel, 0.0f), b (l, F::sub, 0.0f), b (l, F::filterType, 0), b (l, F::cutoff, 420.0f),
                 b (l, F::resonance, 0.24f), b (l, F::envAmount, 3.0f), b (l, F::envDecay, 85.0f), b (l, F::velToEnv, 0.5f), b (l, F::attack, 0.5f),
                 b (l, F::decay, 140.0f), b (l, F::sustain, 0.7f), b (l, F::release, 18.0f), b (l, F::drive, 0.30f), b (l, F::duck, 0.8f),
                 b (l, F::level, 0.56f) };
    }

    // Acid: resonant såg + fyrkant, hög envelope-mängd, accent via velocity, och en LFO som sveper filtret (se rutten nedan).
    std::vector<Value> initAcid (int l)
    {
        return { b (l, F::octave, 1), osc (l, 0, F::aTable, 12), osc (l, 0, F::aLevel, 1.0f), osc (l, 1, F::aTable, 13), osc (l, 1, F::aLevel, 0.22f),
                 b (l, F::sub, 0.0f), b (l, F::filterType, 0), b (l, F::cutoff, 360.0f), b (l, F::resonance, 0.60f), b (l, F::envAmount, 3.6f),
                 b (l, F::envDecay, 110.0f), b (l, F::velToEnv, 0.85f), b (l, F::keyTrack, 0.2f), b (l, F::attack, 0.6f), b (l, F::decay, 150.0f),
                 b (l, F::sustain, 0.8f), b (l, F::release, 30.0f), b (l, F::glide, 70.0f), b (l, F::drive, 0.45f), b (l, F::duck, 0.65f),
                 b (l, F::level, 0.45f) };
    }

    Preset initPreset()
    {
        Preset init { "Init", join ({ initSub (0), initMid (1), initAcid (2),
                                      { { "patternBars", 1 }, { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 8 } } }) };
        init.extras.pattern = {
            { 0, BassGrid::sixteenth, { ". . x:190 . . . x:190 . . . x:190 . . . x:190 .", ". . x:190 . . . x:190 . . . x:190 . . . x:190 ." }, 0.7f },
            { 1, BassGrid::sixteenth, { ". m x x . m x x . m x x . m x X", ". m x x . m x x . m x x . x m x" }, 0.7f },
            { 2, BassGrid::sixteenth, { "x . m . x+3 . x . m+7 . x . X+12 . x+10 .", "x . . m x . x+7 . m . x+3 . x . X+5 x" }, 0.7f },
        };
        init.extras.routes = { { 0, "bass3Cutoff", 0.35f } };
        return init;
    }

    Preset bassPreset (const char* name, std::vector<Value> values) { return { name, std::move (values) }; }
}

namespace
{
    const std::vector<Preset>& fullPresets()
    {
        static const std::vector<Preset> presets { initPreset() };
        return presets;
    }

    const std::vector<Preset>& kickPresets()
    {
        static const std::vector<Preset> presets {
            kickPreset ("Full-On Punch", kickFullOn()),
            kickPreset ("Dark Forest Boom", kickForest()),
            kickPreset ("Progressive Round", kickProgressive()),
            kickPreset ("Hi-Tech Click", kickHiTech()),
            kickPreset ("Zenonesque Punch", kickZenon()),
            kickPreset ("Hard Psy", kickHardPsy()),
        };
        return presets;
    }

    const std::vector<Preset>& bassPresets()
    {
        static const std::vector<Preset> presets {
            bassPreset ("Rolling Classic", bassRolling (0)),
            bassPreset ("Full-On Pluck", bassPluck (0)),
            bassPreset ("Dark Forest", bassForest (0)),
            bassPreset ("Hi-Tech Zap", bassZap (0)),
            bassPreset ("Acid Squelch", bassAcid (0)),
            bassPreset ("Wide Reese", bassReese (0)),
            bassPreset ("Sub Sine", bassSub (0)),
        };
        return presets;
    }

    const std::vector<Preset>& effectChainPresets()
    {
        static const std::vector<Preset> presets { { "Default (all off)", {} } };
        return presets;
    }
}
