#include "SampleSlot.h"

bool SampleSlot::loadFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return false;

    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxSeconds * reader->sampleRate));
    juce::AudioBuffer<float> fileAudio ((int) reader->numChannels, length);
    reader->read (&fileAudio, 0, length, 0, true, true);

    // Mixa ner till mono.
    juce::AudioBuffer<float> mono (1, length);
    mono.clear();
    for (int ch = 0; ch < fileAudio.getNumChannels(); ++ch)
        mono.addFrom (0, 0, fileAudio, ch, 0, length, 1.0f / (float) fileAudio.getNumChannels());

    setOriginal (std::move (mono), reader->sampleRate, file.getFileNameWithoutExtension());
    return true;
}

void SampleSlot::clear()
{
    setOriginal ({}, 0.0, {});
}

juce::String SampleSlot::name() const
{
    const juce::ScopedLock scoped (lock);
    return originalName;
}

void SampleSlot::setOriginal (juce::AudioBuffer<float> mono, double rate, const juce::String& newName)
{
    {
        const juce::ScopedLock scoped (lock);
        original = std::move (mono);
        originalRate = rate;
        originalName = newName;
    }
    rebuild();
}

void SampleSlot::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;
    rebuild();
}

void SampleSlot::rebuild()
{
    std::unique_ptr<ClickSample> fresh;
    const juce::ScopedLock scoped (lock); // (prepareToPlay kan i vissa värdar komma från en annan tråd än gränssnittet)

    if (original.getNumSamples() > 0 && originalRate > 0.0)
    {
        fresh = std::make_unique<ClickSample>();
        fresh->name = originalName;

        const auto ratio = originalRate / currentSampleRate;
        const auto outLength = std::max (1, (int) (original.getNumSamples() / ratio));
        fresh->audio.setSize (1, outLength);

        juce::LagrangeInterpolator interpolator;
        interpolator.process (ratio, original.getReadPointer (0), fresh->audio.getWritePointer (0), outLength);
    }

    // Byt atomiskt. Den gamla samplingen frigörs inte direkt: ljudtråden kan vara mitt i ett block som läser den, och byts samplingen flera
    // gånger i snabb följd (presets eller AI-anrop i rad) får ingen av dem försvinna under ljudtrådens fötter.
    activeSample.store (fresh.get());
    retired.retire (std::move (owned));
    owned = std::move (fresh);
}

juce::ValueTree SampleSlot::toValueTree (const juce::Identifier& type) const
{
    juce::ValueTree tree (type);
    const juce::ScopedLock scoped (lock);

    if (original.getNumSamples() > 0)
    {
        juce::MemoryBlock block (original.getReadPointer (0), (size_t) original.getNumSamples() * sizeof (float));
        tree.setProperty ("name", originalName, nullptr);
        tree.setProperty ("rate", originalRate, nullptr);
        tree.setProperty ("data", block.toBase64Encoding(), nullptr);
    }

    return tree;
}

void SampleSlot::fromValueTree (const juce::ValueTree& tree)
{
    juce::MemoryBlock block;
    if (! tree.hasProperty ("data") || ! block.fromBase64Encoding (tree["data"].toString()))
    {
        clear();
        return;
    }

    const auto numSamples = (int) (block.getSize() / sizeof (float));
    juce::AudioBuffer<float> audio (1, numSamples);
    std::memcpy (audio.getWritePointer (0), block.getData(), (size_t) numSamples * sizeof (float));

    setOriginal (std::move (audio), (double) tree["rate"], tree["name"].toString());
}
