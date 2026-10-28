// Fabrikspresets (inkluderas av Presets.cpp, i samma namnrymd som Preset och Value).
//
// Ljuden är formade efter docs/psytrance-kick-bass-reference.md och mätta med `KickbaseTool audit`
// (topp, RMS, band, stereo, kickens längd) och spektrogram. Tonerna i helpresets är relativa till tonen i
// pianorullen (E4 ger basgrundton E1 på 41 Hz; spela i låtens tempo). Värden är i parameterns egna enheter.

namespace
{
    using F = InstrumentField;
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

    // D7: rena psytrance-kickar (svans ~45 Hz via tune, lite mättnad). Envelopen byggs av svepstart (halvtoner), svepets längd och totallängd (s).
    KickSound cleanKick (float tune, float harmonics, float click, float clickTone, float drive, float clipMode, float sweep, float sweepTime, float length)
    {
        return { { { kickTune, tune }, { kickHarmonics, harmonics }, { kickHarmDecay, 40.0f }, { kickClick, click }, { kickClickLength, 4.5f },
                   { kickClickTone, clickTone }, { kickDrive, drive }, { kickClipMode, clipMode }, { kickLevel, 0.85f } },
                 { { 0.0f, sweep, -0.65f }, { sweepTime * 0.15f, sweep * 0.45f, -0.45f }, { sweepTime * 0.4f, sweep * 0.1f, -0.3f }, { sweepTime, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { 0.0005f, 1.0f, 0.0f }, { length * 0.16f, 1.0f, 0.0f }, { length * 0.4f, 0.78f, 0.15f }, { length, 0.0f, 0.35f } } };
    }

    //==========================================================================================
    // Basar (för lager `l`; bas-presets skrivs för lager 0 och flyttas dit de laddas).

    // D7: rullande bas-varianter. Såg (eller annan tabell) på OSC A, valfri OSC B, snabb filterenvelope och omedelbar amp.
    std::vector<Value> rollBass (int l, float tableA, float tableB, float levelB, float sub, float cutoff, float resonance, float envAmount,
                                 float envDecay, float sustain, float drive, float level)
    {
        return { osc (l, 0, F::aTable, tableA), osc (l, 0, F::aLevel, 1.0f), osc (l, 1, F::aTable, tableB), osc (l, 1, F::aLevel, levelB),
                 b (l, F::sub, sub), b (l, F::filterType, 0), b (l, F::cutoff, cutoff), b (l, F::resonance, resonance), b (l, F::envAmount, envAmount),
                 b (l, F::envDecay, envDecay), b (l, F::velToEnv, 0.5f), b (l, F::attack, 0.5f), b (l, F::decay, 120.0f), b (l, F::sustain, sustain),
                 b (l, F::release, 12.0f), b (l, F::drive, drive), b (l, F::level, level) };
    }

    // Arbetare 2, PSY ROLLING BASS: Saw Soft (15) på OSC A och Psy Roll/Sub Punch/Acid (94/95/99) på OSC B ger mellanregistret (250 Hz-1 kHz)
    // som rollBass saknar; positionerna styr övertonerna (docs/school/wavetables.md). Fas nollställs, random 0, mono (voices 1).
    std::vector<Value> psyRollBass (int l, float posA, float tableB, float posB, float levelB, float semiB, float sub, float cutoff, float resonance,
                                    float envAmount, float envDecay, float sustain, float ampDecay, float release, float drive, float level, float filterModel)
    {
        return { osc (l, 0, F::aTable, 15), osc (l, 0, F::aPos, posA), osc (l, 0, F::aLevel, 1.0f), osc (l, 1, F::aTable, tableB),
                 osc (l, 1, F::aPos, posB), osc (l, 1, F::aLevel, levelB), osc (l, 1, F::aSemi, semiB), b (l, F::sub, sub),
                 b (l, F::filterType, 0), b (l, F::filterModel, filterModel), b (l, F::cutoff, cutoff), b (l, F::resonance, resonance),
                 b (l, F::envAmount, envAmount), b (l, F::envDecay, envDecay), b (l, F::velToEnv, 0.5f), b (l, F::attack, 0.5f),
                 b (l, F::decay, ampDecay), b (l, F::sustain, sustain), b (l, F::release, release), b (l, F::drive, drive), b (l, F::level, level) };
    }

    // Arbetare 2, MID BASS / HI BASS / SYNTH: ett lager (bassPreset). Oktav över rotonen, såg (OSC A, tabell 15 Saw Soft eller 12 Saw) plus en andra tabell
    // på OSC B, unison för bredd. Se docs/school/ för hur oktav, filter och envelope formar registret.
    std::vector<Value> registerBass (int l, int octave, float tableA, float tableB, float levelB, float semiB, float unison, float detune, float cutoff,
                                     float resonance, float envAmount, float envDecay, float sustain, float ampDecay, float release, float drive,
                                     float level, float filterModel)
    {
        return { b (l, F::octave, (float) octave), osc (l, 0, F::aTable, tableA), osc (l, 0, F::aLevel, 1.0f), osc (l, 0, F::aUnison, unison),
                 osc (l, 0, F::aDetune, detune), osc (l, 0, F::aStereo, 0.3f), osc (l, 1, F::aTable, tableB), osc (l, 1, F::aLevel, levelB),
                 osc (l, 1, F::aSemi, semiB), b (l, F::sub, 0.0f), b (l, F::filterType, 0), b (l, F::filterModel, filterModel), b (l, F::cutoff, cutoff),
                 b (l, F::resonance, resonance), b (l, F::envAmount, envAmount), b (l, F::envDecay, envDecay), b (l, F::velToEnv, 0.5f),
                 b (l, F::attack, 0.5f), b (l, F::decay, ampDecay), b (l, F::sustain, sustain), b (l, F::release, release), b (l, F::drive, drive),
                 b (l, F::level, level) };
    }

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
            { 0, InstrumentGrid::sixteenth, { ". . x:190 . . . x:190 . . . x:190 . . . x:190 .", ". . x:190 . . . x:190 . . . x:190 . . . x:190 ." }, 0.7f },
            { 1, InstrumentGrid::sixteenth, { ". m x x . m x x . m x x . m x X", ". m x x . m x x . m x x . x m x" }, 0.7f },
            { 2, InstrumentGrid::sixteenth, { "x . m . x+3 . x . m+7 . x . X+12 . x+10 .", "x . . m x . x+7 . m . x+3 . x . X+5 x" }, 0.7f },
        };
        init.extras.routes = { { 0, "bass3Cutoff", 0.35f } };
        return init;
    }

    // Basic Morph: en bas i tabellen Basic (sinus, triangel, såg, fyrkant) med Morph AV. LFO 1 (en såg, en takt) sveper POS från 0 till 1, så att
    // vågformen HOPPAR mellan de fyra formerna under takten i stället för att tona över. Sätt Morph på (MORPH-knappen i OSC 1) för att höra skillnaden.
    Preset basicMorphPreset()
    {
        constexpr int l = 0;
        Preset preset { "Basic Morph", join ({ std::vector<Value> {
                 { ParamID::instrumentCount, 1 }, { "patternBars", 1 }, { "lfo1On", 1 }, { "lfo1Shape", 2 }, { "lfo1Rate", 8 },
                 osc (l, 0, F::aTable, 0), osc (l, 0, F::aPos, 0.5f), osc (l, 0, F::aLevel, 1.0f), osc (l, 1, F::aLevel, 0.0f),
                 b (l, ParamID::oscMorphField (0), 0.0f), b (l, F::sub, 0.0f), b (l, F::filterType, 0), b (l, F::cutoff, 2400.0f),
                 b (l, F::resonance, 0.15f), b (l, F::envAmount, 1.2f), b (l, F::envDecay, 140.0f), b (l, F::velToEnv, 0.4f), b (l, F::attack, 0.5f),
                 b (l, F::decay, 160.0f), b (l, F::sustain, 0.85f), b (l, F::release, 35.0f), b (l, F::drive, 0.12f), b (l, F::duck, 0.8f),
                 b (l, F::level, 0.6f) } }) };
        preset.extras.pattern = {
            { 0, InstrumentGrid::sixteenth, { ". x x x . x x x . x x x . x x x" }, 0.7f },
        };
        preset.extras.routes = { { 0, "bass1APos", 0.5f } };
        return preset;
    }

    Preset bassPreset (const char* name, std::vector<Value> values) { return { name, std::move (values) }; }

    //==========================================================================================
    // "newest 1-10": hela huvudpresets (kick + bas + FX + mönster). Kicken kommer från mallen (en kick på varje fjärdedel),
    // basens mönster skrivs här. Lager över det första ligger en oktav upp. FX-kanal A/B används som SEND (torrt orört).

    std::vector<Value> sendTo (int l, int channelChoice, float amount)
    {
        return { b (l, F::fxChannel, (float) channelChoice), b (l, F::fxSend, 1), b (l, F::fxAmount, amount) };
    }

    using Line = Presets::Extras::PatternLine;

    Preset newest (const char* name, const KickSound& kick, std::vector<Value> layers, std::vector<Value> fxValues,
                   std::vector<Line> lines, int instruments, int bars)
    {
        Preset p { name, join ({ kick.values, layers, fxValues,
                                 { { ParamID::instrumentCount, (float) instruments }, { "patternBars", (float) bars } } }) };
        p.extras.pitchEnv = kick.pitch;
        p.extras.ampEnv = kick.amp;
        p.extras.pattern = std::move (lines);
        return p;
    }

    // Rullande figur: tre sextondelar efter varje kick.
    constexpr const char* rollBar = ". x x x . x x x . x x x . x x x";

    // 1. Full-on: klassisk rullande såg, parallell mjuk distorsion.
    Preset newest1()
    {
        return newest ("newest 1", kickFullOn(),
                       join ({ rollBass (0, 12, 12, 0.0f, 0.15f, 200.0f, 0.18f, 2.6f, 35.0f, 0.80f, 0.18f, 0.72f), sendTo (0, 1, 0.20f) }),
                       join ({ fx (0, 0, EffectType::distortion, { 0, 0.30f, 5000.0f, 1.0f }), { ch (0, "Duck", 0.8f), ch (0, "EqHp", 120.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x x . x x x . x x x . x x X" }, 0.7f } }, 1, 1);
    }

    // 2. Dark forest: lång skogskick, mörk bas med brus och ett mörkt rum.
    Preset newest2()
    {
        return newest ("newest 2", kickForest(),
                       join ({ bassForest (0), sendTo (0, 1, 0.18f) }),
                       join ({ fx (0, 0, EffectType::reverb, { 0.7f, 0.7f, 1.0f, 400.0f, 1.0f, 20.0f }), { ch (0, "Duck", 0.9f), ch (0, "EqHp", 250.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x m . x x x . m x . . x x x", ". x x x . x m x . x x+3 . . x x-2 x" }, 0.7f } }, 1, 2);
    }

    // 3. Progressive: rund kick, bred reese på offbeat-åttondelar, eko på sidan.
    Preset newest3()
    {
        return newest ("newest 3", kickProgressive(),
                       join ({ bassReese (0), sendTo (0, 1, 0.22f) }),
                       join ({ fx (0, 0, EffectType::echo, { 5, 0, 0, 0.5f, 2, 0.35f, 4500.0f, 0.0f, 0, 0.6f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 200.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". . x:180 . . . x:180 . . . x:180 . . . x:180 ." }, 0.7f } }, 1, 1);
    }

    // 4. Hi-tech: kort snabb kick, FM-zap med oktavhopp, phaser.
    Preset newest4()
    {
        return newest ("newest 4", kickHiTech(),
                       join ({ bassZap (0), { b (0, F::cutoff, 380.0f), b (0, F::level, 0.50f) }, sendTo (0, 1, 0.25f) }),
                       join ({ fx (0, 0, EffectType::phaser, { 0.3f, 0.6f, 900.0f, 0.4f, 0.4f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 180.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x x . x x+12 x . x x x . x+12 x x" }, 0.6f } }, 1, 1);
    }

    // 5. Zenonesque: punch i mellanregistret, hård distorsion med högpassad parallell.
    Preset newest5()
    {
        return newest ("newest 5", kickZenon(),
                       join ({ bassPluck (0), sendTo (0, 1, 0.25f) }),
                       join ({ fx (0, 0, EffectType::distortion, { 1, 0.45f, 5000.0f, 1.0f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 150.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x x . x x x . x x+12 x . x+7 x x" }, 0.7f } }, 1, 1);
    }

    // 6. Hårt och mörkt: hård kick, tung klippt bas, vikt distorsion.
    Preset newest6()
    {
        return newest ("newest 6", kickHardPsy(),
                       join ({ rollBass (0, 12, 13, 0.30f, 0.10f, 260.0f, 0.30f, 3.5f, 30.0f, 0.75f, 0.50f, 0.64f), sendTo (0, 1, 0.20f) }),
                       join ({ fx (0, 0, EffectType::distortion, { 2, 0.50f, 3500.0f, 0.8f }), { ch (0, "Duck", 0.9f), ch (0, "EqHp", 200.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". X x x . X x x . X x x . X x x", ". X x x . X x x . X x-5 x . X x x" }, 0.7f } }, 1, 2);
    }

    // 7. Acid goa: ren kick, resonant acid med glid, LFO 1 på filtret, eko.
    Preset newest7()
    {
        Preset p = newest ("newest 7", cleanKick (0.5f, 0.12f, 0.30f, 5000.0f, 0.16f, 0, 38.0f, 0.10f, 0.18f),
                           join ({ bassAcid (0), sendTo (0, 1, 0.20f),
                                   std::vector<Value> { { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 8 } } }),
                           join ({ fx (0, 0, EffectType::echo, { 5, 0, 0, 0.5f, 2, 0.30f, 5000.0f, 0.0f, 0, 0.5f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 220.0f) } }),
                           { { 0, InstrumentGrid::sixteenth, { "x . m x+3~ . x . m+7 . x . X+12 . x+10 .", "x . . m x . x+7~ . m . x+3 . x . X+5 x" }, 0.7f } }, 1, 2);
        p.extras.routes = { { 0, "bass1Cutoff", 0.25f } };
        return p;
    }

    // 8. Deep reese: lång sub på offbeat plus bred reese som rullar, chorus på reesen.
    Preset newest8()
    {
        return newest ("newest 8", cleanKick (-1.0f, 0.10f, 0.18f, 3500.0f, 0.10f, 0, 30.0f, 0.14f, 0.23f),
                       join ({ initSub (0), bassReese (1), { b (1, F::octave, 1), b (1, F::level, 0.40f) }, sendTo (1, 1, 0.30f) }),
                       join ({ fx (0, 0, EffectType::chorus, { 0.4f, 0.5f, 10.0f, 0.0f, 0.4f }), { ch (0, "Duck", 0.8f), ch (0, "EqHp", 250.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". . x:190 . . . x:190 . . . x:190 . . . x:190 ." }, 0.7f },
                         { 1, InstrumentGrid::sixteenth, { ". m x x . m x x . m x x . m x X" }, 0.7f } }, 2, 1);
    }

    // 9. Minimal natt: gles figur, djup bas, litet rum.
    Preset newest9()
    {
        return newest ("newest 9", cleanKick (-0.5f, 0.06f, 0.10f, 3000.0f, 0.08f, 0, 28.0f, 0.15f, 0.22f),
                       join ({ rollBass (0, 12, 10, 0.0f, 0.30f, 170.0f, 0.20f, 2.4f, 50.0f, 0.88f, 0.20f, 0.72f), sendTo (0, 1, 0.12f) }),
                       join ({ fx (0, 0, EffectType::reverb, { 0.4f, 0.8f, 0.8f, 500.0f, 1.0f, 10.0f }), { ch (0, "Duck", 0.9f), ch (0, "EqHp", 300.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". . x . . . . x . . x . . . x .", ". . x . . . x . . . . x . . x x+7" }, 0.7f } }, 1, 2);
    }

    // 10. Skymning: tre lager (sub, rullande mellanbas, acid) och LFO 1 på acidens filter.
    Preset newest10()
    {
        Preset p = newest ("newest 10", cleanKick (0.0f, 0.10f, 0.30f, 6000.0f, 0.12f, 0, 36.0f, 0.11f, 0.19f),
                           join ({ bassSub (0), rollBass (1, 12, 12, 0.0f, 0.0f, 420.0f, 0.24f, 3.0f, 85.0f, 0.70f, 0.28f, 0.50f),
                                   { b (1, F::octave, 1), b (1, F::duck, 0.8f) },
                                   initAcid (2), { b (2, F::level, 0.40f) }, sendTo (2, 1, 0.20f),
                                   std::vector<Value> { { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 8 } } }),
                           join ({ fx (0, 0, EffectType::echo, { 5, 0, 0, 0.5f, 2, 0.30f, 5000.0f, 0.0f, 0, 0.5f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 250.0f) } }),
                           { { 0, InstrumentGrid::sixteenth, { ". . x:190 . . . x:190 . . . x:190 . . . x:190 ." }, 0.7f },
                             { 1, InstrumentGrid::sixteenth, { ". m x x . m x x . m x x . m x X" }, 0.7f },
                             { 2, InstrumentGrid::sixteenth, { "x . m . x+3 . x . m+7 . x . X+12 . x+10 ." }, 0.7f } }, 3, 1);
        p.extras.routes = { { 0, "bass3Cutoff", 0.30f } };
        return p;
    }
}

namespace
{
    Preset psyRolling1();
    Preset psyRolling2();
    Preset psyRolling3();
    Preset psyRolling4();
    Preset glideBass1();
    Preset glideBass2();
    Preset glideBass3();
    Preset duckDemo();
    Preset newInit1();
    Preset newInit2();
    Preset newInit3();
    Preset newInit4();
    Preset newInit5();

    const std::vector<Preset>& fullPresets()
    {
        // Empty ligger sist: rendersig-index och hashar för de andra är oförändrade.
        static const std::vector<Preset> presets { initPreset(), basicMorphPreset(),
                                                   { "Empty", { { ParamID::instrumentCount, 1 }, { "patternBars", 1 } }, PatternTemplate::clear },
                                                   // "newest 1-10" sist (efter Empty) så att äldre index och hashar är oförändrade.
                                                   newest1(), newest2(), newest3(), newest4(), newest5(),
                                                   newest6(), newest7(), newest8(), newest9(), newest10(),
                                                   // Arbetare 2, kategori psyRollingCategory.
                                                   psyRolling1(), psyRolling2(), psyRolling3(), psyRolling4(),
                                                   // Arbetare 2, kategori glideBassesCategory.
                                                   glideBass1(), glideBass2(), glideBass3(),
                                                   // Arbetare 2: demo av FUNDAMENTAL DUCK (basen krockar med kickens grundton).
                                                   duckDemo(),
                                                   // Arbetare 2: New Init 1-5, varianter av fabriks-Init (samma lagerljud).
                                                   newInit1(), newInit2(), newInit3(), newInit4(), newInit5() };
        return presets;
    }

    // Arbetare 2: kickar i stil med TK_Kick Psy140 Clean G. Ren sinus (inga övertoner, klick, drive), brant svep (70 halvtoner på ~120 ms,
    // hårt framtungt) och nästan platt volym till strax före slutet. Tonen är Fixed (C1 + tune), så tailHz = 32.7 * 2^(tune/12) oberoende av tangent.
    // Kategorin hålls här på ett ställe; fabrikspresets får kategori via Arbetare 1:s namn-till-kategori-tabell.
    constexpr const char* psyCleanCategory = "PSY CLEAN";
    constexpr const char* psyRollingBassCategory = "PSY ROLLING BASS"; // basar Full-On Roll, Dekel Groove, Forest 1/32, Wide Mid

    // tone: halvtoner över C1. sweepScale/timeScale skalar Psy140:s svep, length är totallängd (s), attack är stigtiden (s).
    KickSound psyCleanKick (float tone, float sweepScale, float timeScale, float length, float attack, float level, float click, float drive)
    {
        const float s = sweepScale, t = timeScale;
        return { { { kickKeyFollow, 0 }, { kickTune, tone }, { kickHarmonics, 0.0f }, { kickClick, click }, { kickClickLength, 3.0f },
                   { kickClickTone, 5000.0f }, { kickDrive, drive }, { kickClipMode, 0 }, { kickLevel, level } },
                 { { 0.0f, 70.0f * s, 0.0f }, { 0.002f * t, 50.1f * s, 0.0f }, { 0.004f * t, 42.3f * s, 0.0f }, { 0.006f * t, 38.8f * s, 0.0f },
                   { 0.010f * t, 33.5f * s, 0.0f }, { 0.015f * t, 26.3f * s, 0.0f }, { 0.020f * t, 21.1f * s, 0.0f }, { 0.030f * t, 17.1f * s, 0.0f },
                   { 0.040f * t, 14.1f * s, 0.0f }, { 0.050f * t, 11.4f * s, 0.0f }, { 0.070f * t, 6.8f * s, 0.0f }, { 0.090f * t, 3.1f * s, 0.0f },
                   { 0.120f * t, 0.0f, 0.0f }, { 0.150f * t, 0.0f, 0.0f } },
                 { { 0.0f, 0.0f, 0.0f }, { attack, 1.0f, 0.0f }, { length - 0.03f, 0.9f, 0.0f }, { length, 0.0f, 0.3f } } };
    }

    // FUNDAMENTAL DUCK för hela instrumentet (källa Kick): basens grundton sänks medan kicken klingar. Se docs/school/fundamental-duck.md.
    std::vector<Value> fundDuck (float depthDb, float windowCents)
    {
        return { { ParamID::fundamentalDuckOn, 1 }, { ParamID::fundamentalDuckDepth, depthDb }, { ParamID::fundamentalDuckWindow, windowCents } };
    }

    // Arbetare 2, PSY ROLLING: huvudpresets med PSY CLEAN-kick + ny rollbas, sidechain (Duck) och en FX-send. Ett lager (instrumentCount 1),
    // så lager 1-2 är avstängda och ljudet är det som mäts.
    constexpr const char* psyRollingCategory = "PSY ROLLING";

    // 1. Full-On Roll: ren C-kick, klassisk rull med oktavhopp, parallell mjuk distorsion.
    Preset psyRolling1()
    {
        return newest ("Roll Full-On", psyCleanKick (0.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 0.95f, 94, 0.60f, 1.00f, 12.0f, 0.10f, 1200.0f, 0.20f, 3.0f, 45.0f, 0.40f, 90.0f, 25.0f, 0.15f, 0.55f, 0),
                               sendTo (0, 1, 0.20f), fundDuck (9.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::distortion, { 0, 0.30f, 5000.0f, 1.0f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 150.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x x . x x x . x x x . x x X", ". x x x . x x x . x x+12 x . x x x",
                                                           ". x x x . x x x . x x x . x+3 x x", ". x x x . x x+12 x . x x x . x x X" }, 0.7f } }, 1, 4);
    }

    // 2. Roll Wide: D-kick, bred mellanbas, chorus på sidan.
    Preset psyRolling2()
    {
        return newest ("Roll Wide", psyCleanKick (2.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 1.00f, 94, 1.00f, 1.00f, 12.0f, 0.10f, 1500.0f, 0.18f, 2.0f, 70.0f, 0.40f, 100.0f, 25.0f, 0.05f, 0.52f, 0),
                               sendTo (0, 1, 0.25f), fundDuck (9.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::chorus, { 0.4f, 0.5f, 10.0f, 0.0f, 0.4f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 200.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x x . x x x . x x x . x x X", ". x x x . x x x . x x-2 x . x x x" }, 0.7f } }, 1, 2);
    }

    // 3. Roll Dekel: mjuk F-kick, mörk dekelbas med eko.
    Preset psyRolling3()
    {
        return newest ("Roll Dekel", psyCleanKick (5.0f, 0.8f, 1.2f, 0.20f, 0.003f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 1.00f, 94, 0.80f, 1.00f, 0.0f, 0.10f, 1500.0f, 0.15f, 3.0f, 60.0f, 0.35f, 100.0f, 25.0f, 0.10f, 0.58f, 0),
                               sendTo (0, 1, 0.22f), fundDuck (9.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::echo, { 5, 0, 0, 0.5f, 2, 0.30f, 4500.0f, 0.0f, 0, 0.5f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 220.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x m . x x x . x m x . x x X", ". x x x . x m x . x x x . x+3 x x-2" }, 0.7f } }, 1, 2);
    }

    // 4. Roll Forest: A-kick med klick, acidlik skogsbas (TB-303-filter), litet rum.
    Preset psyRolling4()
    {
        return newest ("Roll Forest", psyCleanKick (9.0f, 1.0f, 0.85f, 0.17f, 0.0005f, 0.85f, 0.10f, 0.05f),
                       join ({ psyRollBass (0, 1.00f, 99, 0.90f, 1.00f, 0.0f, 0.10f, 2200.0f, 0.45f, 3.0f, 30.0f, 0.30f, 50.0f, 20.0f, 0.15f, 0.50f, 1),
                               sendTo (0, 1, 0.18f), fundDuck (9.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::reverb, { 0.5f, 0.7f, 1.0f, 400.0f, 1.0f, 20.0f }), { ch (0, "Duck", 0.9f), ch (0, "EqHp", 250.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x m . x x x . m x . . x x x", ". x x x . x m x . x x+3 . . x x-2 x" }, 0.7f } }, 1, 2);
    }

    // Arbetare 2, GLIDE BASSES: slide-toner (~) glider från föregående tons höjd till sin egen under sin längd (:procent av ett steg),
    // så glidlängden sätts av tonens längd (F::glide används inte). Se docs/school/glide-bass.md. Lång amp-decay och hög sustain så att glidet hörs.
    constexpr const char* glideBassesCategory = "GLIDE BASSES";

    // 1. Glide Sub: mjuk sub-bas, hopp och glid uppåt/nedåt mellan rotton, kvint och oktav.
    Preset glideBass1()
    {
        return newest ("Glide Sub", psyCleanKick (0.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 0.30f, 95, 0.50f, 0.60f, 0.0f, 0.25f, 700.0f, 0.15f, 2.0f, 120.0f, 0.80f, 400.0f, 40.0f, 0.15f, 0.50f, 0),
                               sendTo (0, 1, 0.15f), fundDuck (12.0f, 250.0f) }),
                       join ({ fx (0, 0, EffectType::distortion, { 0, 0.20f, 5000.0f, 1.0f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 150.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { "X . . . . . x . x+7~:200 . . . x+12~:100 . . .", "x . . . . . x . x+5~:200 . . . x-5~:200 . . ." }, 0.9f } }, 1, 2);
    }

    // 2. Glide Acid: TB-303-filter och Psy Acid, korta slides mellan sextondelar.
    Preset glideBass2()
    {
        return newest ("Glide Acid", psyCleanKick (2.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 1.00f, 99, 0.90f, 1.00f, 0.0f, 0.10f, 1800.0f, 0.40f, 3.0f, 90.0f, 0.60f, 200.0f, 25.0f, 0.15f, 0.52f, 1),
                               sendTo (0, 1, 0.18f), fundDuck (9.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::echo, { 5, 0, 0, 0.5f, 2, 0.30f, 4500.0f, 0.0f, 0, 0.5f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 200.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { ". x x . x+3~ x . x . x+7~ . x x-2~ . x X", ". x x . x+5~ x . x . x+12~ . x x+7~ . x X" }, 0.7f } }, 1, 2);
    }

    // 3. Glide Dive: långa oktavfall och uppslid, mörk och bred mellanregister-bas.
    Preset glideBass3()
    {
        return newest ("Glide Dive", psyCleanKick (5.0f, 0.8f, 1.2f, 0.20f, 0.003f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 0.60f, 94, 0.70f, 0.80f, 0.0f, 0.15f, 1200.0f, 0.20f, 2.0f, 100.0f, 0.70f, 300.0f, 35.0f, 0.20f, 0.55f, 0),
                               sendTo (0, 1, 0.20f), fundDuck (9.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::chorus, { 0.4f, 0.5f, 10.0f, 0.0f, 0.4f }), { ch (0, "Duck", 0.85f), ch (0, "EqHp", 180.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { "X . . . x+12 . . . x+12~:200 . . . x~:300 . . .", "x . . x . . x+7 . x+7~:100 . x+12~:100 . x+0~:300 . . ." }, 0.8f } }, 1, 2);
    }

    // Duck Demo: visar varför FUNDAMENTAL DUCK finns. Kickens grundton är D1 (ton 2, ca 36,7 Hz). Basens långa toner ligger en halvton
    // bredvid (x+1 = ±100 cent mot kicken, x+3, x-1): grundtonerna ligger nära varandra men inte i samklang, så de svävar och tar ut varandra.
    // Duck står på med 12 dB och fönster 200 cent: grundtonen dippas bara när kicken ljuder. Stäng av FUNDAMENTAL DUCK och lyssna på skillnaden.
    Preset duckDemo()
    {
        return newest ("Duck Demo", psyCleanKick (2.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f),
                       join ({ psyRollBass (0, 0.30f, 95, 0.50f, 0.60f, 0.0f, 0.25f, 700.0f, 0.15f, 2.0f, 120.0f, 0.90f, 600.0f, 40.0f, 0.10f, 0.55f, 0),
                               sendTo (0, 1, 0.0f), fundDuck (12.0f, 200.0f) }),
                       join ({ fx (0, 0, EffectType::distortion, { 0, 0.10f, 5000.0f, 1.0f }), { ch (0, "Duck", 0.0f), ch (0, "EqHp", 20.0f) } }),
                       { { 0, InstrumentGrid::sixteenth, { "x+1 . . . . . . . x+3 . . . . . . .", "x-1 . . . . . . . x+1 . . . . . . ." }, 0.9f } }, 1, 2);
    }

    // New Init 1-5: startpunkter byggda på fabriks-Init (initSub, initMid, initAcid). Init själv är oförändrad.
    // 1. Init med FUNDAMENTAL DUCK på: alla tre lager viker för kickens grundton.
    Preset newInit1()
    {
        auto p = initPreset();
        p.name = "New Init 1";
        for (int layer = 0; layer < 3; ++layer)
            p.values.push_back ({ ParamID::bass (layer, F::duck).toStdString(), layer == 0 ? 0.9f : 0.8f });
        for (const auto& v : fundDuck (9.0f, 200.0f))
            p.values.push_back (v);
        return p;
    }

    // 2. Halftime: sub och acid i två lager, glesa mönster över två takter.
    Preset newInit2()
    {
        Preset p { "New Init 2", join ({ initSub (0), initAcid (1), { { ParamID::instrumentCount, 2 }, { "patternBars", 2 }, { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 4 } } }) };
        p.extras.pattern = {
            { 0, InstrumentGrid::sixteenth, { ". . x:380 . . . . . . . x:380 . . . . .", ". . x:380 . . . . . . . x:190 . x:190 . ." }, 0.7f },
            { 1, InstrumentGrid::sixteenth, { ". . . . x+3 . . . . . . . X+7 . . .", ". . . . x+5 . . . . . . . X+12 . x+10 ." }, 0.7f } };
        p.extras.routes = { { 0, "bass2Cutoff", 0.35f } };
        return p;
    }

    // 3. Bara sub: ett lager, offbeat, den renaste startpunkten.
    Preset newInit3()
    {
        Preset p { "New Init 3", join ({ initSub (0), { { ParamID::instrumentCount, 1 }, { "patternBars", 1 } } }) };
        p.extras.pattern = { { 0, InstrumentGrid::sixteenth, { ". . x:190 . . . x:190 . . . x:190 . . . x:190 ." }, 0.7f } };
        return p;
    }

    // 4. Rullande: sub och mellanbas, mellanbasen fyller sextondelarna mellan kickarna.
    Preset newInit4()
    {
        Preset p { "New Init 4", join ({ initSub (0), initMid (1), { { ParamID::instrumentCount, 2 }, { "patternBars", 1 } } }) };
        p.extras.pattern = {
            { 0, InstrumentGrid::sixteenth, { ". . x:190 . . . x:190 . . . x:190 . . . x:190 ." }, 0.7f },
            { 1, InstrumentGrid::sixteenth, { ". x x x . x x x . x x x . x x X" }, 0.7f } };
        return p;
    }

    // 5. Acid-duo: mellanbas och acid, LFO 1 sveper acidens filter över två takter.
    Preset newInit5()
    {
        Preset p { "New Init 5", join ({ initMid (0), initAcid (1), { { ParamID::instrumentCount, 2 }, { "patternBars", 2 }, { "lfo1On", 1 }, { "lfo1Shape", 0 }, { "lfo1Rate", 4 } } }) };
        p.extras.pattern = {
            { 0, InstrumentGrid::sixteenth, { ". m x x . m x x . m x x . m x X", ". m x x . m x x . m x x . x m x" }, 0.7f },
            { 1, InstrumentGrid::sixteenth, { "x . m . x+3 . x . m+7 . x . X+12 . x+10 .", "x . . m x . x+7 . m . x+3 . x . X+5 x" }, 0.7f } };
        p.extras.routes = { { 0, "bass2Cutoff", 0.4f } };
        return p;
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
            // D7: rena psytrance-kickar, sist så att äldre index och hashar är oförändrade.
            kickPreset ("Clean Full-On", cleanKick (0.0f, 0.10f, 0.30f, 6000.0f, 0.12f, 0, 36.0f, 0.11f, 0.19f)),
            kickPreset ("Clean Tight", cleanKick (1.0f, 0.08f, 0.35f, 7000.0f, 0.10f, 0, 40.0f, 0.08f, 0.15f)),
            kickPreset ("Clean Deep", cleanKick (-1.0f, 0.10f, 0.18f, 3500.0f, 0.10f, 0, 30.0f, 0.14f, 0.23f)),
            kickPreset ("Clean Round", cleanKick (0.0f, 0.06f, 0.12f, 3000.0f, 0.08f, 0, 28.0f, 0.15f, 0.21f)),
            kickPreset ("Clean Punchy", cleanKick (0.5f, 0.15f, 0.45f, 5500.0f, 0.18f, 0, 40.0f, 0.10f, 0.17f)),
            kickPreset ("Clean Long", cleanKick (-0.5f, 0.08f, 0.20f, 4000.0f, 0.10f, 0, 32.0f, 0.14f, 0.26f)),
            kickPreset ("Clean Snappy", cleanKick (1.5f, 0.12f, 0.55f, 8000.0f, 0.15f, 1, 46.0f, 0.07f, 0.13f)),
            kickPreset ("Clean Warm", cleanKick (0.0f, 0.20f, 0.15f, 3000.0f, 0.20f, 2, 30.0f, 0.12f, 0.20f)),
            kickPreset ("Clean Sub", cleanKick (-1.5f, 0.04f, 0.10f, 2500.0f, 0.05f, 0, 26.0f, 0.16f, 0.24f)),
            kickPreset ("Clean Modern", cleanKick (0.5f, 0.14f, 0.40f, 6500.0f, 0.14f, 0, 42.0f, 0.09f, 0.16f)),
            // Arbetare 2, kategori psyCleanCategory. Argument: ton (halvtoner över C1), svep, svepets tid, längd, attack, nivå, klick, drive.
            kickPreset ("Psy Clean C", psyCleanKick (0.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f)),
            kickPreset ("Psy Clean C Tight", psyCleanKick (0.0f, 1.0f, 0.75f, 0.15f, 0.0005f, 0.85f, 0.0f, 0.0f)),
            kickPreset ("Psy Clean D", psyCleanKick (2.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f)),
            kickPreset ("Psy Clean D Long", psyCleanKick (2.0f, 1.0f, 1.1f, 0.205f, 0.0005f, 0.85f, 0.0f, 0.0f)),
            kickPreset ("Psy Clean F Soft", psyCleanKick (5.0f, 0.8f, 1.2f, 0.20f, 0.003f, 0.85f, 0.0f, 0.0f)),
            kickPreset ("Psy Clean F Sub", psyCleanKick (5.0f, 0.9f, 1.4f, 0.20f, 0.001f, 0.88f, 0.0f, 0.0f)),
            kickPreset ("Psy Clean A Punch", psyCleanKick (9.0f, 1.0f, 0.85f, 0.17f, 0.0005f, 0.85f, 0.10f, 0.05f)),
            kickPreset ("Psy Clean G", psyCleanKick (7.0f, 1.0f, 1.0f, 0.20f, 0.0005f, 0.85f, 0.0f, 0.0f)),
            // Arbetare 2, genrer (1 kick + 2 basar per genre). Kick: se psyCleanKick; bas: se registerBass.
            kickPreset ("Minimal Kick", psyCleanKick (4.0f, 0.8f, 1.3f, 0.18f, 0.002f, 0.85f, 0.05f, 0.0f)),
            kickPreset ("Goa Kick", psyCleanKick (1.0f, 1.0f, 0.9f, 0.18f, 0.0005f, 0.85f, 0.08f, 0.05f)),
            kickPreset ("Dark Psy Kick", psyCleanKick (1.0f, 1.1f, 1.2f, 0.22f, 0.0005f, 0.85f, 0.0f, 0.15f)),
            kickPreset ("Dance Kick", psyCleanKick (7.0f, 0.9f, 0.8f, 0.15f, 0.001f, 0.85f, 0.15f, 0.10f)),
            kickPreset ("Trance Kick", psyCleanKick (6.0f, 1.0f, 1.0f, 0.18f, 0.001f, 0.85f, 0.10f, 0.05f)),
            kickPreset ("Techno Kick", psyCleanKick (8.0f, 1.2f, 0.7f, 0.16f, 0.0005f, 0.88f, 0.20f, 0.20f)),
            kickPreset ("Empirikal Kick", psyCleanKick (3.0f, 1.0f, 1.1f, 0.20f, 0.0005f, 0.85f, 0.05f, 0.10f)),
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
            // D7: rullande psytrance-basar, sist så att äldre index och hashar är oförändrade.
            bassPreset ("Rolling Tight", rollBass (0, 12, 12, 0.0f, 0.15f, 200.0f, 0.18f, 2.6f, 35.0f, 0.80f, 0.18f, 0.72f)),
            bassPreset ("Rolling Deep", rollBass (0, 12, 10, 0.0f, 0.30f, 170.0f, 0.20f, 2.4f, 50.0f, 0.88f, 0.20f, 0.72f)),
            bassPreset ("Rolling Bright", rollBass (0, 12, 13, 0.30f, 0.12f, 320.0f, 0.26f, 3.4f, 40.0f, 0.80f, 0.28f, 0.68f)),
            bassPreset ("Rolling Fat", rollBass (0, 12, 12, 0.50f, 0.20f, 230.0f, 0.22f, 3.0f, 55.0f, 0.85f, 0.35f, 0.66f)),
            bassPreset ("Rolling Soft", rollBass (0, 0, 10, 0.40f, 0.20f, 190.0f, 0.15f, 2.0f, 60.0f, 0.90f, 0.12f, 0.74f)),
            bassPreset ("Rolling Snap", rollBass (0, 12, 13, 0.20f, 0.10f, 280.0f, 0.30f, 4.0f, 28.0f, 0.70f, 0.30f, 0.70f)),
            // Arbetare 2, kategori psyRollingBassCategory. Argument: se psyRollBass.
            bassPreset ("Full-On Roll", psyRollBass (0, 0.95f, 94, 0.60f, 1.00f, 12.0f, 0.10f, 1200.0f, 0.20f, 3.0f, 45.0f, 0.40f, 90.0f, 25.0f, 0.15f, 0.55f, 0)),
            bassPreset ("Dekel Groove", psyRollBass (0, 1.00f, 94, 0.80f, 1.00f, 0.0f, 0.10f, 1500.0f, 0.15f, 3.0f, 60.0f, 0.35f, 100.0f, 25.0f, 0.10f, 0.58f, 0)),
            bassPreset ("Forest 1/32", psyRollBass (0, 1.00f, 99, 0.90f, 1.00f, 0.0f, 0.10f, 2200.0f, 0.45f, 3.0f, 30.0f, 0.30f, 50.0f, 20.0f, 0.15f, 0.50f, 1)),
            bassPreset ("Wide Mid", psyRollBass (0, 1.00f, 94, 1.00f, 1.00f, 12.0f, 0.10f, 1500.0f, 0.18f, 2.0f, 70.0f, 0.40f, 100.0f, 25.0f, 0.05f, 0.52f, 0)),
            // Arbetare 2, kategorierna MID BASS (namn "Mid "), HI BASS ("Hi ") och SYNTH ("Synth "). Argument: se registerBass.
            bassPreset ("Mid Growl", registerBass (0, 1, 15, 13, 0.35f, 0.0f, 2, 12.0f, 900.0f, 0.35f, 3.0f, 80.0f, 0.55f, 160.0f, 30.0f, 0.30f, 0.50f, 1)),
            bassPreset ("Mid Pluck", registerBass (0, 1, 12, 94, 0.50f, 0.0f, 1, 0.0f, 700.0f, 0.20f, 3.5f, 45.0f, 0.20f, 90.0f, 15.0f, 0.20f, 0.52f, 0)),
            bassPreset ("Mid Wide", registerBass (0, 1, 15, 12, 0.60f, 7.0f, 4, 18.0f, 1100.0f, 0.15f, 2.0f, 120.0f, 0.75f, 220.0f, 40.0f, 0.15f, 0.46f, 0)),
            bassPreset ("Hi Zap", registerBass (0, 2, 13, 12, 0.40f, 12.0f, 1, 0.0f, 2200.0f, 0.30f, 3.5f, 40.0f, 0.15f, 70.0f, 12.0f, 0.25f, 0.42f, 0)),
            bassPreset ("Hi Acid", registerBass (0, 2, 12, 99, 0.60f, 0.0f, 1, 0.0f, 1600.0f, 0.55f, 3.5f, 90.0f, 0.55f, 160.0f, 25.0f, 0.40f, 0.40f, 1)),
            bassPreset ("Hi Lead", registerBass (0, 2, 15, 12, 0.45f, 0.0f, 3, 14.0f, 3000.0f, 0.20f, 1.5f, 150.0f, 0.80f, 250.0f, 60.0f, 0.15f, 0.40f, 0)),
            bassPreset ("Synth Pad", registerBass (0, 1, 15, 15, 0.70f, 0.0f, 5, 20.0f, 1400.0f, 0.12f, 1.2f, 400.0f, 0.90f, 600.0f, 300.0f, 0.05f, 0.40f, 0)),
            bassPreset ("Synth Pluck", registerBass (0, 1, 13, 12, 0.30f, 12.0f, 2, 8.0f, 1800.0f, 0.25f, 3.0f, 70.0f, 0.15f, 130.0f, 80.0f, 0.12f, 0.46f, 0)),
            bassPreset ("Synth Stab", registerBass (0, 2, 12, 13, 0.50f, 7.0f, 3, 10.0f, 2400.0f, 0.30f, 2.5f, 90.0f, 0.10f, 110.0f, 60.0f, 0.18f, 0.42f, 0)),
            bassPreset ("Minimal Sub", registerBass (0, 1, 15, 12, 0.20f, 0.0f, 1, 0.0f, 500.0f, 0.15f, 1.5f, 200.0f, 0.85f, 400.0f, 60.0f, 0.10f, 0.52f, 0)),
            bassPreset ("Minimal Pluck", registerBass (0, 1, 12, 15, 0.30f, 0.0f, 1, 0.0f, 600.0f, 0.25f, 2.5f, 60.0f, 0.25f, 120.0f, 20.0f, 0.15f, 0.50f, 0)),
            bassPreset ("Goa Roll", registerBass (0, 1, 15, 12, 0.40f, 0.0f, 2, 8.0f, 800.0f, 0.30f, 3.0f, 70.0f, 0.50f, 150.0f, 25.0f, 0.25f, 0.50f, 1)),
            bassPreset ("Goa Acid", registerBass (0, 1, 12, 99, 0.50f, 0.0f, 1, 0.0f, 1200.0f, 0.50f, 3.0f, 100.0f, 0.45f, 170.0f, 25.0f, 0.35f, 0.46f, 1)),
            bassPreset ("Dark Psy Growl", registerBass (0, 1, 15, 13, 0.45f, 0.0f, 3, 14.0f, 700.0f, 0.40f, 3.5f, 110.0f, 0.60f, 200.0f, 35.0f, 0.40f, 0.46f, 1)),
            bassPreset ("Dark Psy Sub", registerBass (0, 1, 15, 12, 0.25f, 0.0f, 1, 0.0f, 450.0f, 0.30f, 2.5f, 130.0f, 0.70f, 260.0f, 40.0f, 0.30f, 0.50f, 1)),
            bassPreset ("Dance Pump", registerBass (0, 1, 15, 12, 0.50f, 0.0f, 2, 10.0f, 1000.0f, 0.20f, 2.5f, 90.0f, 0.65f, 180.0f, 30.0f, 0.15f, 0.50f, 0)),
            bassPreset ("Dance Pluck", registerBass (0, 1, 13, 12, 0.40f, 12.0f, 2, 8.0f, 1500.0f, 0.25f, 3.0f, 60.0f, 0.20f, 110.0f, 20.0f, 0.15f, 0.46f, 0)),
            bassPreset ("Trance Roll", registerBass (0, 1, 15, 15, 0.55f, 0.0f, 3, 12.0f, 1100.0f, 0.18f, 2.0f, 110.0f, 0.70f, 190.0f, 35.0f, 0.12f, 0.48f, 0)),
            bassPreset ("Trance Pluck", registerBass (0, 2, 12, 13, 0.45f, 7.0f, 3, 10.0f, 2000.0f, 0.25f, 2.5f, 70.0f, 0.20f, 120.0f, 50.0f, 0.15f, 0.44f, 0)),
            bassPreset ("Techno Sub", registerBass (0, 1, 15, 12, 0.20f, 0.0f, 1, 0.0f, 550.0f, 0.30f, 2.0f, 100.0f, 0.60f, 200.0f, 30.0f, 0.25f, 0.52f, 1)),
            bassPreset ("Techno Stab", registerBass (0, 2, 12, 99, 0.50f, 0.0f, 1, 0.0f, 1800.0f, 0.45f, 3.0f, 60.0f, 0.10f, 100.0f, 20.0f, 0.35f, 0.44f, 1)),
            bassPreset ("Empirikal Reese", registerBass (0, 1, 15, 15, 0.80f, 0.0f, 3, 16.0f, 900.0f, 0.25f, 2.0f, 150.0f, 0.80f, 260.0f, 50.0f, 0.30f, 0.46f, 1)),
            bassPreset ("Empirikal Growl", registerBass (0, 1, 12, 13, 0.50f, 0.0f, 2, 12.0f, 850.0f, 0.45f, 3.5f, 90.0f, 0.55f, 170.0f, 30.0f, 0.40f, 0.44f, 1)),
            // Arbetare 2, kategorin UNISON (namn "Unison "): S3 Unison/Reese, två lätt förstämda sågor (OSC B finstämd) med unison. Se docs/school/reese.md.
            bassPreset ("Unison Reese", registerBass (0, 0, 15, 15, 1.0f, 0.12f, 5, 18.0f, 1000.0f, 0.18f, 1.0f, 150.0f, 0.90f, 260.0f, 40.0f, 0.25f, 0.44f, 1)),
            bassPreset ("Unison Wide", registerBass (0, 0, 15, 12, 0.90f, -0.12f, 7, 25.0f, 1400.0f, 0.15f, 1.5f, 180.0f, 0.90f, 280.0f, 60.0f, 0.20f, 0.40f, 0)),
            bassPreset ("Unison Dark", registerBass (0, 0, 15, 15, 1.0f, 0.10f, 3, 12.0f, 650.0f, 0.30f, 1.5f, 120.0f, 0.85f, 240.0f, 30.0f, 0.35f, 0.46f, 1)),
        };
        return presets;
    }

    const std::vector<Preset>& effectChainPresets()
    {
        static const std::vector<Preset> presets { { "Default (all off)", {} } };
        return presets;
    }
}
