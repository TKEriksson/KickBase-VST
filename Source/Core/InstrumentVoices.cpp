#include "InstrumentVoices.h"

void InstrumentVoices::prepare (double sampleRate)
{
    fadeStep = (float) (1.0 / (stealRampMs * 0.001 * sampleRate));
    for (auto& voice : voices)
        voice.prepare (sampleRate);
    reset();
}

void InstrumentVoices::setSeed (juce::int64 seed)
{
    for (int k = 0; k < maxVoices; ++k)
        voices[(size_t) k].setSeed (seed + 1000 * (juce::int64) k);
}

void InstrumentVoices::reset()
{
    for (auto& voice : voices)
        voice.reset();
    slots = {};
    lastVoice = 0;
    noteCounter = 0;
    busy = numVoices > 1;
}

void InstrumentVoices::setParams (const InstrumentParams& newParams)
{
    params = newParams;
    baseCutoffHz = newParams.cutoffHz;
    if (cutoffLockHz > 0.0f)
        params.cutoffHz = cutoffLockHz;
    for (int k = 0; k < maxVoices; ++k)
        if (k == lastVoice || voices[(size_t) k].isActive())
            voices[(size_t) k].setParams (params);
}

void InstrumentVoices::setCutoffLock (float hz)
{
    const auto wanted = hz > 0.0f ? hz : 0.0f;
    if (wanted == cutoffLockHz)
        return;
    cutoffLockHz = wanted;
    params.cutoffHz = wanted > 0.0f ? wanted : baseCutoffHz;
    for (int k = 0; k < maxVoices; ++k)
        if (k == lastVoice || voices[(size_t) k].isActive())
            voices[(size_t) k].setParams (params);
}

void InstrumentVoices::setTempo (double newBpm)
{
    bpm = newBpm;
    for (int k = 0; k < maxVoices; ++k)
        if (k == lastVoice || voices[(size_t) k].isActive())
            voices[(size_t) k].setTempo (bpm);
}

bool InstrumentVoices::occupied (int voice) const
{
    const auto& slot = slots[(size_t) voice];
    return slot.pending || (voices[(size_t) voice].isActive() && ! slot.fading);
}

int InstrumentVoices::countSounding() const
{
    auto count = 0;
    for (int k = 0; k < maxVoices; ++k)
        count += occupied (k) ? 1 : 0;
    return count;
}

void InstrumentVoices::setNumVoices (int count)
{
    count = juce::jlimit (1, maxVoices, count);
    if (count == numVoices)
        return;
    numVoices = count;
    // De äldsta rösterna över gränsen tonas ut; den senast spelade blir kvar.
    for (auto sounding = countSounding(); sounding > numVoices; --sounding)
    {
        auto oldest = -1;
        for (int k = 0; k < maxVoices; ++k)
            if (occupied (k) && ! slots[(size_t) k].pending && k != lastVoice
                && (oldest < 0 || slots[(size_t) k].order < slots[(size_t) oldest].order))
                oldest = k;
        if (oldest < 0)
            break;
        startFade (oldest);
    }
    busy = busy || numVoices > 1;
}

void InstrumentVoices::startFade (int voice)
{
    auto& slot = slots[(size_t) voice];
    if (! slot.fading)
    {
        slot.fading = true;
        slot.fadeGain = 1.0f;
    }
    busy = true;
}

// Äldsta rösten i release, annars äldsta som hålls. Röster som tonas ut eller väntar räknas inte.
int InstrumentVoices::chooseVictim() const
{
    auto best = -1;
    auto bestReleasing = false;
    for (int k = 0; k < maxVoices; ++k)
    {
        const auto& slot = slots[(size_t) k];
        if (! occupied (k) || slot.pending)
            continue;
        const auto releasing = voices[(size_t) k].isReleasing();
        if (best < 0 || (releasing && ! bestReleasing)
            || (releasing == bestReleasing && slot.order < slots[(size_t) best].order))
        {
            best = k;
            bestReleasing = releasing;
        }
    }
    return best;
}

// En röst som aktiveras får fönstrets parametrar och tempo och börjar från noll, som en ny röst.
void InstrumentVoices::startNote (int voice, int semitones, float velocity, int gateSamples, int slice)
{
    auto& synth = voices[(size_t) voice];
    synth.setTempo (bpm);
    synth.setParams (params);
    synth.reset();
    synth.noteOn (semitones, velocity, gateSamples, slice);
    slots[(size_t) voice].order = ++noteCounter;
}

