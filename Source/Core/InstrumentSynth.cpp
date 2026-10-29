#include "InstrumentSynth.h"
#include "Modulation.h" // syncTimeInQuarters (ENV 2 och ENV 3 i läget Sync)

namespace
{

    double wrap (double phase) { return phase - std::floor (phase); }
}

void InstrumentSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    Wavetables::ensure (0); // Basic måste finnas; övriga tabeller byggs av TableBuilder / ensureTablesInUse (inte på ljudtråden)

    filterMid.prepare (sampleRate); // (allokerar kamfiltrets fördröjningslinje och bygger halvbandsdesignen: aldrig på ljudtråden)
    filterSide.prepare (sampleRate);
    filter2Mid.prepare (sampleRate);
    filter2Side.prepare (sampleRate);
    filter2WasOn = false;

    reset();
}

void InstrumentSynth::reset()
{
    stage = Stage::idle;
    ampLevel = 0.0f;
    gateRemaining = 0;
    glideSamplesLeft = 0;
    samplePosition = 0.0;
    sliceVoice = false;
    sliceNote = false;
    env2.reset();
    env3.reset();
    pitchEnv = 0.0f;
    warpDirect = 0.0f;
    fmCarrierPhase = fmModPhase = 0.0;
    fmEnvLevel = 0.0f;
    noiseState = 0.0f;
    pinkB0 = pinkB1 = pinkB2 = brownState = vinylHiss = 0.0f;
    filterMid.reset();
    filterSide.reset();
    filter2Mid.reset();
    filter2Side.reset();
}

double InstrumentSynth::warpPhase (WarpMode mode, float amount, double phase, float fmInput)
{
    const auto a = (double) juce::jlimit (0.0f, 1.0f, amount);
    if (a <= 0.0)
        return phase;

    switch (mode)
    {
        case WarpMode::sync:   return wrap (phase * (1.0 + 7.0 * a));
        case WarpMode::bend:   return std::pow (phase, 1.0 + 4.0 * a);
        case WarpMode::pwm:
        {
            const auto w = 0.5 - 0.48 * a;
            return phase < w ? 0.5 * phase / w : 0.5 + 0.5 * (phase - w) / (1.0 - w);
        }
        case WarpMode::mirror:
        {
            const auto mirrored = phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0;
            return wrap (phase + a * (mirrored * 0.5 - phase));
        }
        case WarpMode::fm:     return wrap (phase + a * 2.0 * fmInput);
        case WarpMode::flip:
        {
            const auto mirrored = phase < 0.5 ? phase : 1.5 - phase;
            return phase + a * (mirrored - phase);
        }
        case WarpMode::quantize:
        {
            const auto steps = std::pow (2.0, 7.0 - 6.0 * a); // 128 steg ... 2 steg
            return std::floor (phase * steps) / steps;
        }
        case WarpMode::am:
        case WarpMode::ring:
        case WarpMode::off:    break;
    }
    return phase;
}

void InstrumentSynth::Oscillator::retrigger (const OscParams& p, juce::Random& rng, double extraPhase)
{
    for (auto& ph : phases)
        ph = wrap (p.phase + extraPhase + p.randomPhase * rng.nextDouble());
}

InstrumentSynth::Stereo InstrumentSynth::Oscillator::next (const OscParams& p, double frequency, double rate, float warpAmount, float fmInput)
{
    const auto& table = p.table == Wavetables::userIndex() ? (p.user != nullptr ? *p.user : Wavetables::get (0))
                                                           : Wavetables::get (p.table);
    const auto voices = juce::jlimit (1, maxUnison, p.unison);
    if (p.semis != lastSemis || p.fineCents != lastFine)
    {
        lastSemis = p.semis;
        lastFine = p.fineCents;
        pitchRatio = std::exp2 ((p.semis + p.fineCents * 0.01f) / 12.0f);
    }
    const auto base = frequency * pitchRatio;
    if (voices != lastVoices || p.detuneCents != lastDetune)
    {
        lastVoices = voices;
        lastDetune = p.detuneCents;
        for (int v = 0; v < voices; ++v)
            detuneRatios[(size_t) v] = std::exp2 ((voices > 1 ? (double) v / (voices - 1) * 2.0 - 1.0 : 0.0) * p.detuneCents / 1200.0);
    }

    // Sidostämmorna sprids jämnt ±detune; mittstämman (eller de två mittersta) har full nivå.
    auto sumLeft = 0.0f, sumRight = 0.0f, gainSum = 0.0f;
    for (int v = 0; v < voices; ++v)
    {
        const auto spread = voices > 1 ? (double) v / (voices - 1) * 2.0 - 1.0 : 0.0;
        const auto hz = base * detuneRatios[(size_t) v];
        const auto centre = voices == 1 || std::abs (spread) < 1.0 / voices + 1.0e-6;
        const auto gain = centre ? 1.0f : p.blend;

        auto& phase = phases[(size_t) v];
        const auto warped = warpPhase (p.warp, warpAmount, phase, fmInput);
        // CYCLES: framen sträcker sig över cyclesPerFrame perioder, så fasen går så många gånger långsammare och
        // fler av frame-övertonerna ryms under Nyquist (frame-tonen är hz / cycles).
        const auto frameHz = hz / (double) p.cyclesPerFrame;
        auto voice = gain * table.sample (p.position, warped, rate * 0.5 / std::max (1.0, frameHz), p.morph);
        if (p.warp == WarpMode::am && warpAmount > 0.0f)
        {
            // AM: vågen multipliceras med en sinus en oktav upp (Serums AM-warp).
            const auto depth = juce::jlimit (0.0f, 1.0f, warpAmount);
            voice *= 1.0f - depth + depth * (0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 2.0 * phase));
        }
        else if (p.warp == WarpMode::ring && warpAmount > 0.0f)
        {
            // RING (B): vågen multipliceras med OSC 2 och/eller OSC 3 (fmInput = faktorn, -1..1). Warp = blandning torr -> ren ringmodulation.
            const auto depth = juce::jlimit (0.0f, 1.0f, warpAmount);
            voice *= 1.0f - depth + depth * fmInput;
        }

        // Stämmorna sprids över stereobilden (balans: mitten oförändrad, ytterkanten tystnar åt andra sidan).
        const auto voicePan = voices > 1 ? (float) spread * p.stereo : 0.0f;
        sumLeft += voice * (voicePan > 0.0f ? 1.0f - voicePan : 1.0f);
        sumRight += voice * (voicePan < 0.0f ? 1.0f + voicePan : 1.0f);
        gainSum += gain * gain;

        phase = wrap (phase + frameHz / rate);
    }
    const auto normal = 1.0f / std::sqrt (std::max (1.0f, gainSum));
    const auto pan = juce::jlimit (-1.0f, 1.0f, p.pan);
    return { sumLeft * normal * (pan > 0.0f ? 1.0f - pan : 1.0f), sumRight * normal * (pan < 0.0f ? 1.0f + pan : 1.0f) };
}

