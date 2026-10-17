// Utvecklingsverktyg: renderar ljud offline och tar skärmbilder av gränssnittet, så att
// pluginsen kan kontrolleras utan DAW.
//
//   MyOwnFruitTool render   instrument|fx <ut.wav> [bpm] [takter] [ton eller toner, t.ex. 66,64,61,59] [preset] [effektpreset]
//   (flera toner: en per takt, bytet sker exakt på taktstrecket; preset kan följas av kick- och
//   baspreset, t.ex. 16+k3+b5 = helpreset 16, sedan kickpreset 3 och baspreset 5; p:id=värde sätter
//   en parameter, t.ex. p:fxA1Type=2+p:bass1FxChannel=1)
//   MyOwnFruitTool export   <ut.wav> full|kick|bass <takter> [bpm] [ton] [preset]   (som EXPORT-knappen)
//   MyOwnFruitTool snapshot instrument|fx <ut.png> [flik] [mod]   (mod = med exempel-LFO-kopplingar)
//   MyOwnFruitTool presettest
//   MyOwnFruitTool analyze  <ljudfil> [start-sekund] [slut-sekund]
//   MyOwnFruitTool fitbass  <ljudfil> <start> <slut> [steg per slag]
//   MyOwnFruitTool fitkick  <ljudfil> <start> <slut>
//   MyOwnFruitTool fitbeat  <ljudfil> <start> <slut> [rolling|galloping|triplet|offbeat]
//   MyOwnFruitTool comparebeat <ljudfil> <start> <slut> <mönster> <rendering.wav>

#include "../Source/Core/MixAnalysis.h"
#include "../Source/Ui/LogoPainter.h"
#include "../Source/Ui/ListenButton.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Core/OfflineRender.h"
#include "../Source/Core/ToolLayer.h"
#include "../Source/Core/AiChat.h"
#include "../Source/Core/Settings.h"
#include "../Source/Core/WavetablePack.h"
#include "../Source/Core/RollFiles.h"
#include "../Source/Instrument/InstrumentProcessor.h"
#include "Analyzer.h"
#include "../Source/Core/AudioAnalysis.h"
#include <juce_dsp/juce_dsp.h>
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

}

#include "PresetAudit.inl"

namespace
{
    int render (const juce::String& kind, const juce::File& outFile, double bpm, int bars, const juce::Array<int>& notes,
                const juce::String& presets, int effectPreset)
    {
        auto processor = createProcessor (kind);
        auto clipBars = 0;
        auto noteOnFirst = false;
        for (const auto& token : juce::StringArray::fromTokens (presets, "+", {}))
        {
            // clip=N: tonen släpps och slås an igen var N:e takt (som mönsterklipp i FL); clipon=N: med "ton på" före "ton av".
            if (token.startsWith ("clip"))
            {
                noteOnFirst = token.startsWith ("clipon");
                clipBars = token.fromFirstOccurrenceOf ("=", false, false).getIntValue();
                continue;
            }

            // p:id=värde sätter en parameter (i dess egen enhet), t.ex. p:fxA1Type=2.
            if (token.startsWith ("p:"))
            {
                const auto id = token.substring (2).upToFirstOccurrenceOf ("=", false, false);
                const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
                if (auto* parameter = processor->apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                else
                    std::printf ("Okänd parameter: %s\n", id.toRawUTF8());
                continue;
            }

            // f:<sökväg> laddar ett sparat helpreset (.mofp), t.ex. ett som sparats från pluginet.
            if (token.startsWith ("f:"))
            {
                const juce::File file (token.substring (2));
                const auto loaded = processor->loadUserPreset (Presets::Section::full, file);
                std::printf ("Helpreset %s: %s\n", file.getFileName().toRawUTF8(), loaded ? "laddat" : "kunde inte laddas");
                continue;
            }

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
        settings.clipBars = clipBars;
        settings.noteOnFirst = noteOnFirst;

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
        writeSpectrogram (output, renderRate, bpm, outFile.withFileExtension ("spec.png"));

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
        const auto owner = std::make_unique<InstrumentProcessor>(); // på heapen: för stor för stacken
        auto& processor = *owner;
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

    // Längdändringar i gränssnittet: förkortas en rad vars innehåll bara är en upprepning tas det bort utan fråga, och längderna
    // följer varandra (totalen är minst den längsta raden). Dialogerna (som frågar när något går förlorat) testas inte här.
    int lengthTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

        auto runFor = [] (int milliseconds) { juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds); };
        auto setChoice = [&processor] (const char* id, int index)
        {
            auto* parameter = processor.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        };

        runFor (300); // grundnivån
        auto failed = false;
        auto check = [&failed] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what); failed |= ! ok; };

        // Init: totalt 2 takter, kicken på fjärdedelar i alla takter. Kicken kortas till 1 takt: bara en upprepning försvinner.
        const auto& kickAt = [&processor] (int tick) { return processor.pattern.note (Pattern::kickLayer, tick).on.load(); };
        check (kickAt (0) && kickAt (Pattern::ticksPerBar), "kicken har toner i takt 2 från början");
        setChoice (ParamID::rowBars (0), 1); // 1 bar
        runFor (400);
        check (kickAt (0) && ! kickAt (Pattern::ticksPerBar) && ! kickAt (Pattern::ticksPerBar + Pattern::ticksPerQuarter), "kortare kick: upprepningen i takt 2 togs bort utan fråga");

        // En rad längre än totalen höjer totalen.
        setChoice (ParamID::rowBars (2), 4); // 8 bars (index 4 = 8 takter)
        runFor (400);
        const auto totalIndex = (int) std::round (processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
        check (totalIndex >= 3, "totalen höjdes till minst den längsta raden (8 takter)");

        // Förlängningen gör att en fråga (kopiera upprepningen?) har öppnats: stäng den innan editorn tas bort.
        juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
        runFor (200);
        editor.reset();
        std::puts (failed ? "FEL: längdtestet misslyckades" : "Längdtestet gick igenom");
        return failed ? 1 : 0;
    }

    // En pianorulle längre än totalen: totalen följer med, och rullen spelar hela sin längd.
    int rollLengthTest (bool withEditor)
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        if (withEditor)
            editor.reset (processor.createEditorAndMakeActive());

