#include "BassSynth.h"

namespace
{
    constexpr double filterAttackSeconds = 0.001;

    float softClip (float x, float drive)
    {
        if (drive <= 0.0f)
            return x;

        const auto gain = 1.0f + drive * 9.0f;
        return std::tanh (gain * x) / std::tanh (gain);
    }

    double wrap (double phase) { return phase - std::floor (phase); }
}

void BassSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    Wavetables::count(); // bygg tabellerna här (inte på ljudtråden)

    for (auto* filter : { &filter1, &filter2, &sideFilter1, &sideFilter2 })
    {
        filter->prepare ({ sampleRate, 512, 1 });
        filter->setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    reset();
}

void BassSynth::reset()
{
    stage = Stage::idle;
    ampLevel = 0.0f;
    gateRemaining = 0;
    glideSamplesLeft = 0;
    samplePosition = 0.0;
    filter1.reset();
    filter2.reset();
    sideFilter1.reset();
    sideFilter2.reset();
}

double BassSynth::warpPhase (WarpMode mode, float amount, double phase, float fmInput)
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
        case WarpMode::off:    break;
    }
    return phase;
}

void BassSynth::Oscillator::retrigger (const OscParams& p, juce::Random& rng)
{
    for (auto& ph : phases)
        ph = wrap (p.phase + p.randomPhase * rng.nextDouble());
}

BassSynth::Stereo BassSynth::Oscillator::next (const OscParams& p, double frequency, double rate, float warpAmount, float fmInput)
{
    const auto& table = p.table >= Wavetables::userIndex() ? (p.user != nullptr ? *p.user : Wavetables::get (0))
                                                           : Wavetables::get (p.table);
    const auto voices = juce::jlimit (1, maxUnison, p.unison);
    const auto base = frequency * std::exp2 ((p.semis + p.fineCents * 0.01f) / 12.0f);

    // Sidostämmorna sprids jämnt ±detune; mittstämman (eller de två mittersta) har full nivå.
    auto sumLeft = 0.0f, sumRight = 0.0f, gainSum = 0.0f;
    for (int v = 0; v < voices; ++v)
    {
        const auto spread = voices > 1 ? (double) v / (voices - 1) * 2.0 - 1.0 : 0.0;
        const auto hz = base * std::exp2 (spread * p.detuneCents / 1200.0);
        const auto centre = voices == 1 || std::abs (spread) < 1.0 / voices + 1.0e-6;
        const auto gain = centre ? 1.0f : p.blend;

        auto& phase = phases[(size_t) v];
        const auto warped = warpPhase (p.warp, warpAmount, phase, fmInput);
        const auto voice = gain * table.sample (p.position, warped, rate * 0.5 / std::max (1.0, hz));

        // Stämmorna sprids över stereobilden (balans: mitten oförändrad, ytterkanten tystnar åt andra sidan).
        const auto voicePan = voices > 1 ? (float) spread * p.stereo : 0.0f;
        sumLeft += voice * (voicePan > 0.0f ? 1.0f - voicePan : 1.0f);
        sumRight += voice * (voicePan < 0.0f ? 1.0f + voicePan : 1.0f);
        gainSum += gain * gain;

        phase = wrap (phase + hz / rate);
    }
    const auto normal = 1.0f / std::sqrt (std::max (1.0f, gainSum));
    const auto pan = juce::jlimit (-1.0f, 1.0f, p.pan);
    return { sumLeft * normal * (pan > 0.0f ? 1.0f - pan : 1.0f), sumRight * normal * (pan < 0.0f ? 1.0f + pan : 1.0f) };
}

void BassSynth::noteOn (int semitones, float newVelocity, int gateSamples)
{
    const auto frequency = params.rootHz * std::exp2 (semitones / 12.0f);
    targetLogFrequency = std::log2 ((double) frequency);
    logFrequency = targetLogFrequency;
    glideSamplesLeft = 0;

    // Samma startfas varje slag ger ett jämnt, tight anslag. Men om förra tonen fortfarande
    // hörs skulle ett fashopp knäppa, så då fortsätter fasen där den är.
    if (ampLevel < 0.05f)
    {
        oscA.retrigger (params.oscA, random);
        oscB.retrigger (params.oscB, random);
        oscC.retrigger (params.oscC, random);
        subPhase = 0.0;
    }

    noteTime = 0.0;
    samplePosition = params.sampleData != nullptr ? (double) params.sampleStart * (double) params.sampleData->audio.getNumSamples() : 0.0;
    velocity = newVelocity;
    stage = Stage::attack; // attacken börjar från nuvarande nivå, så inget knäpp
    gateRemaining = gateSamples;
}

bool BassSynth::glideTo (int semitones, int samples)
{
    if (stage == Stage::idle)
        return false;

    targetLogFrequency = std::log2 ((double) (params.rootHz * std::exp2 (semitones / 12.0f)));
    glideSamplesLeft = std::max (1, samples);
    glideStep = (targetLogFrequency - logFrequency) / (double) glideSamplesLeft;
    return true;
}

void BassSynth::release()
{
    if (stage != Stage::idle)
        stage = Stage::release;
}

float BassSynth::filterEnvelopeAt (const BassParams& p, double seconds)
{
    if (seconds < filterAttackSeconds)
        return (float) (seconds / filterAttackSeconds);

    // Exponentiellt fall, -60 dB efter envDecayMs.
    const auto decay = std::max (0.001, p.envDecayMs * 0.001);
    return (float) std::exp (std::log (0.001) * (seconds - filterAttackSeconds) / decay);
}

double BassSynth::coefficientFor (double milliseconds, double rate)
{
    // Andel av avståndet till målet per sample, så att -60 dB nås efter `milliseconds`.
    return 1.0 - std::exp (std::log (0.001) / (std::max (0.1, milliseconds) * 0.001 * rate));
}

