// Utvecklingsverktyg: renderar ljud offline och tar skärmbilder av gränssnittet, så att
// pluginsen kan kontrolleras utan DAW.
//
//   MyOwnFruitTool render   instrument|fx <ut.wav> [bpm] [takter] [ton eller toner, t.ex. 66,64,61,59] [preset] [effektpreset]
//   (flera toner: en per takt, bytet sker exakt på taktstrecket)
//   MyOwnFruitTool snapshot instrument|fx <ut.png> [flik]
//   MyOwnFruitTool presettest
//   MyOwnFruitTool analyze  <ljudfil> [start-sekund] [slut-sekund]

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Instrument/InstrumentProcessor.h"
#include "Analyzer.h"
#include <cstdio>

namespace
{
    constexpr double renderRate = 48000.0;
    constexpr int blockSize = 512;

    struct FakePlayHead : juce::AudioPlayHead
    {
        PositionInfo info;
        juce::Optional<PositionInfo> getPosition() const override { return info; }
    };

    std::unique_ptr<PluginBase> createProcessor (const juce::String& kind)
    {
        if (kind == "fx")
            return std::make_unique<FxProcessor>();
        return std::make_unique<InstrumentProcessor>();
    }

    void printStats (const juce::AudioBuffer<float>& audio, double bpm)
    {
        auto peak = 0.0f;
        auto sumSquares = 0.0, dc = 0.0;
        auto nonFinite = 0;

        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto s = audio.getSample (0, i);
            if (! std::isfinite (s))
                ++nonFinite;
            peak = std::max (peak, std::abs (s));
            sumSquares += s * s;
            dc += s;
        }

        const auto n = (double) audio.getNumSamples();
        std::printf ("peak %.3f (%.1f dBFS)  rms %.3f  dc %.5f  icke-ändliga %d\n",
                     peak, juce::Decibels::gainToDecibels (peak), std::sqrt (sumSquares / n), dc / n, nonFinite);

