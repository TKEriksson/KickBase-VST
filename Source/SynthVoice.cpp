#include "SynthVoice.h"

void SynthVoice::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    adsr.setSampleRate (sampleRate);

    filter.prepare ({ sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) numChannels });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    voiceBuffer.setSize (numChannels, maxBlockSize);
}

void SynthVoice::setParams (const VoiceParams& newParams)
{
    params = newParams;
    adsr.setParameters (params.adsr);
    filter.setCutoffFrequency (params.cutoff);
    filter.setResonance (params.resonance);
}

bool SynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*> (sound) != nullptr;
}

void SynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    const auto frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    phaseIncrement = frequency / getSampleRate();
    phase = 0.0;
    level = velocity;

    filter.reset();
    adsr.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff(); // låt release-fasen klinga ut, rösten frigörs i renderNextBlock
    }
    else
    {
        adsr.reset();
        clearCurrentNote();
    }
}

float SynthVoice::nextOscillatorSample()
{
    const auto p = (float) phase;
    float sample = 0.0f;

    switch (params.waveform)
    {
        case Waveform::sine:     sample = std::sin (juce::MathConstants<float>::twoPi * p); break;
        case Waveform::saw:      sample = 2.0f * p - 1.0f; break;
        case Waveform::square:   sample = p < 0.5f ? 1.0f : -1.0f; break;
        case Waveform::triangle: sample = 1.0f - 4.0f * std::abs (p - 0.5f); break;
    }

    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return sample;
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    // Rendera i en egen buffer först, så att filter och envelope bara påverkar den här rösten.
    voiceBuffer.clear();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = nextOscillatorSample() * level;
        for (int ch = 0; ch < voiceBuffer.getNumChannels(); ++ch)
            voiceBuffer.setSample (ch, i, sample);
    }

    auto block = juce::dsp::AudioBlock<float> (voiceBuffer).getSubBlock (0, (size_t) numSamples);
    filter.process (juce::dsp::ProcessContextReplacing<float> (block));

    adsr.applyEnvelopeToBuffer (voiceBuffer, 0, numSamples);

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
        output.addFrom (ch, startSample, voiceBuffer, ch % voiceBuffer.getNumChannels(), 0, numSamples);

    if (! adsr.isActive())
        clearCurrentNote();
}
