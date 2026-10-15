#include "OfflineRender.h"

namespace
{
    struct FakePlayHead : juce::AudioPlayHead
    {
        PositionInfo info;
        juce::Optional<PositionInfo> getPosition() const override { return info; }
    };
}

namespace OfflineRender
{
    juce::AudioBuffer<float> render (juce::AudioProcessor& processor, const Settings& settings)
    {
        const auto rate = settings.sampleRate;
        const auto blockSize = settings.blockSize;
        const auto numInputs = processor.getTotalNumInputChannels();
        processor.setPlayConfigDetails (numInputs, 2, rate, blockSize);
        processor.prepareToPlay (rate, blockSize);

        FakePlayHead playHead;
        playHead.info.setIsPlaying (true);
        playHead.info.setBpm (settings.bpm);
        playHead.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        processor.setPlayHead (&playHead);

        const auto samplesPerBar = rate * 60.0 / settings.bpm * 4.0;
        const auto totalSamples = (int) ((settings.bars + settings.extraBars) * samplesPerBar);
        const auto noteAt = [&] (int bar) { return settings.notes[bar % settings.notes.size()]; };

        juce::AudioBuffer<float> output (2, totalSamples);
        output.clear();
        juce::AudioBuffer<float> block (std::max (2, numInputs), blockSize);

        for (int done = 0; done < totalSamples; done += blockSize)
        {
            const auto n = std::min (blockSize, totalSamples - done);
            block.setSize (block.getNumChannels(), n, false, false, true);
            block.clear();
            if (numInputs > 0 && settings.fillInput)
                settings.fillInput (block, done);

            const auto ppq = done / rate * settings.bpm / 60.0;
            playHead.info.setPpqPosition (ppq);
            playHead.info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
            playHead.info.setTimeInSamples (done);

            // Vid ett taktstreck med ny ton släpps den förra och nästa slås an på samma sample.
            juce::MidiBuffer midi;
            for (int bar = 0; bar <= settings.bars; ++bar)
            {
                const auto at = (int) (bar * samplesPerBar);
                if (at < done || at >= done + n)
                    continue;
                const auto changes = bar == 0 || bar == settings.bars || noteAt (bar) != noteAt (bar - 1);
                if (! changes)
                    continue;
                if (bar > 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, noteAt (bar - 1)), at - done);
                if (bar < settings.bars)
                    midi.addEvent (juce::MidiMessage::noteOn (1, noteAt (bar), (juce::uint8) 100), at - done);
            }

            processor.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                output.copyFrom (ch, done, block, ch, 0, n);
        }

        processor.setPlayHead (nullptr);
        processor.releaseResources();
        return output;
    }

    bool writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& file)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (sampleRate)
                                                       .withNumChannels (audio.getNumChannels())
                                                       .withBitsPerSample (24));
        return writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }
}