        auto runFor = [] (int milliseconds) { juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds); };
        auto setChoice = [&processor] (const juce::String& id, int index)
        {
            auto* parameter = processor.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        };
        auto failed = false;
        auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

        // Bara instrument 1 ska höras: pianorulle med en ton i takt 1 och en i takt 2 (totalen är 2 takter).
        setChoice (ParamID::kickOn, 0);
        for (int layer = 1; layer < 3; ++layer)
            setChoice (ParamID::bass (layer, BassField::on), 0);
        processor.pattern.clearNotes (0);
        processor.pattern.addNote (0, 0, 96, 0);
        processor.pattern.addNote (0, Pattern::ticksPerBar, 96, 0);
        processor.pattern.setCustom (0, true);
        setChoice (ParamID::patternBars, 1); // totalt 2 takter
        runFor (300);
        setChoice (ParamID::rowBars (1), 3); // rullen: 4 takter
        runFor (600);
        const auto lengths = Params::readPatternLengths (processor.apvts);
        check (lengths.total >= 4 && lengths.barsOf (1) == 4, "totalen (" + juce::String (lengths.total) + ") är minst rullens längd (4 takter)");
        const auto totalIndex = (int) std::round (processor.apvts.getRawParameterValue (ParamID::patternBars)->load());
        check (totalIndex >= 2, "TOTAL LENGTH visar minst 4 takter (index " + juce::String (totalIndex) + ")");
        // Frågan om att kopiera upprepningen in i de nya takterna: svara "Copy" (1), som en användare skulle.
        if (auto* modal = juce::ModalComponentManager::getInstance()->getModalComponent (0))
        {
            std::puts ("en fråga öppnades (Longer pattern): svarar Copy");
            modal->exitModalState (1);
            runFor (300);
        }

        OfflineRender::Settings settings;
        settings.sampleRate = renderRate;
        settings.blockSize = blockSize;
        settings.bpm = 140.0;
        settings.bars = 4;
        settings.notes = { 52 };
        const auto audio = OfflineRender::render (processor, settings);
        const auto barSamples = (int) (renderRate * 60.0 / 140.0 * 4.0);
        std::array<float, 4> peaks {};
        for (int bar = 0; bar < 4; ++bar)
            peaks[(size_t) bar] = audio.getMagnitude (0, bar * barSamples, std::min (barSamples, audio.getNumSamples() - bar * barSamples));
        std::printf("toppar per takt: %.3f %.3f %.3f %.3f\n", peaks[0], peaks[1], peaks[2], peaks[3]);
        check (peaks[0] > 0.01f && peaks[1] > 0.01f && peaks[2] > 0.01f && peaks[3] > 0.01f, "rullen spelar alla fyra takterna");

        editor.reset();
        std::puts (failed ? "FEL: rolltest" : "Rolltestet gick igenom");
        return failed ? 1 : 0;
    }

    // Mixanalysen på det som ligger i pluginet (Init eller preset 1) och skriver rapporten.
    int analyseTest (int sensitivity)
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);
        processor.transportStatus.bpm = 145.0;
        processor.transportStatus.keyNote = 52;
        const auto preset = juce::SystemStats::getEnvironmentVariable ("KB_PRESET", "-1").getIntValue();
        if (preset >= 0)
            processor.loadFactoryPreset (Presets::Section::full, preset);

        MixAnalysis::Options options;
        options.sensitivity = sensitivity;
        const auto prepareStart = juce::Time::getMillisecondCounterHiRes();
        auto copy = MixAnalysis::prepareCopy (processor);
        std::printf ("prepareCopy: %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - prepareStart);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto report = MixAnalysis::run (*copy, 145.0, 52, options, nullptr, {}, {});
        std::printf ("%s\n(%.1f s)\n", report.toText().toRawUTF8(), (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0);
        for (const auto& source : report.sources)
            if (source.present)
                std::printf ("  %s: peak %.1f dB, f0 %.1f Hz %s\n", source.name.toRawUTF8(), source.peakDb, source.fundamentalHz, source.note.toRawUTF8());
        return report.valid ? 0 : 1;
    }

    // EQ per källa: högpass på kicken, ett instrument och mastern tar bort bas, och ett lyft ökar nivån. Mixanalysen ska också ge ett giltigt svar.
    int eqTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);

        auto failed = false;
        auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };
        auto setValue = [&processor] (const juce::String& id, float real)
        {
            auto* parameter = processor.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (real));
        };
        auto rmsOf = [&processor] (PluginBase::ExportPart part)
        {
            OfflineRender::Settings settings;
            settings.sampleRate = renderRate;
            settings.bpm = 140.0;
            settings.bars = 2;
            settings.notes = { 52 };
            settings.extraBars = 1;
            const auto audio = processor.renderOffline (settings, part);
            return audio.getRMSLevel (0, 0, audio.getNumSamples());
        };

        const auto kickFlat = rmsOf (PluginBase::ExportPart::kick);
        setValue ("kickEqHp", 400.0f);
        const auto kickHighPassed = rmsOf (PluginBase::ExportPart::kick);
        check (kickHighPassed < 0.5f * kickFlat, "högpass på kicken tar bort det mesta (" + juce::String (kickFlat, 3) + " -> " + juce::String (kickHighPassed, 3) + ")");
        setValue ("kickEqHp", 20.0f);
        check (std::abs (rmsOf (PluginBase::ExportPart::kick) - kickFlat) < 0.01f * kickFlat, "med högpasset av är kicken som förut");

        const auto bassFlat = rmsOf (PluginBase::ExportPart::bass);
        setValue ("bass1EqHp", 500.0f);
        setValue ("bass2EqHp", 500.0f);
        setValue ("bass3EqHp", 500.0f);
        const auto bassHighPassed = rmsOf (PluginBase::ExportPart::bass);
        check (bassHighPassed < 0.7f * bassFlat, "högpass på instrumenten minskar dem (" + juce::String (bassFlat, 3) + " -> " + juce::String (bassHighPassed, 3) + ")");
        for (int i = 1; i <= 3; ++i)
            setValue ("bass" + juce::String (i) + "EqHp", 20.0f);

        setValue ("masterEq1G", 12.0f);
        setValue ("masterEq1F", 60.0f);
        const auto boosted = rmsOf (PluginBase::ExportPart::full);
        setValue ("masterEq1G", 0.0f);
        const auto neutral = rmsOf (PluginBase::ExportPart::full);
        check (boosted > 1.15f * neutral || boosted < 0.9f * neutral, "ett lyft i masterns EQ ändrar ljudet (" + juce::String (neutral, 3) + " -> " + juce::String (boosted, 3) + ")");

        // SIDECHAIN som effekt: instrument 2 går 100 % genom FX1, och ett SIDECHAIN med instrument 1 som källa sänker det.
        {
            const auto mixerGain = [] (int source) { return Mixer::paramID (Mixer::firstBassSource + source, "Gain"); };
            for (const auto source : { 0, 2 })
                setValue (mixerGain (source), 0.0f); // instrument 1 och 3 hörs inte, men finns kvar som källor
            setValue (ParamID::bass (1, BassField::fxChannel), 1.0f);
            setValue (ParamID::bass (1, BassField::fxAmount), 1.0f);
            const auto routed = rmsOf (PluginBase::ExportPart::bass);
            Effects::setSlotType (processor.apvts, 0, 0, EffectType::ducker);
            const auto ducked = rmsOf (PluginBase::ExportPart::bass);
            check (routed > 0.01f && ducked < 0.92f * routed, "SIDECHAIN-effekten sänker instrument 2 efter instrument 1 (" + juce::String (routed, 3) + " -> " + juce::String (ducked, 3) + ")");
            Effects::setSlotType (processor.apvts, 0, 0, EffectType::off);

            // Samma sak som en AI gör den: set_effect med typen Sidechain och källan vald med namn.
            ToolLayer tools (processor);
            const auto result = tools.call ("set_effect", juce::JSON::parse (R"({"channel":"FX1","slot":1,"type":"Sidechain","knobs":{"SOURCE":"Instr 1","THRESHOLD":-24,"DEPTH":0.8}})"));
            check ((bool) result["ok"] && Effects::slotType (processor.apvts, 0, 0) == EffectType::ducker, "set_effect kan lägga en Sidechain-effekt: " + juce::JSON::toString (result, true).substring (0, 120));
            const auto viaTool = rmsOf (PluginBase::ExportPart::bass);
            check (viaTool < 0.92f * routed, "sidechainen som AI:n lägger fungerar (" + juce::String (viaTool, 3) + ")");
            Effects::setSlotType (processor.apvts, 0, 0, EffectType::off);
            setValue (ParamID::bass (1, BassField::fxChannel), 0.0f);
            for (const auto source : { 0, 2 })
                setValue (mixerGain (source), 1.0f);
        }

        // analyse_mix körs i bakgrunden: ett anrop som startar den och svarar direkt, och get_mix_report som ger svaret när det är klart.
        {
            // En LFO på instrument 1:s cutoff, så att moduleringen syns i ramarna.
            {
                const auto& parameterList = processor.getParameters();
                for (int i = 0; i < parameterList.size(); ++i)
                    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameterList[i]); withId != nullptr && withId->paramID == "bass1Cutoff")
                        processor.modRoutes.add (0, i, 0.4f);
                if (auto* lfoOn = processor.apvts.getParameter ("lfo1On"))
                    lfoOn->setValueNotifyingHost (1.0f);
            }
            ToolLayer tools (processor);
            const auto started = tools.call ("analyse_mix", juce::JSON::parse ("{}"));
            check ((bool) started["ok"] && started["data"]["status"].toString() == "started", "analyse_mix startar och svarar direkt");
            auto answer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
            for (int i = 0; i < 600 && answer["data"]["status"].toString() != "done"; ++i)
            {
                juce::Thread::sleep (100);
                answer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
            }
            check ((bool) answer["ok"] && answer["data"]["status"].toString() == "done" && answer["data"]["findings"].isArray(), "get_mix_report ger rapporten (utan att vänta i anropet)");
            check (answer["data"]["kick_tail_hz"].operator double() > 25.0, "rapporten kommer med i svaret (data)");
            {
                // Samma analys delas med fliken i pluginet: den som verktygen startade är den som ligger hos processorn.
                check (processor.getMixAnalyser().hasReport() && processor.getMixAnalyser().getReport().valid, "analysen ligger hos processorn (delas med ANALYZER-fliken)");
                const auto measured = answer["data"]["kick_tail_hz"].operator double(), design = answer["data"]["kick_tail_design_hz"].operator double();
                check (design > 25.0 && std::abs (12.0 * std::log2 (measured / design)) < 1.0, "kickens svans mäts i tidsdomänen nära det inställningarna ger (" + juce::String (measured, 1) + " av " + juce::String (design, 1) + " Hz)");
                const auto sourcesList = answer["data"]["sources"];
                check (sourcesList[0]["sample_peak_db"].isDouble() && (double) sourcesList[0]["sample_peak_db"] < 12.0 && sourcesList[0]["rms_db"].isDouble(), "källorna har riktig toppnivå och RMS (inte bara bandnivå)");
            }

            // Mer data: bandnivåer och övertoner per takt via get_mix_report, och hela analysen som fil.
            auto detailArgs = juce::DynamicObject::Ptr (new juce::DynamicObject());
            detailArgs->setProperty ("detail", "bars");
            const auto detailed = tools.call ("get_mix_report", juce::var (detailArgs.get()));
            check ((bool) detailed["ok"] && detailed["data"]["per_bar"].isArray() && detailed["data"]["kick_hits"].isArray(), "get_mix_report detail:'bars' ger data per takt och kickens slag");
            if (auto* perBar = detailed["data"]["per_bar"].getArray())
                check (perBar->size() >= 3 && (*perBar)[0]["bars"][0]["average_band_db"].isArray(), "bandnivåer per källa och takt finns");

            const auto exportFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_test_analysis.json");
            auto saveArgs = juce::DynamicObject::Ptr (new juce::DynamicObject());
            saveArgs->setProperty ("file", exportFile.getFullPathName());
            const auto saved = tools.call ("save_mix_report", juce::var (saveArgs.get()));
            check ((bool) saved["ok"] && exportFile.existsAsFile(), "save_mix_report sparar en fil");
            const auto parsed = juce::JSON::parse (exportFile.loadFileAsString());
            check (parsed["about"]["plugin"].isObject() && parsed["about"]["how_to_analyse"].isArray(), "filen har förklaringen för en ny AI");
            check (parsed["frames"]["seconds"].isArray() && parsed["frames"]["seconds"].size() > 5 && parsed["frames"]["band_db"].isObject(), "filen har bandnivåerna för varje ram");
            check (parsed["per_bar"].isArray() && parsed["sources"].isArray() && parsed["kick_hits"].isArray(), "filen har översikt, takt för takt och kickens slag");
            check (parsed["synth"]["parameters"].isObject() && parsed["pattern_timeline"].isArray() && parsed["pattern_timeline"].size() >= 1, "filen har alla inställningar och mönstret i loopens tid");
            check (parsed["synth_overview"]["instruments"].isArray() && parsed["synth_overview"]["instruments"][0]["oscillators"]["A"]["Table"].isString() && parsed["presets"].isObject(), "filen har inställningarna ordnade (oscillatorer med wavetable) och presets");
            {
                const auto series = parsed["frames"]["modulated_parameters"]["bass1Cutoff"];
                auto low = 1.0e9, high = -1.0e9;
                if (auto* list = series.getArray())
                    for (const auto& v : *list)
                    {
                        low = std::min (low, (double) v);
                        high = std::max (high, (double) v);
                    }
                check (series.size() > 5 && high - low > 1.0 && parsed["frames"]["modulation_sources"].isObject() && parsed["frames"]["limiter_reduction_db"].isArray()
                           && parsed["frames"]["beat_in_bar"].isArray(), "filen har moduleringen genom loopen (cutoff " + juce::String (low, 0) + "-" + juce::String (high, 0) + " Hz), limitern och taktslaget per ram");
            }
            check (parsed["meta"]["where_is_time_and_snapshots"].toString().contains ("SNAPSHOT"), "meta säger var tiden och ögonblicksbilderna finns");
            check (parsed["per_bar"][1]["bars"][0]["rms_db"].isDouble() && parsed["frames"]["rms_db"].isObject() && parsed["frames"]["ducking_db"].isObject(), "filen har nivå, ljusstyrka och duckning per takt och ram");

            // Filter: bara kicken, takt 1 och 30-300 Hz ger en mycket mindre fil.
            auto smallArgs = juce::DynamicObject::Ptr (new juce::DynamicObject());
            smallArgs->setProperty ("file", exportFile.getFullPathName());
            smallArgs->setProperty ("sources", juce::JSON::parse ("[\"kick\"]"));
            smallArgs->setProperty ("bars", "1");
            smallArgs->setProperty ("min_hz", 30.0);
            smallArgs->setProperty ("max_hz", 300.0);
            smallArgs->setProperty ("include", juce::JSON::parse ("[\"bars\",\"frames\",\"kicks\"]"));
            const auto bytesBefore = exportFile.getSize();
            const auto savedSmall = tools.call ("save_mix_report", juce::var (smallArgs.get()));
            const auto small = juce::JSON::parse (exportFile.loadFileAsString());
            check ((bool) savedSmall["ok"] && exportFile.getSize() < bytesBefore / 4 && small["per_bar"].size() == 1 && small["meta"]["sources"].size() == 1
                       && small["meta"]["bands_included"]["first_band_index"].operator int() >= 1, "filter (källor, takt, frekvensområde) gör filen mindre");
            std::printf ("exportfilen: %lld kB\n", (long long) (exportFile.getSize() / 1024));
            exportFile.deleteFile();
        }

        // Instrumentets EQ följer med när instrumentet töms.
        setValue ("bass2EqHp", 300.0f);
        processor.wipeInstrument (1);
        check (processor.apvts.getRawParameterValue ("bass2EqHp")->load() <= 20.5f, "EQ:n nollställs när instrumentet töms");

        // Mixanalysen: ett giltigt svar med rimliga uppgifter.
        MixAnalysis::Options options;
        auto copy = MixAnalysis::prepareCopy (processor);
        const auto report = MixAnalysis::run (*copy, 145.0, 52, options, nullptr, {}, {});
        check (report.valid && report.bars >= 1 && report.kickTailHz > 25.0f && report.kickTailHz < 70.0f, "mixanalysen ger en rapport (kickens svans " + juce::String (report.kickTailHz, 1) + " Hz)");

        // En påhittad ögonblicksbild: två källor som spelar exakt lika starkt i samma band ska ge en krock, och en tyst källa ingen.
        {
            MixAnalysis::Capture capture;
            capture.rate = 48000.0;
            capture.bpm = 140.0;
            capture.bars = 2;
            capture.resize (24);
            for (int f = 0; f < 24; ++f)
                capture.frameBar[(size_t) f] = f / 12;
            capture.present[(size_t) MixAnalysis::kickSource] = capture.present[(size_t) MixAnalysis::firstInstrument] = capture.present[(size_t) MixAnalysis::firstInstrument + 1] = true;
            capture.present[(size_t) MixAnalysis::masterSource] = true;
            capture.names[(size_t) MixAnalysis::kickSource] = "Kick";
            capture.names[(size_t) MixAnalysis::firstInstrument] = "A";
            capture.names[(size_t) MixAnalysis::firstInstrument + 1] = "B";
            capture.names[(size_t) MixAnalysis::masterSource] = "Master";
            for (int f = 0; f < 24; ++f)
            {
                capture.band (MixAnalysis::firstInstrument, f, 6) = -10.0f;      // 100-125 Hz
                capture.band (MixAnalysis::firstInstrument + 1, f, 6) = -12.0f;  // lika starkt
                capture.band (MixAnalysis::kickSource, f, 3) = -20.0f;            // kicken ligger någon annanstans
                capture.band (MixAnalysis::masterSource, f, 6) = -8.0f;
            }
            const auto fake = MixAnalysis::evaluate (capture, options, nullptr);
            auto found = false, falseKick = false;
            for (const auto& finding : fake.findings)
            {
                found = found || (finding.category == "collision" && finding.sources.contains ("A") && finding.sources.contains ("B"));
                falseKick = falseKick || (finding.category == "collision" && finding.sources.contains ("Kick"));
            }
            check (fake.valid && found, "två lika starka källor i samma band ger en krock");
            check (! falseKick, "kicken i ett annat band ger ingen krock");
            for (int f = 0; f < 24; ++f)
                capture.band (MixAnalysis::firstInstrument + 1, f, 6) = -40.0f; // B är tydligt svagare och under golvet
            const auto quiet = MixAnalysis::evaluate (capture, options, nullptr);
            auto stillThere = false;
            for (const auto& finding : quiet.findings)
                stillThere = stillThere || finding.category == "collision";
            check (! stillThere, "en tydligt svagare källa ger ingen krock");
        }

        std::puts (failed ? "FEL: eqtest" : "EQ- och analystestet gick igenom");
        return failed ? 1 : 0;
    }

    // Loggan ensam (med glöden på väg åt höger och åt vänster) som en bild, för att se glaset och reflektionerna.
    int logoImage (const juce::File& outFile)
    {
        juce::ScopedJuceInitialiser_GUI gui;
        LogoPainter logo ("FX");
        juce::Image image (juce::Image::ARGB, logo.getWidth() * 4 + 60, LogoPainter::height * 4 * 3, true, juce::SoftwareImageType());
        juce::Graphics g (image);
        g.fillAll (juce::Colour (0xff1b1e30));
        g.addTransform (juce::AffineTransform::scale (4.0f));
        const juce::Colour kickColour (0xffff9a3c), bassColour (0xff3fd2ff);
        for (int row = 0; row < 3; ++row)
        {
            // Rad 0: inget ljud (släckt), rad 1: kickens glöd, rad 2: instrumentens glöd.
            LogoPainter::Sweep kick { 0.15f + 0.3f * (float) row, row == 1 ? 0.9f : 0.0f, kickColour }, bass { 0.85f - 0.3f * (float) row, row == 2 ? 0.9f : 0.0f, bassColour };
            logo.paint (g, { 8.0f, (float) (row * LogoPainter::height) }, row == 0 ? 0.0f : 1.0f, kick, bass);
        }
        outFile.deleteFile();
        juce::FileOutputStream stream (outFile);
        juce::PNGImageFormat png;
        return stream.openedOk() && png.writeImageToStream (image, stream) ? 0 : 1;
    }

    // Lyssnarknappen: ett klick spelar medan man håller, och ett dubbelklick låser så att det fortsätter spela (stoppas inte av det andra klickets släpp).
    int listenTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        ListenButton button;
        button.setBounds (0, 0, 30, 24);
        auto starts = 0, stops = 0;
        button.onStart = [&starts] { ++starts; };
        button.onStop = [&stops] { ++stops; };

        auto event = [&button] (int clicks)
        {
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), juce::Point<float> (10.0f, 10.0f), juce::ModifierKeys(), 1.0f,
                                     juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                                     juce::MouseInputSource::defaultTiltY, &button, &button, now, juce::Point<float> (10.0f, 10.0f), now, clicks, false);
        };
        auto failed = false;
        auto check = [&failed] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what); failed |= ! ok; };

        button.mouseDown (event (1));
        check (starts == 1 && stops == 0, "ett tryck startar");
        button.mouseUp (event (1));
        check (stops == 1, "ett släpp stoppar");

        starts = stops = 0;
        button.mouseDown (event (1));
        button.mouseUp (event (1));
        button.mouseDown (event (2));
        button.mouseUp (event (2));
        button.mouseDoubleClick (event (2));
        check (starts == 2 && stops == 1, "dubbelklick: det andra släppet stoppar inte (startar 2, stoppar 1)");

        button.mouseDown (event (1)); // ett klick medan den är låst stänger av den
        button.mouseUp (event (1));
        check (stops == 2, "ett klick stänger av det som låsts");

        std::puts (failed ? "FEL: listentest" : "Lyssnartestet gick igenom");
        return failed ? 1 : 0;
    }

    // Ett fingeravtryck av ljudet för varje fabrikspreset (hash över utsignalens bitar): före och efter en optimering ska de vara lika.
    // Alla hela presets, alla kickpresets (på Init) och alla instrumentpresets (i instrument 1 på Init).
    int renderSignatures()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        auto signature = [&processor] (const juce::String& label)
        {
            OfflineRender::Settings settings;
            settings.sampleRate = renderRate;
            settings.blockSize = blockSize;
            settings.bpm = 145.0;
            settings.bars = 2;
            settings.notes = { 52 };
            settings.extraBars = 1;
            const auto audio = processor.renderOffline (settings, PluginBase::ExportPart::full);
            juce::uint64 hash = 1469598103934665603ull;
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                for (int i = 0; i < audio.getNumSamples(); ++i)
                {
                    juce::uint32 bits;
                    const auto value = audio.getSample (ch, i);
                    std::memcpy (&bits, &value, sizeof (bits));
                    hash = (hash ^ bits) * 1099511628211ull;
                }
            std::printf ("%-34s %016llx peak %.4f rms %.4f\n", label.toRawUTF8(), (unsigned long long) hash,
                         audio.getMagnitude (0, audio.getNumSamples()), audio.getRMSLevel (0, 0, audio.getNumSamples()));
        };

        for (int index = 0; index < Presets::factoryCount (Presets::Section::full, false); ++index)
        {
            processor.loadFactoryPreset (Presets::Section::full, index);
            signature ("full " + juce::String (index) + " " + processor.getPresetName (Presets::Section::full));
        }
        for (int index = 0; index < Presets::factoryCount (Presets::Section::kick, false); ++index)
        {
            processor.loadFactoryPreset (Presets::Section::full, 0);
            processor.loadFactoryPreset (Presets::Section::kick, index);
            signature ("kick " + juce::String (index) + " " + processor.getPresetName (Presets::Section::kick));
        }
        for (int index = 0; index < Presets::factoryCount (Presets::Section::bass, false); ++index)
        {
            processor.loadFactoryPreset (Presets::Section::full, 0);
            processor.loadFactoryPreset (Presets::Section::bass, index, 0);
            signature ("instr " + juce::String (index) + " " + processor.getPresetName (Presets::Section::bass, 0));
        }
        return 0;
    }

    // Flera instrument: lägga till, spara/läsa, ta bort (allt ska rensas) och gamla projekt (kicken var lager 3).
    int instrumentTest()
    {
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);

        auto failed = false;
        auto check = [&failed] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what); failed |= ! ok; };
        auto heardOf = [] (InstrumentProcessor& target, int what)
        {
            target.requestAudition (what);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            auto peak = 0.0f;
            for (int block = 0; block < 40; ++block)
            {
                buffer.clear();
                target.processBlock (buffer, midi);
                peak = std::max (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
            }
            return peak > 0.01f;
        };
        auto setValue = [&processor] (const juce::String& id, float real)
        {
            auto* parameter = processor.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (real));
        };

        check (Presets::bassLayerOf ("bass10Cutoff") == 9 && Presets::bassLayerOf ("bass1Cutoff") == 0 && Presets::bassLayerOf ("bass3On") == 2
                   && Presets::toBassLayer ("bass1Cutoff", 9) == "bass10Cutoff" && Presets::toBassLayer ("bass10Cutoff", 1) == "bass2Cutoff",
               "lagernumret läses ur parameter-ID:n (ett och två tecken)");
        check (processor.activeInstruments() == 3, "tre instrument från början");

        processor.setInstrumentCount (6);
        check (processor.activeInstruments() == 6, "sex instrument efter att ha lagt till tre");
        check (heardOf (processor, PluginBase::layerAudition (5)), "instrument 6 hörs");

        // Innehåll i instrument 5: ton, namn, ändrad inställning, och en LFO-koppling.
        processor.pattern.addNote (4, 0, 48, 3);
        processor.pattern.setCustom (4, true);
        processor.setLayerName (4, "Lead");
        setValue (ParamID::bass (4, BassField::cutoff), 777.0f);
        const auto& parameters = processor.getParameters();
        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == ParamID::bass (4, BassField::cutoff))
                processor.modRoutes.add (0, i, 0.3f);
        juce::String what;
        check (processor.instrumentHasContent (4, &what) && what.contains ("piano roll") && what.contains ("Lead") && what.contains ("modulation"), "innehållet i instrument 5 beskrivs");
        std::printf ("  (%s)\n", what.toRawUTF8());
        check (! processor.instrumentHasContent (5), "ett orört instrument har inget innehåll");

        // Spara och läs: antal, namn, ton och inställning följer med.
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        {
            const auto other = std::make_unique<InstrumentProcessor>();
            other->setStateInformation (state.getData(), (int) state.getSize());
            check (other->activeInstruments() == 6 && other->getLayerName (4) == "Lead" && other->pattern.note (4, 0).on.load() && other->pattern.note (4, 0).pitch.load() == 3
                       && std::abs (other->apvts.getRawParameterValue (ParamID::bass (4, BassField::cutoff))->load() - 777.0f) < 1.0f,
                   "sex instrument sparas och läses tillbaka med namn, toner och inställningar");
        }

        // Ta bort: instrument 4-6 rensas helt.
        processor.setInstrumentCount (3);
        check (processor.activeInstruments() == 3, "tillbaka till tre instrument");
        check (! processor.pattern.note (4, 0).on.load() && ! processor.pattern.isCustom (4), "mönstret i det borttagna instrumentet är tomt");
        check (processor.getLayerName (4) == "INSTR 5", "namnet är tillbaka till standard");
        check (std::abs (processor.apvts.getRawParameterValue (ParamID::bass (4, BassField::cutoff))->load()
                         - processor.apvts.getParameter (ParamID::bass (4, BassField::cutoff))->convertFrom0to1 (processor.apvts.getParameter (ParamID::bass (4, BassField::cutoff))->getDefaultValue())) < 1.0f,
               "inställningarna är tillbaka till standard");
        auto routes = 0;
        for (const auto& route : processor.modRoutes.routes)
            if (route.source.load() >= 0)
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[route.target.load()]); withId != nullptr && Presets::bassLayerOf (withId->paramID) == 4)
                    ++routes;
        check (routes == 0, "LFO-kopplingarna till instrumentet är borta");
        check (! processor.instrumentHasContent (4) && processor.instrumentHasContent (0), "det borttagna är tomt, de kvarvarande har innehåll");

        // Verktygslagret (MCP): antal instrument och källor i matrisen.
        {
            ToolLayer tools (processor);
            tools.call ("set_instruments", juce::JSON::parse (R"({"count":4})"));
            check (processor.activeInstruments() == 4, "set_instruments sätter antalet");
            tools.call ("set_modulation", juce::JSON::parse (R"({"routes":[{"instrument":2,"source":"ENV 3","param":"bass1Cutoff","amount":0.5},{"macro":2,"param":"kickTune","amount":0.2}]})"));
            check (processor.modRoutes.find (Modulation::instrumentSource (1, 1), processor.modulator.indexOf ("bass1Cutoff")) >= 0
                       && processor.modRoutes.find (Modulation::firstMacro + 1, processor.modulator.indexOf ("kickTune")) >= 0,
                   "matriskopplingar med instrument- och makrokällor sätts via verktyget");
            tools.call ("set_instruments", juce::JSON::parse (R"({"count":2})"));
            check (processor.activeInstruments() == 2, "set_instruments tar bort de sista");
            processor.setInstrumentCount (3);
        }

        // LFO med egen kurva och fri hastighet (verktyget), och att kurvan överlever spara/läsa.
        {
            ToolLayer tools (processor);
            tools.call ("set_modulation", juce::JSON::parse (R"({"lfos":[{"lfo":2,"points":[[0,1,0],[0.5,-1,0],[1,1,0]],"hz":3.5}]})"));
            check ((int) processor.apvts.getRawParameterValue ("lfo2Shape")->load() == 6 && processor.lfoCurves[1].getPoints().size() == 3
                       && (int) processor.apvts.getRawParameterValue ("lfo2Mode")->load() == 1 && std::abs (processor.apvts.getRawParameterValue ("lfo2Hz")->load() - 3.5f) < 0.01f,
                   "en egen LFO-kurva och fri hastighet sätts via verktyget");
            juce::MemoryBlock state;
            processor.getStateInformation (state);
            const auto other = std::make_unique<InstrumentProcessor>();
            other->setStateInformation (state.getData(), (int) state.getSize());
            check (other->lfoCurves[1].getPoints().size() == 3 && std::abs (other->lfoCurves[1].evaluate (0.5f) - 1.0f) < 0.01f, "LFO-kurvan sparas och läses tillbaka i projektet");
        }

        // Gamla projekt (version 5): kicken var lager 3.
        {
            auto legacy = std::make_unique<Pattern>();
            auto tree = legacy->toValueTree();
            tree.setProperty ("version", 5, nullptr);
            for (auto child : tree)
                if (child.hasType ("Notes") && (int) child["layer"] == Pattern::kickLayer)
                    child.setProperty ("layer", 3, nullptr);
            for (int i = tree.getNumChildren() - 1; i >= 0; --i)
                if ((int) tree.getChild (i)["layer"] > 3 && (int) tree.getChild (i)["layer"] != Pattern::kickLayer)
                    tree.removeChild (i, nullptr);
            auto fresh = std::make_unique<Pattern>();
            fresh->addNote (5, 96, 24); // ska rensas: projektet nämner inte instrument 6
            fresh->fromValueTree (tree);
            check (fresh->note (Pattern::kickLayer, 0).on.load() && ! fresh->note (3, 0).on.load(), "ett gammalt projekt: kicken hamnar i kicklagret, inte i instrument 4");
            check (! fresh->note (5, 96).on.load(), "ett gammalt projekt: instrument som det inte nämner töms");
        }

        std::puts (failed ? "FEL: instrumenttestet misslyckades" : "Instrumenttestet gick igenom");
        return failed ? 1 : 0;
    }

    // Hur tungt det är: tid per sekund ljud med 3 och med 10 instrument som alla spelar (under 1,0 = snabbare än realtid på en kärna).
    int perfTest()
    {
        auto timeIt = [] (int count)
        {
            const auto owner = std::make_unique<InstrumentProcessor>();
            auto& processor = *owner;
            processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
            processor.prepareToPlay (renderRate, blockSize);
            processor.setInstrumentCount (count);
            for (int layer = 3; layer < count; ++layer)
                processor.pattern.applyLayerTemplate (layer, PatternTemplate::rolling);
            juce::MidiBuffer first, none;
            first.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
            juce::AudioBuffer<float> buffer (2, blockSize);
            const auto seconds = 5.0;
            const auto blocks = (int) (renderRate * seconds / blockSize);
            const auto start = juce::Time::getMillisecondCounterHiRes();
            for (int block = 0; block < blocks; ++block)
            {
                buffer.clear();
                processor.processBlock (buffer, block == 0 ? first : none);
            }
            return (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0 / seconds;
        };
        const auto one = timeIt (1), few = timeIt (3), many = timeIt (10);
        std::puts (("1 instrument:  " + juce::String (one, 3) + " s CPU per sekund ljud").toRawUTF8());
        std::puts (("3 instrument:  " + juce::String (few, 3) + " s CPU per sekund ljud").toRawUTF8());
        std::puts (("10 instrument: " + juce::String (many, 3) + " s CPU per sekund ljud").toRawUTF8());
        return 0;
    }

    // Inställningarna (mappar, nyckel, skala) och uppackningen av vågformspaketet, utan att röra användarens egna filer.
    int settingsTest()
    {
        auto failed = false;
        auto check = [&failed] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what); failed |= ! ok; };

        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_settings_test");
        temp.deleteRecursively();
        temp.createDirectory();
        Settings::useFile (temp.getChildFile ("settings.xml"));

        check (Settings::presetsFolder() == Settings::defaultFolder ("presetsFolder") && Settings::aiKey().isEmpty() && Settings::uiScale() == 0.0f, "standardvärden utan inställningsfil");
        const auto custom = temp.getChildFile ("my presets");
        Settings::setFolder ("presetsFolder", custom);
        check (Settings::presetsFolder() == custom && Presets::folder (Presets::Section::kick, false) == custom.getChildFile ("Kick"), "egen presetmapp används av presetmenyerna");
        check (RollFiles::folder() == custom.getChildFile ("Piano Roll"), "pianorullarnas mapp följer presetmappen");
        Settings::setFolder ("presetsFolder", {});
        check (Settings::presetsFolder() == Settings::defaultFolder ("presetsFolder") && Settings::get ("presetsFolder").isEmpty(), "tillbaka till standardmappen");

        Settings::set ("aiKey", "  hemlig  ");
        check (Settings::aiKey() == "hemlig", "åtkomstnyckeln sparas utan mellanslag");
        const auto key = Settings::makeKey();
        check (key.length() == 16 && key != Settings::makeKey(), "en ny nyckel är slumpmässig");
        Settings::setUiScale (0.8f);
        check (std::abs (Settings::uiScale() - 0.8f) < 0.001f, "fönstrets skala sparas");
        Settings::set ("uiScale", "9");
        check (Settings::uiScale() == 0.0f, "orimlig skala ignoreras");

        // Vågformspaketet: ett arkiv med två vågformer, en annan fil och licensen.
        Settings::setFolder ("wavetablesFolder", temp.getChildFile ("waves"));
        const auto archive = temp.getChildFile ("pack.zip");
        {
            juce::ZipFile::Builder builder;
            auto add = [&builder] (const juce::String& name)
            {
                builder.addEntry (new juce::MemoryInputStream ("RIFFdata", 8, true), 5, name, juce::Time::getCurrentTime());
            };
            add ("AKWF-FREE-master/AKWF/AKWF_0001/AKWF_0001.wav");
            add ("AKWF-FREE-master/AKWF/AKWF_0002/AKWF_0002.wav");
            add ("AKWF-FREE-master/README.md");
            add ("AKWF-FREE-master/LICENSE");
            juce::FileOutputStream out (archive);
            builder.writeToStream (out, nullptr);
        }
        check (WavetablePack::installedCount() == 0, "paketet är inte installerat från början");
        const auto written = WavetablePack::extract (archive);
        check (written == 2 && WavetablePack::installedCount() == 2, "bara vågformerna packas upp");
        check (temp.getChildFile ("waves").getChildFile ("AKWF").getChildFile ("AKWF_0002").getChildFile ("AKWF_0002.wav").existsAsFile()
                   && temp.getChildFile ("waves").getChildFile ("LICENSE-CC0.md").existsAsFile()
                   && ! temp.getChildFile ("waves").getChildFile ("README.md").existsAsFile(),
               "mappstrukturen och licensen kommer med, övrigt inte");

        Settings::useFile ({});
        temp.deleteRecursively();
        std::puts (failed ? "FEL: inställningstestet misslyckades" : "Inställningstestet gick igenom");
        return failed ? 1 : 0;
    }

    // AI-chatten: nyckeln sparas krypterad, och ett riktigt anrop med en ogiltig nyckel ska ge ett begripligt fel (visar att nätverksvägen fungerar).
    int chatTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        auto failed = false;
        auto check = [&failed] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what); failed |= ! ok; };

        const auto settingsFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_chat_test.xml");
        settingsFile.deleteFile();
        Settings::useFile (settingsFile);

        Settings::setChatApiKey ("sk-ant-api03-hemlig-nyckel");
        check (Settings::chatApiKey() == "sk-ant-api03-hemlig-nyckel", "nyckeln läses tillbaka");
        check (! settingsFile.loadFileAsString().contains ("hemlig"), "nyckeln ligger inte i klartext i filen");
        check (Settings::chatModel() == "claude-sonnet-5-5", "standardmodellen");
        Settings::setChatApiKey ("sk-ant-invalid-test-key");

        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& chat = owner->getAiChat();
        chat.send ("Hej");
        for (int i = 0; i < 1500 && (chat.isBusy() || i < 5); ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        const auto entries = chat.getEntries();
        auto sawUser = false, sawError = false;
        juce::String errorText;
        for (const auto& entry : entries)
        {
            sawUser = sawUser || entry.kind == AiChat::Entry::Kind::user;
            if (entry.kind == AiChat::Entry::Kind::error)
            {
                sawError = true;
                errorText = entry.text;
            }
        }
        std::puts (("  svar: " + errorText).toRawUTF8());
        check (sawUser && sawError && ! chat.isBusy(), "ett ogiltigt anrop ger ett fel i samtalet och avslutas");
        check (errorText.contains ("401") || errorText.contains ("Could not reach"), "felet är begripligt (nyckeln nekas, eller ingen anslutning)");

        // Hela slingan mot en liten lokal låtsasserver: AI:n svarar med ett verktygsanrop, får resultatet och svarar med text.
        {
            std::atomic<int> requests { 0 };
            std::atomic<bool> sawToolResult { false }, stopServer { false };
            juce::StreamingSocket listener;
            const auto port = 18765;
            if (! listener.createListener (port, "127.0.0.1"))
            {
                check (false, "kunde inte starta låtsasservern");
            }
            else
            {
                std::thread server ([&]
                {
                    while (! stopServer)
                    {
                        std::unique_ptr<juce::StreamingSocket> client (listener.waitForNextConnection());
                        if (client == nullptr || stopServer)
                            continue;
                        std::string request;
                        char byte;
                        auto headerEnd = std::string::npos;
                        while (headerEnd == std::string::npos && client->read (&byte, 1, true) == 1)
                        {
                            request.push_back (byte);
                            headerEnd = request.find ("\r\n\r\n");
                        }
                        auto length = 0;
                        auto lower = request;
                        std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
                        if (const auto at = lower.find ("content-length:"); at != std::string::npos)
                            length = std::atoi (request.c_str() + at + 15);
                        std::string body;
                        for (int i = 0; i < length && client->read (&byte, 1, true) == 1; ++i)
                            body.push_back (byte);

                        const auto number = ++requests;
                        juce::String reply;
                        if (number == 1)
                            reply = R"({"type":"message","role":"assistant","stop_reason":"tool_use","usage":{"input_tokens":1000,"output_tokens":50},"content":[{"type":"text","text":"Raising the kick tune."},{"type":"tool_use","id":"toolu_test1","name":"set_parameters","input":{"values":{"kickTune":2.0}}}]})";
                        else
                        {
                            sawToolResult = body.find ("tool_result") != std::string::npos && body.find ("toolu_test1") != std::string::npos;
                            reply = R"({"type":"message","role":"assistant","stop_reason":"end_turn","usage":{"input_tokens":1200,"output_tokens":20},"content":[{"type":"text","text":"Done: the kick is tuned up."}]})";
                        }
                        const auto text = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + juce::String (reply.getNumBytesAsUTF8())
                                        + "\r\nConnection: close\r\n\r\n" + reply;
                        client->write (text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
                    }
                });

                Settings::set ("chatEndpoint", "http://127.0.0.1:" + juce::String (port) + "/v1/messages");
                chat.newChat();
                chat.send ("Tune the kick up");
                for (int i = 0; i < 1500 && (chat.isBusy() || i < 5); ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);

                stopServer = true;
                juce::StreamingSocket wake;
                wake.connect ("127.0.0.1", port, 500); // väcker servern så att den ser stopSignalen
                wake.close();
                server.join();

                for (const auto& entry : chat.getEntries())
                    std::puts (("  [" + juce::String ((int) entry.kind) + "] " + entry.text.substring (0, 200)).toRawUTF8());
                std::printf ("  anrop: %d\n", (int) requests);
                auto toolShown = false, answerShown = false;
                for (const auto& entry : chat.getEntries())
                {
                    toolShown = toolShown || (entry.kind == AiChat::Entry::Kind::tool && entry.text.contains ("set_parameters"));
                    answerShown = answerShown || (entry.kind == AiChat::Entry::Kind::assistant && entry.text.contains ("tuned up"));
                }
                check (requests >= 2 && sawToolResult, "AI:n får verktygsresultatet i nästa anrop");
                check (std::abs (owner->apvts.getRawParameterValue (ParamID::kickTune)->load() - 2.0f) < 0.01f, "verktygsanropet ändrade ljudet (kickTune = 2)");
                check (toolShown && answerShown, "anropet och svaret visas i samtalet");
                check (chat.getUsage().contains ("in"), "tokenräknaren visar användningen");
                Settings::set ("chatEndpoint", {});
            }
        }

        Settings::setChatApiKey ({});
        check (Settings::chatApiKey().isEmpty(), "nyckeln kan tas bort");
        Settings::useFile ({});
        settingsFile.deleteFile();
        std::puts (failed ? "FEL: chatten misslyckades" : "Chatten gick igenom");
        return failed ? 1 : 0;
    }

    // Provlyssning: requestAudition ska ge ljud (kick och baslager) och publicera kickens spelmarkör.
    int auditionTest()
    {
        auto run = [] (int what, bool& heard, float& markerSeen)
        {
            const auto owner = std::make_unique<InstrumentProcessor>();
            auto& processor = *owner;
            processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
            processor.prepareToPlay (renderRate, blockSize);
            processor.requestAudition (what);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            auto peak = 0.0f;
            markerSeen = -1.0f;
            for (int block = 0; block < 40; ++block)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
                peak = std::max (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
                markerSeen = std::max (markerSeen, processor.transportStatus.kickPlayTime.load());
            }
            heard = peak > 0.01f;
        };

        auto failed = false;
        bool heard = false;
        float marker = -1.0f;
        run (0, heard, marker);
        std::printf ("kick: hörs %s, markör %.3f s\n", heard ? "ja" : "NEJ", marker);
        failed |= ! heard || marker < 0.0f;
        run (PluginBase::layerAudition (0), heard, marker);
        std::printf ("bas 1: hörs %s\n", heard ? "ja" : "NEJ");
        failed |= ! heard;

        // Bara en källa: OSC 3 är av som standard men ska höras när man provlyssnar den, och samplingen (ingen laddad) är tyst.
        run (PluginBase::soloAudition (0, 2), heard, marker);
        std::printf ("bas 1, bara OSC 3: hörs %s\n", heard ? "ja" : "NEJ");
        failed |= ! heard;
        run (PluginBase::soloAudition (0, 3), heard, marker);
        std::printf ("bas 1, bara samplingen (ingen laddad): hörs %s\n", heard ? "JA (fel)" : "nej");
        failed |= heard;
        // Förhandsspelning av en pianorulle (utan DAW-ton): kicken och bas 2 ska höras var för sig.
        run (PluginBase::previewRequest (Pattern::kickLayer), heard, marker);
        std::printf ("förhandsspelning kick: hörs %s\n", heard ? "ja" : "NEJ");
        failed |= ! heard;
        run (PluginBase::previewRequest (1), heard, marker);
        std::printf ("förhandsspelning bas 2: hörs %s\n", heard ? "ja" : "NEJ");
        failed |= ! heard;
        std::puts (failed ? "FEL: provlyssningen fungerar inte" : "Provlyssningen fungerar");
        return failed ? 1 : 0;
    }

    // Sparar ett kickpreset, ändrar ljudet, laddar presetet och kontrollerar värdena.
    // Testfilen tas bort efteråt.
    int presetTest()
    {
        // På heapen: signaluttagen i processorn är flera MB, mer än en konsolprograms stack.
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        auto* pitch = processor.apvts.getParameter (ParamID::kickTune);
        auto* cutoff = processor.apvts.getParameter (ParamID::bass (0, BassField::cutoff));

        pitch->setValueNotifyingHost (pitch->convertTo0to1 (5.0f));
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (1000.0f));

        const auto name = juce::String ("__presettest__");
        if (! processor.saveUserPreset (Presets::Section::kick, name))
        {
            std::puts ("FEL: kunde inte spara");
            return 1;
        }

        const auto file = Presets::folder (Presets::Section::kick, false).getChildFile (name + ".mofp");
        pitch->setValueNotifyingHost (pitch->convertTo0to1 (-3.0f));
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (500.0f));

        const auto loaded = processor.loadUserPreset (Presets::Section::kick, file);
        const auto pitchBack = pitch->convertFrom0to1 (pitch->getValue());
        const auto cutoffAfter = cutoff->convertFrom0to1 (cutoff->getValue());
        file.deleteFile();

        std::printf ("laddad %d, kick tune %.2f (väntat 5), bas cutoff %.1f (väntat 500, orörd)\n",
                     loaded ? 1 : 0, pitchBack, cutoffAfter);

        auto ok = loaded && std::abs (pitchBack - 5.0f) < 0.02f && std::abs (cutoffAfter - 500.0f) < 1.0f
                     && processor.getPresetName (Presets::Section::kick) == name;

        // Mönster-preset: en rad (noter, GATE, rutnät, längd) sparas och laddas i en annan rad; övriga rader rörs inte.
        {
            auto& pattern = processor.pattern;
            const auto rowName = juce::String ("__rowpresettest__");
            pattern.clearNotes (1);
            pattern.addNote (1, 96, 24, 7, 0.5f);
            pattern.addNote (1, 192, 48, -5, 0.9f, true);
            pattern.setCustom (1, true);
            pattern.gateStep (1, 3) = false;
            pattern.addNote (2, 0, 24, 1, 0.8f);
            pattern.addNote (Pattern::kickLayer, 0, 24, 0, 1.0f);
            auto* gridParameter = processor.apvts.getParameter (ParamID::bass (1, BassField::grid));
            auto* delayParameter = processor.apvts.getParameter (ParamID::bass (1, BassField::delay));
            gridParameter->setValueNotifyingHost (gridParameter->convertTo0to1 ((float) (int) BassGrid::eighthTriplet));
            delayParameter->setValueNotifyingHost (delayParameter->convertTo0to1 (12.0f));

            const auto saved = processor.saveUserPreset (Presets::Section::pattern, rowName, 1);
            const auto rowFile = Presets::folder (Presets::Section::pattern, false).getChildFile (rowName + ".mofp");
            const auto listed = processor.getUserPresets (Presets::Section::pattern, 1).contains (rowFile);
            const auto listedForKick = processor.getUserPresets (Presets::Section::pattern, Pattern::kickLayer).contains (rowFile);
            const auto intoKick = processor.loadUserPreset (Presets::Section::pattern, rowFile, Pattern::kickLayer); // en instrumentrad får inte in i kicken

            const auto loadedRow = processor.loadUserPreset (Presets::Section::pattern, rowFile, 4);
            const auto& note = pattern.note (4, 192);
            auto* otherGrid = processor.apvts.getParameter (ParamID::bass (4, BassField::grid));
            const auto movedGrid = (int) std::lround (otherGrid->convertFrom0to1 (otherGrid->getValue()));
            const auto movedDelay = processor.apvts.getParameter (ParamID::bass (4, BassField::delay))->convertFrom0to1 (processor.apvts.getParameter (ParamID::bass (4, BassField::delay))->getValue());
            const auto rowOk = saved && listed && ! listedForKick && ! intoKick && loadedRow
                            && pattern.note (4, 96).on.load() && pattern.note (4, 96).pitch.load() == 7
                            && note.on.load() && note.slide.load() && note.pitch.load() == -5 && note.length.load() == 48
                            && pattern.isCustom (4) && ! pattern.isGateStepOn (4, 3)
                            && movedGrid == (int) BassGrid::eighthTriplet && std::abs (movedDelay - 12.0f) < 0.5f
                            && pattern.note (2, 0).on.load() && pattern.note (Pattern::kickLayer, 0).on.load()
                            && processor.getPresetName (Presets::Section::pattern, 4) == rowName;
            std::printf ("  saved %d listed %d listedForKick %d intoKick %d loaded %d n96 %d/%d n192 %d slide %d pitch %d len %d custom %d gate3 %d grid %d(%d) delay %.1f other %d kick %d name '%s'\n",
                         saved, listed, listedForKick, intoKick, loadedRow, (int) pattern.note (4, 96).on.load(), pattern.note (4, 96).pitch.load(), (int) note.on.load(), (int) note.slide.load(), note.pitch.load(),
                         note.length.load(), (int) pattern.isCustom (4), (int) pattern.isGateStepOn (4, 3), movedGrid, (int) BassGrid::eighthTriplet, movedDelay,
                         (int) pattern.note (2, 0).on.load(), (int) pattern.note (Pattern::kickLayer, 0).on.load(), processor.getPresetName (Presets::Section::pattern, 4).toRawUTF8());
            rowFile.deleteFile();
            std::printf ("mönster-preset (en rad): " "%s" "\n", rowOk ? "ok" : "FEL");
            ok = ok && rowOk;
        }

        std::puts (ok ? "Presettest OK" : "FEL: presettest");
        return ok ? 0 : 1;
    }

    int snapshot (const juce::String& kind, const juce::File& outFile, int tab, bool withModulation)
    {
        juce::ScopedJuceInitialiser_GUI gui;

        auto processor = createProcessor (kind);
        processor->selectedTab = tab % 10;            // nedre panelen
        processor->selectedSynthTab = tab / 10 % 10;  // övre panelen (tiotal)
        processor->selectedFxView = tab / 100 % 10;   // EFFECTS-vyn (hundratal)
        processor->pianoRollOpen = tab / 1000 % 10 > 0; // pianorullen i stället för stegen (tusental)
        processor->selectedPatternMode = tab / 10000 % 10; // 1 = GATE-läget (tiotusental)
        processor->snapshotPaste = tab / 100000000 % 10 > 0; // pianorullen visar en inklistring (hundramiljontal)
        processor->selectedPatternLayer = tab / 10000000 % 10; // valt lager i PATTERN: 0-8 instrument, 9 = kicken (tiomiljontal)
        if (processor->selectedPatternLayer == 9)
            processor->selectedPatternLayer = Pattern::kickLayer;
        processor->sequencerRowHeight = juce::SystemStats::getEnvironmentVariable ("KB_ROWHEIGHT", "0").getIntValue(); // radhöjden (zoom)
        if (juce::SystemStats::getEnvironmentVariable ("KB_CHATDEMO", "0").getIntValue() != 0) // AI-fliken med ett exempelsamtal (och en nyckel i en tillfällig fil)
        {
            Settings::useFile (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_demo_settings.xml"));
            Settings::setChatApiKey ("demo");
            auto& chat = processor->getAiChat();
            using Kind = AiChat::Entry::Kind;
            chat.addEntry ({ Kind::user, "Make the kick punchier and the bass tighter under it.", {} });
            chat.addEntry ({ Kind::assistant, "I will shorten the kick's tail a little, add a short click on top, and give the bass a faster sidechain so it gets out of the way. Let me first look at the current sound.", {} });
            chat.addEntry ({ Kind::tool, "get_state  {}", {} });
            chat.addEntry ({ Kind::tool, "set_parameters  {\"values\":{\"kickClick\":0.5,\"kickLength\":0.85}}", {} });
            chat.addEntry ({ Kind::error, "The API key was not accepted (401). Check it with the KEY button.", {} });
            chat.addEntry ({ Kind::note, "Undid the last change.", {} });
        }
        if (juce::SystemStats::getEnvironmentVariable ("KB_LFOCUSTOM", "0").getIntValue() != 0) // LFO 1 och 2 med egna kurvor, LFO 3 i Hz
        {
            auto set = [&processor] (const char* id, float value)
            {
                if (auto* parameter = processor->apvts.getParameter (id))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            set ("lfo1Shape", 6.0f);
            set ("lfo2Shape", 6.0f);
            set ("lfo3Mode", 1.0f);
            processor->lfoCurves[1].setPoints ({ { 0.0f, 1.0f, 0.0f }, { 0.2f, 0.0f, -0.6f }, { 0.35f, 0.7f, 0.0f }, { 0.6f, 0.2f, 0.0f }, { 1.0f, 1.0f, 0.0f } });
        }
        processor->modEnvelopesOpen = juce::SystemStats::getEnvironmentVariable ("KB_MODOPEN", "0").getIntValue() != 0; // MODULATION-dragspelet öppet
        processor->selectedInstrument = juce::SystemStats::getEnvironmentVariable ("KB_INSTRUMENT", "0").getIntValue(); // valt instrument (0-9)
        if (const auto count = juce::SystemStats::getEnvironmentVariable ("KB_INSTRUMENTS", {}); count.isNotEmpty())
            processor->setInstrumentCount (count.getIntValue()); // antal instrument i bilden
        if (tab / 1000000 > 0)
        {
            // Exempel: 16 takter, bas 1 på 4 takter och bas 2 på 1 takt (miljontal).
            auto set = [&processor] (const char* id, int choice)
            {
                if (auto* p = processor->apvts.getParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) choice));
            };
            set (ParamID::patternBars, 4);
            set (ParamID::rowBars (1), 3);
            set (ParamID::rowBars (2), 1);
        }
        if (tab / 100000 % 10 > 0)
        {
            // Exempel: lager 2 som pianorulle, med en lång ton och en glide-ton (hundratusental).
            processor->pattern.addNote (1, 0, 288, 0);
            processor->pattern.addNote (1, 96, 24, 7, 0.8f, true);
            processor->pattern.addNote (1, 192, 96, 0);
            processor->pattern.setCustom (1, true);
        }
        processor->setPlayConfigDetails (kind == "fx" ? 2 : 0, 2, renderRate, blockSize);
        processor->prepareToPlay (renderRate, blockSize);

        if (juce::SystemStats::getEnvironmentVariable ("KB_SPECTRUM", "0").getIntValue() != 0) // ANALYZER-fliken med analysen på: spela två takter först
        {
            processor->snapshotSpectrum = true;
            processor->instrumentTapsOn = true;
            OfflineRender::Settings settings;
            settings.sampleRate = renderRate;
            settings.blockSize = blockSize;
            settings.bpm = 138.0;
            settings.bars = 2;
            settings.notes = { 52 };
            settings.extraBars = 0;
            OfflineRender::render (*processor, settings);
        }

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
            processor->modRoutes.add (0, indexOf ("bass1Cutoff"), 0.35f);
            processor->modRoutes.add (1, indexOf ("bass1Cutoff"), -0.2f);
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

// Verktygets kommandon. Startas av KickbaseMcp.exe när det heter MyOwnFruitTool.exe eller får `tool` som första argument.
int toolMain (int argc, char* argv[])
{
    if (argc == 2 && juce::String (argv[1]) == "presettest")
        return presetTest();
    if (argc == 2 && juce::String (argv[1]) == "auditiontest")
        return auditionTest();
    if (argc == 2 && juce::String (argv[1]) == "chattest")
        return chatTest();
    if (argc == 2 && juce::String (argv[1]) == "settingstest")
        return settingsTest();
    if (argc == 2 && juce::String (argv[1]) == "perftest")
        return perfTest();
    if (argc == 2 && juce::String (argv[1]) == "instrumenttest")
        return instrumentTest();
    if (argc == 2 && juce::String (argv[1]) == "lengthtest")
        return lengthTest();
    if (argc == 3 && juce::String (argv[1]) == "logoimage")
        return logoImage (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    if (argc == 2 && juce::String (argv[1]) == "rendersig")
        return renderSignatures();
    if (argc == 2 && juce::String (argv[1]) == "listentest")
        return listenTest();
    if (argc == 2 && juce::String (argv[1]) == "eqtest")
        return eqTest();
    if (argc >= 2 && juce::String (argv[1]) == "analysetest")
        return analyseTest (argc > 2 ? juce::String (argv[2]).getIntValue() : 1);
    if (argc == 2 && juce::String (argv[1]) == "rolltest")
        return rollLengthTest (true);

    // params [instrument|fx]: parameterregistret.
    if (argc >= 2 && juce::String (argv[1]) == "params")
        return dumpParameters (argc > 2 ? juce::String (argv[2]) : juce::String ("instrument"));

    // audit <del> [nummer] [png-mapp] [bpm]: renderar och mäter fabrikspresets (full, kick, bass, effects).
    if (argc >= 3 && juce::String (argv[1]) == "audit")
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto only = argc > 3 && juce::String (argv[3]) != "all" ? juce::String (argv[3]).getIntValue() : -1;
        const auto folder = argc > 4 ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[4])) : juce::File();
        return auditPresets (juce::String (argv[2]), only, folder, argc > 5 ? juce::String (argv[5]).getDoubleValue() : 145.0);
    }

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
