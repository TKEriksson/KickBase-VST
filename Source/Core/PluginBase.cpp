#include "PluginBase.h"
#include "Presets.h"

namespace
{
    constexpr double maxClickSeconds = 2.0;
    const juce::Identifier clickTreeType { "ClickSample" };
}

PluginBase::PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout)
    : AudioProcessor (buses),
      apvts (*this, nullptr, "Parameters", std::move (layout)),
      params (apvts)
{
}

void PluginBase::prepareBase (double sampleRate)
{
    currentSampleRate = sampleRate;
    clock.prepare (sampleRate);
    rebuildClickForSampleRate();
}

juce::Optional<juce::AudioPlayHead::PositionInfo> PluginBase::getHostPosition() const
{
    if (auto* hostPlayHead = getPlayHead())
        return hostPlayHead->getPosition();

    return juce::nullopt;
}

void PluginBase::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", presetName, nullptr);
    state.appendChild (pattern.toValueTree(), nullptr);
    state.appendChild (clickToValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginBase::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    presetName = state.getProperty ("preset", "Init").toString();

    if (auto patternTree = state.getChildWithName (Pattern::treeType); patternTree.isValid())
    {
        pattern.fromValueTree (patternTree);
        state.removeChild (patternTree, nullptr);
        ++patternVersion;
    }

    if (auto clickTree = state.getChildWithName (clickTreeType); clickTree.isValid())
    {
        clickFromValueTree (clickTree);
        state.removeChild (clickTree, nullptr);
    }

    apvts.replaceState (state);
}

void PluginBase::patternChanged()
{
    ++patternVersion;
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PluginBase::applyPatternTemplate (PatternTemplate t)
{
    const auto grid = pattern.applyTemplate (t);

    if (auto* gridParam = apvts.getParameter (ParamID::bassGrid))
        gridParam->setValueNotifyingHost (gridParam->convertTo0to1 ((float) grid));

    patternChanged();
}

void PluginBase::loadPreset (int index)
{
    Presets::apply (index, apvts, pattern);
    presetName = Presets::name (index);
    patternChanged();
}

bool PluginBase::loadClickSample (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
        return false;

    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxClickSeconds * reader->sampleRate));
    juce::AudioBuffer<float> fileAudio ((int) reader->numChannels, length);
    reader->read (&fileAudio, 0, length, 0, true, true);

    // Mixa ner till mono.
    juce::AudioBuffer<float> mono (1, length);
    mono.clear();
    for (int ch = 0; ch < fileAudio.getNumChannels(); ++ch)
        mono.addFrom (0, 0, fileAudio, ch, 0, length, 1.0f / (float) fileAudio.getNumChannels());

    setClickOriginal (std::move (mono), reader->sampleRate, file.getFileNameWithoutExtension());
    patternChanged();
    return true;
}

void PluginBase::clearClickSample()
{
    setClickOriginal ({}, 0.0, {});
    patternChanged();
}

juce::String PluginBase::getClickSampleName() const
{
    const juce::ScopedLock lock (clickLock);
    return clickName;
}

void PluginBase::setClickOriginal (juce::AudioBuffer<float> audio, double rate, const juce::String& name)
{
    {
        const juce::ScopedLock lock (clickLock);
        clickOriginal = std::move (audio);
        clickOriginalRate = rate;
        clickName = name;
    }
    rebuildClickForSampleRate();
}

void PluginBase::rebuildClickForSampleRate()
{
    std::unique_ptr<ClickSample> fresh;

    {
        const juce::ScopedLock lock (clickLock);

        if (clickOriginal.getNumSamples() > 0 && clickOriginalRate > 0.0)
        {
            fresh = std::make_unique<ClickSample>();
            fresh->name = clickName;

            const auto ratio = clickOriginalRate / currentSampleRate;
            const auto outLength = (int) (clickOriginal.getNumSamples() / ratio);
            fresh->audio.setSize (1, outLength);

            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio, clickOriginal.getReadPointer (0), fresh->audio.getWritePointer (0), outLength);
        }
    }

    // Byt atomiskt. Den gamla samplingen hålls kvar tills nästa byte, eftersom ljudtråden
    // kan vara mitt i ett block som läser den.
    activeClick.store (fresh.get());
    retiredClick = std::move (ownedClick);
    ownedClick = std::move (fresh);
}

juce::ValueTree PluginBase::clickToValueTree() const
{
    juce::ValueTree tree (clickTreeType);
    const juce::ScopedLock lock (clickLock);

    if (clickOriginal.getNumSamples() > 0)
    {
        juce::MemoryBlock block (clickOriginal.getReadPointer (0), (size_t) clickOriginal.getNumSamples() * sizeof (float));
        tree.setProperty ("name", clickName, nullptr);
        tree.setProperty ("rate", clickOriginalRate, nullptr);
        tree.setProperty ("data", block.toBase64Encoding(), nullptr);
    }

    return tree;
}

void PluginBase::clickFromValueTree (const juce::ValueTree& tree)
{
    juce::MemoryBlock block;
    if (! tree.hasProperty ("data") || ! block.fromBase64Encoding (tree["data"].toString()))
    {
        setClickOriginal ({}, 0.0, {});
        return;
    }

    const auto numSamples = (int) (block.getSize() / sizeof (float));
    juce::AudioBuffer<float> audio (1, numSamples);
    std::memcpy (audio.getWritePointer (0), block.getData(), (size_t) numSamples * sizeof (float));

    setClickOriginal (std::move (audio), (double) tree["rate"], tree["name"].toString());
}
