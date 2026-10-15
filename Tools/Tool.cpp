// Utvecklingsverktyg: renderar ljud offline och tar skärmbilder av gränssnittet, så att
// pluginsen kan kontrolleras utan DAW.
//
//   MyOwnFruitTool render   instrument|fx <ut.wav> [bpm] [takter] [ton eller toner, t.ex. 66,64,61,59] [preset] [effektpreset]
//   (flera toner: en per takt, bytet sker exakt på taktstrecket; preset kan följas av kick- och
//   baspreset, t.ex. 16+k3+b5 = helpreset 16, sedan kickpreset 3 och baspreset 5)
//   MyOwnFruitTool export   <ut.wav> full|kick|bass <takter> [bpm] [ton] [preset]   (som EXPORT-knappen)
//   MyOwnFruitTool snapshot instrument|fx <ut.png> [flik] [mod]   (mod = med exempel-LFO-kopplingar)
//   MyOwnFruitTool presettest
//   MyOwnFruitTool analyze  <ljudfil> [start-sekund] [slut-sekund]
//   MyOwnFruitTool fitbass  <ljudfil> <start> <slut> [steg per slag]
//   MyOwnFruitTool fitkick  <ljudfil> <start> <slut>
//   MyOwnFruitTool fitbeat  <ljudfil> <start> <slut> [rolling|galloping|triplet|offbeat]
//   MyOwnFruitTool comparebeat <ljudfil> <start> <slut> <mönster> <rendering.wav>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Core/OfflineRender.h"
#include "../Source/Instrument/InstrumentProcessor.h"
#include "Analyzer.h"
#include <cstdio>

namespace
{
    constexpr double renderRate = 44100.0; // samma som referenslåtarna, så att analysen jämför lika
    constexpr int blockSize = 512;

    std::unique_ptr<PluginBase> createProcessor (const juce::String& kind)
    {
        if (kind == "fx")
            return std::make_unique<FxProcessor>();
        return std::make_unique<InstrumentProcessor>();
    }

    void printStats (const juce::AudioBuffer<float>& audio, double bpm, double rate = renderRate)
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
        const auto samplesPerSixteenth = rate * 60.0 / bpm / 4.0;
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

    int render (const juce::String& kind, const juce::File& outFile, double bpm, int bars, const juce::Array<int>& notes,
                const juce::String& presets, int effectPreset)
    {
        auto processor = createProcessor (kind);
        for (const auto& token : juce::StringArray::fromTokens (presets, "+", {}))
        {
            auto section = Presets::Section::full;
            auto index = token;
            if (token.startsWithIgnoreCase ("k")) { section = Presets::Section::kick; index = token.substring (1); }
            if (token.startsWithIgnoreCase ("b")) { section = Presets::Section::bass; index = token.substring (1); }
            if (index.isEmpty() || index.getIntValue() < 0)
                continue;
            processor->loadFactoryPreset (section, index.getIntValue());
            std::printf ("%s: %s\n", Presets::sectionName (section).toRawUTF8(), processor->getPresetName (section).toRawUTF8());
        }
        if (effectPreset >= 0)
        {
            processor->loadFactoryPreset (Presets::Section::effects, effectPreset);
            std::printf ("Effekter: %s\n", processor->getPresetName (Presets::Section::effects).toRawUTF8());
        }
        OfflineRender::Settings settings;
        settings.sampleRate = renderRate;
        settings.blockSize = blockSize;
        settings.bpm = bpm;
        settings.bars = bars;
        settings.notes = notes;
        settings.extraBars = 1; // en takt efter tonen

        // FX-versionen: låtsas-Serum, en hållen sågtand i 55 Hz.
        double inputPhase = 0.0;
        settings.fillInput = [&inputPhase] (juce::AudioBuffer<float>& block, int)
        {
            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                const auto s = (float) (2.0 * inputPhase - 1.0) * 0.5f;
                for (int ch = 0; ch < block.getNumChannels(); ++ch)
                    block.setSample (ch, i, s);
                inputPhase += 55.0 / renderRate;
                inputPhase -= std::floor (inputPhase);
            }
        };

        const auto output = OfflineRender::render (*processor, settings);
        printStats (output, bpm);
        writeWaveformImage (output, bpm, outFile.withFileExtension ("png"));

