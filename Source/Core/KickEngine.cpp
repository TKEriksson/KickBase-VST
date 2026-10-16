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
            default: return std::tanh (gain * x) / std::tanh (gain);
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
    }
    highCut.prepare ({ sampleRate, 512, 1 });
    highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    highCut.setResonance (0.7071f);

    reset();
}

void KickEngine::reset()
{
    for (auto& voice : voices)
    {
        voice.active = false;
        voice.clickFilter.reset();
    }
    highCut.reset();
    envelope = 0.0f;
}

void KickEngine::trigger (float velocity, int transposeSemitones)
{
    // Den gamla rösten tonas ut på några millisekunder i stället för att klippas.
    auto& old = voices[currentVoice];
    if (old.active)
        old.fadingOut = true;

    currentVoice = 1 - currentVoice;
    auto& voice = voices[currentVoice];
    voice.active = true;
    voice.time = 0.0;
    voice.phase = 0.0; // sinus från noll: ingen DC-knäpp i anslaget
    voice.velocity = velocity;
    voice.transpose = std::exp2 ((float) transposeSemitones / 12.0f);
    voice.fade = 1.0f;
    voice.fadingOut = false;
    voice.sampleIndex = 0;
    voice.clickFilter.reset();
}

float KickEngine::nextSample()
{
    auto out = 0.0f;
    for (auto& voice : voices)
        if (voice.active)
            out += renderVoice (voice);

    const auto& current = voices[currentVoice];
    envelope = current.active ? amplitudeAt (params, current.time) * current.fade : 0.0f;

    auto y = clip (out, params.drive, params.clipMode);
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
    const auto amplitude = amplitudeAt (params, t);

    if (t > lengthSeconds (params) || voice.fade <= 0.0f)
    {
        voice.active = false;
        return 0.0f;
    }

    // Kropp: sinus vars frekvens sveper ner till tailHz: ett snabbt svep (pitch) och ett
    // långsamt (glide) ovanpå varandra.
    const auto frequency = params.tailHz * voice.transpose * std::exp2 (pitchSemitonesAt (params, t) / 12.0f);
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;
    auto body = (float) std::sin (twoPi * (voice.phase + params.startPhase));

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

    // Klick: inladdad sampling om det finns en, annars bandpassfiltrerat brus.
    auto click = 0.0f;
    if (clickSample != nullptr && clickSample->audio.getNumSamples() > 0)
    {
        if (voice.sampleIndex < clickSample->audio.getNumSamples())
            click = clickSample->audio.getSample (0, voice.sampleIndex++);
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

    return (body + click * params.clickLevel) * voice.velocity * voice.fade;
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

    juce::AudioBuffer<float> audio (1, std::max (1, (int) ((lengthSeconds (params) + 0.01) * sampleRate)));
    for (int i = 0; i < audio.getNumSamples(); ++i)
        audio.setSample (0, i, engine.nextSample());
    return audio;
}
