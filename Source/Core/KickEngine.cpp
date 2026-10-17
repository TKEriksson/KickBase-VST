#include "KickEngine.h"

namespace
{
    constexpr double fadeOutSeconds = 0.003;

    // Drive: mjuk (tanh), hård klippning eller rör (osymmetrisk, ger jämna övertoner).
    // Normaliserad så att toppvärdet 1 förblir ungefär 1 oavsett drive.
    float clip (float x, float drive, int mode)
    {
        if (drive <= 0.0f)
            return x;

        const auto gain = 1.0f + drive * 9.0f;
        switch (mode)
        {
            case 1: return juce::jlimit (-1.0f, 1.0f, gain * x) / std::min (1.0f, gain);
            case 2:
            {
                constexpr float bias = 0.3f;
                const auto y = std::tanh (gain * x + bias) - std::tanh (bias);
                return y / (std::tanh (gain + bias) - std::tanh (bias));
            }
            case 3: return std::sin (juce::MathConstants<float>::halfPi * gain * x * 0.5f);   // vikning: höga värden viks tillbaka
            default: return std::tanh (gain * x) / std::tanh (gain);
        }
    }

    // Kroppens vågform (fas 0..1, börjar vid noll och går uppåt som en sinus).
    float bodyWave (double phase, int wave)
    {
        constexpr auto twoPi = juce::MathConstants<double>::twoPi;
        switch (wave)
        {
            case 1:
            {
                const auto p = phase - 0.25 - std::floor (phase - 0.25);
                return (float) (4.0 * std::abs (p - 0.5) - 1.0);
            }
            case 2: return (float) (std::tanh (4.0 * std::sin (twoPi * phase)) / std::tanh (4.0));
            case 3:
            {
                // Såg av de sex första övertonerna, så att den inte aliasar vid pitch-svepets snabba start.
                auto sum = 0.0;
                for (int k = 1; k <= 6; ++k)
                    sum += ((k % 2 == 1) ? 1.0 : -1.0) * std::sin (twoPi * k * phase) / k;
                return (float) (sum * 0.85);
            }
            default: return (float) std::sin (twoPi * phase);
        }
    }
}

void KickEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    fadeStep = (float) (1.0 / (fadeOutSeconds * sampleRate));

    for (auto& voice : voices)
    {
        voice.clickFilter.prepare ({ sampleRate, 512, 1 });
        voice.clickFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        voice.clickFilter.setResonance (0.8f);
        voice.noiseFilter.prepare ({ sampleRate, 512, 1 });
        voice.noiseFilter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        voice.noiseFilter.setResonance (0.7071f);
    }
    highCut.prepare ({ sampleRate, 512, 1 });
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highCut.setResonance (0.7071f);
    lowCut.prepare ({ sampleRate, 512, 1 });
    lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    lowCut.setResonance (0.7071f);

    reset();
}

void KickEngine::reset()
{
    for (auto& voice : voices)
    {
        voice.active = false;
        voice.clickFilter.reset();
        voice.noiseFilter.reset();
    }
    highCut.reset();
    lowCut.reset();
    envelope = 0.0f;
}

void KickEngine::trigger (float velocity, int transposeSemitones)
{
    // Den gamla rösten tonas ut på några millisekunder i stället för att klippas.
    auto& old = voices[currentVoice];
    if (old.active)
        old.fadingOut = true;

    currentVoice = 1 - currentVoice;
    ++triggerCount;
    auto& voice = voices[currentVoice];
    voice.active = true;
    voice.time = 0.0;
    voice.phase = 0.0; // sinus från noll: ingen DC-knäpp i anslaget
    voice.velocity = velocity;
    voice.transpose = std::exp2 ((float) transposeSemitones / 12.0f);
    voice.fade = 1.0f;
    voice.fadingOut = false;
    voice.subPhase = 0.0;
    voice.fmPhase = 0.0;
    voice.pitchScale = 1.0f - params.velPitch * (1.0f - juce::jlimit (0.0f, 1.0f, velocity));
    voice.sampleFollower = 0.0f;
    voice.samplePosition = -(double) params.sampleOffsetMs * 0.001 * sampleRate * std::exp2 ((double) params.sampleTuneSemis / 12.0); // plus = väntar, minus = hoppar in
    voice.clickFilter.reset();
    voice.noiseFilter.reset();
}