const ClickSample* InstrumentSynth::activeSample() const noexcept
{
    if (params.sampleMulti)
    {
        const auto* zone = params.sampleBank != nullptr ? params.sampleBank->zone (multiZone) : nullptr;
        return zone != nullptr ? &zone->sample : nullptr;
    }
    return params.sampleData;
}

void InstrumentSynth::noteOn (int semitones, float newVelocity, int gateSamples, int slice)
{
    forcedReleaseMs = 0.0; // en ny ton släpps med sin vanliga RELEASE
    targetSemitones = semitones;
    const auto frequency = params.rootHz * std::exp2 (semitones / 12.0f);
    targetLogFrequency = std::log2 ((double) frequency);
    logFrequency = targetLogFrequency;
    glideSamplesLeft = 0;

    // Samma startfas varje slag ger ett jämnt, tight anslag. Men om förra tonen fortfarande
    // hörs skulle ett fashopp knäppa, så då fortsätter fasen där den är.
    const auto extraPhase = (double) params.phaseDeg / 360.0; // PH: gemensam startfas för OSC 1-3 och sub (0 = som förut)
    if (ampLevel < 0.05f)
    {
        oscA.retrigger (params.oscA, random, extraPhase);
        oscB.retrigger (params.oscB, random, extraPhase);
        oscC.retrigger (params.oscC, random, extraPhase);
        subPhase = wrap (extraPhase + (double) params.subPhaseDeg / 360.0);
        fmCarrierPhase = wrap (extraPhase + (double) params.fmPhaseDeg / 360.0);
        fmModPhase = 0.0;
    }
    fmEnvLevel = 1.0f;

    noteTime = 0.0;
    noteOffTime = -1.0;
    // 5b: SYNC-fördröjningen (takter ur tempot) låses nu; en tempoändring mitt i tonen rör den inte.
    for (size_t e = 0; e < delaySeconds.size(); ++e)
    {
        delaySeconds[e] = engineDelaySeconds (params, (int) e, bpm);
        resetPending[e] = delaySeconds[e] > 0.0;
        lengthLocked[e] = params.lengthSync[e] > 0;
        lengthSeconds[e] = lengthLocked[e] ? engineLengthSeconds (params, (int) e, bpm) : 0.0;
    }
    env2.trigger();
    env3.trigger();
    pitchEnv = 1.0f;
    warpDirect = 1.0f;
    randomValue = modRandom.nextFloat();
    multiZone = -1;
    if (params.sampleMulti && params.sampleBank != nullptr)
    {
        // Tonen ur frekvensen (A4 = 440 Hz = 69), anslaget 1..127; zonen slås upp igen per sample mot aktuell bank.
        const auto key = juce::jlimit (0, 127, juce::roundToInt (69.0f + 12.0f * std::log2 (frequency / 440.0f)));
        multiZone = params.sampleBank->select (key, juce::jlimit (1, 127, juce::roundToInt (newVelocity * 127.0f)));
    }
    sampleAge = 0.0;
    const auto* startSample = activeSample();
    samplePosition = startSample != nullptr ? (double) params.sampleStart * (double) startSample->audio.getNumSamples() : 0.0;
    if (startSample != nullptr && params.sampleReverse)
        samplePosition = std::max (0.0, (double) startSample->audio.getNumSamples() - 1.0 - samplePosition);
    sliceVoice = false;
    sliceNote = slice >= 0 && ! params.sampleMulti; // multisampling har inga skivor
    if (sliceNote && params.sampleData != nullptr)
    {
        // En not i raden "Slice N" väljer skivan; saknas skivan är det tyst, ingen wrap.
        const auto& slices = params.sampleData->slices;
        const auto index = slice;
        if ((size_t) index < slices.size())
        {
            sliceBegin = slices[(size_t) index];
            sliceEnd = (size_t) index + 1 < slices.size() ? slices[(size_t) index + 1] : params.sampleData->audio.getNumSamples();
            sliceVoice = sliceEnd > sliceBegin;
            samplePosition = params.slicerReverse ? (double) (sliceEnd - 1) : (double) sliceBegin;
        }
    }
    velocity = newVelocity;
    stage = Stage::attack; // attacken börjar från nuvarande nivå, så inget knäpp
    gateRemaining = gateSamples;
}

