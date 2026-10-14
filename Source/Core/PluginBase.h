#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TransportStatus.h"

// Det som instrumentet och FX-versionen har gemensamt: parametrar, sparat läge och klockan.
class PluginBase : public juce::AudioProcessor
{
public:
    PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout)
        : AudioProcessor (buses), apvts (*this, nullptr, "Parameters", std::move (layout))
    {
    }

    void releaseResources() override {}
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    juce::AudioProcessorValueTreeState apvts;
    TransportStatus transportStatus;

protected:
    // Läser DAW:ens position. Får bara anropas från processBlock.
    juce::Optional<juce::AudioPlayHead::PositionInfo> getHostPosition() const
    {
        if (auto* hostPlayHead = getPlayHead())
            return hostPlayHead->getPosition();

        return juce::nullopt;
    }

    float getParam (const char* paramID) const { return apvts.getRawParameterValue (paramID)->load(); }

    TransportClock clock;
};