float KickEngine::nextSample()
{
    auto out = 0.0f;
    for (auto& voice : voices)
        if (voice.active)
            out += renderVoice (voice);

    const auto& current = voices[currentVoice];
    // Duckningen följer kicken: syntesens envelope (inte vid ersättning) eller samplingens nivå, den som är högst.
    const auto synthEnvelope = params.sampleMode == 2 && clickSample != nullptr ? 0.0f : amplitudeAt (params, current.time);
    envelope = current.active ? std::max (synthEnvelope, current.sampleFollower) * current.fade : 0.0f;

    auto y = clip (out, params.drive, params.clipMode);
    if (params.crush > 0.0f)
    {
        const auto levels = std::exp2 (15.0f - params.crush * 12.0f); // 16 bitar ... 4 bitar
        y = std::round (y * levels) / levels;
    }
    if (params.lowCutHz > 21.0f)
    {
        lowCut.setCutoffFrequency (juce::jlimit (20.0f, 400.0f, params.lowCutHz));
        y = lowCut.processSample (0, y);
    }
    if (params.highCutHz < 19000.0f)
    {
        highCut.setCutoffFrequency (juce::jlimit (200.0f, (float) sampleRate * 0.45f, params.highCutHz));
        y = highCut.processSample (0, y);
    }
    return y * params.level;
}

float KickEngine::renderVoice (Voice& voice)
{
    const auto t = voice.time;
    const auto hasSample = clickSample != nullptr && clickSample->audio.getNumSamples() > 0;
    const auto replace = hasSample && params.sampleMode == 2;
    const auto layered = hasSample && params.sampleMode >= 1;

    if (t > totalSeconds (params, clickSample, sampleRate) || voice.fade <= 0.0f)
    {
        voice.active = false;
        return 0.0f;
    }

    constexpr auto twoPi = juce::MathConstants<double>::twoPi;
    auto body = 0.0f;
    auto top = 0.0f;
    auto click = 0.0f;

    if (! replace)
    {
        const auto amplitude = amplitudeAt (params, t);

        // Kropp: en vågform vars frekvens sveper ner till tailHz: ett snabbt svep (pitch) och ett långsamt (glide) ovanpå
        // varandra. Lägre velocity ger ett mindre svep (velPitch).
        const auto frequency = params.tailHz * voice.transpose * std::exp2 (pitchSemitonesAt (params, t) * voice.pitchScale / 12.0f);

        // FM: en andra sinus (fmRatio gånger frekvensen) flyttar kroppens fas, mest i början.
        auto fmShift = 0.0;
        if (params.fm > 0.0f)
        {
            const auto fmEnvelope = std::exp (-t / std::max (0.001, params.fmDecayMs * 0.001));
            fmShift = 0.8 * params.fm * fmEnvelope * std::sin (twoPi * voice.fmPhase);
            voice.fmPhase += frequency * params.fmRatio / sampleRate;
            voice.fmPhase -= std::floor (voice.fmPhase);
        }

        const auto phase = voice.phase + params.startPhase + fmShift;
        body = bodyWave (phase, params.wave);

        // Sub: en sinus en oktav under kroppen.
        if (params.sub > 0.0f)
        {
            body += params.sub * (float) std::sin (twoPi * voice.subPhase);
            voice.subPhase += 0.5 * frequency / sampleRate;
            voice.subPhase -= std::floor (voice.subPhase);
        }

        // Övertoner 2-9 på kroppen (följer svepet), som klingar av för sig.
        if (params.harmonics > 0.0f)
        {
            auto sum = 0.0f, norm = 0.0f;
            for (int n = 2; n <= 9; ++n)
            {
                const auto gain = std::pow ((float) n, -params.harmonicsTilt);
                sum += gain * (float) std::sin (twoPi * n * (voice.phase + params.startPhase));
                norm += gain;
            }
            const auto decay = (float) std::exp (-t / std::max (0.001, params.harmonicsDecayMs * 0.001));
            body += params.harmonics * decay * sum / std::max (1.0f, norm);
        }
        body *= amplitude;
        voice.phase += frequency / sampleRate;
        voice.phase -= std::floor (voice.phase);

        // Brusig topp: högpassat brus som avtar för sig (luft och snärt ovanpå kicken).
        if (params.noise > 0.0f)
        {
            voice.noiseFilter.setCutoffFrequency (juce::jlimit (200.0f, (float) sampleRate * 0.45f, params.noiseToneHz));
            const auto decay = (float) std::exp (-t / std::max (0.002, params.noiseDecayMs * 0.001));
            top = voice.noiseFilter.processSample (0, random.nextFloat() * 2.0f - 1.0f) * decay * params.noise;
        }
    }

    // Samplingen: som klick (de första millisekunderna, nivå = clickLevel), eller som hel kick (lager/ersätt, nivå = sampleLevel).
    auto sampled = 0.0f;
    if (hasSample)
    {
        const auto& audio = clickSample->audio;
        const auto length = audio.getNumSamples();
        if (voice.samplePosition < (double) length - 1.0)
        {
            if (voice.samplePosition >= 0.0)
            {
                const auto* data = audio.getReadPointer (0);
                const auto index = (int) voice.samplePosition;
                const auto fraction = (float) (voice.samplePosition - (double) index);
                sampled = data[index] + fraction * (data[index + 1] - data[index]);
            }
            voice.samplePosition += std::exp2 ((double) params.sampleTuneSemis / 12.0);
        }
        voice.sampleFollower = std::max (std::abs (sampled) * (layered ? params.sampleLevel : params.clickLevel),
                                         voice.sampleFollower * (float) std::exp (-1.0 / (0.012 * sampleRate)));
    }
    else
    {
        const auto clickSeconds = params.clickLengthMs * 0.001;
        if (t < clickSeconds)
        {
            const auto shape = (float) (1.0 - t / clickSeconds);
            voice.clickFilter.setCutoffFrequency (juce::jlimit (100.0f, (float) sampleRate * 0.45f, params.clickToneHz));
            click = voice.clickFilter.processSample (0, random.nextFloat() * 2.0f - 1.0f) * shape * shape * 2.0f;
        }
    }

    voice.time += 1.0 / sampleRate;

    if (voice.fadingOut)
        voice.fade = std::max (0.0f, voice.fade - fadeStep);

    const auto sampleOut = layered ? sampled * params.sampleLevel : sampled * params.clickLevel;
    return (body + (hasSample ? sampleOut : click * params.clickLevel) + top) * voice.velocity * voice.fade;
}