namespace
{
constexpr double syncBars[] { 0.0, 0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 1.0 / 32.0, 1.0 / 64.0 }; // SYNC-valen 1/16 ... 4 bars, sedan 1/32 och 1/64, i takter (se Params::timingSyncNames)
double barSeconds (double bpm) { return 4.0 * 60.0 / juce::jlimit (20.0, 400.0, bpm); }
}

double InstrumentSynth::engineDelaySeconds (const InstrumentParams& p, int engine, double bpm)
{
    const auto e = (size_t) engine;
    const auto sync = juce::jlimit (0, (int) std::size (syncBars) - 1, p.delaySync[e]);
    const auto seconds = sync > 0 ? (double) juce::jlimit (1, 8, p.delayCount[e]) * syncBars[sync] * barSeconds (bpm) : (double) p.delayMs[e] * 0.001;
    return std::max (0.0, seconds);
}

double InstrumentSynth::engineLengthSeconds (const InstrumentParams& p, int engine, double bpm)
{
    const auto e = (size_t) engine;
    const auto sync = juce::jlimit (0, (int) std::size (syncBars) - 1, p.lengthSync[e]);
    return std::max (0.0, sync > 0 ? (double) juce::jlimit (1, 8, p.lengthCount[e]) * syncBars[sync] * barSeconds (bpm) : (double) p.lengthMs[e] * 0.001);
}

float InstrumentSynth::gateAt (double t, double delay, double length, float curve) noexcept
{
    if (delay <= 0.0 && length <= 0.0)
        return 1.0f;
    if (t < delay)
        return 0.0f;
    if (length <= 0.0)
        return 1.0f;
    const auto u = (t - delay) / length;
    if (u >= 1.0)
        return 0.0f;
    return (float) std::pow (1.0 - u, std::exp2 (2.0 * (double) curve)); // CURVE 0 = linjär, > 0 faller snabbt först, < 0 långsamt först
}

float InstrumentSynth::engineGate (int engine) const noexcept
{
    const auto length = lengthLocked[(size_t) engine] ? lengthSeconds[(size_t) engine] : (double) params.lengthMs[(size_t) engine] * 0.001;
    return gateAt (noteTime, delaySeconds[(size_t) engine], length, params.curve[(size_t) engine]);
}

bool InstrumentSynth::glideTo (int semitones, int samples)
{
    if (stage == Stage::idle)
        return false;

    targetSemitones = semitones;
    targetLogFrequency = std::log2 ((double) (params.rootHz * std::exp2 (semitones / 12.0f)));
    glideSamplesLeft = std::max (1, samples);
    glideStep = (targetLogFrequency - logFrequency) / (double) glideSamplesLeft;
    return true;
}

void InstrumentSynth::fadeOutQuickly (double milliseconds)
{
    if (stage == Stage::idle)
        return;
    forcedReleaseMs = milliseconds;
    gateRemaining = 0;
    if (stage != Stage::release)
        noteOffTime = noteTime;
    stage = Stage::release;
}

void InstrumentSynth::release()
{
    if (stage != Stage::idle)
    {
        if (stage != Stage::release)
            noteOffTime = noteTime;
        stage = Stage::release;
        env2.release();
        env3.release();
    }
}

ModEnvParams ModEnvParams::withTempo (double bpm) const
{
    if (! sync)
        return *this;
    const auto msPerQuarter = 60000.0 / juce::jlimit (20.0, 400.0, bpm);
    ModEnvParams result = *this;
    result.attackMs = (float) std::max (0.1, Modulation::syncTimeInQuarters (attackNote) * msPerQuarter);
    result.decayMs = (float) std::max (5.0, Modulation::syncTimeInQuarters (decayNote) * msPerQuarter);
    result.releaseMs = (float) std::max (5.0, Modulation::syncTimeInQuarters (releaseNote) * msPerQuarter);
    return result;
}

InstrumentParams InstrumentParams::withTempo (double bpm, bool applyOnOff) const
{
    InstrumentParams result = *this;
    result.env2 = env2.withTempo (bpm);
    result.env3 = env3.withTempo (bpm);
    const auto msPerQuarter = 60000.0 / juce::jlimit (20.0, 400.0, bpm);
    const auto noteMs = [msPerQuarter] (int note, double minimum) { return (float) std::max (minimum, Modulation::syncTimeInQuarters (note) * msPerQuarter); };
    // Samma minimum som ms-rattarna.
    if (env1Sync)
    {
        result.attackMs = noteMs (env1AttackNote, 0.0);
        result.decayMs = noteMs (env1DecayNote, 5.0);
        result.releaseMs = noteMs (env1ReleaseNote, 1.0);
    }
    if (filterEnvSync)
    {
        result.envAttackMs = noteMs (filterAttackNote, 0.0);
        result.envDecayMs = noteMs (filterDecayNote, 5.0);
    }
    if (applyOnOff && ! env1On)
    {
        result.attackMs = 1.0f;
        result.sustain = 1.0f;
        result.releaseMs = 5.0f;
    }
    if (applyOnOff && ! filterEnvOn)
        result.envAmount = 0.0f;
    return result;
}

