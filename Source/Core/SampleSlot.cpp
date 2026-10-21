#include "SampleSlot.h"

namespace
{
    void addStart (std::vector<int>& starts, int start, int length, int minLength)
    {
        if (start < 0 || start >= length || (int) starts.size() >= SampleSlicing::maxSlices)
            return;
        if (! starts.empty() && (start - starts.back() < minLength || length - start < minLength))
            return;
        starts.push_back (start);
    }

    // Skivornas startsamples för den omsamplade kopian. Körs på message thread, aldrig i ljudtråden.
    std::vector<int> buildSlices (const juce::AudioBuffer<float>& audio, double rate, int mode, float sensitivity, int gridCount, std::vector<float> markers)
    {
        const auto length = audio.getNumSamples();
        const auto minLength = std::max (1, (int) (SampleSlicing::minSliceSeconds * rate));
        std::vector<int> starts;
        addStart (starts, 0, length, minLength);

        if (mode == SampleSlicing::grid)
        {
            const auto n = juce::jlimit (1, SampleSlicing::maxSlices, gridCount);
            for (int i = 1; i < n; ++i)
                addStart (starts, (int) ((juce::int64) i * length / n), length, minLength);
        }
        else if (mode == SampleSlicing::manual)
        {
            std::sort (markers.begin(), markers.end());
            for (const auto m : markers)
                addStart (starts, (int) (juce::jlimit (0.0f, 1.0f, m) * (float) length), length, minLength);
        }
        else
        {
            // Transientdetektion: ökning av nivån (log) mellan två fönster på minsta skivlängden.
            const auto* data = audio.getReadPointer (0);
            const auto hop = minLength;
            const auto frames = length / hop;
            std::vector<float> level ((size_t) frames, 0.0f);
            for (int f = 0; f < frames; ++f)
            {
                double sum = 0.0;
                for (int i = 0; i < hop; ++i)
                    sum += (double) data[f * hop + i] * (double) data[f * hop + i];
                level[(size_t) f] = (float) std::sqrt (sum / hop);
            }
            const auto threshold = 0.2f + 2.0f * (1.0f - juce::jlimit (0.0f, 1.0f, sensitivity));
            auto rise = [&] (int f) { return f < 1 ? 0.0f : std::log ((level[(size_t) f] + 1.0e-5f) / (level[(size_t) f - 1] + 1.0e-5f)); };
            for (int f = 1; f < frames; ++f)
            {
                if (level[(size_t) f] < 1.0e-3f || rise (f) < threshold)
                    continue;
                if (f + 1 < frames && rise (f + 1) > rise (f)) // toppen är i nästa fönster
                    continue;
                addStart (starts, (f - 1) * hop, length, minLength);
            }
        }
        return starts;
    }
}

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
        fresh->slices = buildSlices (fresh->audio, currentSampleRate, mode, sensitivity, gridCount, markers);
    }

    // Byt atomiskt. Den gamla samplingen frigörs inte direkt: ljudtråden kan vara mitt i ett block som läser den, och byts samplingen flera
    // gånger i snabb följd (presets eller AI-anrop i rad) får ingen av dem försvinna under ljudtrådens fötter.
    activeSample.store (fresh.get());
    retired.retire (std::move (owned));
    owned = std::move (fresh);
}

void SampleSlot::setSlicing (int newMode, float newSensitivity, int newGridCount)
{
    {
        const juce::ScopedLock scoped (lock);
        mode = juce::jlimit (0, 2, newMode);
        sensitivity = juce::jlimit (0.0f, 1.0f, newSensitivity);
        gridCount = juce::jlimit (1, SampleSlicing::maxSlices, newGridCount);
    }
    rebuild();
}

void SampleSlot::setManualMarkers (std::vector<float> normalised)
{
    {
        const juce::ScopedLock scoped (lock);
        markers = std::move (normalised);
        if (markers.size() > (size_t) SampleSlicing::maxSlices)
            markers.resize ((size_t) SampleSlicing::maxSlices);
    }
    rebuild();
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
        // Skivor: bara när de avviker från standard, så att oanvända samplingar sparas som förut.
        if (mode != SampleSlicing::grid || sensitivity != 0.5f || gridCount != 8 || ! markers.empty())
        {
            tree.setProperty ("sliceMode", mode, nullptr);
            tree.setProperty ("sliceSensitivity", (double) sensitivity, nullptr);
            tree.setProperty ("sliceGrid", gridCount, nullptr);
            juce::StringArray parts;
            for (const auto m : markers)
                parts.add (juce::String (m, 6));
            tree.setProperty ("sliceMarkers", parts.joinIntoString (","), nullptr);
        }
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

    {
        const juce::ScopedLock scoped (lock);
        mode = juce::jlimit (0, 2, (int) tree.getProperty ("sliceMode", SampleSlicing::grid));
        sensitivity = juce::jlimit (0.0f, 1.0f, (float) (double) tree.getProperty ("sliceSensitivity", 0.5));
        gridCount = juce::jlimit (1, SampleSlicing::maxSlices, (int) tree.getProperty ("sliceGrid", 8));
        markers.clear();
        for (const auto& part : juce::StringArray::fromTokens (tree.getProperty ("sliceMarkers", "").toString(), ",", ""))
            if (part.isNotEmpty() && markers.size() < (size_t) SampleSlicing::maxSlices)
                markers.push_back (part.getFloatValue());
    }
    setOriginal (std::move (audio), (double) tree["rate"], tree["name"].toString());
}