double KickEngine::totalSeconds (const KickParams& p, const ClickSample* sample, double sampleRate)
{
    const auto synth = lengthSeconds (p);
    if (sample == nullptr || sample->audio.getNumSamples() <= 0 || p.sampleMode == 0)
        return synth;

    const auto sampleSeconds = (double) sample->audio.getNumSamples() / sampleRate / std::exp2 ((double) p.sampleTuneSemis / 12.0)
                             + std::max (0.0, (double) p.sampleOffsetMs * 0.001);
    return p.sampleMode == 2 ? sampleSeconds : std::max (synth, sampleSeconds);
}

float KickEngine::amplitudeAt (const KickParams& p, double seconds)
{
    if (p.ampEnv != nullptr)
        return seconds >= lengthSeconds (p) ? 0.0f : p.ampEnv->lookup (seconds / std::max (0.05f, p.lengthScale));

    const auto attack = p.attackMs * 0.001;
    const auto hold = p.holdMs * 0.001;
    const auto decay = std::max (0.001, p.decayMs * 0.001);

    if (seconds < attack)
        return (float) (seconds / attack);

    seconds -= attack;
    if (seconds < hold)
        return 1.0f;

    seconds -= hold;
    if (seconds < decay)
        return (float) std::pow (1.0 - seconds / decay, (double) p.decayCurve);

    return 0.0f;
}

float KickEngine::pitchEnvelopeAt (const KickParams& p, double seconds)
{
    const auto time = std::max (0.001, p.pitchTimeMs * 0.001);
    if (seconds >= time)
        return 0.0f;

    return (float) std::pow (1.0 - seconds / time, (double) p.pitchCurve);
}

float KickEngine::glideEnvelopeAt (const KickParams& p, double seconds)
{
    const auto time = std::max (0.001, p.glideTimeMs * 0.001);
    return seconds >= time ? 0.0f : (float) (1.0 - seconds / time);
}

float KickEngine::pitchSemitonesAt (const KickParams& p, double seconds)
{
    if (p.pitchEnv != nullptr)
        return p.pitchEnv->lookup (seconds / std::max (0.05f, p.lengthScale)) * p.pitchAmount;

    return p.pitchSemis * pitchEnvelopeAt (p, seconds) + p.glideSemis * glideEnvelopeAt (p, seconds);
}

double KickEngine::lengthSeconds (const KickParams& p)
{
    if (p.ampEnv != nullptr)
        return p.ampEnv->endSeconds() * std::max (0.05f, p.lengthScale);
    return (p.attackMs + p.holdMs + p.decayMs) * 0.001;
}

juce::AudioBuffer<float> KickEngine::render (const KickParams& p, const ClickSample* click, double sampleRate)
{
    KickEngine engine;
    engine.prepare (sampleRate);
    auto params = p;
    params.level = 1.0f;
    engine.setParams (params);
    engine.setClickSample (click);
    engine.trigger (1.0f);

    juce::AudioBuffer<float> audio (1, std::max (1, (int) ((totalSeconds (params, click, sampleRate) + 0.01) * sampleRate)));
    for (int i = 0; i < audio.getNumSamples(); ++i)
        audio.setSample (0, i, engine.nextSample());
    return audio;
}
