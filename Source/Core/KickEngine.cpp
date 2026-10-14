#include "KickEngine.h"

namespace
{
    constexpr double fadeOutSeconds = 0.003;

    float softClip (float x, float drive)
    {
        if (drive <= 0.0f)
            return x;

        // tanh, normaliserad så att toppvärdet 1 förblir 1 oavsett drive.
        const auto gain = 1.0f + drive * 9.0f;
        return std::tanh (gain * x) / std::tanh (gain);
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

    reset();
}

void KickEngine::reset()
{
    for (auto& voice : voices)
    {
        voice.active = false;
        voice.clickFilter.reset();
    }
    envelope = 0.0f;
}

void KickEngine::trigger (float velocity)
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

    return softClip (out, params.drive) * params.level;
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

    // Kropp: sinus vars frekvens sveper från tailHz * 2^(pitchSemis/12) ner till tailHz.
    const auto frequency = params.tailHz * std::exp2 (params.pitchSemis / 12.0f * pitchEnvelopeAt (params, t));
    const auto body = (float) std::sin (juce::MathConstants<double>::twoPi * voice.phase) * amplitude;
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

double KickEngine::lengthSeconds (const KickParams& p)
{
    return (p.attackMs + p.holdMs + p.decayMs) * 0.001;
}