BassSynth::Output BassSynth::next()
{
    if (stage == Stage::idle)
        return {};

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
            ampLevel += (params.sustain - ampLevel) * (float) coefficientFor (params.decayMs, sampleRate);
            break;
        case Stage::release:
            ampLevel -= ampLevel * (float) coefficientFor (params.releaseMs, sampleRate);
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

    // Glide-tonen: linjärt i log-frekvens, så att varje halvton tar lika lång tid, och tonen blir kvar på målet.
    if (glideSamplesLeft > 0)
    {
        logFrequency += glideStep;
        if (--glideSamplesLeft == 0)
            logFrequency = targetLogFrequency;
    }

    const auto frequency = juce::jlimit (1.0, sampleRate * 0.45, std::exp2 (logFrequency)); // extrema tonhöjder (±96 halvtoner) får inte ge aliasing
    const auto envelope = filterEnvelopeAt (params, noteTime);

    // OSC B först: den kan frekvensmodulera OSC A (warp FM). Warp env lägger till warp på A
    // i början av varje ton.
    const auto levelA = params.oscA.on ? params.oscA.level : 0.0f;
    const auto levelB = params.oscB.on ? params.oscB.level : 0.0f;
    const auto levelC = params.oscC.on ? params.oscC.level : 0.0f;
    const auto bNeeded = params.oscB.on && (params.oscB.level > 0.0f || params.oscA.warp == WarpMode::fm);
    const auto b = bNeeded ? oscB.next (params.oscB, frequency, sampleRate, params.oscB.warpAmount, 0.0f) : Stereo {};
    const auto warpA = params.oscA.warpAmount + params.warpEnv * envelope;
    const auto a = levelA > 0.0f ? oscA.next (params.oscA, frequency, sampleRate, warpA, b.mid()) : Stereo {};
    const auto c = levelC > 0.0f ? oscC.next (params.oscC, frequency, sampleRate, params.oscC.warpAmount, 0.0f) : Stereo {};

    // Sub och brus ligger i mitten. Mitten och sidan filtreras var för sig (samma filter), och
    // blir sedan vänster och höger igen.
    auto left = a.left * levelA + b.left * levelB + c.left * levelC;
    auto right = a.right * levelA + b.right * levelB + c.right * levelC;
    auto centre = params.sub * (float) std::sin (juce::MathConstants<double>::twoPi * subPhase);
    if (params.noise > 0.0f)
        centre += params.noise * (random.nextFloat() * 2.0f - 1.0f);

    // Samplingen: linjär interpolation. Med KEY följer hastigheten noten (samplingens egen tonhöjd = sampleRootHz).
    if (params.sampleOn && params.sampleData != nullptr && params.sampleLevel > 0.0f)
    {
        const auto& audio = params.sampleData->audio;
        const auto length = audio.getNumSamples();
        if (length > 1)
        {
            if (samplePosition >= (double) length && params.sampleLoop)
                samplePosition = std::fmod (samplePosition, (double) length);
            if (samplePosition < (double) length)
            {
                const auto* data = audio.getReadPointer (0);
                const auto index = (int) samplePosition;
                const auto fraction = (float) (samplePosition - (double) index);
                const auto following = index + 1 < length ? data[index + 1] : (params.sampleLoop ? data[0] : 0.0f);
                centre += params.sampleLevel * (data[index] + fraction * (following - data[index]));
                samplePosition += params.sampleKeyTrack ? frequency / (double) std::max (1.0f, params.sampleRootHz) : 1.0;
            }
        }
    }
    auto sample = 0.5f * (left + right) + centre;
    auto side = 0.5f * (left - right);

    subPhase = wrap (subPhase + (params.subOctave == 0 ? frequency : frequency * 0.5) / sampleRate);

    // Filter: grundcutoff öppnas av envelopen, mer ju hårdare steget slås an.
    const auto envDepth = params.envAmount * (1.0f - params.velToEnv + params.velToEnv * velocity);
    const auto keyFollow = params.keyTrack > 0.0f ? std::pow ((float) frequency / 65.41f, params.keyTrack) : 1.0f;
    const auto cutoff = params.cutoffHz * keyFollow * std::exp2 (envDepth * envelope);
    const auto clampedCutoff = juce::jlimit (20.0f, (float) sampleRate * 0.45f, cutoff);
    const auto q = 0.5f * std::pow (16.0f, params.resonance);

    // LP 24 = två lågpass efter varandra; övriga typer ett filter (12 dB).
    using Type = juce::dsp::StateVariableTPTFilterType;
    const auto type = params.filterType == BassFilterType::bandpass ? Type::bandpass
                    : params.filterType == BassFilterType::highpass ? Type::highpass
                                                                    : Type::lowpass;
    for (auto* filter : { &filter2, &sideFilter2 })
    {
        filter->setType (type);
        filter->setCutoffFrequency (clampedCutoff);
        filter->setResonance (q);
    }

    if (params.filterType == BassFilterType::lowpass24)
    {
        for (auto* filter : { &filter1, &sideFilter1 })
        {
            filter->setCutoffFrequency (clampedCutoff);
            filter->setResonance (0.707f);
        }
        sample = filter1.processSample (0, sample);
        side = sideFilter1.processSample (0, side);
    }
    sample = filter2.processSample (0, sample);
    side = sideFilter2.processSample (0, side);
    noteTime += 1.0 / sampleRate;

    // Drive verkar på källan (före gaten), så att distorsionen låter likadant oavsett envelope.
    return { softClip (sample + side, params.drive), softClip (sample - side, params.drive), ampLevel * velocity * params.level };
}
