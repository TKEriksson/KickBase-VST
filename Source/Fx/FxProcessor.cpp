#include "FxProcessor.h"
#include "FxEditor.h"

namespace
{
    constexpr auto gateLengthID = "gateLength";

    constexpr double gateRampSeconds = 0.002;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        // Andel av steget som gaten är öppen.
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gateLengthID, 1 }, "Gate Length",
                                                                 juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.5f));
        return layout;
    }
}

FxProcessor::FxProcessor()
    : PluginBase (BusesProperties()
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                  createLayout())
{
}

void FxProcessor::prepareToPlay (double sampleRate, int)
{
    clock.prepare (sampleRate);
    gateSamplesLeft = 0;
    gateLevel = 0.0f;
    gateRampStep = (float) (1.0 / (gateRampSeconds * sampleRate));
}

bool FxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet() == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void FxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    clock.beginBlock (getHostPosition(), numSamples);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto gateLength = (double) getParam (gateLengthID);
    const auto samplesPerStep = clock.getSamplesPerQuarter() / quarterCursor.stepsPerQuarter;

    auto processedUpTo = 0;
    clock.forEachStep (quarterCursor, 0, numSamples, [&] (int offset, int)
    {
        applyGate (buffer, processedUpTo, offset);
        gateSamplesLeft = (int) (gateLength * samplesPerStep);
        processedUpTo = offset;
    });
    applyGate (buffer, processedUpTo, numSamples);

    transportStatus.publish (clock);
}

void FxProcessor::applyGate (juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    for (int i = startSample; i < endSample; ++i)
    {
        const auto target = gateSamplesLeft > 0 ? 1.0f : 0.0f;
        gateLevel = target > gateLevel ? std::min (target, gateLevel + gateRampStep)
                                       : std::max (target, gateLevel - gateRampStep);

        if (gateSamplesLeft > 0)
            --gateSamplesLeft;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * gateLevel);
    }
}

juce::AudioProcessorEditor* FxProcessor::createEditor()
{
    return new FxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FxProcessor();
}
