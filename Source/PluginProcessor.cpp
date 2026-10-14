#include "PluginProcessor.h"
#include "PluginEditor.h"

MyOwnFruitProcessor::MyOwnFruitProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice());

    synth.addSound (new SynthSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout MyOwnFruitProcessor::createParameterLayout()
{
    using namespace juce;

    // Skev skala så att korta tider får mer av rattens vandring.
    auto timeRange = NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.3f);
    auto cutoffRange = NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.25f);

    return {
        std::make_unique<AudioParameterChoice> (ParameterID { ParamID::waveform, 1 }, "Waveform",
                                                StringArray { "Sine", "Saw", "Square", "Triangle" }, 1),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::attack, 1 },  "Attack",  timeRange, 0.01f),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::decay, 1 },   "Decay",   timeRange, 0.2f),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::sustain, 1 }, "Sustain", 0.0f, 1.0f, 0.7f),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::release, 1 }, "Release", timeRange, 0.3f),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::cutoff, 1 },  "Cutoff",  cutoffRange, 2000.0f),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::resonance, 1 }, "Resonance",
                                               NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.5f), 0.707f),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::gain, 1 }, "Gain", 0.0f, 1.0f, 0.5f),
    };
}

VoiceParams MyOwnFruitProcessor::readVoiceParams() const
{
    auto value = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    VoiceParams p;
    p.waveform = (Waveform) (int) value (ParamID::waveform);
    p.adsr = { value (ParamID::attack), value (ParamID::decay), value (ParamID::sustain), value (ParamID::release) };
    p.cutoff = value (ParamID::cutoff);
    p.resonance = value (ParamID::resonance);
    return p;
}

void MyOwnFruitProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    gain.reset (sampleRate, 0.02);
    gain.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamID::gain)->load());
}

bool MyOwnFruitProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MyOwnFruitProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    const auto params = readVoiceParams();
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            voice->setParams (params);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    gain.setTargetValue (apvts.getRawParameterValue (ParamID::gain)->load());
    gain.applyGain (buffer, buffer.getNumSamples());
}

juce::AudioProcessorEditor* MyOwnFruitProcessor::createEditor()
{
    return new MyOwnFruitEditor (*this);
}

void MyOwnFruitProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MyOwnFruitProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// JUCE anropar den här för att skapa plugin-instansen.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyOwnFruitProcessor();
}