void InstrumentSynth::refreshEnvTimes()
{
    params = rawParams.withTempo (bpm);
    envTimes2 = params.env2;
    envTimes3 = params.env3;
}

void InstrumentSynth::ModEnv::next (const ModEnvParams& p, double rate)
{
    switch (envStage)
    {
        case EnvStage::attack:
            level += (float) (1.0 / (std::max (0.05f, p.attackMs) * 0.001 * rate));
            if (level >= 1.0f)
            {
                level = 1.0f;
                envStage = EnvStage::decay;
            }
            break;
        case EnvStage::decay:
            level += (p.sustain - level) * (float) decayMemo.get (p.decayMs, rate);
            break;
        case EnvStage::release:
            level -= level * (float) releaseMemo.get (p.releaseMs, rate);
            if (level < 1.0e-4f)
                reset();
            break;
        case EnvStage::idle:
            break;
    }
}

float InstrumentSynth::filterEnvelopeAt (const InstrumentParams& p, double seconds)
{
    const auto attack = std::max (0.0001, p.envAttackMs * 0.001);
    if (seconds < attack)
        return (float) (seconds / attack);

    // Exponentiellt fall, -60 dB efter envDecayMs.
    const auto decay = std::max (0.001, p.envDecayMs * 0.001);
    return (float) std::exp (std::log (0.001) * (seconds - attack) / decay);
}

float InstrumentSynth::warpEnvSourceAt (const InstrumentParams& p, double seconds, double noteOffSeconds)
{
    // Samma matematik som ModEnv::next / warpDirect i next(), men som en funktion av tiden (för kurvan i WARP ENV-gruppen).
    if (p.warpEnvSource == 0)
        return filterEnvelopeAt (p, seconds);
    if (p.warpEnvSource == 1)
        return (float) std::exp (std::log (0.001) * seconds / (std::max (0.1, (double) p.warpEnvDecayMs) * 0.001));

    const auto& e = p.warpEnvSource == 2 ? p.env2 : p.env3;
    if (! e.on)
        return 0.0f;

    // Nivån på sekunden t medan tonen hålls (attack, sedan fall mot sustain).
    const auto held = [&e] (double t)
    {
        const auto attack = std::max (0.05, (double) e.attackMs) * 0.001;
        if (t < attack)
            return t / attack;
        const auto decay = std::max (0.1, (double) e.decayMs) * 0.001;
        return (double) e.sustain + (1.0 - e.sustain) * std::exp (std::log (0.001) * (t - attack) / decay);
    };

    if (noteOffSeconds < 0.0 || seconds < noteOffSeconds)
        return (float) held (seconds);

    const auto release = std::max (0.1, (double) e.releaseMs) * 0.001;
    return (float) (held (noteOffSeconds) * std::exp (std::log (0.001) * (seconds - noteOffSeconds) / release));
}

double InstrumentSynth::coefficientFor (double milliseconds, double rate)
{
    // Andel av avståndet till målet per sample, så att -60 dB nås efter `milliseconds`.
    return 1.0 - std::exp (std::log (0.001) / (std::max (0.1, milliseconds) * 0.001 * rate));
}

