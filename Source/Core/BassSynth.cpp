#include "BassSynth.h"

namespace
{
    constexpr double filterAttackSeconds = 0.001;
    constexpr float maxFmIndex = 8.0f; // FM-mängd 1 = modulationsindex 8 (mycket övertoner)

    // Mjukar upp vågformens hopp så att höga toner inte viker ner som falska övertoner.
    double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    float softClip (float x, float drive)
    {
        if (drive <= 0.0f)
            return x;

        const auto gain = 1.0f + drive * 9.0f;
        return std::tanh (gain * x) / std::tanh (gain);
    }

    float waveform (int index, double t, double dt, double pulseWidth)
    {
        switch (index)
        {
            case 0: return (float) std::sin (juce::MathConstants<double>::twoPi * t);
            case 1: return (float) (1.0 - 4.0 * std::abs (t - 0.5));
            case 2: return (float) (2.0 * t - 1.0 - polyBlep (t, dt));
            default:
            {
                // Fyrkant med pulsbredd; medelvärdet dras bort så att smal puls inte ger DC.
                auto square = (t < pulseWidth ? 1.0 : -1.0) + polyBlep (t, dt) - polyBlep (std::fmod (t + 1.0 - pulseWidth, 1.0), dt);
                return (float) (square - (2.0 * pulseWidth - 1.0));
            }
        }
    }
}

void BassSynth::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto* filter : { &filter1, &filter2 })
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
    slideHeld = false;
    filter1.reset();
    filter2.reset();
}

void BassSynth::noteOn (int semitones, float newVelocity, int gateSamples, bool slide)
{
    const auto frequency = params.rootHz * std::exp2 (semitones / 12.0f);
    targetLogFrequency = std::log2 ((double) frequency);

    const auto legato = slideHeld && (stage == Stage::attack || stage == Stage::decay);

    if (legato)
    {
        // Glid till den nya tonen utan att slå an envelopen igen.
        gliding = true;
    }
    else
    {
        logFrequency = targetLogFrequency;
        gliding = false;

        // Samma startfas varje slag ger ett jämnt, tight anslag. Men om förra tonen fortfarande
        // hörs skulle ett fashopp knäppa, så då fortsätter fasen där den är.
        if (ampLevel < 0.05f)
        {
            phase = 0.0;
            subPhase = 0.0;
            phase2 = 0.0;
        }

        noteTime = 0.0;
        velocity = newVelocity;
        stage = Stage::attack; // attacken börjar från nuvarande nivå, så inget knäpp
    }

    gateRemaining = gateSamples;
    slideHeld = slide;
}

void BassSynth::release()
{
    if (stage != Stage::idle)
        stage = Stage::release;

    slideHeld = false;
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

float BassSynth::oscillator (float shape, float pulseWidth, double t, double dt)
{
    // Blanda de två närmaste vågformerna: 0 sinus, 1 triangel, 2 sågtand, 3 fyrkant.
    const auto s = juce::jlimit (0.0f, 3.0f, shape);
    const auto lower = std::min (2, (int) s);
    const auto mix = s - (float) lower;
    const auto pw = (double) juce::jlimit (0.05f, 0.95f, pulseWidth);

    const auto a = waveform (lower, t, dt, pw);
    if (mix <= 0.0f)
        return a;
    return a + (waveform (lower + 1, t, dt, pw) - a) * mix;
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

    // Glide i log-frekvens så att varje halvton tar lika lång tid.
    if (gliding)
        logFrequency += (targetLogFrequency - logFrequency) * coefficientFor (params.glideMs, sampleRate);

    const auto frequency = std::exp2 (logFrequency);
    const auto dt = frequency / sampleRate;
    const auto dt2 = dt * std::exp2 ((params.osc2Semis + params.osc2Detune * 0.01f) / 12.0f);
    const auto envelope = filterEnvelopeAt (params, noteTime);

    // Oscillator 2: hörs med sin nivå och modulerar oscillator 1 med FM (fasmodulering).
    const auto osc2 = oscillator (params.osc2Shape, params.osc2PulseWidth, phase2, dt2);
    const auto fmIndex = maxFmIndex * (params.fmAmount + params.fmEnvAmount * envelope);
    auto modulatedPhase = phase;
    if (fmIndex > 0.0f)
    {
        modulatedPhase += fmIndex * osc2 / juce::MathConstants<double>::twoPi;
        modulatedPhase -= std::floor (modulatedPhase);
    }

    auto sample = oscillator (params.shape, params.pulseWidth, modulatedPhase, dt);
    sample += params.osc2Level * osc2;
    sample += params.sub * (float) std::sin (juce::MathConstants<double>::twoPi * subPhase);
    if (params.noise > 0.0f)
        sample += params.noise * (random.nextFloat() * 2.0f - 1.0f);

    phase += dt;
    phase -= std::floor (phase);
    phase2 += dt2;
    phase2 -= std::floor (phase2);
    subPhase += params.subOctave == 0 ? dt : dt * 0.5;
    subPhase -= std::floor (subPhase);

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
    filter2.setType (type);
    filter2.setCutoffFrequency (clampedCutoff);
    filter2.setResonance (q);

    if (params.filterType == BassFilterType::lowpass24)
    {
        filter1.setCutoffFrequency (clampedCutoff);
        filter1.setResonance (0.707f);
        sample = filter1.processSample (0, sample);
    }
    sample = filter2.processSample (0, sample);
    noteTime += 1.0 / sampleRate;

    // Drive verkar på källan (före gaten), så att distorsionen låter likadant oavsett envelope.
    return { softClip (sample, params.drive), ampLevel * velocity * params.level };
}