void InstrumentVoices::noteOn (int semitones, float velocity, int gateSamples, bool chordNote, int slice)
{
    if (numVoices == 1)
    {
        if (chordNote)
            return;
        // Monofoniskt som förut: samma röst, attacken börjar från nuvarande nivå.
        voices[(size_t) lastVoice].noteOn (semitones, velocity, gateSamples, slice);
        slots[(size_t) lastVoice].order = ++noteCounter;
        lastNotePoly = false;
        return;
    }

    if (! chordNote)
    {
        randomValue = modRandom.nextFloat();
        lastNotePoly = true;
    }
    auto victim = -1;
    if (countSounding() >= numVoices)
        if ((victim = chooseVictim()) >= 0)
            startFade (victim);

    for (int k = 0; k < maxVoices; ++k)
    {
        if (! voices[(size_t) k].isActive() && ! slots[(size_t) k].fading && ! slots[(size_t) k].pending)
        {
            startNote (k, semitones, velocity, gateSamples, slice);
            if (! chordNote)
                lastVoice = k;
            return;
        }
    }

    // Alla åtta klingar: tonen väntar på den stulna rösten (eller den röst som tonats ut längst).
    auto target = victim;
    for (int k = 0; target < 0 && k < maxVoices; ++k)
        if (slots[(size_t) k].fading && ! slots[(size_t) k].pending)
            target = k;
    if (target < 0)
    {
        target = 0;
        for (int k = 1; k < maxVoices; ++k)
            if (slots[(size_t) k].fadeGain < slots[(size_t) target].fadeGain)
                target = k;
    }
    startFade (target);
    auto& slot = slots[(size_t) target];
    slot.pending = true;
    slot.pendingSemitones = semitones;
    slot.pendingVelocity = velocity;
    slot.pendingGate = gateSamples;
    slot.pendingSlice = slice;
    if (! chordNote)
        lastVoice = target;
}

bool InstrumentVoices::glideTo (int semitones, int samples)
{
    if (slots[(size_t) lastVoice].fading || ! voices[(size_t) lastVoice].isSounding())
        return false;

    // Alla klingande toner flyttas lika långt som den senast spelade, så att ett ackord glider som ett ackord.
    const auto shift = semitones - voices[(size_t) lastVoice].getTargetSemitones();
    for (int k = 0; k < maxVoices; ++k)
        if (! slots[(size_t) k].fading && voices[(size_t) k].isSounding())
            voices[(size_t) k].glideTo (voices[(size_t) k].getTargetSemitones() + shift, samples);
    return true;
}

void InstrumentVoices::release()
{
    for (int k = 0; k < maxVoices; ++k)
    {
        slots[(size_t) k].pending = false;
        if (k == lastVoice || voices[(size_t) k].isActive())
            voices[(size_t) k].release();
    }
}

void InstrumentVoices::fadeOutQuickly (double milliseconds)
{
    for (int k = 0; k < maxVoices; ++k)
    {
        slots[(size_t) k].pending = false;
        if (k == lastVoice || voices[(size_t) k].isActive())
            voices[(size_t) k].fadeOutQuickly (milliseconds);
    }
}

InstrumentVoices::Output InstrumentVoices::next()
{
    if (! busy)
        return voices[(size_t) lastVoice].next();

    auto left = 0.0f, right = 0.0f;
    auto anyFading = false;
    for (int k = 0; k < maxVoices; ++k)
    {
        auto& voice = voices[(size_t) k];
        auto& slot = slots[(size_t) k];
        if (! voice.isActive() && ! slot.fading)
            continue;
        const auto out = voice.next();
        auto gain = out.amplitude;
        if (slot.fading)
        {
            gain *= slot.fadeGain;
            slot.fadeGain -= fadeStep;
            if (slot.fadeGain <= 0.0f)
            {
                voice.reset();
                slot.fading = false;
                slot.fadeGain = 1.0f;
                if (slot.pending)
                {
                    slot.pending = false;
                    startNote (k, slot.pendingSemitones, slot.pendingVelocity, slot.pendingGate, slot.pendingSlice);
                }
            }
            else
            {
                anyFading = true;
            }
        }
        left += out.left * gain;
        right += out.right * gain;
    }
    busy = numVoices > 1 || anyFading;
    return { left, right, 1.0f };
}

bool InstrumentVoices::isActive() const
{
    for (int k = 0; k < maxVoices; ++k)
        if (voices[(size_t) k].isActive() || slots[(size_t) k].pending)
            return true;
    return false;
}

float InstrumentVoices::getModulationValue (int source) const
{
    if (! lastNotePoly)
        return voices[(size_t) lastVoice].getModulationValue (source);
    if (source == 3)
        return randomValue;
    // AMP ENV / FILT ENV i poly: den senaste rösten, som ENV 2/3.
    return voices[(size_t) lastVoice].isActive() ? voices[(size_t) lastVoice].getModulationValue (source) : 0.0f;
}