InstrumentSynth::Output InstrumentSynth::next()
{
    if (stage == Stage::idle)
    {
        env2.reset();
        env3.reset();
        return {};
    }

    // Gate: när tonens tid är slut börjar release, om den inte väntar på en slide.
    if (gateRemaining > 0 && --gateRemaining == 0)
        release();

    // Amplitud-envelope.
    switch (stage)
    {
        case Stage::attack:
            ampLevel += (float) (1.0 / (std::max (0.05f, params.attackMs) * 0.001 * sampleRate));
            if (ampLevel >= 1.0f)
            {
                ampLevel = 1.0f;
                stage = Stage::decay;
            }
            break;
        case Stage::decay:
            ampLevel += (params.sustain - ampLevel) * (float) ampDecayMemo.get (params.decayMs, sampleRate);
            break;
        case Stage::release:
            ampLevel -= ampLevel * (float) ampReleaseMemo.get (forcedReleaseMs > 0.0 ? (float) forcedReleaseMs : params.releaseMs, sampleRate);
            if (ampLevel < 1.0e-4f)
            {
                stage = Stage::idle;
                ampLevel = 0.0f;
                return {};
            }
            break;
        case Stage::idle:
            break;
    }

    // Modulationsenvelopes och tonhöjdsenvelopen.
    env2.next (envTimes2, sampleRate);
    env3.next (envTimes3, sampleRate);
    if (params.pitchEnvSemis != 0.0f)
        pitchEnv -= pitchEnv * (float) pitchEnvMemo.get (params.pitchEnvDecayMs, sampleRate);

    // Glide-tonen: linjärt i log-frekvens, så att varje halvton tar lika lång tid, och tonen blir kvar på målet.
    if (glideSamplesLeft > 0)
    {
        logFrequency += glideStep;
        if (--glideSamplesLeft == 0)
            logFrequency = targetLogFrequency;
    }

    const auto pitchOffset = params.pitchEnvSemis * pitchEnv / 12.0;
    if (logFrequency != lastLogFrequency || pitchOffset != lastPitchOffset)
    {
        lastLogFrequency = logFrequency;
        lastPitchOffset = pitchOffset;
        cachedFrequency = juce::jlimit (1.0, sampleRate * 0.45, std::exp2 (logFrequency + pitchOffset)); // extrema tonhöjder (±96 halvtoner) får inte ge aliasing
    }
    const auto frequency = cachedFrequency;
    const auto envelope = filterEnvelopeAt (params, noteTime);
    filterEnvLevel = envelope;

    // OSC B först: den kan frekvensmodulera OSC A (warp FM). Warp env lägger till warp på A
    // i början av varje ton.
    // 5b: när POS DELAY passerats startar motorn om från sin startfas (tight anslag).
    for (size_t e = 0; e < resetPending.size(); ++e)
    {
        if (! resetPending[e] || noteTime < delaySeconds[e])
            continue;
        resetPending[e] = false;
        const auto extraPhase = (double) params.phaseDeg / 360.0;
        if (e == 0) oscA.retrigger (params.oscA, random, extraPhase);
        else if (e == 1) oscB.retrigger (params.oscB, random, extraPhase);
        else if (e == 2) oscC.retrigger (params.oscC, random, extraPhase);
        else if (e == 5) subPhase = wrap (extraPhase + (double) params.subPhaseDeg / 360.0);
        else if (e == 6) { fmCarrierPhase = wrap (extraPhase + (double) params.fmPhaseDeg / 360.0); fmModPhase = 0.0; }
        if (params.delayShift[e]) // SHIFT: motorns egna tidsförlopp börjar också om (FM-envelopen, warp-envelopen för OSC)
        {
            if (e == 6) fmEnvLevel = 1.0f;
            else if (e <= 2) warpDirect = 1.0f;
        }
    }
    const auto levelA = (params.oscA.on ? params.oscA.level : 0.0f) * engineGate (0);
    const auto levelB = (params.oscB.on ? params.oscB.level : 0.0f) * engineGate (1);
    const auto levelC = (params.oscC.on ? params.oscC.level : 0.0f) * engineGate (2);
    const auto ringOn = params.oscA.warp == WarpMode::ring;
    const auto fmOn = params.oscA.warp == WarpMode::fm, fmFromC = fmOn && params.oscA.fmSource == 1;
    const auto bNeeded = params.oscB.on && (params.oscB.level > 0.0f || (fmOn && ! fmFromC) || (ringOn && params.oscA.ringSource != 1));
    const auto cNeeded = params.oscC.on && (params.oscC.level > 0.0f || fmFromC || (ringOn && params.oscA.ringSource != 0));
    const auto bRaw = bNeeded ? oscB.next (params.oscB, frequency, sampleRate, params.oscB.warpAmount, 0.0f) : Stereo {};
    const auto bSide = 0.5f * (bRaw.left - bRaw.right) * params.bWidth;
    const auto b = params.bWidth == 1.0f ? bRaw : Stereo { bRaw.mid() + bSide, bRaw.mid() - bSide };
    float warpSource = envelope;
    if (params.warpEnvSource == 1)
    {
        warpSource = warpDirect;
        warpDirect -= warpDirect * (float) warpDirectMemo.get (params.warpEnvDecayMs, sampleRate);
    }
    else if (params.warpEnvSource == 2)
        warpSource = env2.level;
    else if (params.warpEnvSource == 3)
        warpSource = env3.level;
    const auto warpA = params.oscA.warpAmount + params.warpEnv * warpSource;
    // Bredd per OSC: sidan (L-R) skalas; 1 = orört (bit för bit som förut). b.mid() ändras inte av breddningen.
    const auto widen = [] (Stereo s, float width)
    {
        if (width == 1.0f)
            return s;
        const auto centre = s.mid(), side = 0.5f * (s.left - s.right) * width;
        return Stereo { centre + side, centre - side };
    };
    // OSC C före A, eftersom RING (B) kan multiplicera A med C. Av/tyst källa ger faktor 0 (inget hörs vid full RING).
    const auto c = widen (cNeeded ? oscC.next (params.oscC, frequency, sampleRate, params.oscC.warpAmount, 0.0f) : Stereo {}, params.cWidth);
    const auto bMid = params.oscB.on ? b.mid() : 0.0f, cMid = params.oscC.on ? c.mid() : 0.0f;
    const auto modInput = ringOn ? (params.oscA.ringSource == 0 ? bMid : params.oscA.ringSource == 1 ? cMid : bMid * cMid) : fmFromC ? cMid : b.mid();
    const auto a = widen (levelA > 0.0f ? oscA.next (params.oscA, frequency, sampleRate, warpA, modInput) : Stereo {}, params.aWidth);

    // Sub och brus ligger i mitten. Mitten och sidan filtreras var för sig (samma filter), och
    // blir sedan vänster och höger igen.
    // fas S: en källa med FILTER av (eller hela FILTER av) går förbi Filter 1+2 och läggs till efter dem. Inget förbigånget = som förut (tillägg av 0 ändrar inget).
    const auto fOn = params.filterOn;
    const auto levelAF = fOn && params.filterSrc[0] ? levelA : 0.0f, levelBF = fOn && params.filterSrc[1] ? levelB : 0.0f, levelCF = fOn && params.filterSrc[2] ? levelC : 0.0f;
    auto left = a.left * levelAF + b.left * levelBF + c.left * levelCF;
    auto right = a.right * levelAF + b.right * levelBF + c.right * levelCF;
    auto dryLeft = a.left * (levelA - levelAF) + b.left * (levelB - levelBF) + c.left * (levelC - levelCF);
    auto dryRight = a.right * (levelA - levelAF) + b.right * (levelB - levelBF) + c.right * (levelC - levelCF);
    float dryCentre = 0.0f;
    float subShape;
    switch (params.subWave)
    {
        case 1:  subShape = 1.0f - 4.0f * (float) std::abs (subPhase - 0.5); break;                                 // triangel
        case 2:  subShape = std::tanh (4.0f * (float) std::sin (juce::MathConstants<double>::twoPi * subPhase)); break; // mjuk fyrkant
        case 3:  subShape = 2.0f * (float) subPhase - 1.0f; break;                                                   // såg
        default: subShape = (float) std::sin (juce::MathConstants<double>::twoPi * subPhase); break;
    }
    auto centre = 0.0f;
    // INSTR-MIX: pan 0 lägger källan i mitten precis som förut (samma summering, ljudet bit för bit oförändrat); annars balanspan (mitten = full nivå) in i vänster/höger.
    auto addPanned = [&] (bool filtered, float l, float r, float pan)
    {
        (filtered ? left : dryLeft) += l * (pan > 0.0f ? 1.0f - pan : 1.0f);
        (filtered ? right : dryRight) += r * (pan < 0.0f ? 1.0f + pan : 1.0f);
    };
    auto addMono = [&] (bool filtered, float x, float pan)
    {
        if (pan == 0.0f)
            (filtered ? centre : dryCentre) += x;
        else
            addPanned (filtered, x, x, pan);
    };
    // Bredd för monokällor (SAMPLE, FM): som bruset, en fördröjd kopia (17 samplingar) är den okorrelerade delen. 1 = mono som förut.
    widthDelayPos = (widthDelayPos + 1) & 63;
    auto addMonoWide = [&] (bool filtered, float x, float pan, float width, float* line)
    {
        line[widthDelayPos] = x; // skrivs alltid, så att fördröjningslinjen är fylld när WIDTH dras upp från 1 (annars klick)
        if (width <= 1.0f)
            return addMono (filtered, x, pan);
        const auto delayed = line[(widthDelayPos + 64 - 17) & 63];
        const auto angle = juce::jmin (1.0f, width - 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        addPanned (filtered, x * std::cos (angle) + delayed * std::sin (angle), x * std::cos (angle) - delayed * std::sin (angle), pan);
    };
    (fOn ? centre : dryCentre) += params.sub * engineGate (5) * subShape;
    if (params.noise > 0.0f)
    {
        auto white = random.nextFloat() * 2.0f - 1.0f;
        switch (params.noiseType)
        {
            case 1: // Pink: tre-poligt närmevärde (Paul Kellet), nivåjusterat mot vitt
                pinkB0 = 0.99765f * pinkB0 + white * 0.0990460f;
                pinkB1 = 0.96300f * pinkB1 + white * 0.2965164f;
                pinkB2 = 0.57000f * pinkB2 + white * 1.0526913f;
                white = 0.35f * (pinkB0 + pinkB1 + pinkB2 + white * 0.1848f);
                break;
            case 2: // Brown: läckande integrator av vitt brus
                brownState = 0.995f * brownState + 0.06f * white;
                white = juce::jlimit (-1.0f, 1.0f, brownState * 3.0f);
                break;
            case 3: // Vinyl: tyst fräs plus enstaka knaster (sällsynta, slumpade, med olika styrka)
            {
                vinylHiss += 0.2f * (white - vinylHiss);
                const auto crackle = random.nextFloat() < 0.0004f ? (random.nextFloat() * 2.0f - 1.0f) : 0.0f;
                white = 0.25f * vinylHiss + 2.0f * crackle;
                break;
            }
            default: break;
        }
        if (params.noiseTone < 0.999f)
        {
            // NOISE TONE: ett lågpass som gör bruset mörkare (100 Hz ... 20 kHz); nivån hålls uppe så att det inte försvinner.
            const auto cutoffHz = 100.0f * std::pow (200.0f, params.noiseTone);
            const auto coefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
            noiseState += coefficient * (white - noiseState);
            white = noiseState * (1.0f + 2.5f * (1.0f - params.noiseTone));
        }
        const auto noiseOut = params.noise * engineGate (4) * white;
        const auto filteredNoise = fOn && params.filterSrc[4];
        if (params.noiseWidth > 1.0f)
        {
            // Bredd: en fördröjd kopia (17 samplingar) är den okorrelerade delen; L = x cos + d sin, R = x cos - d sin (samma effekt, 200 % = helt okorrelerat).
            noiseDelay[noiseDelayPos] = noiseOut;
            const auto delayed = noiseDelay[(noiseDelayPos + 64 - 17) & 63];
            noiseDelayPos = (noiseDelayPos + 1) & 63;
            const auto angle = juce::jmin (1.0f, params.noiseWidth - 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            addPanned (filteredNoise, noiseOut * std::cos (angle) + delayed * std::sin (angle), noiseOut * std::cos (angle) - delayed * std::sin (angle), params.noisePan);
        }
        else
            addMono (filteredNoise, noiseOut, params.noisePan);
    }

    // fas S2: FM-källan: sin(2π·(carrierfas + modulator·index)). Av (eller nivå 0) = körs inte alls och ljudet är oförändrat.
    // Indexet (0..1) ger index² · 8 rad, skalat ned när sidbanden skulle vika över Nyquist: min(1, 8000 / (f0·(ratio + 1))).
    if (params.fmOn && params.fmLevel > 0.0f)
    {
        fmEnvLevel -= fmEnvLevel * (float) fmEnvMemo.get (params.fmEnvMs, sampleRate);
        const auto carrierHz = juce::jlimit (1.0, sampleRate * 0.45, frequency * std::exp2 ((double) params.fmSemis / 12.0));
        const auto modHz = juce::jlimit (1.0, sampleRate * 0.45, carrierHz * (double) juce::jlimit (0.25f, 8.0f, params.fmRatio));
        const auto amount = juce::jlimit (0.0f, 1.0f, params.fmEnvAmount);
        const auto index = juce::jlimit (0.0f, 1.0f, params.fmIndex) * (1.0f - amount + amount * fmEnvLevel);
        const auto antiAlias = (float) std::min (1.0, 8000.0 / (carrierHz + modHz));
        const auto radians = 8.0f * index * index * antiAlias;
        const auto modulator = radians * (float) std::sin (juce::MathConstants<double>::twoPi * fmModPhase);
        const auto fm = params.fmLevel * engineGate (6) * (float) std::sin (juce::MathConstants<double>::twoPi * fmCarrierPhase + (double) modulator);
        addMonoWide (fOn && params.fmFilter, fm, params.fmPan, params.fmWidth, fmDelay);
        fmCarrierPhase = wrap (fmCarrierPhase + carrierHz / sampleRate);
        fmModPhase = wrap (fmModPhase + modHz / sampleRate);
    }

    // Samplingen: linjär interpolation. Med KEY följer hastigheten noten (samplingens egen tonhöjd = sampleRootHz).
    const auto* playing = activeSample();
    // 5b: POS DELAY för SAMPLE – positionen och editorkurvornas ålder står still tills fördröjningen gått, så samplet börjar från sitt eget start.
    if (params.sampleOn && playing != nullptr && params.sampleLevel > 0.0f && noteTime >= delaySeconds[3])
    {
        const auto& audio = playing->audio;
        const auto length = audio.getNumSamples();
        // Sample-editor: AMP-kurvan (nivå) och PITCH-kurvan (hastighet); av = 1 och 1, ljudet som förut.
        auto editorGain = engineGate (3);
        auto editorSpeed = 1.0;
        if (params.sampleAmpOn)
            editorGain *= InstrumentSynth::sampleCurve (1.0f - (float) (sampleAge / (0.001 * (double) params.sampleAmpMs * sampleRate)), params.sampleAmpShape);
        if (params.samplePitchOn)
            editorSpeed = std::exp2 ((double) params.samplePitchSemis / 12.0 * InstrumentSynth::sampleCurve (1.0f - (float) (sampleAge / (0.001 * (double) params.samplePitchMs * sampleRate)), params.samplePitchShape));
        sampleAge += 1.0;
        if (sliceNote)
        {
            // Skiva: originalhastighet framåt eller bakåt, stannar vid skivans kant; 64 samples fade mot kanterna mot klick.
            if (sliceVoice && length > 1 && samplePosition >= (double) sliceBegin && samplePosition < (double) sliceEnd)
            {
                const auto* data = audio.getReadPointer (0);
                const auto index = (int) samplePosition;
                const auto fraction = (float) (samplePosition - (double) index);
                const auto following = index + 1 < sliceEnd ? data[index + 1] : 0.0f;
                const auto edge = std::min (samplePosition - (double) sliceBegin + 1.0, (double) sliceEnd - samplePosition);
                const auto fade = (float) std::min (1.0, edge / 64.0);
                addMonoWide (fOn && params.filterSrc[3], fade * editorGain * params.sampleLevel * (data[index] + fraction * (following - data[index])), params.samplePan, params.sampleWidth, sampleDelay);
                samplePosition += params.slicerReverse ? -1.0 : 1.0;
            }
        }
        else if (length > 1)
        {
            if (samplePosition >= (double) length && params.sampleLoop)
                samplePosition = std::fmod (samplePosition, (double) length);
            if (samplePosition < (double) length)
            {
                const auto* data = audio.getReadPointer (0);
                const auto index = (int) samplePosition;
                const auto fraction = (float) (samplePosition - (double) index);
                const auto following = index + 1 < length ? data[index + 1] : (params.sampleLoop ? data[0] : 0.0f);
                addMonoWide (fOn && params.filterSrc[3], editorGain * params.sampleLevel * (data[index] + fraction * (following - data[index])), params.samplePan, params.sampleWidth, sampleDelay);
                auto step = params.sampleMulti ? editorSpeed * frequency / (double) std::max (1.0f, params.sampleBank->zone (multiZone)->rootHz)
                                               : editorSpeed * (params.sampleKeyTrack ? frequency / (double) std::max (1.0f, params.sampleRootHz) : 1.0);
                if (params.sampleReverse)
                {
                    samplePosition -= step;
                    if (samplePosition < 0.0) // förbi början: loopa från slutet eller sluta (positionen = längden är "färdig")
                        samplePosition = params.sampleLoop ? std::fmod (samplePosition, (double) length) + (double) length : (double) length;
                }
                else
                    samplePosition += step;
            }
        }
    }
    auto sample = 0.5f * (left + right) + centre;
    auto side = 0.5f * (left - right);
    const auto dryMid = 0.5f * (dryLeft + dryRight) + dryCentre, drySide = 0.5f * (dryLeft - dryRight);

    subPhase = wrap (subPhase + (params.subOctave == 0 ? frequency : frequency * 0.5) / sampleRate);

    // Filter: grundcutoff öppnas av envelopen, mer ju hårdare steget slås an.
    const auto envDepth = params.envAmount * (1.0f - params.velToEnv + params.velToEnv * velocity);
    auto keyFollow = 1.0f;
    if (params.keyTrack > 0.0f)
    {
        if ((float) frequency != lastKeyFrequency || params.keyTrack != lastKeyTrack)
        {
            lastKeyFrequency = (float) frequency;
            lastKeyTrack = params.keyTrack;
            cachedKeyFollow = std::pow ((float) frequency / 65.41f, params.keyTrack);
        }
        keyFollow = cachedKeyFollow;
    }
    const auto cutoff = params.cutoffHz * keyFollow * std::exp2 (envDepth * envelope);
    const auto clampedCutoff = juce::jlimit (20.0f, (float) sampleRate * 0.45f, cutoff);
    // Filtret (FilterUnit.h: Kickbase, TB-303, Ladder, Formant eller Comb) för mitten och sidan: samma inställning, så att det blir stereo utan att ljudet låter
    // olika i vänster och höger. Cutoff och resonans uppdateras här, i samma takt som filter-envelopen ändrar cutoff (per sampling).
    const auto filterInMid = sample, filterInSide = side; // (Filter 2 i läget Parallel får samma insignal som Filter 1)
    filterMid.set (params.filterModel, clampedCutoff, params.resonance, params.filterP1, params.filterP2, (int) params.filterType);
    filterSide.set (params.filterModel, clampedCutoff, params.resonance, params.filterP1, params.filterP2, (int) params.filterType);
    sample = filterMid.process (sample);
    side = filterSide.process (side);

    // Filter 2 (fas D3), direkt efter Filter 1: Serial = Filter 1 -> Filter 2 med MIX som Filter 2:s torr/våt (0 = bara Filter 1); Parallel = (1 - MIX) Filter 1 + MIX Filter 2,
    // båda med samma insignal. Av: körs inte alls (ingen CPU), och ljudet är oförändrat. Ingen envelope och ingen keytrack: cutoff är den inställda (och moduleringen).
    if (params.filter2On)
    {
        if (! filter2WasOn)
        {
            filter2Mid.reset();
            filter2Side.reset();
            filter2WasOn = true;
        }
        const auto cutoff2 = juce::jlimit (20.0f, (float) sampleRate * 0.45f, params.filter2CutoffHz);
        filter2Mid.set (params.filter2Model, cutoff2, params.filter2Resonance, params.filter2P1, params.filter2P2, params.filter2Type);
        filter2Side.set (params.filter2Model, cutoff2, params.filter2Resonance, params.filter2P1, params.filter2P2, params.filter2Type);
        const auto parallel = params.filterRouting == 1;
        const auto wetMid = filter2Mid.process (parallel ? filterInMid : sample);
        const auto wetSide = filter2Side.process (parallel ? filterInSide : side);
        const auto mix = juce::jlimit (0.0f, 1.0f, params.filterMix);
        sample += mix * (wetMid - sample);
        side += mix * (wetSide - side);
    }
    else
        filter2WasOn = false;

    // Allpass: fyra första ordningens allpass vid FREQ roterar fasen utan att ändra nivån; AMOUNT blandar mot torrt. AMOUNT 0 körs inte alls.
    if (params.allpassAmount > 0.0f)
    {
        if (! allpassWasOn)
        {
            std::fill (&allpassState[0][0], &allpassState[0][0] + 8, 0.0f);
            allpassWasOn = true;
        }
        const auto t = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, 2000.0f, params.allpassFreq) / (float) sampleRate);
        const auto a = (t - 1.0f) / (t + 1.0f);
        const auto amount = juce::jmin (1.0f, params.allpassAmount);
        const auto run = [a] (float x, float* z)
        {
            for (int i = 0; i < 4; ++i)
            {
                const auto y = a * x + z[i];
                z[i] = x - a * y;
                x = y;
            }
            return x;
        };
        sample += amount * (run (sample, allpassState[0]) - sample);
        side += amount * (run (side, allpassState[1]) - side);
    }
    else
        allpassWasOn = false;
    sample += dryMid; // fas S: förbigångna källor (FILTER av) läggs till efter båda filtren
    side += drySide;
    noteTime += 1.0 / sampleRate;

    // Drive verkar på källan (före gaten), så att distorsionen låter likadant oavsett envelope.
    driveShape.update (params.drive, params.driveMode);
    return { driveShape.apply (sample + side), driveShape.apply (sample - side), ampLevel * velocity * params.level * (params.invertPolarity ? -1.0f : 1.0f) };
}