        // Toppvärde per sextondel, så att mönstret syns som text.
        const auto samplesPerSixteenth = renderRate * 60.0 / bpm / 4.0;
        const auto numSteps = (int) (n / samplesPerSixteenth);
        std::puts ("Topp per sextondel (en rad per takt, 0-9):");
        for (int step = 0; step < numSteps; ++step)
        {
            const auto start = (int) (step * samplesPerSixteenth);
            const auto end = std::min (audio.getNumSamples(), (int) ((step + 1) * samplesPerSixteenth));
            auto stepPeak = 0.0f;
            for (int i = start; i < end; ++i)
                stepPeak = std::max (stepPeak, std::abs (audio.getSample (0, i)));

            std::printf ("%d", std::min (9, (int) (stepPeak * 10.0f)));
            if (step % 4 == 3)  std::printf (" ");
            if (step % 16 == 15) std::printf ("\n");
        }
        std::puts ("");
    }

    // Ritar vågformen med ett rutnät av sextondelar, så att mönstret syns.
    void writeWaveformImage (const juce::AudioBuffer<float>& audio, double bpm, const juce::File& file)
    {
        constexpr int width = 1800, height = 500;
        // Bildtypen är explicit mjukvara: en Direct2D-bild blir inte färdigritad förrän
        // Graphics-objektet förstörs, därför också det inre blocket.
        juce::Image image (juce::Image::RGB, width, height, true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.fillAll (juce::Colour (0xff10112a));

            const auto samplesPerSixteenth = renderRate * 60.0 / bpm / 4.0;
            const auto total = audio.getNumSamples();
            for (int step = 0; step * samplesPerSixteenth < total; ++step)
            {
                const auto x = (float) (step * samplesPerSixteenth / total * width);
                g.setColour (juce::Colour (step % 16 == 0 ? 0xff8b90c0 : (step % 4 == 0 ? 0xff3a3f78 : 0xff22264a)));
                g.drawVerticalLine ((int) x, 0.0f, (float) height);
            }

            g.setColour (juce::Colour (0xff3fd2ff));
            const auto mid = height / 2.0f;
            for (int x = 0; x < width; ++x)
            {
                const auto start = (int) ((double) x / width * total);
                const auto end = std::max (start + 1, (int) ((double) (x + 1) / width * total));
                auto lo = 0.0f, hi = 0.0f;
                for (int i = start; i < end && i < total; ++i)
                {
                    lo = std::min (lo, audio.getSample (0, i));
                    hi = std::max (hi, audio.getSample (0, i));
                }
                g.drawVerticalLine (x, mid - hi * mid, mid - lo * mid + 1.0f);
            }
        }

        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::printf ("Skrev %s\n", file.getFullPathName().toRawUTF8());
    }

    int render (const juce::String& kind, const juce::File& outFile, double bpm, int bars, const juce::Array<int>& notes, int preset, int effectPreset)
    {
        auto processor = createProcessor (kind);
        if (preset >= 0)
        {
            processor->loadFactoryPreset (Presets::Section::full, preset);
            std::printf ("Preset: %s\n", processor->getPresetName (Presets::Section::full).toRawUTF8());
        }
        if (effectPreset >= 0)
        {
            processor->loadFactoryPreset (Presets::Section::effects, effectPreset);
            std::printf ("Effekter: %s\n", processor->getPresetName (Presets::Section::effects).toRawUTF8());
        }
        processor->setPlayConfigDetails (kind == "fx" ? 2 : 0, 2, renderRate, blockSize);
        processor->prepareToPlay (renderRate, blockSize);

        FakePlayHead playHead;
        playHead.info.setIsPlaying (true);
        playHead.info.setBpm (bpm);
        playHead.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        processor->setPlayHead (&playHead);

        const auto samplesPerBar = renderRate * 60.0 / bpm * 4.0;
        const auto noteSamples = (int) (bars * samplesPerBar);
        const auto totalSamples = noteSamples + (int) samplesPerBar; // en takt efter tonen
        juce::AudioBuffer<float> output (2, totalSamples);
        output.clear();

        juce::AudioBuffer<float> block (2, blockSize);
        double inputPhase = 0.0;

        for (int done = 0; done < totalSamples; done += blockSize)
        {
            const auto n = std::min (blockSize, totalSamples - done);
            block.setSize (2, n, false, false, true);
            block.clear();

            const auto ppq = done / renderRate * bpm / 60.0;
            playHead.info.setPpqPosition (ppq);
            playHead.info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
            playHead.info.setTimeInSamples (done);

            juce::MidiBuffer midi;
            if (kind == "fx")
            {
                // Låtsas-Serum: en hållen sågtand i 55 Hz.
                for (int i = 0; i < n; ++i)
                {
                    const auto s = (float) (2.0 * inputPhase - 1.0) * 0.5f;
                    block.setSample (0, i, s);
                    block.setSample (1, i, s);
                    inputPhase += 55.0 / renderRate;
                    inputPhase -= std::floor (inputPhase);
                }
            }
            else
            {
                // En ton per takt; vid taktstrecket släpps förra tonen och nästa slås an på samma sample.
                for (int bar = 0; bar <= bars; ++bar)
                {
                    const auto at = (int) (bar * samplesPerBar);
                    if (at < done || at >= done + n)
                        continue;
                    if (bar > 0)
                        midi.addEvent (juce::MidiMessage::noteOff (1, notes[(bar - 1) % notes.size()]), at - done);
                    if (bar < bars)
                        midi.addEvent (juce::MidiMessage::noteOn (1, notes[bar % notes.size()], (juce::uint8) 100), at - done);
                }
            }

            processor->processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                output.copyFrom (ch, done, block, ch, 0, n);
        }

        printStats (output, bpm);
        writeWaveformImage (output, bpm, outFile.withFileExtension ("png"));

        outFile.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (outFile.createOutputStream());
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (renderRate)
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (24));
        if (writer == nullptr)
        {
            std::puts ("Kunde inte skriva WAV");
            return 1;
        }
        writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples());
        std::printf ("Skrev %s\n", outFile.getFullPathName().toRawUTF8());
        return 0;
    }

    // Sparar ett kickpreset, ändrar ljudet, laddar presetet och kontrollerar värdena.
    // Testfilen tas bort efteråt.
    int presetTest()
    {
        InstrumentProcessor processor;
        auto* pitch = processor.apvts.getParameter (ParamID::kickPitch);
        auto* cutoff = processor.apvts.getParameter (ParamID::bassCutoff);

        pitch->setValueNotifyingHost (pitch->convertTo0to1 (55.0f));
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (1000.0f));

        const auto name = juce::String ("__presettest__");
        if (! processor.saveUserPreset (Presets::Section::kick, name))
        {
            std::puts ("FEL: kunde inte spara");
            return 1;
        }

        const auto file = Presets::folder (Presets::Section::kick, false).getChildFile (name + ".mofp");
        pitch->setValueNotifyingHost (pitch->convertTo0to1 (20.0f));
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (500.0f));

        const auto loaded = processor.loadUserPreset (Presets::Section::kick, file);
        const auto pitchBack = pitch->convertFrom0to1 (pitch->getValue());
        const auto cutoffAfter = cutoff->convertFrom0to1 (cutoff->getValue());
        file.deleteFile();

        std::printf ("laddad %d, kick pitch %.2f (väntat 55), bas cutoff %.1f (väntat 500, orörd)\n",
                     loaded ? 1 : 0, pitchBack, cutoffAfter);

        const auto ok = loaded && std::abs (pitchBack - 55.0f) < 0.1f && std::abs (cutoffAfter - 500.0f) < 1.0f
                     && processor.getPresetName (Presets::Section::kick) == name;
        std::puts (ok ? "Presettest OK" : "FEL: presettest");
        return ok ? 0 : 1;
    }

    int snapshot (const juce::String& kind, const juce::File& outFile, int tab)
    {
        juce::ScopedJuceInitialiser_GUI gui;

        auto processor = createProcessor (kind);
        processor->selectedTab = tab;
        processor->setPlayConfigDetails (kind == "fx" ? 2 : 0, 2, renderRate, blockSize);
        processor->prepareToPlay (renderRate, blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

        outFile.deleteFile();
        juce::FileOutputStream stream (outFile);
        juce::PNGImageFormat png;
        if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
        {
            std::puts ("Kunde inte skriva PNG");
            return 1;
        }

        std::printf ("Skrev %s (%dx%d)\n", outFile.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
        editor.reset();
        return 0;
    }
}

int main (int argc, char* argv[])
{
    if (argc == 2 && juce::String (argv[1]) == "presettest")
        return presetTest();

    if (argc >= 3 && juce::String (argv[1]) == "analyze")
    {
        juce::ScopedJuceInitialiser_GUI juce; // Windows Media-avkodaren behöver JUCE:s meddelandesystem
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2]));
        const auto start = argc > 3 ? juce::String (argv[3]).getDoubleValue() : -1.0;
        const auto end = argc > 4 ? juce::String (argv[4]).getDoubleValue() : -1.0;
        return analyzeFile (file, start, end);
    }

    if (argc < 4)
    {
        std::puts ("Användning:\n"
                   "  MyOwnFruitTool render   instrument|fx <ut.wav> [bpm] [takter] [ton]\n"
                   "  MyOwnFruitTool snapshot instrument|fx <ut.png>");
        return 1;
    }

    const juce::String command (argv[1]), kind (argv[2]);
    const auto outFile = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[3]));

    if (command == "render")
    {
        const auto bpm = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 145.0;
        const auto bars = argc > 5 ? juce::String (argv[5]).getIntValue() : 2;
        juce::Array<int> notes;
        for (const auto& token : juce::StringArray::fromTokens (argc > 6 ? juce::String (argv[6]) : juce::String ("66"), ",", {}))
            notes.add (token.getIntValue()); // 66 = F#5 i FL
        const auto preset = argc > 7 ? juce::String (argv[7]).getIntValue() : -1;
        const auto effectPreset = argc > 8 ? juce::String (argv[8]).getIntValue() : -1;
        return render (kind, outFile, bpm, bars, notes, preset, effectPreset);
    }

    if (command == "snapshot")
        return snapshot (kind, outFile, argc > 4 ? juce::String (argv[4]).getIntValue() : 0);

    std::puts ("Okänt kommando");
    return 1;
}