        if (! OfflineRender::writeWav (output, renderRate, outFile))
        {
            std::puts ("Kunde inte skriva WAV");
            return 1;
        }
        std::printf ("Skrev %s\n", outFile.getFullPathName().toRawUTF8());
        return 0;
    }

    // Provar pluginets WAV-export, som gränssnittets EXPORT-knapp.
    int exportPart (const juce::File& outFile, const juce::String& part, int bars, double bpm, int note, int preset)
    {
        InstrumentProcessor processor;
        if (preset >= 0)
            processor.loadFactoryPreset (Presets::Section::full, preset);
        processor.transportStatus.bpm = bpm;
        processor.transportStatus.keyNote = note;
        const auto which = part == "kick" ? PluginBase::ExportPart::kick
                         : (part == "bass" ? PluginBase::ExportPart::bass : PluginBase::ExportPart::full);
        if (! processor.exportWav (outFile, which, bars))
        {
            std::puts ("Exporten misslyckades");
            return 1;
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (outFile));
        if (reader == nullptr)
            return 1;
        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
        std::printf ("%.2f s, %.0f Hz. ", audio.getNumSamples() / reader->sampleRate, reader->sampleRate);
        printStats (audio, bpm, reader->sampleRate);
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

    int snapshot (const juce::String& kind, const juce::File& outFile, int tab, bool withModulation)
    {
        juce::ScopedJuceInitialiser_GUI gui;

        auto processor = createProcessor (kind);
        processor->selectedTab = tab;
        processor->setPlayConfigDetails (kind == "fx" ? 2 : 0, 2, renderRate, blockSize);
        processor->prepareToPlay (renderRate, blockSize);

        if (withModulation)
        {
            // Exempelkopplingar, så att LFO-ringarna syns på bilden.
            const auto& parameters = processor->getParameters();
            auto indexOf = [&parameters] (const char* id)
            {
                for (int i = 0; i < parameters.size(); ++i)
                    if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); p != nullptr && p->paramID == id)
                        return i;
                return -1;
            };
            processor->modRoutes.add (0, indexOf ("bassCutoff"), 0.35f);
            processor->modRoutes.add (1, indexOf ("bassCutoff"), -0.2f);
            processor->modRoutes.add (2, indexOf ("kickPitch"), 0.25f);
        }

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

    if (argc >= 5 && (juce::String (argv[1]) == "fitbass" || juce::String (argv[1]) == "fitkick"
                      || juce::String (argv[1]) == "fitbeat" || juce::String (argv[1]) == "comparebeat"))
    {
        juce::ScopedJuceInitialiser_GUI juce; // Windows Media-avkodaren behöver JUCE:s meddelandesystem
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2]));
        const auto start = juce::String (argv[3]).getDoubleValue();
        const auto end = juce::String (argv[4]).getDoubleValue();
        if (juce::String (argv[1]) == "fitkick")
            return fitKick (file, start, end);
        if (juce::String (argv[1]) == "fitbeat")
            return fitBeat (file, start, end, argc > 5 ? juce::String (argv[5]) : juce::String ("rolling"));
        if (juce::String (argv[1]) == "comparebeat" && argc > 6)
            return compareBeat (file, start, end, juce::String (argv[5]),
                                juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[6])));
        return fitBass (file, start, end, argc > 5 ? juce::String (argv[5]).getIntValue() : 4);
    }

    if (argc >= 5 && juce::String (argv[1]) == "export")
    {
        juce::ScopedJuceInitialiser_GUI juce;
        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2]));
        return exportPart (file, juce::String (argv[3]), juce::String (argv[4]).getIntValue(),
                           argc > 5 ? juce::String (argv[5]).getDoubleValue() : 145.0,
                           argc > 6 ? juce::String (argv[6]).getIntValue() : 60,
                           argc > 7 ? juce::String (argv[7]).getIntValue() : -1);
    }

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
        const auto preset = argc > 7 ? juce::String (argv[7]) : juce::String();
        const auto effectPreset = argc > 8 ? juce::String (argv[8]).getIntValue() : -1;
        return render (kind, outFile, bpm, bars, notes, preset, effectPreset);
    }

    if (command == "snapshot")
        return snapshot (kind, outFile, argc > 4 ? juce::String (argv[4]).getIntValue() : 0, argc > 5 && juce::String (argv[5]) == "mod");

    std::puts ("Okänt kommando");
    return 1;
}
