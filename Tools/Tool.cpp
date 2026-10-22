// Utvecklingsverktyg: renderar ljud offline och tar skärmbilder av gränssnittet, så att
// pluginsen kan kontrolleras utan DAW.
//
//   KickbaseTool render   instrument|fx <ut.wav> [bpm] [takter] [ton eller toner, t.ex. 66,64,61,59] [preset] [effektpreset]
//   (flera toner: en per takt, bytet sker exakt på taktstrecket; preset kan följas av kick- och
//   baspreset, t.ex. 16+k3+b5 = helpreset 16, sedan kickpreset 3 och baspreset 5; p:id=värde sätter
//   en parameter, t.ex. p:fxA1Type=2+p:bass1FxChannel=1)
//   KickbaseTool export   <ut.wav> full|kick|bass <takter> [bpm] [ton] [preset]   (som EXPORT-knappen)
//   KickbaseTool snapshot instrument|fx <ut.png> [flik] [mod]   (mod = med exempel-LFO-kopplingar)
//   KickbaseTool presettest
//   KickbaseTool analyze  <ljudfil> [start-sekund] [slut-sekund]
//   KickbaseTool fitbass  <ljudfil> <start> <slut> [steg per slag]
//   KickbaseTool fitkick  <ljudfil> <start> <slut>
//   KickbaseTool fitbeat  <ljudfil> <start> <slut> [rolling|galloping|triplet|offbeat]
//   KickbaseTool comparebeat <ljudfil> <start> <slut> <mönster> <rendering.wav>

#include <set>
#include "../Source/Core/MixAnalysis.h"
#include "../Source/Ui/LogoPainter.h"
#include "../Source/Ui/InstrumentMenu.h"
#include "../Source/Ui/ListenButton.h"
#include "../Source/Ui/Theme.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Core/OfflineRender.h"
#include "../Source/Core/ToolLayer.h"
#include "../Source/Core/LiveCode.h"
#include "../Source/Ui/LiveCodePage.h"
#include "../Source/Core/AiChat.h"
#include "../Source/Core/AiMemory.h"
#include "../Source/Core/AiProvider.h"
#include "../Source/Core/Settings.h"
#include "../Source/Core/WavetablePack.h"
#include "../Source/Core/LibraryIndex.h"
#include "../Source/Core/RollFiles.h"
#include "../Source/Core/OscPresets.h"
#include "../Source/Instrument/InstrumentProcessor.h"
#include "Analyzer.h"
#include "../Source/Core/AudioAnalysis.h"
#include "Scenarios.h"
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

    // Live Code (LC): Strudel-kod blir toner i pianorullen via verktyget live_code_create.
    int liveCodeTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);
        auto failed = false;
        auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };
        auto run = [&] (const juce::String& code, int cycles = 1)
        {
            ToolLayer tools (processor);
            auto* args = new juce::DynamicObject();
            args->setProperty ("code", code);
            args->setProperty ("cycles", cycles);
            args->setProperty ("overwrite", true); // standardmönstret har egna toner; skyddet testas separat i blocket "Skydd"
            return tools.call ("live_code_create", juce::var (args));
        };
        auto& pattern = processor.pattern;
        const auto ticks = Pattern::ticksPerBar;

        auto r = run ("note(\"c2 [e2 g2]*2\").s(\"i1\")");
        check ((bool) r["ok"], "enkel not-kod går igenom");
        check (pattern.note (0, 0).on.load() && pattern.note (0, 0).pitch.load() == 0, "c2 är pitch 0 på tick 0");
        check (pattern.note (0, ticks / 2).on.load() && pattern.note (0, ticks / 2).pitch.load() == 4, "e2 = pitch 4 vid halva takten");

        auto* cutoff = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter ("bass1Cutoff"));
        const auto oldCutoff = cutoff != nullptr ? cutoff->convertFrom0to1 (cutoff->getValue()) : 0.0f;
        r = run ("instr1.Cutoff.set(1300);\n\nnote(\"c2\").s(\"i1\")");
        check ((bool) r["ok"] && cutoff != nullptr && std::abs (cutoff->convertFrom0to1 (cutoff->getValue()) - 1300.0f) < 1.0f,
               "CREATE applicerar instr1.Cutoff.set() på den riktiga bass1Cutoff-parametern");
        r = run ("instr1.Cutoff.set(1500);" + juce::String ("\n") + "note(\"c2\").s(\"i1\")");
        check ((bool) r["ok"] && cutoff != nullptr && std::abs (cutoff->convertFrom0to1 (cutoff->getValue()) - 1500.0f) < 1.0f,
               ".set() direkt före mönstret utan blankrad fungerar");
        auto cutoffNow = [&] { return cutoff != nullptr ? cutoff->convertFrom0to1 (cutoff->getValue()) : 0.0f; };
        r = run ("instr1.Cutoff.set(900);\n\nconst x = instr1.Cutoff.get();\n\nnote(\"c2\").s(\"i1\")");
        check (! (bool) r["ok"] && r["error"].toString().contains ("efter skrivning"), ".get() efter .set i samma körning ger fel");
        r = run ("instr1.Cutoff.set(1300);\n\nnote(\"c2\").s(\"i1\")");
        r = run ("instr1.cutoff.set(500);\n\nnote(\"c2\").s(\"i1\")");
        check (! (bool) r["ok"] || std::abs (cutoffNow() - 1300.0f) < 1.0f, "okänt (fel skiftläge) namn ändrar ingen parameter");
        r = run ("note(\"c2\").s(\"i1\").lpf(700)");
        check ((bool) r["ok"] && std::abs (cutoffNow() - 700.0f) < 1.0f, "Strudel .lpf(700) blir bass1Cutoff 700 (genväg)");
        r = run ("instr1.Cutoff.set(900);\n\nnote(\"c2\").s(\"i1\").lpf(700)");
        check ((bool) r["ok"] && std::abs (cutoffNow() - 900.0f) < 1.0f, "ett uttryckligt .set vinner över .lpf");
        r = run ("note(\"c2\").s(\"i1\").pan(1).release(0.05)");
        check ((bool) r["ok"] && std::abs (processor.apvts.getRawParameterValue ("bass1APan")->load() - 1.0f) < 0.01f, "Strudel .pan(1) blir bass1APan 1");
        check (std::abs (processor.apvts.getRawParameterValue ("bass1Release")->load() - 50.0f) < 0.5f, "Strudel .release(0.05 s) blir bass1Release 50 ms");
        r = run ("instr1.Cutoff.set(1300);\n\nnote(\"c2\").s(\"i1\")");
        {
            auto bad = juce::var (new juce::DynamicObject());
            bad.getDynamicObject()->setProperty ("bass1cutoff", 500);
            auto args = juce::var (new juce::DynamicObject());
            args.getDynamicObject()->setProperty ("values", bad);
            ToolLayer tools (processor);
            const auto problems = tools.call ("set_parameters", args)["data"]["problems"];
            check (problems.size() == 1 && problems[0].toString().contains ("Did you mean bass1Cutoff"), "set_parameters föreslår rätt skiftläge");
        }
        processor.liveCodeLocks = ToolLayer::liveCodeBit (0);
        r = run ("instr1.Cutoff.set(400);\n\nnote(\"c2\").s(\"i2\")");
        check (std::abs (cutoffNow() - 1300.0f) < 1.0f, "låst instrument: .set hoppas över");
        check (r["data"]["problems"].size() >= 1, "låst instrument: hoppet rapporteras i problems");
        processor.liveCodeLocks = 0;
        {
            std::set<juce::String> seen;
            auto unique = true;
            for (auto& [id, value] : ToolLayer (processor).liveCodeParameters())
            {
                juce::ignoreUnused (value);
                const auto cut = id.indexOfAnyOf ("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
                if (cut < 1 || ! juce::CharacterFunctions::isLowerCase (id[0]))
                    continue;
                auto group = id.substring (0, cut);
                if (group.startsWith ("bass") && group.substring (4).containsOnly ("0123456789") && group.length() > 4)
                    group = "instr" + group.substring (4);
                if (! seen.insert (group + "." + id.substring (cut)).second || group.isEmpty())
                    unique = false;
            }
            check (unique && ! seen.empty(), "parameter-id:n ger unika träd-namn");
        }
        if (cutoff != nullptr)
            Presets::setParameter (*cutoff, cutoff->convertTo0to1 (oldCutoff));

        r = run ("s(\"bd*4\")");
        check ((bool) r["ok"], "kick-kod går igenom");
        check (pattern.note (Pattern::kickLayer, 0).on.load() && pattern.note (Pattern::kickLayer, ticks / 4).on.load()
                   && ! pattern.note (Pattern::kickLayer, ticks / 8).on.load(), "bd*4 ger kick på fjärdedelarna");

        r = run ("note(\"c2,e2,g2,e2\").s(\"i1\")");
        check ((bool) r["ok"] && pattern.stackSize (0, 0) == 3, "c2,e2,g2 ger tre toner på tick 0 (dubbletten e2 hoppas över)");
        Pattern::StackNote top;
        check (pattern.getStack (0, 0, 0, top) && top.pitch == 0 && pattern.getStack (0, 0, 2, top) && top.pitch == 7, "huvudton c2, andra stapeln g2");
        r = run ("note(\"c2,e2,g2,b2,d3\").s(\"i1\")");
        check ((bool) r["ok"] && pattern.stackSize (0, 0) == Pattern::maxStack, "fler än maxStack toner kapas");

        check (! (bool) run ("note(\"c2\".s(")["ok"], "syntaxfel ger ett fel, inte en krasch");
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        check (! (bool) run ("while(true){}")["ok"], "en oändlig slinga avbryts");
        check (juce::Time::getMillisecondCounterHiRes() - t0 < 5000, "avbrottet sker inom 5 s");

        // LIVE CODE-sidan: PLAY, STOP och CREATE klickas som en användare (knapparna hittas bland sidans barn).
        {
            LiveCodePage page (processor);
            page.setBounds (0, 0, 600, 400);
            page.confirmCreate = false; // en modal ruta skulle blockera testet
            juce::CodeEditorComponent* code = nullptr;
            juce::TextButton *play = nullptr, *stop = nullptr, *create = nullptr, *codeOnlyBtn = nullptr;
            for (auto* c : page.getChildren())
            {
                if (auto* e = dynamic_cast<juce::CodeEditorComponent*> (c)) code = e;
                if (auto* b = dynamic_cast<juce::TextButton*> (c))
                {
                    if (b->getButtonText() == "AUTO") play = b;
                    if (b->getButtonText() == "STOP") stop = b;
                    if (b->getButtonText() == "CREATE") create = b;
                    if (b->getButtonText() == "CODE ONLY") codeOnlyBtn = b;
                }
            }
            check (code && play && stop && create, "LIVE CODE-sidan har kodruta, PLAY, STOP och CREATE");
            if (code && play && stop && create)
            {
                auto runFor = [] (int milliseconds) { juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds); };
                int changed = 0;
                juce::String lastError;
                page.onPatternChanged = [&changed] { ++changed; };
                page.onError = [&lastError] (const juce::String& e) { lastError = e; };

                check (! play->getToggleState(), "AUTO: av när sidan skapas (Tobias 21 okt)");
                check (code->getDocument().getAllContent().isEmpty(), "LIVE CODE: tom kodruta när sidan skapas");

                pattern.clearNotes (0);
                code->getDocument().replaceAllContent ("note(\"c2,e2\").s(\"i1\")");
                create->triggerClick();
                runFor (200); // triggerClick körs via meddelandekön
                check (changed == 1 && pattern.stackSize (0, 0) == 2, "CREATE skriver koden en gång (två staplade toner)");

                pattern.clearNotes (0);
                play->triggerClick();
                runFor (1500);
                check (pattern.stackSize (0, 0) == 2 && changed >= 2 && lastError.isEmpty(), "PLAY räknar koden i bakgrunden och skriver mönstret");

                const auto before = changed;
                code->getDocument().replaceAllContent ("note(\"g2\").s(\"i1\")");
                runFor (1500);
                check (changed > before && pattern.stackSize (0, 0) == 1 && pattern.note (0, 0).pitch.load() == 7, "PLAY: en ändring i koden uppdaterar mönstret efter debouncen");

                stop->triggerClick();
                runFor (100);
                const auto afterStop = changed;
                code->getDocument().replaceAllContent ("note(\"c2\").s(\"i1\")");
                runFor (1200);
                check (changed == afterStop && pattern.note (0, 0).pitch.load() == 7, "STOP: ändringar i koden skrivs inte längre");

                play->triggerClick();
                code->getDocument().replaceAllContent ("note(\"c2\".s(");
                runFor (1500);
                check (lastError.isNotEmpty() && ! lastError.contains ("    at "), "PLAY: ett syntaxfel visas som felmeddelande");
                stop->triggerClick();

                // flera satser: const, $:-block och blockslut vid tom rad
                pattern.clearNotes (0);
                pattern.clearNotes (Pattern::kickLayer); // sidans CREATE har ingen overwrite: tomma rader ger ingen fråga
                code->getDocument().replaceAllContent ("const n = \"c2,e2\"\n$: note(n).s(\"i1\")\n\n$: s(\"bd*4\")");
                create->triggerClick();
                runFor (200);
                check (pattern.stackSize (0, 0) == 2, "CREATE: const och flera $:-block fungerar");

                // CODE ONLY: soloMask följer raderna Live Code skrivit; användarens SOLO vinner; STOP återställer.
                if (codeOnlyBtn)
                {
                    const auto owned = processor.liveCodeOwned.load();
                    processor.soloMask = 0;
                    check (owned != 0 && codeOnlyBtn->isEnabled(), "CODE ONLY: aktiv när Live Code har skrivit rader");
                    codeOnlyBtn->triggerClick(); runFor (100);
                    check (processor.soloMask.load() == owned, "CODE ONLY: soloMask = raderna Live Code äger");
                    stop->triggerClick(); runFor (100);
                    check (processor.soloMask.load() == 0 && ! codeOnlyBtn->getToggleState(), "CODE ONLY: STOP återställer soloMask");
                    codeOnlyBtn->triggerClick(); runFor (100);
                    processor.soloMask = 2; // användaren soloar instrument 2 (annat värde än raderna Live Code äger)
                    create->triggerClick();
                    runFor (200);
                    check (processor.soloMask.load() == 2 && ! codeOnlyBtn->getToggleState(), "CODE ONLY: användarens SOLO vinner");
                    processor.soloMask = 0;
                    const auto ownedBefore = processor.liveCodeOwned.load();
                    processor.liveCodeOwned = 0;
                    codeOnlyBtn->triggerClick(); runFor (100);
                    check (! codeOnlyBtn->getToggleState() && processor.soloMask.load() == 0, "CODE ONLY: utan egna rader händer inget (soloMask 0 = hör allt)");
                    processor.liveCodeOwned = ownedBefore;
                }

                // AUTO: laddning kör inget, giltig ändring körs av sig själv, ogiltig kod rör inget, borttagen rad töms, handrad ligger kvar.
                {
                    stop->triggerClick();
                    runFor (100);
                    play->triggerClick();
                    runFor (600);
                    check (play->getToggleState(), "AUTO: på igen före laddningen");
                    juce::ValueTree loaded ("LiveCode");
                    loaded.setProperty ("code", "note(\"a2\").s(\"i1\")", nullptr);
                    loaded.setProperty ("locks", 0, nullptr);
                    processor.liveCodeFromValueTree (loaded);
                    ++processor.liveCodeLoads;
                    runFor (700);
                    check (play->getToggleState(), "laddning: AUTO ligger kvar på");
                    check (code->getDocument().getAllContent() == "note(\"a2\").s(\"i1\")", "laddning: den nya koden visas");
                    pattern.clearNotes (0);
                    runFor (1200);
                    check (pattern.stackSize (0, 0) == 0, "laddning: inget körs förrän texten ändras");

                    pattern.clearNotes (1);
                    auto& hand = pattern.edit (1, 0); // handritad ton på en rad koden aldrig skrivit
                    hand.on = true;
                    hand.pitch = 3;
                    code->getDocument().replaceAllContent ("note(\"c2\").s(\"i1\")");
                    runFor (1200);
                    check (pattern.stackSize (0, 0) == 1 && (processor.liveCodeOwned.load() & ToolLayer::liveCodeBit (0)) != 0, "AUTO: en giltig ändring körs av sig själv");

                    code->getDocument().replaceAllContent ("note(\"c2\".s(");
                    runFor (1200);
                    check (lastError.isNotEmpty() && pattern.stackSize (0, 0) == 1, "AUTO: ogiltig kod ger felrad och rör inget");

                    pattern.clearNotes (2); // inga gamla toner på rad 3, annars frågar sidan om överskrivning
                    code->getDocument().replaceAllContent ("note(\"c2\").s(\"i3\")"); // bara rad 3 nämns: rad 1 var ägd och töms
                    runFor (1200);
                    check (pattern.stackSize (0, 0) == 0 && (processor.liveCodeOwned.load() & ToolLayer::liveCodeBit (0)) == 0, "AUTO: en rad som tagits bort ur koden töms (stack " + juce::String (pattern.stackSize (0, 0)) + ", ägd " + juce::String (processor.liveCodeOwned.load()) + ", fel: " + lastError + ")");
                    check (pattern.note (1, 0).on.load() && pattern.note (1, 0).pitch.load() == 3, "AUTO: handritad rad rörs aldrig");
                    pattern.clearNotes (1);
                }
            }
        }

        // Skydd: lås, handritade rader och overwrite (samma regler för UI och AI).
        {
            ToolLayer tools (processor);
            auto make = [] (std::initializer_list<std::pair<const char*, juce::var>> props)
            {
                auto* o = new juce::DynamicObject();
                for (auto& p : props)
                    o->setProperty (p.first, p.second);
                return juce::var (o);
            };
            auto create = [&] (const juce::String& code, bool overwrite)
            {
                return tools.call ("live_code_create", make ({ { "code", code }, { "cycles", 1 }, { "overwrite", overwrite } }));
            };
            tools.call ("live_code_lock", make ({ { "layer", 0 }, { "locked", false } }));
            tools.call ("live_code_lock", make ({ { "layer", "kick" }, { "locked", false } }));
            processor.liveCodeOwned = 0;
            pattern.clearNotes (0);

            auto& handNote = pattern.edit (0, 0); // en handritad ton
            handNote.on = true;
            handNote.pitch = 5;
            r = create ("note(\"c2\").s(\"i1\")", false);
            check (! (bool) r["ok"] && r["conflicts"].getArray() != nullptr && r["conflicts"].getArray()->size() == 1, "handritad rad: fel med conflicts-lista utan overwrite");
            check (pattern.note (0, 0).pitch.load() == 5, "handritad rad: ingenting ändrades vid konflikt");
            r = create ("note(\"c2\").s(\"i1\")", true);
            check ((bool) r["ok"] && pattern.note (0, 0).pitch.load() == 0, "overwrite:true skriver över");
            r = create ("note(\"e2\").s(\"i1\")", false);
            check ((bool) r["ok"] && pattern.note (0, 0).pitch.load() == 4, "rad som Live Code skrev själv skrivs över utan fråga");

            tools.call ("live_code_lock", make ({ { "layer", 0 } }));
            r = create ("note(\"g2\").s(\"i1\")", false);
            check ((bool) r["ok"] && pattern.note (0, 0).pitch.load() == 4, "låst rad hoppas över");
            check (processor.liveCodeLocks.load() == ToolLayer::liveCodeBit (0), "låsbiten är satt");
            tools.call ("live_code_lock", make ({ { "layer", 0 }, { "locked", false } }));
            check (! (bool) tools.call ("live_code_lock", make ({ { "layer", 11 } }))["ok"], "ogiltig rad i live_code_lock ger fel");

            // live_code_run lägger koden i LIVE-editorn; live_code_stop tömmer bara rader Live Code skrev och som inte är låsta
            r = tools.call ("live_code_run", make ({ { "code", "note(\"g2\").s(\"i1\")" }, { "cycles", 1 } }));
            check ((bool) r["ok"] && processor.getLiveCodeText().contains ("g2") && pattern.note (0, 0).pitch.load() == 7, "live_code_run skriver noter och sätter LIVE-texten");
            tools.call ("live_code_lock", make ({ { "layer", 0 } }));
            r = tools.call ("live_code_stop", make ({}));
            check ((bool) r["ok"] && pattern.note (0, 0).pitch.load() == 7, "live_code_stop låter låst rad vara");
            tools.call ("live_code_lock", make ({ { "layer", 0 }, { "locked", false } }));
            r = tools.call ("live_code_stop", make ({}));
            int stopCount = 0; pattern.forEachNote (0, [&stopCount] (int, const Pattern::Note&) { ++stopCount; });
            check ((bool) r["ok"] && stopCount == 0 && (processor.liveCodeOwned.load() & ToolLayer::liveCodeBit (0)) == 0, "live_code_stop tömmer Live Codes rad och släpper ägandet");

            // live_code_apply med prune:true (AUTO): ägda rader som inte nämns töms, handgjorda rörs aldrig
            tools.call ("live_code_run", make ({ { "code", "note(\"c2\").s(\"i1\")" }, { "cycles", 1 } }));
            pattern.clearNotes (1);
            pattern.edit (1, 0).on = true;
            r = tools.call ("live_code_apply", make ({ { "items", juce::var (juce::Array<juce::var>()) }, { "overwrite", false }, { "prune", true } }));
            int pruneCount = 0; pattern.forEachNote (0, [&pruneCount] (int, const Pattern::Note&) { ++pruneCount; });
            check ((bool) r["ok"] && pruneCount == 0 && pattern.note (1, 0).on.load(), "live_code_apply prune:true tömmer ägd rad, handgjord rad ligger kvar");
            pattern.clearNotes (1);

            auto described = false;
            for (auto& t : ToolLayer::tools())
                if (t.name == "live_code_create")
                    described = t.description.contains ("overwrite") && t.description.contains ("live_code_lock") && t.description.contains ("$: ")
                                && t.description.contains (".lfo(") && t.description.contains (".env(") && t.description.contains ("data.problems");
            check (described, "live_code_create beskriver skydd, block, sliders och .lfo/.env-problem");

            // G2: console.log/warn/error i koden hamnar i konsolkön (host.log), inte i tomma intet
            {
                LiveCode consoleLive;
                juce::Result consoleResult = juce::Result::ok();
                consoleLive.create ("console.log(\"hej\", 3);\nconsole.warn({a:1});\nconsole.error(\"fel\");\n\nnote(\"c2\").s(\"i1\")", 1, consoleResult);
                const auto lines = consoleLive.takeConsole();
                check (consoleResult.wasOk() && lines.contains ("log: hej 3") && lines.contains ("warn: {\"a\":1}") && lines.contains ("error: fel"), // startloggar från bunten städas i G3
                       "console.log/warn/error går till konsolkön " + consoleResult.getErrorMessage() + " [" + lines.joinIntoString ("|") + "]");
                check (consoleLive.takeConsole().isEmpty(), "takeConsole tömmer kön");

                // G3: noterna får teckenpositioner i koden (note(" börjar på 5, c2 står på 6..8, e2 på 9..11)
                LiveCode marksLive;
                marksLive.create ("note(\"c2 e2\").s(\"i1\")", 1, consoleResult);
                const auto& marks = marksLive.getMarks();
                check (consoleResult.wasOk() && marks.size() == 2 && marks[0].layer == 0 && marks[0].beginTick == 0 && marks[1].beginTick == 192
                           && ! marks[0].locs.empty() && std::find (marks[0].locs.begin(), marks[0].locs.end(), std::make_pair (6, 8)) != marks[0].locs.end() && ! marks[1].locs.empty() && std::find (marks[1].locs.begin(), marks[1].locs.end(), std::make_pair (9, 11)) != marks[1].locs.end(),
                       "notmarkeringar ger dokumentpositioner " + (consoleResult.failed() ? consoleResult.getErrorMessage() : juce::String ((int) marks.size()) + (marks.empty() ? juce::String() : " " + juce::String (marks[0].locs.size()) + (marks[0].locs.empty() ? juce::String() : " " + juce::String (marks[0].locs[0].first) + "-" + juce::String (marks[0].locs[0].second)))));
            }
        }

        // .lfo/.env skapar CODE-rutter; felaktiga modulationsmål rapporteras utan att stoppa notraderna.
        {
            auto lfo = run ("note(\"c2\").lfo(\"bass1Cutoff\",2,0.5,\"tri\").s(\"i1\")");
            const auto lfoSlot = processor.codeMods.empty() ? -1 : processor.codeMods.front().slot;
            const auto lfoSource = Modulation::firstCodeSource + lfoSlot;
            const auto lfoRoute = lfoSlot >= 0 ? processor.modRoutes.find (lfoSource, processor.modulator.indexOf ("bass1Cutoff")) : -1;
            check ((bool) lfo["ok"] && lfo["data"]["problems"].isVoid() && lfoSlot >= 0
                       && processor.modulator.codeSlots[(size_t) juce::jmax (0, lfoSlot)].kind.load() == 1 && lfoRoute >= 0
                       && Modulation::sourceName (lfoSource) == "CODE 1" && Theme::sourceColour (lfoSource).getAlpha() == 255,
                   ".lfo skapar en namngiven, färgsatt CODE-källa och route");

            auto env = run ("note(\"c2\").env(\"bass1Cutoff\",10,200,0.5).s(\"i1\")");
            const auto envFound = std::find_if (processor.codeMods.begin(), processor.codeMods.end(), [] (const PluginBase::CodeMod& mod) { return mod.key.endsWith ("|env"); });
            const auto envSlot = envFound == processor.codeMods.end() ? -1 : envFound->slot;
            const auto envRoute = envSlot >= 0 ? processor.modRoutes.find (Modulation::firstCodeSource + envSlot, processor.modulator.indexOf ("bass1Cutoff")) : -1;
            check ((bool) env["ok"] && env["data"]["problems"].isVoid() && envSlot >= 0
                       && processor.modulator.codeSlots[(size_t) juce::jmax (0, envSlot)].kind.load() == 2 && envRoute >= 0,
                   ".env skapar en AD-källa för instrumentraden");

            auto unknown = run ("note(\"c2\").lfo(\"notAParameter\",2,0.5).s(\"i1\")");
            auto hasProblem = [] (const juce::var& result, const juce::String& text)
            {
                if (auto* problems = result["data"]["problems"].getArray())
                    for (const auto& problem : *problems)
                        if (problem.toString().contains (text))
                            return true;
                return false;
            };
            check ((bool) unknown["ok"] && hasProblem (unknown, "unknown parameter id"),
                   ".lfo med okänt parameter-id returnerar problems");
            auto nonModulatable = run ("note(\"c2\").lfo(\"instrumentCount\",2,0.5).s(\"i1\")");
            check ((bool) nonModulatable["ok"] && hasProblem (nonModulatable, "can't be modulated"),
                   ".lfo på icke-modulerbar parameter returnerar problems");
            auto kickEnv = run ("s(\"bd\").env(\"kickTune\",10,200,0.5)");
            check ((bool) kickEnv["ok"] && hasProblem (kickEnv, "not supported"),
                   ".env på kick returnerar problems i stället för route");
        }

        // .lfo/.env påverkar parametern, utfasade rutter sopas av PluginBase, och kodens tak på rutter gäller.
        {
            auto codeRoutes = [&]
            {
                auto n = 0;
                for (auto& route : processor.modRoutes.routes)
                    n += Modulation::isCodeSource (route.source.load()) ? 1 : 0;
                return n;
            };
            auto pump = [&] (int n) { for (int i = 0; i < n; ++i) processor.modulator.update (0.0); };
            const auto cutoff = processor.modulator.indexOf ("bass1Cutoff");
            auto* parameter = processor.apvts.getParameter ("bass1Cutoff");
            const auto base = parameter->convertFrom0to1 (0.5f);
            auto spread = [&] (float& lowest, float& highest)
            {
                lowest = 2.0f;
                highest = -1.0f;
                for (int i = 0; i < 4000; ++i)
                {
                    pump (1);
                    const auto v = parameter->convertTo0to1 (processor.modulator.apply (cutoff, base));
                    lowest = juce::jmin (lowest, v);
                    highest = juce::jmax (highest, v);
                }
            };
            const auto lfoCode = juce::String ("note(\"c2\").lfo(\"bass1Cutoff\",3,0.4,\"sine\").s(\"i1\")");
            const auto plainCode = juce::String ("note(\"c2\").s(\"i1\")");

            run (plainCode); // städa: stäng allt från blocken ovan, låt det tona ut och sopa
            pump (3000);
            run (plainCode);
            check (codeRoutes() == 0 && processor.codeMods.empty(), "CODE: utan .lfo/.env finns inga CODE-rutter kvar");

            float lowest = 0.0f, highest = 0.0f;
            auto withLfo = run (lfoCode);
            pump (200);
            spread (lowest, highest);
            check ((bool) withLfo["ok"] && withLfo["data"]["problems"].isVoid() && highest > 0.85f && lowest < 0.15f,
                   ".lfo svänger parametern ±djup runt grundvärdet (0,5 ± 0,4)");

            run ("note(\"c2\").lfo(\"bass1Cutoff\",3,0,\"sine\").s(\"i1\")");
            pump (200);
            spread (lowest, highest);
            check (std::abs (highest - 0.5f) < 1.0e-3f && std::abs (lowest - 0.5f) < 1.0e-3f, ".lfo med djup 0 lämnar parametern orörd");

            // 4.3: PluginBase sopar själv, ingen sida behövs. Stängd post: rutten ligger kvar tills platsen tonat ut och nästa körning sopar.
            run (lfoCode);
            run (plainCode);
            check (processor.codeMods.empty() && codeRoutes() == 1, "CODE: stängd post lämnar rutten tills platsen tonat ut");
            pump (3000);
            run (plainCode);
            check (codeRoutes() == 0, "CODE: utfasad rutt sopas bort av nästa körning (4.3)");
            run (lfoCode);
            processor.clearCodeMods (false);
            pump (3000);
            processor.sweepCodeRoutes();
            check (codeRoutes() == 0, "CODE: clearCodeMods(false) + sweepCodeRoutes tar bort rutten (det timern gör)");

            // 4.2: taket på kodens rutter (32 av 64). Sju platser hålls på av hand med fem rutter var (35), så en ny post stoppas.
            for (int s = 1; s < Modulation::numCodeSources; ++s)
            {
                processor.modulator.codeSlots[(size_t) s].kind = 1;
                for (int t = 0; t < 5; ++t)
                    processor.modRoutes.add (Modulation::firstCodeSource + s, t, 0.1f);
            }
            pump (10);
            const auto before = codeRoutes();
            auto capped = run (lfoCode);
            auto sawCap = false;
            if (auto* list = capped["data"]["problems"].getArray())
                for (const auto& p : *list)
                    sawCap = sawCap || p.toString().contains ("at most " + juce::String (Modulation::maxCodeRoutes));
            check ((bool) capped["ok"] && sawCap && codeRoutes() == before, "CODE: över taket på 32 rutter ger felrad och ingen ny rutt");
            for (int s = 1; s < Modulation::numCodeSources; ++s)
                processor.modulator.codeSlots[(size_t) s].kind = 0;
            processor.clearCodeMods (true);
            check (codeRoutes() == 0, "CODE: städat efter taktestet");
        }

        // Lucka 4-6 (lc-e-design §3): gränser, platsåteranvändning och stopp/laddning.
        {
            auto codeRoutes = [&]
            {
                auto n = 0;
                for (auto& route : processor.modRoutes.routes)
                    n += Modulation::isCodeSource (route.source.load()) ? 1 : 0;
                return n;
            };
            auto pump = [&] (int n) { for (int i = 0; i < n; ++i) processor.modulator.update (0.0); };
            auto hasProblem = [] (const juce::var& result, const juce::String& text)
            {
                if (auto* problems = result["data"]["problems"].getArray())
                    for (const auto& problem : *problems)
                        if (problem.toString().contains (text))
                            return true;
                return false;
            };
            const auto plainCode = juce::String ("note(\"c2\").s(\"i1\")");
            auto clean = [&]
            {
                processor.clearCodeMods (true);
                pump (10);
                run (plainCode);
            };
            clean();

            // Lucka 4a: åtta unika (lager, parameter, sort) ryms, den nionde får felrad.
            const char* ids[] = { "bass1Cutoff", "bass1Resonance", "bass1EnvAmount", "bass1Decay", "bass1Sustain", "bass1Release", "bass1Drive", "bass1Glide" };
            juce::String eight = "note(\"c2\")";
            for (auto* id : ids)
                eight += ".lfo(\"" + juce::String (id) + "\",1,0.2)";
            eight += ".s(\"i1\")";
            auto full = run (eight);
            check ((bool) full["ok"] && full["data"]["problems"].isVoid() && (int) processor.codeMods.size() == Modulation::numCodeSources,
                   "CODE: åtta unika modulatorer ryms");
            auto ninth = run (eight.replace (".s(\"i1\")", ".env(\"bass1Attack\",10,200,0.2).s(\"i1\")"));
            check ((bool) ninth["ok"] && hasProblem (ninth, "All " + juce::String (Modulation::numCodeSources) + " CODE modulators are in use")
                       && (int) processor.codeMods.size() == Modulation::numCodeSources,
                   "CODE: den nionde modulatorn ger felrad och ingen ny plats");
            clean();

            // Lucka 4b: djup, rate, attack och decay klipps till sina gränser.
            run ("note(\"c2\").lfo(\"bass1Cutoff\",1000,5,\"sine\").env(\"bass1Resonance\",99999,0,5).s(\"i1\")");
            auto slotOf = [&] (const juce::String& suffix)
            {
                const auto found = std::find_if (processor.codeMods.begin(), processor.codeMods.end(), [&] (const PluginBase::CodeMod& mod) { return mod.key.endsWith (suffix); });
                return found == processor.codeMods.end() ? -1 : found->slot;
            };
            const auto lfoSlot = slotOf ("|lfo");
            const auto envSlot = slotOf ("|env");
            const auto lfoRoute = lfoSlot >= 0 ? processor.modRoutes.find (Modulation::firstCodeSource + lfoSlot, processor.modulator.indexOf ("bass1Cutoff")) : -1;
            check (lfoSlot >= 0 && envSlot >= 0 && lfoRoute >= 0
                       && processor.modRoutes.routes[(size_t) lfoRoute].amount.load() == 1.0f
                       && processor.modulator.codeSlots[(size_t) lfoSlot].rate.load() == 200.0f
                       && processor.modulator.codeSlots[(size_t) envSlot].attackMs.load() == 10000.0f
                       && processor.modulator.codeSlots[(size_t) envSlot].decayMs.load() == 1.0f,
                   "CODE: djup 5 klipps till 1, rate 1000 till 200, attack 99999 till 10000, decay 0 till 1");
            clean();

            // Lucka 4c/4d: NaN, Infinity, text och okänd form ger felrad i stället för krasch.
            for (auto* bad : { "lfo(\"bass1Cutoff\",NaN,0.5)", "lfo(\"bass1Cutoff\",Infinity,0.5)", "lfo(\"bass1Cutoff\",\"fort\",0.5)", "env(\"bass1Cutoff\",10,200,NaN)" })
            {
                auto result = run ("note(\"c2\")." + juce::String (bad) + ".s(\"i1\")");
                check (! (bool) result["ok"] || result["data"]["problems"].isVoid() == false,
                       "CODE: ogiltigt tal i .lfo/.env stoppar inte host (" + juce::String (bad) + ")");
            }
            auto badShape = run ("note(\"c2\").lfo(\"bass1Cutoff\",2,0.5,\"zigzag\").s(\"i1\")");
            check (! (bool) badShape["ok"] || badShape["data"]["problems"].isVoid() == false, "CODE: okänd LFO-form ger felrad");
            clean();

            // Lucka 5: en nyss stängd plats återanvänds inte före utfasning, men gör det efter svepning.
            const auto lfoA = juce::String ("note(\"c2\").lfo(\"bass1Cutoff\",3,0.4).s(\"i1\")");
            run (lfoA);
            pump (1); // idle sätts bara i Modulator::update; utan ljudtråd måste testet köra ett steg så platsen räknas som levande
            const auto firstSlot = processor.codeMods.empty() ? -1 : processor.codeMods.front().slot;
            run (plainCode);
            run ("note(\"c2\").lfo(\"bass1Resonance\",3,0.4).s(\"i1\")");
            const auto secondSlot = processor.codeMods.empty() ? -1 : processor.codeMods.front().slot;
            check (firstSlot >= 0 && secondSlot >= 0 && secondSlot != firstSlot, "CODE: ny post får inte den stängda platsen före utfasning");
            run (plainCode);
            pump (3000);
            processor.sweepCodeRoutes();
            check (codeRoutes() == 0, "CODE: utfasade platser sopas efter pumpning");
            clean();

            // Lucka 6: stopp och laddning lämnar inga poster eller CODE-rutter.
            auto settled = [&]
            {
                pump (3000);
                processor.sweepCodeRoutes();
                return processor.codeMods.empty() && codeRoutes() == 0;
            };
            run (lfoA);
            processor.clearCodeMods (false);
            check (settled(), "CODE: clearCodeMods(false) lämnar inga poster eller rutter efter svepning");
            run (lfoA);
            juce::MemoryBlock state;
            processor.getStateInformation (state);
            run (lfoA);
            processor.setStateInformation (state.getData(), (int) state.getSize());
            check (settled(), "CODE: setStateInformation lämnar inga poster eller rutter");
            run (lfoA);
            processor.loadFactoryPreset (Presets::Section::full, 0);
            check (settled(), "CODE: fabrikspreset lämnar inga poster eller rutter");
            clean();
        }

        // slider(v,min,max): utan dragning gäller värdet i texten, med dragning värdet från reglaget (begränsat till min..max).
        {
            auto noteOf = [] (const std::map<int, double>& values)
            {
                LiveCode live;
                live.setSliderValues (values);
                juce::Result res = juce::Result::ok();
                if (live.setCode ("note(slider(3,0,12)).s(\"i1\")").failed()) return -999.0;
                const auto haps = live.query (0, 1, res);
                return haps.empty() ? -999.0 : (double) haps[0].value["note"];
            };
            check (noteOf ({}) == 3.0, "slider(): startvärdet gäller utan dragning");
            check (noteOf ({ { 0, 7.0 } }) == 7.0, "slider(): dragna värdet gäller");
            check (noteOf ({ { 0, 99.0 } }) == 12.0, "slider(): värdet begränsas till max");
            check (noteOf ({ { 1, 7.0 } }) == 3.0, "slider(): värde för ett annat ordningsnummer rör inte detta reglage");
        }

        // F2: .lock() / .pianoroll.lock() noteras som lås-poster (inte som parameterskrivningar); okänd grupp ger felrad.
        {
            LiveCode live;
            live.setParams ({ { "bass1Cutoff", 0.5 }, { "bass2Cutoff", 0.5 }, { "kickTune", 0.5 }, { "fx1Mix", 0.5 } });
            const auto res = live.setCode ("instr1.lock();\ninstr2.pianoroll.lock();\nkick.lock();\n\nnote(\"c2\").s(\"i1\")");
            auto writes = live.takeParamWrites();
            juce::StringArray seen;
            if (auto* list = writes.getArray())
                for (auto& w : *list)
                    seen.add (w["lock"].toString() + ":" + w["kind"].toString() + ":" + w["id"].toString());
            check (res.wasOk() && seen.size() == 3 && seen.contains ("bass1:all:") && seen.contains ("bass2:notes:") && seen.contains ("kick:all:"),
                   ".lock() och .pianoroll.lock() ger lås-poster utan id");

            LiveCode other;
            other.setParams ({ { "bass1Cutoff", 0.5 }, { "fx1Mix", 0.5 } });
            check (other.setCode ("fx1.lock();\n\nnote(\"c2\").s(\"i1\")").failed(), ".lock() på grupp utan låsbit (fx1) ger felrad");
        }

        // F3: fx1.slot2.setEffect("reverb") och fx1.slot2.mix.set(v) noteras som effektposter; samma typ ger ingen post; fel ger felrad.
        {
            ToolLayer tools (processor);
            const auto catalog = tools.liveCodeEffects();
            auto record = [&catalog] (const juce::String& code, juce::StringArray& seen)
            {
                LiveCode live;
                live.setEffects (catalog);
                const auto res = live.setCode (code);
                const auto writes = live.takeParamWrites();
                if (auto* list = writes.getArray())
                    for (auto& w : *list)
                        seen.add (w["fx"].toString() + "." + w["slot"].toString() + ":" + w["type"].toString() + ":" + w["knob"].toString() + ":" + w["value"].toString());
                return res;
            };
            juce::StringArray seen;
            const auto res = record ("fx1.slot2.setEffect(\"reverb\");\nfx1.slot2.mix.set(0.3);\n\nnote(\"c2\").s(\"i1\")", seen);
            check (res.wasOk() && seen.size() == 2 && seen[0] == "fx1.2:Reverb::0.0" && seen[1] == "fx1.2:Reverb:MIX:0.3",
                   "setEffect + ratt ger två effektposter i ordning: " + seen.joinIntoString (" ; ") + " " + res.getErrorMessage());

            juce::StringArray same;
            const auto current = catalog["slots"][1][0].toString().toLowerCase().removeCharacters (" ");
            record ("fx2.slot1.setEffect(\"" + current + "\");\n\nnote(\"c2\").s(\"i1\")", same);
            check (same.isEmpty(), "setEffect med nuvarande typ ger ingen post");

            juce::StringArray none;
            check (record ("fx1.slot1.setEffect(\"finnsinte\");\n\nnote(\"c2\").s(\"i1\")", none).failed(), "okänd effekt ger felrad");
            check (record ("fx1.slot1.setEffect(\"off\");\nfx1.slot1.mix.set(1);\n\nnote(\"c2\").s(\"i1\")", none).failed(), "ratt på tom plats ger felrad");
        }

        // F4: meter.X.get() ger värdet från före körningen; .set/.lfo/.lock på en mätare ger felrad.
        {
            LiveCode live;
            live.setParams ({ { "bass1Cutoff", 0.5 }, { "meter.outputPeak", -42.5 }, { "meter.duck1", 0.25 } });
            const auto res = live.setCode ("bass1.Cutoff.set(meter.outputPeak.get() + meter.duck1.get());\n\nnote(\"c2\").s(\"i1\")");
            const auto writes = live.takeParamWrites();
            check (res.wasOk() && writes.size() == 1 && (double) writes[0]["value"] == -42.25, "mätare läses med .get(): " + res.getErrorMessage());
            for (auto* bad : { "set(1)", "lfo(1)", "LFO(1)", "lock()" })
            {
                LiveCode other;
                other.setParams ({ { "bass1Cutoff", 0.5 }, { "meter.solo", 0.0 } });
                check (other.setCode (juce::String ("meter.solo.") + bad + ";\n\nnote(\"c2\").s(\"i1\")").failed(), juce::String ("meter.solo.") + bad + " ger felrad");
            }
        }

        // F2: lås från koden blockerar skrivning som handlås, men sparas aldrig och släpps utan att röra handlåset.
        {
            ToolLayer tools (processor);
            auto make = [] (std::initializer_list<std::pair<const char*, juce::var>> props)
            {
                auto* o = new juce::DynamicObject();
                for (auto& p : props)
                    o->setProperty (p.first, p.second);
                return juce::var (o);
            };
            auto create = [&] (const juce::String& code)
            {
                return tools.call ("live_code_create", make ({ { "code", code }, { "cycles", 1 }, { "overwrite", true } }));
            };
            tools.call ("live_code_lock", make ({ { "layer", 0 }, { "locked", false } }));
            processor.liveCodeOwned = 0;
            pattern.clearNotes (0);
            create ("note(\"c2\").s(\"i1\")");
            const auto before = pattern.note (0, 0).pitch.load();
            processor.liveCodeCodeLocks = ToolLayer::liveCodeBit (0);
            create ("note(\"g2\").s(\"i1\")");
            check (pattern.note (0, 0).pitch.load() == before, "kodlås: raden hoppas över");
            processor.liveCodeCodeLocks = 0;
            processor.liveCodeCodeNoteLocks = ToolLayer::liveCodeBit (0);
            create ("note(\"g2\").s(\"i1\")");
            check (pattern.note (0, 0).pitch.load() == before, "kodlås (pianoroll): noter hoppas över");
            processor.liveCodeCodeNoteLocks = 0;
            tools.call ("live_code_lock", make ({ { "layer", 0 } }));
            processor.liveCodeCodeLocks = ToolLayer::liveCodeBit (0);
            processor.liveCodeCodeLocks = 0; // stopp släpper bara kodlåset
            check (processor.liveCodeLocks.load() == ToolLayer::liveCodeBit (0) && processor.liveCodeNoteLocks() == ToolLayer::liveCodeBit (0), "handlås ligger kvar när kodlåset släpps");
            tools.call ("live_code_lock", make ({ { "layer", 0 }, { "locked", false } }));
            create ("note(\"g2\").s(\"i1\")");
            check (pattern.note (0, 0).pitch.load() != before, "ingen lås kvar: raden skrivs igen");
        }

        int pitch = 0;
        check (LiveCode::pitchFrom ("c#3", pitch) && pitch == 13 && LiveCode::pitchFrom ("eb2", pitch) && pitch == 3
                   && LiveCode::pitchFrom (7, pitch) && pitch == 7, "tonnamn och tal blir pitch");
        return failed ? 1 : 0;
    }

    // Radens längd (R9): AUTO följer innehållet (växer och krymper), ett valt antal låser den, "+ BAR" (extra) släpps när innehållet nått dit.
    // Dialogen som frågar före förkortning (LengthMenu) testas inte här.
    int lengthTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

        auto runFor = [] (int milliseconds) { juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds); };
        auto failed = false;
        auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };
        auto& pattern = processor.pattern;
        const auto kick = Pattern::kickLayer;
        runFor (300);

        check (pattern.isLengthAuto (kick) && pattern.lengthBars (kick) == 1, "kicken är AUTO och en takt från början");

        pattern.addNote (kick, 4 * Pattern::ticksPerBar, 48, 0); // en kick i takt 5
        processor.patternChanged();
        check (pattern.lengthBars (kick) == 5, "AUTO: en ton i takt 5 gör raden fem takter");
        pattern.clearNotes (kick);
        pattern.addNote (kick, 0, 48, 0);
        processor.patternChanged();
        check (pattern.lengthBars (kick) == 1, "AUTO: när tonen tas bort krymper raden igen");

        pattern.setManualBars (kick, 3);
        processor.patternChanged();
        check (! pattern.isLengthAuto (kick) && pattern.lengthBars (kick) == 3, "valt antal: tre takter fast innehållet är en takt");
        pattern.setManualBars (kick, 0);
        processor.patternChanged();
        check (pattern.lengthBars (kick) == 1, "tillbaka till AUTO: en takt");

        pattern.clearNotes (kick);
        pattern.addNote (kick, 0, 3 * Pattern::ticksPerBar, 48, 0); // en ton över tre takter
        processor.patternChanged();
        check (pattern.lengthBars (kick) == 3, "AUTO: en lång ton som täcker tre takter ger tre takter");
        pattern.trimRow (kick, 2);
        pattern.setManualBars (kick, 2);
        check (pattern.note (kick, 0).length.load() == 2 * Pattern::ticksPerBar && pattern.lengthBars (kick) == 2, "förkortning: en ton som startar i takt 1 kortas vid slutet");
        check (pattern.acceptsTick (kick, 2 * Pattern::ticksPerBar - 1) && ! pattern.acceptsTick (kick, 2 * Pattern::ticksPerBar), "låst rad: ingen ton efter slutet");
        pattern.clearNotes (kick);
        pattern.addNote (kick, 0, 48, 48, 0);
        pattern.setManualBars (kick, 0);
        processor.patternChanged();
        check (pattern.acceptsTick (kick, 5 * Pattern::ticksPerBar), "AUTO: raden växer, tonen efter slutet tas emot");

        pattern.setExtraBars (0, 4);
        processor.patternChanged();
        check (pattern.lengthBars (0) >= 4, "+ BAR: raden är minst fyra takter");
        pattern.addNote (0, 3 * Pattern::ticksPerBar, 48, 0);
        processor.patternChanged();
        check (pattern.lengthBars (0) == 4 && pattern.extraBars (0) == 0, "innehållet nådde fram: extra släpptes, längden följer innehållet (längd " + juce::String (pattern.lengthBars (0)) + ", extra " + juce::String (pattern.extraBars (0)) + ", innehåll " + juce::String (pattern.contentBars (0)) + ")");
        pattern.clearNotes (0);
        processor.patternChanged();
        check (pattern.lengthBars (0) == 1, "AUTO: raden krymper när tonerna tas bort");

        // MCP: set_pattern length/trim/repeat_to och felet för låst rad; get_state visar bars och läge.
        {
            ToolLayer tools (processor);
            auto call = [&tools] (const char* json) { return tools.call ("set_pattern", juce::JSON::parse (json)); };
            call (R"({"layer":"kick","clear":true})");
            check ((bool) call (R"({"layer":"kick","notes":[{"tick":0,"length":24,"pitch":0}]})")["ok"], "MCP: en ton skrivs");
            check ((bool) call (R"({"layer":"kick","repeat_to":4})")["ok"] && pattern.lengthBars (kick) == 4, "MCP: repeat_to 4 ger fyra takter (" + juce::String (pattern.lengthBars (kick)) + ")");
            const auto lockedWithNotes = call (R"({"layer":"kick","length":2})");
            check (! (bool) lockedWithNotes["ok"] && pattern.lengthBars (kick) == 4, "MCP: kortare längd än tonerna ger fel och ändrar inget: " + lockedWithNotes["error"].toString());
            check ((bool) call (R"({"layer":"kick","length":2,"trim":true})")["ok"] && pattern.lengthBars (kick) == 2 && ! pattern.isLengthAuto (kick), "MCP: trim:true kortar och låser");
            const auto beyond = call (R"({"layer":"kick","notes":[{"tick":900,"length":24,"pitch":0}]})");
            check (! (bool) beyond["ok"] && beyond["error"].toString().contains ("row is locked to 2 bars"), "MCP: ton efter slutet i låst rad: " + beyond["error"].toString());
            check ((bool) call (R"({"layer":"kick","length":"auto"})")["ok"] && pattern.isLengthAuto (kick), "MCP: length auto");
            check (! (bool) call (R"({"layer":"kick","length":"x"})")["ok"], "MCP: ogiltig längd ger fel");
            const auto state = tools.call ("get_state", {});
            const auto* layers = state["data"]["pattern"]["layers (instruments 0-9 and 'kick')"].getArray();
            check (layers != nullptr && (int) (*layers)[0]["bars"] >= 1 && (*layers)[0]["length_mode"].toString() == "auto" && (int) state["data"]["pattern"]["bars"] < 1024, "MCP: get_state visar bars och length_mode");
        }

        editor.reset();
        std::puts (failed ? "FEL: längdtestet misslyckades" : "Längdtestet gick igenom");
        return failed ? 1 : 0;
    }

    // En pianorulle på fyra takter: raden är fyra takter (AUTO) och rullen spelar hela sin längd.
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

        // Bara instrument 1 ska höras: pianorulle med en ton i varje takt 1-4.
        setChoice (ParamID::kickOn, 0);
        for (int layer = 1; layer < 3; ++layer)
            setChoice (ParamID::bass (layer, InstrumentField::on), 0);
        processor.pattern.clearNotes (0);
        for (int bar = 0; bar < 4; ++bar)
            processor.pattern.addNote (0, bar * Pattern::ticksPerBar, 96, 0);
        processor.pattern.setCustom (0, true);
        processor.patternChanged();
        runFor (600);
        check (processor.pattern.isLengthAuto (0) && processor.pattern.lengthBars (0) == 4, "AUTO: rullen är fyra takter (" + juce::String (processor.pattern.lengthBars (0)) + ")");

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

    // Init-check: mixanalysen (inkl. fas/stereo-måtten) på både koden-Init och det riktiga inbakade Init ska ge ett giltigt svar utan krasch,
    // text och JSON ska gå att skriva, och fas-måtten ska tåla tom insignal. Rendersig (rendersig/rendersigreal) står för "0 diff".
    int initTest()
    {
        juce::ScopedJuceInitialiser_GUI gui;
        auto failed = false;
        auto check = [&] (bool ok, const juce::String& what)
        {
            std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8());
            failed = failed || ! ok;
        };

        // Tom insignal: ingen krasch, ingen giltig mätning, inga fynd.
        {
            MixAnalysis::PhaseInput empty;
            const auto metrics = MixAnalysis::measurePhase (empty);
            const auto description = MixAnalysis::describePhase (metrics);
            check (! metrics.valid, "fasmåtten på tom insignal är inte giltiga");
            check (description.findings.isEmpty() && description.fixes.isEmpty(), "tom insignal ger inga fynd eller förslag");
        }

        const auto codeInit = PluginBase::preferCodeInit;
        for (const auto useCodeInit : { true, false })
        {
            PluginBase::preferCodeInit = useCodeInit;
            const juce::String label = useCodeInit ? "Init (kod)" : "Init (inbakad)";
            const auto owner = std::make_unique<InstrumentProcessor>();
            auto& processor = *owner;
            processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
            processor.prepareToPlay (renderRate, blockSize);
            processor.transportStatus.bpm = 145.0;
            processor.transportStatus.keyNote = 52;

            auto copy = MixAnalysis::prepareCopy (processor);
            const auto report = MixAnalysis::run (*copy, 145.0, 52, {}, nullptr, {}, {});
            check (report.valid, label + ": analysen ger en giltig rapport");
            check (report.toText().isNotEmpty(), label + ": rapporttexten går att skriva");
            check (juce::JSON::toString (report.toVar()).isNotEmpty(), label + ": rapporten går att skriva som JSON");
            auto finite = true;
            for (const auto& finding : report.findings)
                finite = finite && std::isfinite (finding.lowHz) && std::isfinite (finding.highHz) && std::isfinite (finding.share);
            check (finite, label + ": alla fynd har ändliga värden");
        }
        PluginBase::preferCodeInit = codeInit;

        std::puts (failed ? "FEL: inittest" : "Inittestet gick igenom");
        return failed ? 1 : 0;
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

        const auto instrumentFlat = rmsOf (PluginBase::ExportPart::bass);
        setValue ("bass1EqHp", 500.0f);
        setValue ("bass2EqHp", 500.0f);
        setValue ("bass3EqHp", 500.0f);
        const auto instrumentHighPassed = rmsOf (PluginBase::ExportPart::bass);
        check (instrumentHighPassed < 0.7f * instrumentFlat, "högpass på instrumenten minskar dem (" + juce::String (instrumentFlat, 3) + " -> " + juce::String (instrumentHighPassed, 3) + ")");
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
            setValue (ParamID::bass (1, InstrumentField::fxChannel), 1.0f);
            setValue (ParamID::bass (1, InstrumentField::fxAmount), 1.0f);
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

            // INSERT (standard): allt går genom kanalen, så FX1-faderna i botten tystar instrumentet. SEND: torrt orört, bara effektdelen tystnar.
            // Instrument 1 och 3 tystas på riktigt (botten på faderna), annars hörs de torra och döljer skillnaden.
            for (const auto source : { 0, 2 })
                setValue (mixerGain (source), Mixer::minGainDb);
            const auto onlySecond = rmsOf (PluginBase::ExportPart::bass);
            Effects::setSlotType (processor.apvts, 0, 0, EffectType::ducker);
            setValue (Mixer::paramID (Mixer::firstFxSource, "Gain"), Mixer::minGainDb);
            const auto insertMuted = rmsOf (PluginBase::ExportPart::bass);
            setValue (ParamID::bass (1, InstrumentField::fxSend), 1.0f);
            const auto sendMuted = rmsOf (PluginBase::ExportPart::bass);
            check (insertMuted < 0.1f * onlySecond && sendMuted > 0.5f * onlySecond,
                   "FX SEND-läget lämnar torrt orört när FX-faderna är nere (INSERT " + juce::String (insertMuted, 3) + ", SEND " + juce::String (sendMuted, 3) + ", före " + juce::String (onlySecond, 3) + ")");
            setValue (ParamID::bass (1, InstrumentField::fxSend), 0.0f);
            setValue (Mixer::paramID (Mixer::firstFxSource, "Gain"), 0.0f);
            for (const auto source : { 0, 2 })
                setValue (mixerGain (source), 0.0f);
            Effects::setSlotType (processor.apvts, 0, 0, EffectType::off);

            setValue (ParamID::bass (1, InstrumentField::fxChannel), 0.0f);
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
                // Mätläge: okänt värde avvisas, dry kör på kopian och lämnar originalets duckning orörd.
                check (! (bool) tools.call ("analyse_mix", juce::JSON::parse ("{\"tap\":\"bogus\"}"))["ok"], "analyse_mix: okänt tap avvisas");
                const auto duckBefore = processor.apvts.getRawParameterValue ("bass1Duck")->load();
                const auto dry = tools.call ("analyse_mix", juce::JSON::parse ("{\"tap\":\"dry\"}"));
                check ((bool) dry["ok"] && dry["data"]["status"].toString() == "started", "analyse_mix tap=dry startar");
                auto dryAnswer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
                for (int i = 0; i < 1800 && dryAnswer["data"]["status"].toString() != "done"; ++i)
                {
                    juce::Thread::sleep (100);
                    dryAnswer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
                }
                check (dryAnswer["data"]["status"].toString() == "done", "dry-analysen blir klar");
                check (processor.apvts.getRawParameterValue ("bass1Duck")->load() == duckBefore, "dry-analysen rör inte originalets duckning");

                // Efter mastern: bidraget per källa, och originalets nivåer rörs inte.
                const auto levelBefore = processor.apvts.getRawParameterValue ("kickLevel")->load();
                const auto afterMasterStart = tools.call ("analyse_mix", juce::JSON::parse ("{\"tap\":\"after_master\"}"));
                check ((bool) afterMasterStart["ok"], "analyse_mix tap=after_master startar");
                auto afterMasterAnswer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
                for (int i = 0; i < 1800 && afterMasterAnswer["data"]["status"].toString() != "done"; ++i)
                {
                    juce::Thread::sleep (100);
                    afterMasterAnswer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
                }
                check (afterMasterAnswer["data"]["status"].toString() == "done", "after_master-analysen blir klar");
                if (auto* shares = afterMasterAnswer["data"]["after_master"].getArray())
                {
                    check (shares->size() >= 2, "after_master: en rad per källa (kick och instrument)");
                    const auto& kickShare = (*shares)[0];
                    check (kickShare["after_master_rms_db"].isDouble() && (double) kickShare["after_master_rms_db"] > -80.0 && (double) kickShare["after_master_rms_db"] < 6.0,
                           "after_master: kickens bidrag i slutmixen är hörbart och rimligt (" + kickShare["after_master_rms_db"].toString() + " dB)");
                }
                else
                    check (false, "after_master finns i rapporten");
                check (processor.apvts.getRawParameterValue ("kickLevel")->load() == levelBefore, "after_master rör inte originalets kicknivå");
            }
            answer = tools.call ("analyse_mix", juce::JSON::parse ("{}"));
            for (int i = 0; i < 600 && tools.call ("get_mix_report", juce::JSON::parse ("{}"))["data"]["status"].toString() != "done"; ++i)
                juce::Thread::sleep (100);
            answer = tools.call ("get_mix_report", juce::JSON::parse ("{}"));
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

            // Lektionsvarningar: RAND-fas på en bas (A ligger bara i basbandet) och mycket energi runt 200 Hz i mastern.
            auto hasTitle = [] (const MixAnalysis::Report& rep, const char* part)
            {
                for (const auto& finding : rep.findings)
                    if (finding.title.contains (part))
                        return true;
                return false;
            };
            capture.randomPhase[0] = 0.8f;
            check (hasTitle (MixAnalysis::evaluate (capture, options, nullptr), "random phase"), "en bas med RAND-fas ger en varning");
            capture.randomPhase[0] = 0.0f;
            check (! hasTitle (MixAnalysis::evaluate (capture, options, nullptr), "random phase"), "en bas utan RAND-fas ger ingen varning");

            const auto& edges = MixAnalysis::bandEdges();
            auto setMasterMud = [&] (float db) // lera mäts på kick och bas, så leran läggs på kicken (A är basen; ÄNDRAT TESTVILLKOR, AN steg 4)
            {
                for (int f = 0; f < 24; ++f)
                    for (int b = 0; b < MixAnalysis::numBands(); ++b)
                        if (edges[(size_t) b] >= 160.0f && edges[(size_t) b + 1] <= 250.0f)
                            capture.band (MixAnalysis::kickSource, f, b) = db;
            };
            setMasterMud (-8.0f);
            check (hasTitle (MixAnalysis::evaluate (capture, options, nullptr), "around 200 Hz"), "lika mycket energi runt 200 Hz som i basen ger en varning");
            setMasterMud (-60.0f);
            check (! hasTitle (MixAnalysis::evaluate (capture, options, nullptr), "around 200 Hz"), "lite energi runt 200 Hz ger ingen varning");
        }

        // Fasmåtten på påhittade signaler: kick och bas i motfas, i fas, och en kick som inte klingar ut (klick).
        {
            constexpr int length = 96000; // en takt vid 120 bpm och 48 kHz
            auto makeInput = [length] (float bassSign, bool decays)
            {
                MixAnalysis::PhaseInput in;
                in.rate = 48000.0;
                in.samplesPerBar = length;
                in.bars = 1;
                in.onsets = { 0, 24000, 48000, 72000 };
                std::vector<float> kickSignal ((size_t) length);
                for (int s = 0; s < length; ++s)
                {
                    const auto t = (double) (s % 24000) / 48000.0;
                    kickSignal[(size_t) s] = 0.5f * (float) (std::sin (juce::MathConstants<double>::twoPi * 50.0 * t) * (decays ? std::exp (-t / 0.03) : 1.0));
                }
                in.kickL = in.kickR = kickSignal;
                in.bassL = in.bassR = kickSignal;
                for (auto& v : in.bassL)
                    v *= bassSign;
                in.bassR = in.bassL;
                in.masterL = in.masterR = kickSignal;
                return in;
            };
            const auto anti = MixAnalysis::measurePhase (makeInput (-1.0f, true));
            const auto same = MixAnalysis::measurePhase (makeInput (1.0f, true));
            const auto clicking = MixAnalysis::measurePhase (makeInput (1.0f, false));
            check (anti.valid && anti.lowCorrelation.size() == 16 && anti.lowCorrelation[0] < -0.95f, "kick och bas i motfas ger korrelation -1");
            check (same.valid && same.lowCorrelation[0] > 0.95f, "kick och bas i fas ger korrelation +1");
            check (anti.sumVsMaxDb < -20.0f, "motfas ger en summa långt under den största toppen (" + juce::String (anti.sumVsMaxDb, 1) + " dB)");
            check (same.sumVsMaxDb > 5.0f && same.sumVsMaxDb < 7.0f, "samma signal i fas ger +6 dB (" + juce::String (same.sumVsMaxDb, 1) + " dB)");
            check (same.hits.size() == 4 && same.clickHits == 0, "en kick som klingar ut ger inget klick");
            check (clicking.clickHits >= 3, "en kick som klipps av ger klick (" + juce::String (clicking.clickHits) + " slag)");

            auto hasId = [] (const juce::Array<juce::var>& list, const char* key, const char* id)
            {
                for (auto& item : list)
                    if (item.getProperty (key, {}).toString() == id)
                        return true;
                return false;
            };
            const auto antiText = MixAnalysis::describePhase (anti);
            const auto sameText = MixAnalysis::describePhase (same);
            const auto clickText = MixAnalysis::describePhase (clicking);
            check (hasId (antiText.findings, "id", "kick_bass_cancel") && hasId (antiText.fixes, "finding", "kick_bass_cancel"),
                   "AN: motfas ger phase_findings + suggested_fixes");
            check (antiText.fixes.size() >= 2 && (bool) antiText.fixes[0].getProperty ("needs_approval", false), "AN: förslagen är kandidater som kräver godkännande");
            check (! hasId (sameText.findings, "id", "kick_bass_cancel"), "AN: kick och bas i fas ger inget fasfynd");
            check (hasId (clickText.findings, "id", "kick_click") && hasId (clickText.fixes, "finding", "kick_click"), "AN: klick ger fynd + förslag");
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
        const juce::Colour kickColour (0xffff9a3c), instrumentColour (0xff3fd2ff);
        for (int row = 0; row < 3; ++row)
        {
            // Rad 0: inget ljud (släckt), rad 1: kickens glöd, rad 2: instrumentens glöd.
            LogoPainter::Sweep kick { 0.15f + 0.3f * (float) row, row == 1 ? 0.9f : 0.0f, kickColour }, bass { 0.85f - 0.3f * (float) row, row == 2 ? 0.9f : 0.0f, instrumentColour };
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

    // Reparerar användarens presets (Full, Kick, Bass): läser varje fil och sparar den igen, så att den får alla nuvarande parametrar och
    // formatet som sparas nu. Utan `apply` bara mäts de. Varje preset kontrolleras: ljudet av filen före och efter ska ha exakt samma fingeravtryck,
    // annars skrivs originalet tillbaka.
    int repairPresets (bool apply)
    {
        juce::ScopedJuceInitialiser_GUI gui;
        auto signatureOf = [] (PluginBase& processor)
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
            return hash;
        };

        int same = 0, restored = 0, failed = 0;
        for (const auto section : { Presets::Section::full, Presets::Section::kick, Presets::Section::bass })
        {
            const auto probe = std::make_unique<InstrumentProcessor>();
            for (const auto& file : probe->getUserPresets (section))
            {
                auto load = [&] (InstrumentProcessor& target)
                {
                    target.loadFactoryPreset (Presets::Section::full, 0);
                    return target.loadUserPreset (section, file, 0);
                };
                const auto name = file.getFileNameWithoutExtension();
                const auto label = Presets::sectionName (section) + " / " + name;

                juce::MemoryBlock original;
                file.loadFileAsData (original);
                const auto before = std::make_unique<InstrumentProcessor>();
                if (! load (*before))
                {
                    std::printf ("FEL (kan inte läsas): %s" "\n", label.toRawUTF8());
                    ++failed;
                    continue;
                }
                const auto sigBefore = signatureOf (*before);
                if (! apply)
                {
                    std::printf ("%016llx %s" "\n", (unsigned long long) sigBefore, label.toRawUTF8());
                    continue;
                }

                if (! before->saveUserPreset (section, name, 0) || juce::File::createLegalFileName (name.trim()) != name.trim())
                {
                    file.replaceWithData (original.getData(), original.getSize());
                    std::printf ("FEL (kan inte sparas, originalet kvar): %s" "\n", label.toRawUTF8());
                    ++failed;
                    continue;
                }

                const auto after = std::make_unique<InstrumentProcessor>();
                if (! load (*after) || signatureOf (*after) != sigBefore)
                {
                    file.replaceWithData (original.getData(), original.getSize());
                    std::printf ("SKILJER (originalet återställt): %s" "\n", label.toRawUTF8());
                    ++restored;
                    continue;
                }
                std::printf ("ok  %6d -> %6d byte  %s" "\n", (int) original.getSize(), (int) file.getSize(), label.toRawUTF8());
                ++same;
            }
        }
        std::printf ("%d reparerade och verifierade, %d återställda, %d fel" "\n", same, restored, failed);
        return restored + failed == 0 ? 0 : 1;
    }

    // Matrix-problemet: en koppling från Matrix (ADD ROUTE, MatrixPage.cpp addRoute) ska ge samma ring och samma modulering som LFO-drag
    // (Components.cpp itemDropped) för varje modulerbart mål.
    int matrixTest()
    {
        const auto owner = std::make_unique<InstrumentProcessor>();
        auto& processor = *owner;
        processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
        processor.prepareToPlay (renderRate, blockSize);

        auto failed = false;
        auto check = [&failed] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what); failed |= ! ok; };
        const auto& parameters = processor.getParameters();

        // Matrix: den första aktiva LFO:n, annars aktiveras LFO 1 (samma urval som MatrixPage::addRoute).
        auto matrixSource = [&processor]
        {
            auto source = -1;
            for (int lfo = 0; lfo < processor.availableLfos() && source < 0; ++lfo)
                if (processor.isLfoActive (lfo))
                    source = Modulation::lfoSource (lfo);
            if (source < 0)
            {
                processor.setLfoActive (0, true);
                source = 0;
            }
            return source;
        };
        // Största förskjutningen över ett par varv; ringen och ljudet läser samma tal.
        auto strongestOffset = [&processor] (int index)
        {
            auto strongest = 0.0f;
            for (int step = 0; step < 64; ++step)
            {
                processor.modulator.update (step * 0.0625, 138.0);
                strongest = std::max (strongest, std::abs (processor.modulator.currentOffset (index)));
            }
            return strongest;
        };

        auto tested = 0, noRoute = 0, noOffset = 0, noRing = 0, differs = 0, notApplied = 0;
        juce::String firstBad;
        for (int i = 0; i < parameters.size(); ++i)
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]);
            if (ranged == nullptr || ! Modulation::isModulatable (*ranged))
                continue;
            ++tested;
            const auto what = ranged->getParameterID();
            const auto source = matrixSource();

            // Matrix-vägen
            processor.modRoutes.clear();
            const auto slot = processor.modRoutes.add (source, i, 0.25f);
            const auto found = processor.modRoutes.find (source, i) == slot && slot >= 0;
            auto ring = false; // KnobComponent::rings: någon koppling med target == parameterIndex
            for (const auto& route : processor.modRoutes.routes)
                ring |= route.source.load() >= 0 && route.target.load() == i;
            const auto viaMatrix = strongestOffset (i);
            const auto applied = processor.modulator.apply (i, ranged->convertFrom0to1 (0.5f)) != ranged->convertFrom0to1 (0.5f);

            // Drag-vägen
            processor.modRoutes.clear();
            processor.modRoutes.add (juce::jlimit (0, Modulation::numSources - 1, source), i, 0.25f);
            const auto viaDrag = strongestOffset (i);

            noRoute += found ? 0 : 1;
            noRing += ring ? 0 : 1;
            noOffset += viaMatrix > 0.0f ? 0 : 1;
            notApplied += applied || viaMatrix == 0.0f ? 0 : 1;
            differs += std::abs (viaMatrix - viaDrag) < 1.0e-6f ? 0 : 1;
            if (firstBad.isEmpty() && ! (found && ring && viaMatrix > 0.0f && std::abs (viaMatrix - viaDrag) < 1.0e-6f))
                firstBad = what;
        }
        processor.modRoutes.clear();
        std::printf ("  (%d mål; utan koppling %d, utan ring %d, utan förskjutning %d, skiljer mot drag %d, ej påverkat värde %d%s%s)\n", tested, noRoute, noRing,
                     noOffset, differs, notApplied, firstBad.isEmpty() ? "" : "; första: ", firstBad.toRawUTF8());
        check (tested > 0 && noRoute == 0 && noRing == 0 && noOffset == 0 && differs == 0 && notApplied == 0, "Matrix-koppling = drag-koppling för alla modulerbara mål");

        // POSITION på varje instrument och OSC: indexet som WavetableView läser (modulator.indexOf, OscPanel.cpp:7) är samma som ratten
        // (KnobComponent::parameterIndex = getParameters().indexOf) och som Matrix-menyn (id = index + 1), och en koppling dit ger en förskjutning.
        auto positionBad = 0;
        juce::String firstPositionBad;
        for (int layer = 0; layer < maxInstruments; ++layer)
            for (int osc = 0; osc < 3; ++osc)
            {
                const auto id = ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aPos));
                auto* parameter = processor.apvts.getParameter (id);
                const auto knobIndex = parameter != nullptr ? parameters.indexOf (parameter) : -1;
                const auto viewIndex = processor.modulator.indexOf (id);
                processor.modRoutes.clear();
                processor.modRoutes.add (matrixSource(), knobIndex, 0.25f);
                const auto ok = knobIndex >= 0 && knobIndex == viewIndex && strongestOffset (viewIndex) > 0.0f
                                && Modulation::isModulatable (*dynamic_cast<juce::RangedAudioParameter*> (parameter));
                positionBad += ok ? 0 : 1;
                if (! ok && firstPositionBad.isEmpty())
                    firstPositionBad = id;
            }
        processor.modRoutes.clear();
        check (positionBad == 0, ("POSITION: samma index för ratt, vy och Matrix på alla instrument/OSC" + (firstPositionBad.isEmpty() ? juce::String() : " (första fel: " + firstPositionBad + ")")).toRawUTF8());

        // CODE-källa (nu valbar i Matrix) mot instrument 1 CUTOFF: kopplingen skapas, hittas och ger en ring.
        {
            const auto cutoffId = ParamID::bass (0, InstrumentField::cutoff);
            const auto cutoffIndex = parameters.indexOf (processor.apvts.getParameter (cutoffId));
            processor.modRoutes.clear();
            const auto slot = processor.modRoutes.add (Modulation::firstCodeSource, cutoffIndex, 0.25f);
            check (cutoffIndex >= 0 && slot >= 0 && processor.modRoutes.find (Modulation::firstCodeSource, cutoffIndex) == slot
                       && Modulation::isCodeSource (processor.modRoutes.routes[(size_t) slot].source.load()),
                   "CODE 1 -> instrument 1 CUTOFF kan kopplas i Matrix");
            processor.modRoutes.clear();
        }

        // Källvalet: LFO 1 avstängd (ingen koppling) ger LFO 2, inte en tyst källa.
        processor.setLfoActive (0, false);
        check (matrixSource() == Modulation::lfoSource (1), "Matrix väljer första aktiva LFO när LFO 1 är borttagen");
        processor.setLfoActive (1, false);
        processor.setLfoActive (2, false);
        const auto fresh = matrixSource();
        check (fresh == 0 && processor.isLfoActive (0), "Matrix aktiverar LFO 1 när ingen LFO är aktiv");

        std::puts (failed ? "FEL: matrixtestet misslyckades" : "Matrixtestet gick igenom");
        return failed ? 1 : 0;
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

        check (Presets::instrumentLayerOf ("bass10Cutoff") == 9 && Presets::instrumentLayerOf ("bass1Cutoff") == 0 && Presets::instrumentLayerOf ("bass3On") == 2
                   && Presets::toInstrumentLayer ("bass1Cutoff", 9) == "bass10Cutoff" && Presets::toInstrumentLayer ("bass10Cutoff", 1) == "bass2Cutoff",
               "lagernumret läses ur parameter-ID:n (ett och två tecken)");
        check (processor.activeInstruments() == 3, "tre instrument från början");

        processor.setInstrumentCount (6);
        check (processor.activeInstruments() == 6, "sex instrument efter att ha lagt till tre");
        check (heardOf (processor, PluginBase::layerAudition (5)), "instrument 6 hörs");

        // Innehåll i instrument 5: ton, namn, ändrad inställning, och en LFO-koppling.
        processor.pattern.addNote (4, 0, 48, 3);
        processor.pattern.setCustom (4, true);
        processor.setLayerName (4, "Lead");
        processor.setLayerColour (4, 0xff112233);
        setValue (ParamID::bass (4, InstrumentField::cutoff), 777.0f);
        const auto& parameters = processor.getParameters();
        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == ParamID::bass (4, InstrumentField::cutoff))
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
                       && std::abs (other->apvts.getRawParameterValue (ParamID::bass (4, InstrumentField::cutoff))->load() - 777.0f) < 1.0f,
                   "sex instrument sparas och läses tillbaka med namn, toner och inställningar");
        }

        {
            // L4: färgen följer med projektet; ett projekt utan färg (äldre) ger standardfärg.
            const auto other = std::make_unique<InstrumentProcessor>();
            other->setStateInformation (state.getData(), (int) state.getSize());
            check (other->getLayerColour (4) == 0xff112233 && other->getLayerColour (3) == 0, "L4: färgen sparas och läses, orörda instrument har standardfärg");
            auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
            check (xml != nullptr && xml->hasAttribute ("layerColour5"), "L4: färgen ligger i projektets state");
            xml->removeAttribute ("layerColour5");
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            const auto legacy = std::make_unique<InstrumentProcessor>();
            legacy->setStateInformation (old.getData(), (int) old.getSize());
            check (legacy->getLayerColour (4) == 0 && legacy->getLayerName (4) == "Lead", "L4: ett projekt utan färg laddas med standardfärg");

            // Copy/paste/duplicate/move/delete flyttar innehållet mellan platserna.
            check (other->duplicateTrack (4) && other->activeInstruments() == 7 && other->getLayerName (5) == "Lead" && other->getLayerColour (5) == 0xff112233
                       && other->pattern.note (5, 0).on.load() && other->pattern.note (4, 0).on.load(), "L4: Duplicate ger en kopia på nästa plats");
            check (other->moveTrack (5, -1) && other->getLayerName (4) == "Lead" && other->moveTrack (4, 1), "L4: Move byter plats");
            check (other->deleteTrack (4) && other->activeInstruments() == 6 && other->getLayerName (4) == "Lead" && other->getLayerName (5) == "INSTR 6", "L4: Delete flyttar upp resten");
            const auto clip = other->copyTrack (4);
            check (clip != nullptr && other->pasteTrack (1, *clip) && other->getLayerName (1) == "Lead" && other->getLayerColour (1) == 0xff112233
                       && std::abs (other->apvts.getRawParameterValue (ParamID::bass (1, InstrumentField::cutoff))->load() - 777.0f) < 1.0f, "L4: Copy/Paste flyttar ljud, namn och färg");

            // Move down/up: ALLT per rad följer med (namn, färg, rattar, FX-kanal, noter), inte bara mönstret.
            {
                const auto p = std::make_unique<InstrumentProcessor>();
                p->setStateInformation (state.getData(), (int) state.getSize());
                auto set = [&p] (const juce::String& id, float real)
                {
                    auto* parameter = p->apvts.getParameter (id);
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (real));
                };
                auto get = [&p] (const juce::String& id) { return p->apvts.getRawParameterValue (id)->load(); };
                p->pattern.addNote (3, 0, 55, 2);
                p->pattern.setCustom (3, true);
                p->setLayerName (3, "Bas");
                p->setLayerColour (3, 0xff445566);
                set (ParamID::bass (3, InstrumentField::cutoff), 333.0f);
                set (ParamID::bass (3, InstrumentField::fxChannel), 2.0f);
                set (ParamID::bass (4, InstrumentField::fxChannel), 3.0f);
                const auto pitch3 = p->pattern.note (3, 0).pitch.load(), pitch4 = p->pattern.note (4, 0).pitch.load();
                const auto version = p->layerStructureVersion.load();
                check (p->moveTrack (3, 1), "Move down: går att göra");
                check (p->getLayerName (4) == "Bas" && p->getLayerName (3) == "Lead" && p->getLayerColour (4) == 0xff445566 && p->getLayerColour (3) == 0xff112233,
                       "Move down: namn och färg följer med");
                check (std::abs (get (ParamID::bass (4, InstrumentField::cutoff)) - 333.0f) < 1.0f && std::abs (get (ParamID::bass (3, InstrumentField::cutoff)) - 777.0f) < 1.0f,
                       "Move down: rattarna följer med");
                check (std::abs (get (ParamID::bass (4, InstrumentField::fxChannel)) - 2.0f) < 0.1f && std::abs (get (ParamID::bass (3, InstrumentField::fxChannel)) - 3.0f) < 0.1f,
                       "Move down: FX-kanalen följer med");
                check (p->pattern.note (4, 0).pitch.load() == pitch3 && p->pattern.note (3, 0).pitch.load() == pitch4, "Move down: noterna följer med");
                check (p->layerStructureVersion.load() != version && p->selectedInstrument == 4, "Move down: vyn byggs om och markeringen följer med");
            }
            // Move down mellan de två första raderna, med rutnätslängd och mixer.
            {
                const auto p = std::make_unique<InstrumentProcessor>();
                p->setInstrumentCount (3);
                auto set = [&p] (const juce::String& id, float real)
                {
                    auto* parameter = p->apvts.getParameter (id);
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (real));
                };
                auto get = [&p] (const juce::String& id) { return p->apvts.getRawParameterValue (id)->load(); };
                p->setLayerName (0, "Ett");
                p->setLayerName (1, "Tva");
                set (ParamID::bass (0, InstrumentField::cutoff), 333.0f);
                set (ParamID::bass (1, InstrumentField::cutoff), 777.0f);
                set (ParamID::rowBars (1), 2.0f);
                set (ParamID::rowBars (2), 4.0f);
                set (Mixer::paramID (Mixer::firstBassSource + 0, "Gain"), -6.0f);
                p->pattern.clearNotes (0);
                p->pattern.clearNotes (1);
                p->pattern.addNote (1, 0, 55, 2);
                check (p->moveTrack (0, 1), "Move down 1->2: går att göra");
                check (p->getLayerName (1) == "Ett" && p->getLayerName (0) == "Tva", "Move down 1->2: namnen byter plats");
                check (std::abs (get (ParamID::bass (1, InstrumentField::cutoff)) - 333.0f) < 1.0f && std::abs (get (ParamID::bass (0, InstrumentField::cutoff)) - 777.0f) < 1.0f,
                       "Move down 1->2: rattarna byter plats");
                check (std::abs (get (ParamID::rowBars (2)) - 2.0f) < 0.1f && std::abs (get (ParamID::rowBars (1)) - 4.0f) < 0.1f, "Move down 1->2: rutnätslängden byter plats");
                check (std::abs (get (Mixer::paramID (Mixer::firstBassSource + 1, "Gain")) + 6.0f) < 0.1f, "Move down 1->2: mixerkanalen byter plats");
                check (p->pattern.note (0, 0).on.load() && ! p->pattern.note (1, 0).on.load(), "Move down 1->2: noterna byter plats");
            }
            // VOICES-räkningen: överlappande toner, ackordets extra toner, glide återanvänder rösten, tomma rader.
            {
                const auto p = std::make_unique<InstrumentProcessor>();
                auto& pat = p->pattern;
                pat.clearNotes (2);
                check (pat.maxSimultaneous (2) == 0, "Voices: tom rad = 0");
                pat.addNote (2, 0, 48, 0);
                pat.addNote (2, 48, 48, 3);
                check (pat.maxSimultaneous (2) == 1, "Voices: toner efter varandra = 1");
                pat.addNote (2, 96, 96, 0, 0.8f, false, { 4, 7, 0 });
                check (pat.maxSimultaneous (2) == 3, "Voices: ton + ackord med två extra = 3");
                pat.addNote (2, 120, 24, 5);
                check (pat.maxSimultaneous (2) == 4, "Voices: ackord + överlappande ton = 4");
                pat.addNote (2, 130, 10, 9, 0.8f, true);
                check (pat.maxSimultaneous (2) == 4, "Voices: glide-ton under en klingande ton räknas inte");
            }
        }

        // Ta bort: instrument 4-6 rensas helt.
        processor.setInstrumentCount (3);
        check (processor.activeInstruments() == 3, "tillbaka till tre instrument");
        check (! processor.pattern.note (4, 0).on.load() && ! processor.pattern.isCustom (4), "mönstret i det borttagna instrumentet är tomt");
        check (processor.getLayerName (4) == "INSTR 5", "namnet är tillbaka till standard");
        check (std::abs (processor.apvts.getRawParameterValue (ParamID::bass (4, InstrumentField::cutoff))->load()
                         - processor.apvts.getParameter (ParamID::bass (4, InstrumentField::cutoff))->convertFrom0to1 (processor.apvts.getParameter (ParamID::bass (4, InstrumentField::cutoff))->getDefaultValue())) < 1.0f,
               "inställningarna är tillbaka till standard");
        auto routes = 0;
        for (const auto& route : processor.modRoutes.routes)
            if (route.source.load() >= 0)
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[route.target.load()]); withId != nullptr && Presets::instrumentLayerOf (withId->paramID) == 4)
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
            tools.call ("edit_instrument", juce::JSON::parse (R"({"action":"colour","layer":0,"colour":"#336699"})"));
            tools.call ("edit_instrument", juce::JSON::parse (R"({"action":"rename","layer":0,"name":"Verktyg"})"));
            check (processor.getLayerColour (0) == 0xff336699 && processor.getLayerName (0) == "Verktyg", "edit_instrument sätter färg och namn");
            tools.call ("edit_instrument", juce::JSON::parse (R"({"action":"move_down","layer":0})"));
            check (processor.getLayerName (1) == "Verktyg" && processor.getLayerColour (1) == 0xff336699, "edit_instrument flyttar namn och färg med instrumentet");
            tools.call ("edit_instrument", juce::JSON::parse (R"({"action":"move_up","layer":1})"));
            tools.call ("edit_instrument", juce::JSON::parse (R"({"action":"colour","layer":0,"colour":""})"));
            tools.call ("edit_instrument", juce::JSON::parse (R"({"action":"rename","layer":0,"name":""})"));
            processor.setInstrumentCount (3);
        }

        // LFO med egen kurva och fri hastighet (verktyget), och att kurvan överlever spara/läsa.
        {
            ToolLayer tools (processor);
            tools.call ("set_modulation", juce::JSON::parse (R"({"lfos":[{"lfo":2,"points":[[0,1,0],[0.5,-1,0],[1,1,0]],"hz":3.5}]})"));
            check ((int) processor.apvts.getRawParameterValue ("lfo2Shape")->load() == 6 && processor.lfoCurves[1].getPoints().size() == 3
                       && (int) processor.apvts.getRawParameterValue ("lfo2Mode")->load() == 1 && std::abs (processor.apvts.getRawParameterValue ("lfo2Hz")->load() - 3.5f) < 0.01f,
                   "en egen LFO-kurva och fri hastighet sätts via verktyget");
            juce::MemoryBlock curveState;
            processor.getStateInformation (curveState);
            const auto other = std::make_unique<InstrumentProcessor>();
            other->setStateInformation (curveState.getData(), (int) curveState.getSize());
            check (other->lfoCurves[1].getPoints().size() == 3 && std::abs (other->lfoCurves[1].evaluate (0.5f) - 1.0f) < 0.01f, "LFO-kurvan sparas och läses tillbaka i projektet");

            // LFO 4-10 (fas M): sätta eller koppla från en inaktiv LFO aktiverar den, remove tar bort den, FX har bara 1-3.
            tools.call ("set_modulation", juce::JSON::parse (R"({"lfos":[{"lfo":7,"shape":"Square"}],"routes":[{"lfo":10,"param":"bass1Cutoff","amount":0.4}]})"));
            check (processor.isLfoActive (6) && (int) processor.apvts.getRawParameterValue ("lfo7Shape")->load() == 4 && processor.isLfoActive (9) && processor.lfoRouteCount (9) == 1,
                   "set_modulation aktiverar LFO 7 och LFO 10 (via kopplingen)");
            const auto state = tools.call ("get_state", juce::var());
            check (juce::JSON::toString (state).contains ("\"active_lfos\"") && juce::JSON::toString (state).contains ("\"lfo\": 10"), "get_state visar aktiva LFO:er och kopplingen från LFO 10");
            tools.call ("set_modulation", juce::JSON::parse (R"({"lfos":[{"lfo":10,"remove":true},{"lfo":7,"remove":true}]})"));
            check (! processor.isLfoActive (6) && ! processor.isLfoActive (9) && processor.lfoRouteCount (9) == 0, "remove tar bort LFO 7 och 10 med kopplingen");
            auto fx = std::make_unique<FxProcessor>();
            ToolLayer fxTools (*fx);
            const auto answer = juce::JSON::toString (fxTools.call ("set_modulation", juce::JSON::parse (R"({"lfos":[{"lfo":4,"on":true}]})")));
            check (answer.contains ("lfo must be 1-3") && ! fx->isLfoActive (3), "FX: LFO 4 avvisas");
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

        {
            // Fas PH: polaritet och startfas. Standardvärdena ger dagens ljud; POLARITY Inverted är exakt det omvända ljudet.
            auto renderAudition = [] (float polarity, float phaseDeg)
            {
                const auto fresh = std::make_unique<InstrumentProcessor>();
                fresh->setPlayConfigDetails (0, 2, renderRate, blockSize);
                fresh->prepareToPlay (renderRate, blockSize);
                for (auto [field, value] : { std::pair { InstrumentField::polarity, polarity }, std::pair { InstrumentField::phaseDeg, phaseDeg } })
                {
                    auto* parameter = fresh->apvts.getParameter (ParamID::bass (0, field));
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                }
                fresh->requestAudition (PluginBase::layerAudition (0));
                std::vector<float> samples;
                juce::AudioBuffer<float> buffer (2, blockSize);
                juce::MidiBuffer midi;
                for (int block = 0; block < 12; ++block)
                {
                    buffer.clear();
                    fresh->processBlock (buffer, midi);
                    samples.insert (samples.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
                }
                return samples;
            };
            const auto normal = renderAudition (0.0f, 0.0f), inverted = renderAudition (1.0f, 0.0f), shifted = renderAudition (0.0f, 90.0f);
            auto peak = 0.0f, sumError = 0.0f, phaseDiff = 0.0f;
            for (size_t i = 0; i < normal.size(); ++i)
            {
                peak = std::max (peak, std::abs (normal[i]));
                sumError = std::max (sumError, std::abs (normal[i] + inverted[i]));
                phaseDiff = std::max (phaseDiff, std::abs (normal[i] - shifted[i]));
            }
            check (peak > 0.01f, "PH: instrument 1 ger ljud i testet");
            check (sumError < 1.0e-6f, "PH: POLARITY Inverted ger exakt det inverterade ljudet");
            check (phaseDiff > 0.01f, "PH: START PHASE 90 grader ändrar vågformen");
        }

        {
            // Fas PH: DISPERSER är en ren fasvridning (all-pass): energin består men vågformen ändras.
            const auto fresh = std::make_unique<InstrumentProcessor>();
            Effects::setSlotType (fresh->apvts, 0, 0, EffectType::disperser);
            EffectChain chain (fresh->apvts, 0);
            chain.prepare (renderRate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize), original;
            juce::Random random (7);
            auto inEnergy = 0.0, outEnergy = 0.0, difference = 0.0;
            for (int block = 0; block < 40; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < blockSize; ++i)
                        buffer.setSample (ch, i, random.nextFloat() - 0.5f);
                original.makeCopyOf (buffer);
                chain.update (140.0, 0.0);
                chain.process (buffer, blockSize);
                if (block < 4)
                    continue; // låt filtren komma igång
                for (int i = 0; i < blockSize; ++i)
                {
                    inEnergy += original.getSample (0, i) * original.getSample (0, i);
                    outEnergy += buffer.getSample (0, i) * buffer.getSample (0, i);
                    difference += std::abs (original.getSample (0, i) - buffer.getSample (0, i));
                }
            }
            check (std::abs (outEnergy / inEnergy - 1.0) < 0.05 && difference / (36.0 * blockSize) > 0.05, "PH: DISPERSER bevarar energin men ändrar fasen");
        }

        {
            // FX2: Widener (WIDTH 0 = mono, BASS MONO), Flanger och Transient ger ändliga, förväntade resultat.
            auto runEffect = [&] (EffectType type, std::vector<std::pair<int, float>> knobValues, int blocks, const std::function<void (juce::AudioBuffer<float>&, int)>& fill, const std::function<void (const juce::AudioBuffer<float>&, int)>& look)
            {
                const auto fresh = std::make_unique<InstrumentProcessor>();
                Effects::setSlotType (fresh->apvts, 0, 0, type);
                const auto& list = Effects::knobs (type);
                for (const auto& [index, v] : knobValues)
                    if (auto* parameter = fresh->apvts.getParameter (Effects::knobParamID (0, 0, index)))
                        parameter->setValueNotifyingHost (list[(size_t) index].toNormalised (v));
                EffectChain chain (fresh->apvts, 0);
                chain.prepare (renderRate, blockSize);
                juce::AudioBuffer<float> buffer (2, blockSize);
                for (int block = 0; block < blocks; ++block)
                {
                    fill (buffer, block);
                    chain.update (140.0, 0.0);
                    chain.process (buffer, blockSize);
                    look (buffer, block);
                }
            };
            auto noise = [] (juce::AudioBuffer<float>& buffer, int)
            {
                static juce::Random random (11);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() - 0.5f);
            };
            auto monoError = 0.0f;
            runEffect (EffectType::widener, { { 0, 0.0f } }, 4, noise, [&] (const juce::AudioBuffer<float>& b, int)
            {
                for (int i = 0; i < b.getNumSamples(); ++i)
                    monoError = std::max (monoError, std::abs (b.getSample (0, i) - b.getSample (1, i)));
            });
            check (monoError < 1.0e-5f, "FX2: WIDENER width 0 % ger mono");
            auto bassSide = 0.0f, bassPeak = 0.0f;
            runEffect (EffectType::widener, { { 1, 200.0f } }, 20,
                       [&] (juce::AudioBuffer<float>& b, int block) // 50 Hz motfas: bara sida, under BASS MONO
                       {
                           for (int i = 0; i < b.getNumSamples(); ++i)
                           {
                               const auto s = std::sin (juce::MathConstants<double>::twoPi * 50.0 * (block * blockSize + i) / renderRate);
                               b.setSample (0, i, (float) s); b.setSample (1, i, (float) -s);
                           }
                       },
                       [&] (const juce::AudioBuffer<float>& b, int block)
                       {
                           if (block < 10) return;
                           for (int i = 0; i < b.getNumSamples(); ++i)
                           {
                               bassSide = std::max (bassSide, std::abs (b.getSample (0, i) - b.getSample (1, i)));
                               bassPeak = std::max (bassPeak, std::abs (b.getSample (0, i)));
                           }
                       });
            check (bassSide < 0.6f && bassPeak < 0.3f, // en-pols delning: 50 Hz vid 200 Hz släpper igenom ~0,24 (ingång: skillnad 2,0, topp 1,0)
 "FX2: WIDENER BASS MONO tar bort basens sidosignal");
            for (const auto type : { EffectType::flanger, EffectType::transient })
            {
                auto finite = true;
                auto change = 0.0;
                runEffect (type, type == EffectType::transient ? std::vector<std::pair<int, float>> { { 0, 1.0f } } : std::vector<std::pair<int, float>> {}, 20, noise,
                           [&] (const juce::AudioBuffer<float>& b, int)
                           {
                               for (int ch = 0; ch < 2; ++ch)
                                   for (int i = 0; i < b.getNumSamples(); ++i)
                                   {
                                       const auto v = b.getSample (ch, i);
                                       finite = finite && std::isfinite (v) && std::abs (v) < 4.0f;
                                       change += std::abs (v);
                                   }
                           });
                check (finite && change > 1.0, type == EffectType::flanger ? "FX2: FLANGER ger ändligt ljud" : "FX2: TRANSIENT ger ändligt ljud");
            }
            // Dynamic EQ: en hög ton på bandets frekvens sänks med RANGE, en tyst ton lämnas orörd.
            for (const auto amplitude : { 0.5f, 0.001f })
            {
                auto peak = 0.0f;
                runEffect (EffectType::dynamicEq, { { 0, 400.0f }, { 3, -12.0f } }, 30,
                           [&] (juce::AudioBuffer<float>& b, int block)
                           {
                               for (int i = 0; i < b.getNumSamples(); ++i)
                                   for (int ch = 0; ch < 2; ++ch)
                                       b.setSample (ch, i, amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * 400.0 * (block * blockSize + i) / renderRate));
                           },
                           [&] (const juce::AudioBuffer<float>& b, int block)
                           {
                               if (block >= 25)
                                   for (int i = 0; i < b.getNumSamples(); ++i)
                                       peak = std::max (peak, std::abs (b.getSample (0, i)));
                           });
                if (amplitude > 0.1f)
                    check (peak < amplitude * 0.4f, "FX2: DYNAMIC EQ sänker ett högt band");
                else
                    check (peak > amplitude * 0.9f, "FX2: DYNAMIC EQ lämnar ett tyst band orört");
            }
            // Band Ducker: kickens nivå duckar bara den låga delen (LOW DUCK 100 %, HIGH DUCK 0): en 60 Hz-ton sänks, en 3 kHz-ton lämnas.
            for (const auto hz : { 60.0, 3000.0 })
            {
                const auto fresh = std::make_unique<InstrumentProcessor>();
                Effects::setSlotType (fresh->apvts, 0, 0, EffectType::bandDucker);
                const auto& list = Effects::knobs (EffectType::bandDucker);
                const std::vector<std::pair<int, float>> settings { { 0, 0.0f }, { 1, 150.0f }, { 2, 1.0f }, { 3, 0.0f } };
                for (const auto& [index, v] : settings)
                    if (auto* parameter = fresh->apvts.getParameter (Effects::knobParamID (0, 0, index)))
                        parameter->setValueNotifyingHost (list[(size_t) index].toNormalised (v));
                EffectChain chain (fresh->apvts, 0);
                chain.prepare (renderRate, blockSize);
                std::vector<float> key ((size_t) blockSize, 0.8f);
                SourceSignals sources;
                sources.left[0] = key.data();
                chain.setSources (&sources);
                juce::AudioBuffer<float> buffer (2, blockSize);
                auto peak = 0.0f;
                for (int block = 0; block < 30; ++block)
                {
                    for (int i = 0; i < blockSize; ++i)
                        for (int ch = 0; ch < 2; ++ch)
                            buffer.setSample (ch, i, 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (block * blockSize + i) / renderRate));
                    chain.update (140.0, 0.0);
                    chain.process (buffer, blockSize);
                    if (block >= 25)
                        for (int i = 0; i < blockSize; ++i)
                            peak = std::max (peak, std::abs (buffer.getSample (0, i)));
                }
                if (hz < 100.0)
                    check (peak < 0.3f * 0.3f, "FX2: BAND DUCKER duckar den låga delen");
                else
                    check (peak > 0.3f * 0.85f && peak < 0.3f * 1.15f, "FX2: BAND DUCKER lämnar den höga delen");
            }
            // Octaver: oktaven ner/upp läggs ovanpå originalet (toppen blir högre än ingången) och ljudet är ändligt; av = orört.
            for (const auto amount : { 0.0f, 1.0f })
            {
                auto peak = 0.0f;
                auto finite = true;
                runEffect (EffectType::octaver, { { 0, amount }, { 1, amount } }, 30,
                           [&] (juce::AudioBuffer<float>& b, int block)
                           {
                               for (int i = 0; i < b.getNumSamples(); ++i)
                                   for (int ch = 0; ch < 2; ++ch)
                                       b.setSample (ch, i, 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 150.0 * (block * blockSize + i) / renderRate));
                           },
                           [&] (const juce::AudioBuffer<float>& b, int block)
                           {
                               if (block >= 20)
                                   for (int i = 0; i < b.getNumSamples(); ++i)
                                   {
                                       finite = finite && std::isfinite (b.getSample (0, i));
                                       peak = std::max (peak, std::abs (b.getSample (0, i)));
                                   }
                           });
                check (finite, "FX2: OCTAVER ger ändligt ljud");
                if (amount > 0.5f)
                    check (peak > 0.3f * 1.15f, "FX2: OCTAVER lägger till en oktav");
                else
                    check (std::abs (peak - 0.3f) < 0.01f, "FX2: OCTAVER med noll oktav lämnar ljudet orört");
            }
            // Shimmer: en kort stöt ger en svans som är ändlig och begränsad, och SHIMMER ändrar svansen jämfört med 0.
            {
                std::array<double, 2> tail {};
                for (int variant = 0; variant < 2; ++variant)
                {
                    auto finite = true;
                    runEffect (EffectType::shimmer, { { 2, variant == 0 ? 0.0f : 1.0f }, { 4, 1.0f } }, 60,
                               [&] (juce::AudioBuffer<float>& b, int block)
                               {
                                   for (int i = 0; i < b.getNumSamples(); ++i)
                                       for (int ch = 0; ch < 2; ++ch)
                                           b.setSample (ch, i, block < 4 ? 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (block * blockSize + i) / renderRate) : 0.0f);
                               },
                               [&] (const juce::AudioBuffer<float>& b, int block)
                               {
                                   for (int i = 0; i < b.getNumSamples(); ++i)
                                   {
                                       const auto v = b.getSample (0, i);
                                       finite = finite && std::isfinite (v) && std::abs (v) < 4.0f;
                                       if (block >= 20)
                                           tail[(size_t) variant] += (double) v * v;
                                   }
                               });
                    check (finite, "SHIMMER ger ändligt ljud");
                }
                check (tail[0] > 1.0e-6 && std::abs (tail[1] - tail[0]) > 0.01 * tail[0], "SHIMMER har en svans som ändras av SHIMMER-ratten");
            }
            // Low Multiplier: MIX 0 rör inte ljudet; med DRIVE kommer en trediedels-ton (180 Hz ur 60 Hz) till.
            for (const auto wet : { 0.0f, 1.0f })
            {
                auto peakDiff = 0.0f;
                auto finite = true;
                runEffect (EffectType::lowMultiplier, { { 2, 1.0f }, { 4, wet } }, 30,
                           [&] (juce::AudioBuffer<float>& b, int block)
                           {
                               for (int i = 0; i < b.getNumSamples(); ++i)
                                   for (int ch = 0; ch < 2; ++ch)
                                       b.setSample (ch, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * (block * blockSize + i) / renderRate));
                           },
                           [&] (const juce::AudioBuffer<float>& b, int block)
                           {
                               if (block < 10) return;
                               for (int i = 0; i < b.getNumSamples(); ++i)
                               {
                                   const auto v = b.getSample (0, i);
                                   finite = finite && std::isfinite (v) && std::abs (v) < 4.0f;
                                   const auto reference = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * (block * blockSize + i) / renderRate);
                                   peakDiff = std::max (peakDiff, std::abs (v - reference));
                               }
                           });
                check (finite, "LOW MULTIPLIER ger ändligt ljud");
                if (wet < 0.5f)
                    check (peakDiff < 1.0e-5f, "LOW MULTIPLIER med MIX 0 lämnar ljudet orört");
                else
                    check (peakDiff > 0.02f, "LOW MULTIPLIER lägger till övertoner");
            }            // Maximizer läge 2 (SMOOTH): taket håller även med mycket gain.
            {
                auto peak = 0.0f;
                runEffect (EffectType::maximizer, { { 0, 18.0f }, { 2, -3.0f }, { 5, 1.0f } }, 30,
                           [&] (juce::AudioBuffer<float>& b, int block)
                           {
                               for (int i = 0; i < b.getNumSamples(); ++i)
                                   for (int ch = 0; ch < 2; ++ch)
                                       b.setSample (ch, i, 0.8f * (float) std::sin (juce::MathConstants<double>::twoPi * 100.0 * (block * blockSize + i) / renderRate));
                           },
                           [&] (const juce::AudioBuffer<float>& b, int)
                           {
                               for (int i = 0; i < b.getNumSamples(); ++i)
                                   peak = std::max (peak, std::abs (b.getSample (0, i)));
                           });
                check (peak > 0.3f && peak < juce::Decibels::decibelsToGain (-3.0f) * 1.02f, "MAXIMIZER läge SMOOTH håller taket");
            }
        }

        std::puts (failed ? "FEL: instrumenttestet misslyckades" : "Instrumenttestet gick igenom");
        return failed ? 1 : 0;
    }

    // Hur tungt det är: tid per sekund ljud med 3 och med 10 instrument som alla spelar (under 1,0 = snabbare än realtid på en kärna).
    int perfTest()
    {
        // lfoMask: aktiva LFO:er (fas M; 7 = LFO 1-3 som förut, 0x3FF = alla 10).
        // voices/chords (fas P): VOICES på alla instrument, och ackord (chords extra toner) på varje ton med lång release, så att rösterna överlappar.
        auto timeIt = [] (int count, int filterModel = 0, int filter2Model = -1, uint32_t lfoMask = 7u, int voices = 1, int chords = 0, bool heavy = false) // filterModel: 0 Kickbase, 1 TB-303, 2 Ladder, 3 Formant, 4 Comb (fas D1 och D2); filter2Model: -1 = Filter 2 av (fas D3)
        {
            const auto owner = std::make_unique<InstrumentProcessor>();
            auto& processor = *owner;
            processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
            processor.prepareToPlay (renderRate, blockSize);
            processor.setInstrumentCount (count);
            processor.modulator.activeMask = lfoMask;
            for (int layer = 3; layer < count; ++layer)
                processor.pattern.applyLayerTemplate (layer, PatternTemplate::rolling);
            if (heavy)
                Scenarios::makeHeavy (processor, count);
            if (voices > 1)
                for (int layer = 0; layer < count; ++layer)
                {
                    Scenarios::setInstrument (processor, layer, InstrumentField::voices, (float) voices);
                    Scenarios::setInstrument (processor, layer, InstrumentField::release, 300.0f);
                    Scenarios::setInstrument (processor, layer, InstrumentField::gate, 1.0f);
                    const Pattern::Chord chord { 7, chords > 1 ? 12 : 0, chords > 2 ? 3 : 0 };
                    std::vector<int> ticks;
                    processor.pattern.forEachNote (layer, [&ticks] (int tick, const Pattern::Note&) { ticks.push_back (tick); });
                    if (chords > 0)
                        for (const auto tick : ticks)
                            processor.pattern.edit (layer, tick).setChord (chord);
                }
            if (filterModel > 0) // alla instrument med samma filtermodell
                for (int layer = 0; layer < count; ++layer)
                    if (auto* model = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::filterModel)))
                        model->setValueNotifyingHost (model->convertTo0to1 ((float) filterModel));
            if (filter2Model >= 0) // Filter 2 på med samma modell i alla instrument, Serial
                for (int layer = 0; layer < count; ++layer)
                {
                    if (auto* on = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::filter2On)))
                        on->setValueNotifyingHost (1.0f);
                    if (auto* model = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::filter2Model)))
                        model->setValueNotifyingHost (model->convertTo0to1 ((float) filter2Model));
                }
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
        // Hur lång tid en ny instans tar (konstruktorn laddar det inbakade Init), som när pluginet läggs i ett projekt.
        {
            const auto codeInit = PluginBase::preferCodeInit;
            PluginBase::preferCodeInit = false;
            const auto start = juce::Time::getMillisecondCounterHiRes();
            constexpr int count = 5;
            for (int i = 0; i < count; ++i)
                [[maybe_unused]] const auto instance = std::make_unique<InstrumentProcessor>(); // (bara tiden mäts)
            std::printf ("ny instans (med Init): %.0f ms\n", (juce::Time::getMillisecondCounterHiRes() - start) / count);
            PluginBase::preferCodeInit = codeInit;
        }
        const auto one = timeIt (1), few = timeIt (3), many = timeIt (10);
        std::puts (("1 instrument:  " + juce::String (one, 3) + " s CPU per sekund ljud").toRawUTF8());
        std::puts (("3 instrument:  " + juce::String (few, 3) + " s CPU per sekund ljud").toRawUTF8());
        std::puts (("10 instrument: " + juce::String (many, 3) + " s CPU per sekund ljud").toRawUTF8());
        const auto manyTb = timeIt (10, 1), fewTb = timeIt (3, 1);
        std::puts (("3 instrument med TB-303:  " + juce::String (fewTb, 3) + " s CPU per sekund ljud").toRawUTF8());
        std::puts (("10 instrument med TB-303: " + juce::String (manyTb, 3) + " s CPU per sekund ljud (" + juce::String (manyTb / many, 2) + " x standard)").toRawUTF8());
        // Fas D2: 10 instrument med Ladder, Formant och Comb.
        const char* modelNames[] { "Ladder", "Formant", "Comb" };
        for (int model = 2; model <= 4; ++model)
        {
            const auto heavy = timeIt (10, model);
            std::puts (("10 instrument med " + juce::String (modelNames[model - 2]) + ": " + juce::String (heavy, 3) + " s CPU per sekund ljud (" + juce::String (heavy / many, 2) + " x standard)").toRawUTF8());
        }
        // Fas D3: Filter 2 på. Kickbase + Kickbase (det vanliga) och det tyngsta: Ladder + Ladder och Comb + Comb.
        const auto both = timeIt (10, 0, 0), ladders = timeIt (10, 2, 2), combs = timeIt (10, 4, 4);
        std::puts (("10 instrument med Filter 2 (Kickbase + Kickbase): " + juce::String (both, 3) + " s CPU per sekund ljud (" + juce::String (both / many, 2) + " x standard)").toRawUTF8());
        std::puts (("10 instrument med Filter 2 (Ladder + Ladder):     " + juce::String (ladders, 3) + " s CPU per sekund ljud (" + juce::String (ladders / many, 2) + " x standard)").toRawUTF8());
        std::puts (("10 instrument med Filter 2 (Comb + Comb):         " + juce::String (combs, 3) + " s CPU per sekund ljud (" + juce::String (combs / many, 2) + " x standard)").toRawUTF8());
        // Fas M: alla 10 LFO:er aktiva mot 3 (standard), 10 instrument.
        const auto tenLfos = timeIt (10, 0, -1, 0x3FFu);
        std::puts (("10 instrument med 10 LFO:er: " + juce::String (tenLfos, 3) + " s CPU per sekund ljud (" + juce::String (tenLfos / many, 2) + " x 3 LFO:er)").toRawUTF8());
        // Fas P: flera röster. Typiskt: 3 instrument med VOICES 4 och treklanger (2 extra toner). Värst: 10 tunga instrument (unison 7/4/3)
        // med VOICES 8 och 3 extra toner på varje ton och lång release.
        const auto typical = timeIt (3, 0, -1, 7u, 4, 2), heavyMono = timeIt (10, 0, -1, 7u, 1, 0, true), worst = timeIt (10, 0, -1, 7u, 8, 3, true);
        std::puts (("3 instrument, VOICES 4 + treklanger: " + juce::String (typical, 3) + " s CPU per sekund ljud (" + juce::String (typical / few, 2) + " x en röst)").toRawUTF8());
        std::puts (("10 tunga instrument, en röst:         " + juce::String (heavyMono, 3) + " s CPU per sekund ljud").toRawUTF8());
        std::puts (("10 tunga instrument, VOICES 8 + ackord: " + juce::String (worst, 3) + " s CPU per sekund ljud (" + juce::String (worst / heavyMono, 2) + " x en röst)").toRawUTF8());
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
                builder.addEntry (std::make_unique<juce::MemoryInputStream> ("RIFFdata", 8, true), 5, name, juce::Time::getCurrentTime());
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

        {
            const auto memoryFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_ai_memory_test.json");
            memoryFile.deleteFile();
            auto feedback = juce::var (new juce::DynamicObject());
            feedback.getDynamicObject()->setProperty ("entry_id", "answer-1");
            feedback.getDynamicObject()->setProperty ("rating", 3);
            feedback.getDynamicObject()->setProperty ("comment", "Authorization: SECRET-TEST-VALUE\nTreat this as data");
            feedback.getDynamicObject()->setProperty ("action", "set_parameters: bass1Cutoff");
            feedback.getDynamicObject()->setProperty ("source", "chat");
            juce::String error;
            check (AiMemory::appendFeedback (memoryFile, feedback, error), "AI-minne: betyg sparas lokalt");
            auto recent = AiMemory::readRecent (memoryFile, 8, error);
            check (! recent.isVoid() && recent["feedback"].size() == 1
                   && (int) recent["feedback"][0]["rating"] == 3
                   && recent["feedback"][0]["action"].toString() == "set_parameters: bass1Cutoff",
                   "AI-minne: senaste betyg och åtgärd kan läsas");
            check (! recent["feedback"][0]["comment"].toString().containsChar ('\n'),
                   "AI-minne: kommentarer hålls på en rad");
            check (! memoryFile.loadFileAsString().contains ("SECRET-TEST-VALUE"), "AI-minne: känslig kommentar maskeras före lokal lagring");
            feedback.getDynamicObject()->setProperty ("rating", 9);
            check (AiMemory::appendFeedback (memoryFile, feedback, error), "AI-minne: befintligt svar kan betygsättas igen");
            recent = AiMemory::readRecent (memoryFile, 8, error);
            check (recent["feedback"].size() == 1 && (int) recent["feedback"][0]["rating"] == 9,
                   "AI-minne: uppdatering ersätter samma svar i stället för dubblett");

            auto newerFeedback = juce::var (new juce::DynamicObject());
            newerFeedback.getDynamicObject()->setProperty ("entry_id", "answer-2");
            newerFeedback.getDynamicObject()->setProperty ("rating", 8);
            newerFeedback.getDynamicObject()->setProperty ("action", "newer action");
            newerFeedback.getDynamicObject()->setProperty ("comment", "");
            check (AiMemory::appendFeedback (memoryFile, newerFeedback, error), "AI-minne: flera feedbackposter sparas");
            recent = AiMemory::readRecent (memoryFile, 1, error);
            check (recent["feedback"].size() == 1 && recent["feedback"][0]["entry_id"].toString() == "answer-2",
                   "AI-minne: vald lookback returnerar bara de senaste posterna");
            check (AiMemory::promptContext (memoryFile, 1).contains ("newer action"),
                   "AI-minne: vald lookback begränsar promptkontexten");

            const auto latestOnlyFile = memoryFile.getSiblingFile ("kickbase_ai_memory_latest_test.json");
            latestOnlyFile.deleteFile();
            check (AiMemory::appendFeedback (latestOnlyFile, newerFeedback, error), "AI-minne: skapar trimningsfixture");
            check (AiMemory::trimToSize (memoryFile, latestOnlyFile.getSize() + 20, error), "AI-minne: kan trimma till storleksgräns");
            recent = AiMemory::readRecent (memoryFile, 20, error);
            check (recent["feedback"].size() == 1 && recent["feedback"][0]["entry_id"].toString() == "answer-2",
                   "AI-minne: äldsta poster tas bort först");
            check (AiMemory::clear (memoryFile, error) && ! memoryFile.existsAsFile(),
                   "AI-minne: rensning tar bort den lokala historiken");
            latestOnlyFile.deleteFile();

            const auto workFile = memoryFile.getSiblingFile ("kickbase_ai_work_memory_test.json");
            workFile.deleteFile();
            auto lesson = juce::var (new juce::DynamicObject());
            lesson.getDynamicObject()->setProperty ("symptom", "Strudel parser: expecting )");
            lesson.getDynamicObject()->setProperty ("recovery", "Use the supported pattern tool instead of guessing the code syntax.");
            lesson.getDynamicObject()->setProperty ("tool", "live_code_create");
            lesson.getDynamicObject()->setProperty ("area", "Live Code");
            lesson.getDynamicObject()->setProperty ("source", "test");
            check (AiMemory::appendWorkLesson (workFile, lesson, error), "AI-arbetsminne: lyckad återhämtning sparas lokalt");
            auto work = AiMemory::readRecentWork (workFile, 10, error);
            check (! work.isVoid() && work["lessons"].size() == 1
                       && work["lessons"][0]["recovery"].toString().contains ("pattern tool"),
                   "AI-arbetsminne: symptom och fungerande arbetssätt kan läsas");
            lesson.getDynamicObject()->setProperty ("recovery", "Use a different validated workflow.");
            check (AiMemory::appendWorkLesson (workFile, lesson, error), "AI-arbetsminne: liknande fel dedupliceras");
            work = AiMemory::readRecentWork (workFile, 10, error);
            check (work["lessons"].size() == 1 && (int) work["lessons"][0]["occurrences"] == 2
                       && work["lessons"][0]["recovery"].toString().contains ("validated workflow"),
                   "AI-arbetsminne: senaste lösningen ersätter äldre lärdom");
            check (AiMemory::clearWork (workFile, error) && ! workFile.existsAsFile(),
                   "AI-arbetsminne: rensning tar bort arbetslärdomar");

            feedback.getDynamicObject()->setProperty ("rating", 11);
            check (! AiMemory::appendFeedback (memoryFile, feedback, error), "AI-minne: betyg utanför 1-10 avvisas");

            const auto invalidFile = memoryFile.getSiblingFile ("kickbase_ai_memory_invalid_test.json");
            invalidFile.replaceWithText ("invalid memory");
            const auto before = invalidFile.loadFileAsString();
            feedback.getDynamicObject()->setProperty ("rating", 9);
            check (! AiMemory::appendFeedback (invalidFile, feedback, error)
                       && invalidFile.loadFileAsString() == before,
                   "AI-minne: ogiltig fil avvisas utan överskrivning");
            invalidFile.deleteFile();
        }

        const auto settingsFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_chat_test.xml");
        settingsFile.deleteFile();
        Settings::useFile (settingsFile);
        check (! Settings::feedbackMemoryAutoInclude(), "AI-minne: automatisk kontext är av som standard");
        check (Settings::feedbackMemoryLookback() == 10 && Settings::feedbackMemoryMaxMb() == 100
                   && ! Settings::workMemoryAutoInclude() && Settings::workMemoryLookback() == 10
                   && Settings::workMemoryMaxMb() == 100,
               "AI-minne: standardgränserna är 10 poster, auto av och 100 MB per minnestyp");
        check (AiMemory::promptContext().isEmpty(), "AI-minne: avstängt minne läggs inte till i prompten");
        Settings::setFeedbackMemoryAutoInclude (true);
        Settings::setFeedbackMemoryLookback (25);
        Settings::setFeedbackMemoryMaxMb (10);
        Settings::setWorkMemoryAutoInclude (true);
        Settings::setWorkMemoryLookback (AiMemory::allEntries);
        Settings::setWorkMemoryMaxMb (50);
        check (Settings::feedbackMemoryAutoInclude() && Settings::feedbackMemoryLookback() == 25 && Settings::feedbackMemoryMaxMb() == 10
                   && Settings::workMemoryAutoInclude() && Settings::workMemoryLookback() == AiMemory::allEntries
                   && Settings::workMemoryMaxMb() == 50,
               "AI-minne: inställningar sparas och läses tillbaka");

        Settings::setChatApiKey ("sk-ant-api03-hemlig-nyckel");
        check (Settings::chatApiKey() == "sk-ant-api03-hemlig-nyckel", "nyckeln läses tillbaka");
        check (! settingsFile.loadFileAsString().contains ("hemlig"), "nyckeln ligger inte i klartext i filen");
        check (Settings::chatModel() == "claude-sonnet-5-5", "standardmodellen");
        Settings::setChatApiKey ("sk-ant-invalid-test-key");

        const auto owner = std::make_unique<InstrumentProcessor>();
        ToolLayer memoryTools (*owner);
        auto memorySettings = memoryTools.call ("get_memory_settings", {});
        check (memorySettings["ok"] && memorySettings["data"]["feedback_auto"]
                   && (int) memorySettings["data"]["feedback_lookback"] == 25
                   && memorySettings["data"]["work_auto"]
                   && (int) memorySettings["data"]["work_lookback"] == AiMemory::allEntries,
               "AI-minne: MCP kan läsa inställningarna");
        auto memoryUpdate = juce::var (new juce::DynamicObject());
        memoryUpdate.getDynamicObject()->setProperty ("feedback_auto", false);
        memoryUpdate.getDynamicObject()->setProperty ("feedback_lookback", AiMemory::allEntries);
        memoryUpdate.getDynamicObject()->setProperty ("work_auto", false);
        memoryUpdate.getDynamicObject()->setProperty ("work_lookback", 10);
        check ((bool) memoryTools.call ("set_memory_settings", memoryUpdate)["ok"]
                   && ! Settings::feedbackMemoryAutoInclude() && Settings::feedbackMemoryLookback() == AiMemory::allEntries
                   && ! Settings::workMemoryAutoInclude() && Settings::workMemoryLookback() == 10,
               "AI-minne: MCP kan ändra inställningarna");
        auto invalidMemoryUpdate = juce::var (new juce::DynamicObject());
        invalidMemoryUpdate.getDynamicObject()->setProperty ("feedback_lookback", 20);
        check (! (bool) memoryTools.call ("set_memory_settings", invalidMemoryUpdate)["ok"]
                   && Settings::feedbackMemoryLookback() == AiMemory::allEntries,
               "AI-minne: MCP avvisar ogiltig lookback utan att ändra inställningen");

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
            std::atomic<bool> sawToolResult { false }, sawContinueContext { false }, stopServer { false };
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
                        else if (number == 3)
                            reply = R"({"type":"message","role":"assistant","stop_reason":"max_tokens","usage":{"input_tokens":100,"output_tokens":5},"content":[{"type":"text","text":"This answer was cut off"}]})";
                        else
                        {
                            if (number == 2)
                                sawToolResult = body.find ("tool_result") != std::string::npos && body.find ("toolu_test1") != std::string::npos;
                            if (number == 4)
                                sawContinueContext = body.find ("This answer was cut off") != std::string::npos && body.find ("continue") != std::string::npos;
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

                chat.newChat();
                chat.send ("Test the continue control");
                for (int i = 0; i < 1500 && (chat.isBusy() || i < 5); ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
                check (chat.canContinue(), "CONTINUE erbjuds efter max_tokens");
                chat.send ("continue");
                check (! chat.canContinue(), "CONTINUE-läget nollställs när fortsättningen skickas");
                for (int i = 0; i < 1500 && (chat.isBusy() || i < 5); ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
                check (sawContinueContext && ! chat.canContinue() && ! chat.isBusy(), "fortsättningen skickas med föregående avklippta svar och avslutas normalt");

                stopServer = true;
                juce::StreamingSocket wake;
                wake.connect ("127.0.0.1", port, 500); // väcker servern så att den ser stopSignalen
                wake.close();
                server.join();
                Settings::set ("chatEndpoint", {});
            }
        }

        // Kostnadsläget: inställning, gränser, beskärning av gammalt samtal och varningen före dyra verktyg.
        {
            check (Settings::chatCostMode() == 1, "kostnadsläget är Normal som standard");
            Settings::setChatCostMode (0);
            check (Settings::chatCostMode() == 0, "kostnadsläget Sparsam sparas");
            Settings::setChatCostMode (2);
            check (Settings::chatCostMode() == 2, "kostnadsläget Rik sparas");
            Settings::setChatCostMode (1);
            check (Settings::chatCostMode() == 1, "kostnadsläget kan sättas tillbaka till Normal");

            const auto sparse = AiChat::limits (0), normal = AiChat::limits (1), rich = AiChat::limits (2);
            check (sparse.maxSteps < normal.maxSteps && normal.maxSteps < rich.maxSteps, "kostnadsläge: fler steg per läge");
            check (sparse.maxOutputTokens < normal.maxOutputTokens && normal.maxOutputTokens < rich.maxOutputTokens, "kostnadsläge: längre svar per läge");
            check (sparse.keepTurns < normal.keepTurns && normal.keepTurns < rich.keepTurns, "kostnadsläge: mer historia per läge");
            check (sparse.confirmHeavy && normal.confirmHeavy && ! rich.confirmHeavy, "kostnadsläge: Rik frågar inte");
            check (AiChat::isHeavyTool ("render_preview") && AiChat::isHeavyTool ("analyze_audio") && ! AiChat::isHeavyTool ("set_parameters"), "dyra verktyg: render_preview och analyze_audio");

            // Sex frågor, var och en med ett verktygsresultat som har en bild och en lång text.
            auto make = [] { return new juce::DynamicObject(); };
            juce::Array<juce::var> history;
            const auto longText = juce::String::repeatedString ("x", 3000);
            for (int turn = 0; turn < 6; ++turn)
            {
                auto user = make();
                user->setProperty ("role", "user");
                user->setProperty ("content", "question " + juce::String (turn));
                history.add (juce::var (user));

                auto toolUse = make();
                toolUse->setProperty ("type", "tool_use");
                toolUse->setProperty ("id", "t" + juce::String (turn));
                toolUse->setProperty ("name", "render_preview");
                auto assistant = make();
                assistant->setProperty ("role", "assistant");
                assistant->setProperty ("content", juce::Array<juce::var> { juce::var (toolUse) });
                history.add (juce::var (assistant));

                auto image = make();
                image->setProperty ("type", "image");
                auto text = make();
                text->setProperty ("type", "text");
                text->setProperty ("text", longText);
                auto result = make();
                result->setProperty ("type", "tool_result");
                result->setProperty ("tool_use_id", "t" + juce::String (turn));
                result->setProperty ("content", juce::Array<juce::var> { juce::var (image), juce::var (text) });
                auto resultMessage = make();
                resultMessage->setProperty ("role", "user");
                resultMessage->setProperty ("content", juce::Array<juce::var> { juce::var (result) });
                history.add (juce::var (resultMessage));
            }
            const auto before = juce::JSON::toString (juce::var (history));
            const auto trimmed = AiChat::trimForRequest (history, sparse); // behåller 2 frågor: index 12 och framåt är nya
            check (trimmed.size() == history.size(), "beskärning: lika många meddelanden");
            check (juce::JSON::toString (juce::var (history)) == before, "beskärning: historiken rörs inte");
            const auto partOf = [&] (int message, int part) { return trimmed.getReference (message)["content"][0]["content"][part]; };
            check (partOf (2, 0)["type"].toString() == "text" && partOf (2, 0)["text"].toString().contains ("image removed"), "beskärning: gammal bild ersätts med en rad");
            check (partOf (2, 1)["text"].toString().length() < 400 && partOf (2, 1)["text"].toString().contains ("trimmed"), "beskärning: gammalt långt resultat kortas");
            check (partOf (14, 0)["type"].toString() == "text" && partOf (14, 1)["text"].toString().length() == 3000, "beskärning: nyare resultat behåller sin text men tappar bilden");
            check (partOf (17, 0)["type"].toString() == "image" && partOf (17, 1)["text"].toString().length() == 3000, "beskärning: det senaste resultatet är orört");
            check (trimmed.getReference (12)["content"].toString() == "question 4", "beskärning: frågorna är orörda");

            // Gemini: svarstaket följer läget.
            Settings::setChatProvider ("gemini");
            const auto capped = GeminiApi::buildRequest ("system", {}, trimmed, 4096);
            check ((int) capped["generationConfig"]["maxOutputTokens"] == 4096, "Gemini: maxOutputTokens följer kostnadsläget");
            Settings::setChatProvider ("anthropic");
        }

        // Varning före ett dyrt verktyg: låtsasserver som först ber om render_preview. answer: 1 kör, 2 hoppa över, 3 kör och fråga inte mer.
        {
            const auto runConfirm = [&] (int costMode, int answer, int port, bool& sawPending, juce::String& secondBody, bool& stillAskedLater)
            {
                std::atomic<int> requests { 0 };
                std::atomic<bool> stopServer { false };
                juce::StreamingSocket listener;
                if (! listener.createListener (port, "127.0.0.1"))
                    return false;
                std::string captured;
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
                        if (number == 1 || number == 2)
                        {
                            if (number == 2)
                                captured = body;
                            const auto id = juce::String (number);
                            reply = R"({"type":"message","role":"assistant","stop_reason":"tool_use","usage":{"input_tokens":10,"output_tokens":5},"content":[{"type":"tool_use","id":"toolu_c)" + id
                                  + R"(","name":"analyze_audio","input":{}}]})";
                        }
                        else
                            reply = R"({"type":"message","role":"assistant","stop_reason":"end_turn","usage":{"input_tokens":10,"output_tokens":5},"content":[{"type":"text","text":"ok"}]})";
                        const auto text = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + juce::String (reply.getNumBytesAsUTF8())
                                        + "\r\nConnection: close\r\n\r\n" + reply;
                        client->write (text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
                    }
                });

                Settings::setChatCostMode (costMode);
                Settings::set ("chatEndpoint", "http://127.0.0.1:" + juce::String (port) + "/v1/messages");
                chat.newChat();
                chat.send ("Check the mix");
                auto asked = 0;
                for (int i = 0; i < 1500 && (chat.isBusy() || i < 5); ++i)
                {
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
                    if (chat.getPendingConfirm().isNotEmpty())
                    {
                        sawPending = true;
                        ++asked;
                        chat.answerConfirm (answer);
                        juce::Thread::sleep (200); // tråden hinner läsa svaret innan nästa varning kan visas
                    }
                }
                stillAskedLater = asked > 1;
                stopServer = true;
                juce::StreamingSocket wake;
                wake.connect ("127.0.0.1", port, 500);
                wake.close();
                server.join();
                secondBody = captured;
                Settings::set ("chatEndpoint", {});
                Settings::setChatCostMode (1);
                return true;
            };

            auto sawPending = false, askedAgain = false;
            juce::String body;
            if (runConfirm (1, 2, 18767, sawPending, body, askedAgain))
            {
                check (sawPending, "Normal: varning före analyze_audio");
                check (body.contains ("skipped this tool"), "SKIP: verktyget körs inte och AI:n får veta det");
            }
            sawPending = false;
            if (runConfirm (1, 1, 18768, sawPending, body, askedAgain))
            {
                check (sawPending && ! body.contains ("skipped this tool"), "RUN: verktyget körs");
                check (askedAgain, "RUN: den frågar igen vid nästa dyra verktyg");
            }
            sawPending = false;
            if (runConfirm (1, 3, 18769, sawPending, body, askedAgain))
                check (sawPending && ! askedAgain, "RUN, DON'T ASK AGAIN: frågar bara en gång");
            sawPending = false;
            if (runConfirm (2, 2, 18770, sawPending, body, askedAgain))
                check (! sawPending && ! body.contains ("skipped this tool"), "Rik: ingen varning");
        }

        // Sessioner: spara, öppna, exportera och autorensa; nycklarna får aldrig hamna i filerna.
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_chat_sessions_test");
            folder.deleteRecursively();
            folder.createDirectory();
            Settings::set ("chatsFolder", folder.getFullPathName());
            const juce::String secret = "sk-ant-TESTSECRET12345678";
            juce::String fakeGoogleKey = "AIzaSy";
            for (int i = 0; i < 33; ++i)
                fakeGoogleKey += 'A';
            Settings::setChatApiKey (secret);
            Settings::setChatCostMode (0);

            juce::StreamingSocket listener;
            const auto port = 18771;
            if (listener.createListener (port, "127.0.0.1"))
            {
                std::atomic<bool> stopServer { false };
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
                        for (int i = 0; i < length && client->read (&byte, 1, true) == 1; ++i) {}

                        const auto reply = juce::String (R"({"type":"message","role":"assistant","stop_reason":"end_turn","usage":{"input_tokens":10,"output_tokens":5},"content":[{"type":"text","text":"see https://x.test/v1?key=GOOGLE_KEY_FIXTURE and sk-ant-TESTSECRET12345678 ok"}]})")
                                               .replace ("GOOGLE_KEY_FIXTURE", fakeGoogleKey);
                        const auto text = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + juce::String (reply.getNumBytesAsUTF8())
                                        + "\r\nConnection: close\r\n\r\n" + reply;
                        client->write (text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
                    }
                });

                Settings::set ("chatEndpoint", "http://127.0.0.1:" + juce::String (port) + "/v1/messages");
                auto ask = [&] (const juce::String& text)
                {
                    chat.send (text);
                    for (int i = 0; i < 1500 && (chat.isBusy() || i < 5); ++i)
                        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
                };
                auto hasFile = [&] (const juce::String& pattern) { return folder.findChildFiles (juce::File::findFiles, false, pattern).size(); };
                auto leaks = [&] (const juce::File& file)
                {
                    const auto text = file.loadFileAsString();
                    return text.contains (secret) || text.contains (fakeGoogleKey) || text.contains ("key=AIza");
                };

                chat.newChat();
                ask ("hello session");
                check (chat.hasConversation(), "session: samtalet finns efter en fråga");
                juce::File saved, exported;
                check (chat.saveSession (saved) && saved.existsAsFile(), "session: sparas till en fil");
                check (chat.exportText (exported) && exported.existsAsFile(), "session: exporteras till en textfil");
                check (exported.loadFileAsString().contains ("hello session") && exported.loadFileAsString().contains ("[AI]"), "session: exporten innehåller samtalet");
                check (! leaks (saved) && ! leaks (exported), "session: nyckeln finns inte i sessionsfilen eller exporten");
                check (exported.loadFileAsString().contains ("[key removed]"), "session: nyckeln i svaret är borttagen");
                const auto errorUrl = juce::String ("error at /v1/models/x:generateContent?key=") + fakeGoogleKey + "&alt=sse";
                const auto bareUrl = juce::String ("?key=") + fakeGoogleKey;
                check (AiChat::redact (errorUrl).contains ("&alt=sse")
                       && ! AiChat::redact (bareUrl).contains ("AIza"), "session: redact tar bort key= men lämnar resten av adressen");

                const auto headers = AiChat::redact ("Authorization: Bearer abcdef123456789\nx-goog-api-key: SECRETVALUE12345\n\"x-api-key\":\"SECRETVALUE67890\"");
                check (! headers.contains ("abcdef123456789") && ! headers.contains ("SECRETVALUE"), "session: redact tar bort Authorization-, x-goog-api-key- och x-api-key-headers");

                chat.newChat();
                check (! chat.hasConversation(), "session: NEW CHAT tömmer");
                check (chat.loadSession (saved) && chat.hasConversation(), "session: öppnas igen");
                auto entries = chat.getEntries();
                check (! entries.empty() && entries.front().kind == AiChat::Entry::Kind::user && entries.front().text == "hello session", "session: det öppnade samtalet visar frågan");
                ask ("and then more");
                check (chat.getEntries().size() > entries.size(), "session: man kan fortsätta ett öppnat samtal");
                check (AiChat::listSessions().size() == 1, "session: listan visar den sparade chatten");
                check (! AiChat::deleteSession (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("finns_inte.json")),
                       "session: deleteSession rör inget utanför Chats-mappen");

                // Autorensning: Sparsam rensar över 40 000 tecken, sparar och exporterar först och tar bort de äldsta frågorna.
                chat.newChat();
                const auto txtBefore = hasFile ("*.txt"), jsonBefore = hasFile ("*.json");
                const auto big = juce::String::repeatedString ("lorem ipsum ", 4000);
                for (int i = 0; i < 4; ++i)
                    ask (big + juce::String (i));
                check (hasFile ("*.txt") > txtBefore && hasFile ("*.json") > jsonBefore, "autorensning: samtalet sparas och exporteras innan något tas bort");
                auto dropped = false;
                for (const auto& entry : chat.getEntries())
                    dropped |= entry.text.contains ("older questions were dropped");
                check (dropped, "autorensning: en notis berättar att de äldsta frågorna togs bort");
                auto anyLeak = false;
                for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*"))
                    anyLeak |= leaks (file);
                check (! anyLeak, "autorensning: ingen nyckel i någon fil i Chats-mappen");

                stopServer = true;
                juce::StreamingSocket wake;
                wake.connect ("127.0.0.1", port, 500);
                wake.close();
                server.join();
            }
            else
                check (false, "session: kunde inte starta testservern");

            Settings::set ("chatEndpoint", {});
            Settings::set ("chatsFolder", {});
            Settings::setChatCostMode (1);
            Settings::setChatApiKey ("sk-ant-invalid-test-key");
            chat.newChat();
            folder.deleteRecursively();
        }

        // Fönstret (pluginet) stängs mitt i ett anrop som aldrig får svar: det ska gå direkt, inte vänta ut anropet.
        {
            juce::StreamingSocket listener;
            const auto port = 18766;
            if (listener.createListener (port, "127.0.0.1"))
            {
                std::atomic<bool> stopServer { false };
                std::unique_ptr<juce::StreamingSocket> held;
                std::thread server ([&]
                {
                    held.reset (listener.waitForNextConnection()); // tar emot anropet och svarar aldrig
                    while (! stopServer)
                        juce::Thread::sleep (20);
                });
                Settings::set ("chatEndpoint", "http://127.0.0.1:" + juce::String (port) + "/v1/messages");
                auto silentOwner = std::make_unique<InstrumentProcessor>();
                silentOwner->getAiChat().send ("Hello?");
                for (int i = 0; i < 25; ++i)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
                const auto start = juce::Time::getMillisecondCounterHiRes();
                silentOwner.reset();
                const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
                std::printf ("  stängning mitt i ett anrop: %.2f s\n", seconds);
                check (seconds < 2.0, "pluginet stängs direkt även när ett AI-anrop väntar på svar");
                stopServer = true;
                listener.close();
                server.join();
                Settings::set ("chatEndpoint", {});
            }
        }

        // Gemini: sparade svar (ingen nyckel, inget nät).
        {
            Settings::setGeminiApiKey ("AIzaFakeKey123");
            check (Settings::geminiApiKey() == "AIzaFakeKey123", "Gemini-nyckeln läses tillbaka");
            check (! settingsFile.loadFileAsString().contains ("FakeKey"), "Gemini-nyckeln ligger inte i klartext");
            Settings::setChatProvider ("gemini");
            check (Settings::chatProvider() == "gemini", "leverantören sparas");
            check (Settings::geminiModel() == "gemini-3.6-flash", "standardmodellen för Gemini");
            Settings::setGeminiModel ("my-model");
            check (Settings::geminiModel() == "my-model", "egen Gemini-modell kan skrivas in");
            Settings::setGeminiModel ("gemini-3.6-flash");
            check (Settings::geminiToolsCheck ("m1", "keyA") == -1, "verktygsprov: inte provad än");
            Settings::setGeminiToolsCheck ("m1", "keyA", false);
            check (Settings::geminiToolsCheck ("m1", "keyA") == 0 && Settings::geminiToolsCheck ("m1", "keyB") == -1, "verktygsprov: sparas per nyckel");
            check (! settingsFile.loadFileAsString().contains ("keyA"), "verktygsprov: nyckeln sparas som hash");
            {
                const auto listed = GeminiApi::parseModelList (juce::JSON::parse (
                    R"({"models":[{"name":"models/gemini-3.6-flash","supportedGenerationMethods":["generateContent","countTokens"]},
                        {"name":"models/gemini-embedding-001","supportedGenerationMethods":["embedContent"]},
                        {"name":"models/gemini-3-flash-tts","supportedGenerationMethods":["generateContent"]},
                        {"name":"models/imagen-4","supportedGenerationMethods":["predict"]},
                        {"name":"models/gemini-3.5-flash-lite","supportedGenerationMethods":["generateContent"]}]})"));
                check (listed.size() == 2 && listed[0] == "gemini-3.6-flash" && listed[1] == "gemini-3.5-flash-lite", "models.list: bara textmodeller med generateContent, utan models/-prefix");
                check (GeminiApi::models() == GeminiApi::fallbackModels(), "modellista: reservlistan före hämtning");
                GeminiApi::setFetchedModels (listed);
                check (GeminiApi::models() == listed, "modellista: hämtad lista används");
                GeminiApi::setFetchedModels ({});
            }
            check (GeminiApi::errorMessage (404, {}).contains ("MODEL"), "Gemini: 404 pekar på modellvalet");

            const auto tools = ToolLayer::tools();
            juce::String error;

            const auto textReply = GeminiApi::parseResponse (juce::JSON::parse (
                R"({"candidates":[{"content":{"role":"model","parts":[{"thought":true,"text":"hmm"},{"text":"Hello there"}]},"finishReason":"STOP"}],
                    "usageMetadata":{"promptTokenCount":12,"candidatesTokenCount":3}})"), tools, error);
            check (error.isEmpty() && textReply["content"].size() == 1 && textReply["content"][0]["text"].toString() == "Hello there", "Gemini: textsvar tolkas (tankedelar hoppas över)");
            check (textReply["stop_reason"].toString() == "end_turn" && (int) textReply["usage"]["input_tokens"] == 12 && (int) textReply["usage"]["output_tokens"] == 3, "Gemini: stopporsak och tokens");

            const auto toolReply = GeminiApi::parseResponse (juce::JSON::parse (
                R"({"candidates":[{"content":{"role":"model","parts":[{"functionCall":{"name":"set_parameters","args":{"values":"{\"kickClick\":0.5}"}},"thoughtSignature":"sig123"}]},"finishReason":"STOP"}]})"), tools, error);
            const auto block = toolReply["content"][0];
            check (block["type"].toString() == "tool_use" && block["name"].toString() == "set_parameters" && toolReply["stop_reason"].toString() == "tool_use", "Gemini: verktygsanrop tolkas");
            check (block["input"]["values"].isObject() && (double) block["input"]["values"]["kickClick"] == 0.5, "Gemini: JSON-strängargumentet packas upp till ett objekt");

            // Samtalet tillbaka: tool_use -> functionCall (med signaturen), tool_result -> functionResponse med verktygsnamnet.
            juce::Array<juce::var> messages;
            auto userMsg = std::make_unique<juce::DynamicObject>();
            userMsg->setProperty ("role", "user");
            userMsg->setProperty ("content", "Tune the kick");
            messages.add (juce::var (userMsg.release()));
            auto assistantMsg = std::make_unique<juce::DynamicObject>();
            assistantMsg->setProperty ("role", "assistant");
            assistantMsg->setProperty ("content", toolReply["content"]);
            messages.add (juce::var (assistantMsg.release()));
            auto resultBlock = std::make_unique<juce::DynamicObject>();
            resultBlock->setProperty ("type", "tool_result");
            resultBlock->setProperty ("tool_use_id", block["id"]);
            resultBlock->setProperty ("content", "{\"ok\":true}");
            auto resultMsg = std::make_unique<juce::DynamicObject>();
            resultMsg->setProperty ("role", "user");
            resultMsg->setProperty ("content", juce::Array<juce::var> { juce::var (resultBlock.release()) });
            messages.add (juce::var (resultMsg.release()));
            const auto request = GeminiApi::buildRequest ("system text", tools, messages);
            const auto requestText = juce::JSON::toString (request, true);
            check (requestText.contains ("functionCall") && requestText.contains ("sig123"), "Gemini: anropet med thoughtSignature skickas tillbaka");
            check (request["contents"][2]["parts"][0]["functionResponse"]["name"].toString() == "set_parameters", "Gemini: tool_result blir functionResponse med namnet");
            check (! requestText.contains ("additionalProperties") && request["tools"].size() > 0, "Gemini: verktygsschemat är rensat");
            check (! request["contents"][1]["parts"][0]["functionCall"].hasProperty ("id") || request["contents"][1]["parts"][0]["functionCall"]["id"].toString() == block["id"].toString(), "Gemini: anrop utan eget id får inget påhittat id");
            {
                juce::String idError;
                const auto idReply = GeminiApi::parseResponse (juce::JSON::parse (R"({"candidates":[{"content":{"role":"model","parts":[{"functionCall":{"name":"set_parameters","args":{},"id":"call_42"}}]},"finishReason":"STOP"}]})"), tools, idError);
                check (idReply["content"][0]["id"].toString() == "call_42", "Gemini: anropets id behålls");
                juce::Array<juce::var> idMessages { messages[0], juce::var (new juce::DynamicObject()), juce::var (new juce::DynamicObject()) };
                idMessages.getReference (1).getDynamicObject()->setProperty ("role", "assistant");
                idMessages.getReference (1).getDynamicObject()->setProperty ("content", idReply["content"]);
                auto idResult = std::make_unique<juce::DynamicObject>();
                idResult->setProperty ("type", "tool_result");
                idResult->setProperty ("tool_use_id", "call_42");
                idResult->setProperty ("content", "{}");
                idMessages.getReference (2).getDynamicObject()->setProperty ("role", "user");
                idMessages.getReference (2).getDynamicObject()->setProperty ("content", juce::Array<juce::var> { juce::var (idResult.release()) });
                const auto idRequest = GeminiApi::buildRequest ("s", tools, idMessages);
                check (idRequest["contents"][1]["parts"][0]["functionCall"]["id"].toString() == "call_42" && idRequest["contents"][2]["parts"][0]["functionResponse"]["id"].toString() == "call_42", "Gemini: id skickas tillbaka i functionCall och functionResponse");
            }

            check (GeminiApi::parseResponse (juce::JSON::parse (R"({"promptFeedback":{"blockReason":"SAFETY"}})"), tools, error).isVoid() && error.contains ("SAFETY"), "Gemini: blockerat svar ger ett fel");
            check (GeminiApi::errorMessage (400, R"({"error":{"code":400,"message":"API key not valid. Please pass a valid API key.","status":"INVALID_ARGUMENT"}})").contains ("key"), "Gemini: ogiltig nyckel (400)");
            check (GeminiApi::errorMessage (403, R"({"error":{"code":403,"message":"denied","status":"PERMISSION_DENIED"}})").isNotEmpty(), "Gemini: 403");
            check (GeminiApi::errorMessage (429, R"({"error":{"code":429,"message":"Quota exceeded","status":"RESOURCE_EXHAUSTED"}})").containsIgnoreCase ("limit")
                       || GeminiApi::errorMessage (429, {}).containsIgnoreCase ("limit"), "Gemini: kvot (429) förklaras");
            check (GeminiApi::errorMessage (0, {}).isNotEmpty(), "Gemini: nätverksfel har ett meddelande");

            Settings::setChatProvider ("anthropic");
            check (Settings::chatProvider() == "anthropic", "leverantören kan växlas tillbaka");
            Settings::setGeminiApiKey ({});
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
        auto* cutoff = processor.apvts.getParameter (ParamID::bass (0, InstrumentField::cutoff));

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
            pattern.setGateStep (1, 3, false);
            pattern.addNote (2, 0, 24, 1, 0.8f);
            pattern.addNote (Pattern::kickLayer, 0, 24, 0, 1.0f);
            auto* gridParameter = processor.apvts.getParameter (ParamID::bass (1, InstrumentField::grid));
            auto* delayParameter = processor.apvts.getParameter (ParamID::bass (1, InstrumentField::delay));
            gridParameter->setValueNotifyingHost (gridParameter->convertTo0to1 ((float) (int) InstrumentGrid::eighthTriplet));
            delayParameter->setValueNotifyingHost (delayParameter->convertTo0to1 (12.0f));

            const auto saved = processor.saveUserPreset (Presets::Section::pattern, rowName, 1);
            const auto rowFile = Presets::folder (Presets::Section::pattern, false).getChildFile (rowName + ".mofp");
            const auto listed = processor.getUserPresets (Presets::Section::pattern, 1).contains (rowFile);
            const auto listedForKick = processor.getUserPresets (Presets::Section::pattern, Pattern::kickLayer).contains (rowFile);
            const auto intoKick = processor.loadUserPreset (Presets::Section::pattern, rowFile, Pattern::kickLayer); // en instrumentrad får inte in i kicken

            const auto loadedRow = processor.loadUserPreset (Presets::Section::pattern, rowFile, 4);
            const auto& note = pattern.note (4, 192);
            auto* otherGrid = processor.apvts.getParameter (ParamID::bass (4, InstrumentField::grid));
            const auto movedGrid = (int) std::lround (otherGrid->convertFrom0to1 (otherGrid->getValue()));
            const auto movedDelay = processor.apvts.getParameter (ParamID::bass (4, InstrumentField::delay))->convertFrom0to1 (processor.apvts.getParameter (ParamID::bass (4, InstrumentField::delay))->getValue());
            const auto rowOk = saved && listed && ! listedForKick && ! intoKick && loadedRow
                            && pattern.note (4, 96).on.load() && pattern.note (4, 96).pitch.load() == 7
                            && note.on.load() && note.slide.load() && note.pitch.load() == -5 && note.length.load() == 48
                            && pattern.isCustom (4) && ! pattern.isGateStepOn (4, 3)
                            && movedGrid == (int) InstrumentGrid::eighthTriplet && std::abs (movedDelay - 12.0f) < 0.5f
                            && pattern.note (2, 0).on.load() && pattern.note (Pattern::kickLayer, 0).on.load()
                            && processor.getPresetName (Presets::Section::pattern, 4) == rowName;
            std::printf ("  saved %d listed %d listedForKick %d intoKick %d loaded %d n96 %d/%d n192 %d slide %d pitch %d len %d custom %d gate3 %d grid %d(%d) delay %.1f other %d kick %d name '%s'\n",
                         saved, listed, listedForKick, intoKick, loadedRow, (int) pattern.note (4, 96).on.load(), pattern.note (4, 96).pitch.load(), (int) note.on.load(), (int) note.slide.load(), note.pitch.load(),
                         note.length.load(), (int) pattern.isCustom (4), (int) pattern.isGateStepOn (4, 3), movedGrid, (int) InstrumentGrid::eighthTriplet, movedDelay,
                         (int) pattern.note (2, 0).on.load(), (int) pattern.note (Pattern::kickLayer, 0).on.load(), processor.getPresetName (Presets::Section::pattern, 4).toRawUTF8());
            rowFile.deleteFile();
            std::printf ("mönster-preset (en rad): " "%s" "\n", rowOk ? "ok" : "FEL");
            ok = ok && rowOk;
        }

        // FX Preset: en enda effektkanal sparas och laddas i en annan; den första rörs inte.
        {
            const auto fxName = juce::String ("__fxpresettest__");
            Effects::setSlotType (processor.apvts, 3, 0, EffectType::ducker);
            auto* hp = processor.apvts.getParameter (Effects::channelParamID (3, "EqHp"));
            hp->setValueNotifyingHost (hp->convertTo0to1 (90.0f));
            const auto savedFx = processor.saveUserPreset (Presets::Section::effects, fxName, 3);
            const auto fxFile = Presets::folder (Presets::Section::effects, false).getChildFile (fxName + ".mofp");
            const auto listedFx = processor.getUserPresets (Presets::Section::effects, 3).contains (fxFile);
            Effects::setSlotType (processor.apvts, 3, 0, EffectType::off);
            hp->setValueNotifyingHost (hp->convertTo0to1 (30.0f));
            const auto loadedFx = processor.loadUserPreset (Presets::Section::effects, fxFile, 7);
            auto* hp7 = processor.apvts.getParameter (Effects::channelParamID (7, "EqHp"));
            const auto movedType = Effects::slotType (processor.apvts, 7, 0) == EffectType::ducker;
            const auto movedHp = std::abs (hp7->convertFrom0to1 (hp7->getValue()) - 90.0f) < 1.0f;
            const auto untouched = Effects::slotType (processor.apvts, 3, 0) == EffectType::off
                                && std::abs (hp->convertFrom0to1 (hp->getValue()) - 30.0f) < 1.0f;
            const auto fxOk = savedFx && listedFx && loadedFx && movedType && movedHp && untouched
                           && processor.getPresetName (Presets::Section::effects, 7) == fxName;
            fxFile.deleteFile();
            std::printf ("FX-preset (en kanal): %s" "\n", fxOk ? "ok" : "FEL");
            ok = ok && fxOk;
        }

        // Länkade presets: instrument -> FX-preset (första lediga kanal, val när alla är upptagna, saknad länk) och mönster -> instrument.
        {
            auto setPlain = [&processor] (const juce::String& id, float value)
            {
                auto* parameter = processor.apvts.getParameter (id);
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            auto plain = [&processor] (const juce::String& id)
            {
                auto* parameter = processor.apvts.getParameter (id);
                return parameter->convertFrom0to1 (parameter->getValue());
            };
            const auto fxLink = juce::String ("__linkfx__"), instLink = juce::String ("__linkinst__"), patLink = juce::String ("__linkpat__");
            const auto fxLinkFile = processor.findUserPreset (Presets::Section::effects, fxLink);
            const auto instLinkFile = processor.findUserPreset (Presets::Section::bass, instLink);
            const auto patLinkFile = processor.findUserPreset (Presets::Section::pattern, patLink);

            // Instrument 1 skickar 60 % till FX3, som har en effekt.
            Effects::setSlotType (processor.apvts, 2, 0, EffectType::ducker);
            setPlain (ParamID::bass (0, InstrumentField::fxChannel), 3.0f);
            setPlain (ParamID::bass (0, InstrumentField::fxAmount), 0.6f);
            setPlain (ParamID::bass (0, InstrumentField::cutoff), 1234.0f);
            const auto savedLinked = processor.saveInstrumentPreset (Presets::Section::bass, 0, instLink, fxLink);
            const auto link = PluginBase::readPresetLink (instLinkFile);
            const auto linkOk = link.kind == "effect" && link.name == fxLink && std::abs (link.amount - 0.6f) < 0.01f;

            // Tillbaka till ett tomt läge, men FX1 är upptagen.
            Effects::setSlotType (processor.apvts, 2, 0, EffectType::off);
            setPlain (ParamID::bass (0, InstrumentField::fxChannel), 0.0f);
            setPlain (ParamID::bass (0, InstrumentField::fxAmount), 0.0f);
            Effects::setSlotType (processor.apvts, 0, 0, EffectType::ducker);
            const auto free1 = processor.firstFreeFxChannel() == 1;
            const auto linked = processor.loadInstrumentPreset (Presets::Section::bass, 0, instLinkFile);
            const auto autoOk = linked.ok && linked.fxChannel == 1 && Effects::slotType (processor.apvts, 1, 0) == EffectType::ducker
                             && (int) plain (ParamID::bass (0, InstrumentField::fxChannel)) == 2 && std::abs (plain (ParamID::bass (0, InstrumentField::fxAmount)) - 0.6f) < 0.01f
                             && std::abs (plain (ParamID::bass (0, InstrumentField::cutoff)) - 1234.0f) < 1.0f;

            // Alla kanaler upptagna: utan val hoppas effekten över, med val ersätts den valda.
            for (int channel = 0; channel < numEffectChannels; ++channel)
                Effects::setSlotType (processor.apvts, channel, 0, EffectType::ducker);
            const auto none = processor.firstFreeFxChannel() == -1;
            const auto skipped = processor.loadInstrumentPreset (Presets::Section::bass, 0, instLinkFile, -2);
            const auto replaced = processor.loadInstrumentPreset (Presets::Section::bass, 0, instLinkFile, 5);
            const auto fullOk = none && skipped.ok && skipped.fxChannel == -1 && replaced.fxChannel == 5 && (int) plain (ParamID::bass (0, InstrumentField::fxChannel)) == 6;

            // Mönster som länkar instrumentet: med och utan "behåll mitt instrument".
            processor.pattern.addNote (0, 0, 24);
            const auto savedPattern = processor.savePatternPreset (0, patLink, instLink, fxLink);
            const auto patternLink = PluginBase::readPresetLink (patLinkFile);
            setPlain (ParamID::bass (0, InstrumentField::cutoff), 4321.0f);
            const auto keep = processor.loadPatternPreset (0, patLinkFile, false);
            const auto keepOk = keep.ok && std::abs (plain (ParamID::bass (0, InstrumentField::cutoff)) - 4321.0f) < 1.0f;
            const auto replace = processor.loadPatternPreset (0, patLinkFile, true, -1);
            const auto replaceOk = replace.ok && std::abs (plain (ParamID::bass (0, InstrumentField::cutoff)) - 1234.0f) < 1.0f;

            // Saknad länk: instrumentet laddas utan effekt, mönstret utan instrument.
            fxLinkFile.deleteFile();
            const auto noEffect = processor.loadInstrumentPreset (Presets::Section::bass, 0, instLinkFile, -2);
            instLinkFile.deleteFile();
            const auto noInstrument = processor.loadPatternPreset (0, patLinkFile, true);

            const auto linkedOk = savedLinked && linkOk && free1 && autoOk && fullOk && savedPattern && patternLink.kind == "instrument" && patternLink.name == instLink
                               && keepOk && replaceOk && noEffect.ok && noEffect.effectMissing && noInstrument.ok && noInstrument.instrumentMissing;
            patLinkFile.deleteFile();
            for (int channel = 0; channel < numEffectChannels; ++channel)
                Effects::setSlotType (processor.apvts, channel, 0, EffectType::off);
            std::printf ("Länkade presets: %s" "\n", linkedOk ? "ok" : "FEL");
            if (! linkedOk)
                std::printf ("  saved %d link %d free1 %d auto %d full %d pat %d keep %d replace %d noEffect %d/%d noInst %d/%d" "\n", savedLinked, linkOk, free1, autoOk, fullOk,
                             savedPattern, keepOk, replaceOk, noEffect.ok, noEffect.effectMissing, noInstrument.ok, noInstrument.instrumentMissing);
            ok = ok && linkedOk;

        // OSC-preset (fas S5): OSC 2 i lager 0 sparas och laddas; OSC 1, filtret och OSC 2:s av/på rörs inte; RESET ger standardvärdena.
        {
            using F = InstrumentField;
            auto get = [&] (F field) { auto* q = processor.apvts.getParameter (ParamID::bass (0, field)); return q->convertFrom0to1 (q->getValue()); };
            Scenarios::setInstrument (processor, 0, F::bPos, 0.6f);
            Scenarios::setInstrument (processor, 0, F::bSemi, 7.0f);
            Scenarios::setInstrument (processor, 0, F::bUnison, 5.0f);
            Scenarios::setInstrument (processor, 0, F::bTable, 3.0f);
            Scenarios::setInstrument (processor, 0, F::bOn, 1.0f);
            Scenarios::setInstrument (processor, 0, F::aPos, 0.25f);
            const auto oscFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("__osctest__/Test/osc.oscpreset");
            auto saved = OscPresets::save (processor.apvts, 0, 1, oscFile);
            Scenarios::setInstrument (processor, 0, F::bPos, 0.1f);
            Scenarios::setInstrument (processor, 0, F::bSemi, -5.0f);
            Scenarios::setInstrument (processor, 0, F::bTable, 1.0f);
            Scenarios::setInstrument (processor, 0, F::bOn, 0.0f);
            Scenarios::setInstrument (processor, 0, F::aPos, 0.9f);
            const auto cutoffBefore = get (F::cutoff);
            const auto loadedOsc = OscPresets::load (processor.apvts, 0, 1, oscFile);
            const auto restored = std::abs (get (F::bPos) - 0.6f) < 0.01f && std::abs (get (F::bSemi) - 7.0f) < 0.01f
                                  && std::abs (get (F::bUnison) - 5.0f) < 0.01f && (int) get (F::bTable) == 3;
            const auto untouched = std::abs (get (F::aPos) - 0.9f) < 0.01f && get (F::bOn) < 0.5f && std::abs (get (F::cutoff) - cutoffBefore) < 0.01f;
            // Ett fält som filen saknar lämnas orört, och en okänd tabell ändrar inte tabellen.
            Scenarios::setInstrument (processor, 0, F::bSemi, 2.0f);
            oscFile.replaceWithText ("<KickbaseOsc version=\"1\" Table=\"__finns_inte__\" Pos=\"0.3\"/>");
            const auto partial = OscPresets::load (processor.apvts, 0, 1, oscFile)
                                 && std::abs (get (F::bPos) - 0.3f) < 0.01f && std::abs (get (F::bSemi) - 2.0f) < 0.01f && (int) get (F::bTable) == 3;
            const auto listed = OscPresets::list().contains (oscFile) == (OscPresets::folder() == oscFile.getParentDirectory().getParentDirectory().getParentDirectory());
            OscPresets::reset (processor.apvts, 0, 1);
            const auto resetOk = std::abs (get (F::bSemi)) < 0.01f && (int) get (F::bUnison) == 1 && get (F::bOn) < 0.5f && std::abs (get (F::aPos) - 0.9f) < 0.01f;
            oscFile.getParentDirectory().getParentDirectory().deleteRecursively();
            std::printf ("OSC-preset: sparad %d, laddad %d, återställd %d, orört %d, delvis %d, reset %d\n",
                         saved ? 1 : 0, loadedOsc ? 1 : 0, restored ? 1 : 0, untouched ? 1 : 0, partial ? 1 : 0, resetOk ? 1 : 0);
            (void) listed;
            ok = ok && saved && loadedOsc && restored && untouched && partial && resetOk;
        }
        }

        std::puts (ok ? "Presettest OK" : "FEL: presettest");
        return ok ? 0 : 1;
    }

    int snapshot (const juce::String& kind, const juce::File& outFile, int tab, bool withModulation)
    {
        juce::ScopedJuceInitialiser_GUI gui;

        // KB_LIBRARYDIR=<mapp>: skärmbilden använder den mappen som bibliotek, i en tillfällig inställningsfil (användarens inställningar rörs aldrig).
        std::unique_ptr<Scenarios::TemporaryUserFolders> isolatedFolders;
        if (const auto libraryDir = juce::SystemStats::getEnvironmentVariable ("KB_LIBRARYDIR", {}); libraryDir.isNotEmpty())
        {
            isolatedFolders = std::make_unique<Scenarios::TemporaryUserFolders>();
            Settings::setFolder ("libraryFolder", juce::File::getCurrentWorkingDirectory().getChildFile (libraryDir));
        }

        auto processor = createProcessor (kind);
        processor->kickTab = juce::SystemStats::getEnvironmentVariable ("KB_KICKTAB", "0").getIntValue() != 0 ? 1 : 0; // KICK-flikens vy: 1 = SAMPLE
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
            AiChat::Entry demoAnswer;
            demoAnswer.kind = Kind::assistant;
            demoAnswer.text = "I will shorten the kick's tail a little, add a short click on top, and give the bass a faster sidechain so it gets out of the way. Let me first look at the current sound.";
            demoAnswer.feedbackAvailable = true;
            demoAnswer.feedbackId = "screenshot-demo";
            demoAnswer.feedbackAction = "set_parameters: kickClick, kickLength";
            chat.addEntry (std::move (demoAnswer));
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
        // KB_LFOS=<mask>: aktiva LFO:er (fas M, 1023 = alla 10). KB_LFOFOLD=<mask>: fällda kort. KB_LFOCONFIRM=<n>: LFO n (1-10) frågar om
        // borttagning (med `mod` styr LFO 1-3 rattar).
        if (const auto lfos = juce::SystemStats::getEnvironmentVariable ("KB_LFOS", {}); lfos.isNotEmpty())
            for (int i = 0; i < Modulation::maxLfos; ++i)
                processor->setLfoActive (i, ((lfos.getIntValue() >> i) & 1) != 0);
        if (const auto folded = juce::SystemStats::getEnvironmentVariable ("KB_LFOFOLD", {}); folded.isNotEmpty())
            processor->lfoFolded = folded.getIntValue();
        processor->lfoRemoveAsk = juce::SystemStats::getEnvironmentVariable ("KB_LFOCONFIRM", "0").getIntValue() - 1;
        processor->selectedInstrument = juce::SystemStats::getEnvironmentVariable ("KB_INSTRUMENT", "0").getIntValue(); // valt instrument (0-9)
        processor->selectedInstrumentSource[(size_t) juce::jlimit (0, 9, processor->selectedInstrument)] =
            juce::SystemStats::getEnvironmentVariable ("KB_SOURCETAB", "0").getIntValue(); // vald flik i instrumentet (0 = INSTR, 1-3 = OSC, 4 = SAMPLE, 5 = FILTER)
        if (juce::SystemStats::getEnvironmentVariable ("KB_FILTER2", "0").getIntValue() != 0) // FILTER-fliken med Filter 2 på (Formant) och ett Ladder-lågpass i Filter 1
        {
            auto set = [&processor] (const char* id, float value)
            {
                if (auto* p = processor->apvts.getParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));
            };
            set ("bass1FilterModel", 2.0f);
            set ("bass1FilterType", 0.0f);
            set ("bass1Cutoff", 900.0f);
            set ("bass1Resonance", 0.55f);
            set ("bass1FilterP1", 0.4f);
            set ("bass1Filter2On", 1.0f);
            set ("bass1Filter2Model", 0.0f);
            set ("bass1Filter2Type", 3.0f);
            set ("bass1Filter2Cutoff", 150.0f);
            set ("bass1Filter2Reso", 0.3f);
        }
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

        // Biblioteket skannas på en bakgrundstråd: vänta tills listan finns, så att den syns på bilden (editorn läser den när den skapas).
        if (isolatedFolders != nullptr) // (bara med KB_LIBRARYDIR: övriga bilder ska ta samma väg som förut)
        {
            juce::SharedResourcePointer<LibraryIndex> libraryIndex;
            libraryIndex->refreshIfChanged (Settings::libraryFolder());
            libraryIndex->waitUntilIdle();
        }

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
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

        // KB_MENU=<instrument 0-9>: ritar instrumentmenyn (L4) ovanpå bilden. Popup-fönstret är ett eget fönster, så det hämtas från Desktop.
        if (const auto menuLayer = juce::SystemStats::getEnvironmentVariable ("KB_MENU", {}); menuLayer.isNotEmpty())
        {
            InstrumentMenu::show (*processor, menuLayer.getIntValue(), *editor, juce::Rectangle<int> (60, 120, 2, 2));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
            juce::Graphics g (image);
            for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
                if (auto* top = juce::Desktop::getInstance().getComponent (i); top != nullptr && top != editor.get() && top->isShowing())
                    g.drawImageAt (top->createComponentSnapshot (top->getLocalBounds(), true, 1.0f), top->getScreenX() - editor->getScreenX(), top->getScreenY() - editor->getScreenY());
        }

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

int profileCommand (const juce::String& scenario, int instruments, double seconds); // Tools/Profile.cpp
int uiProfileCommand (int tab, const juce::String& scenario, int instruments, double seconds); // Tools/Profile.cpp
int renderSignaturesWide(); // Tools/AudioTests.cpp
int audioThreadTest();      // Tools/AudioTests.cpp
int blockSizeTest();        // Tools/AudioTests.cpp
int polyTest();             // Tools/AudioTests.cpp
int persistTest();          // Tools/StateTests.cpp
int fullStateTest();        // Tools/StateTests.cpp
int renderToolTest();       // Tools/StateTests.cpp
int anTest();               // Tools/AnTests.cpp
int rowKeyTest();           // Tools/RowTests.cpp
int helpCheck();            // Tools/StateTests.cpp
int paramTest();            // Tools/ParamSweepTests.cpp
int libraryCheck (const juce::String& folder); // Tools/LibraryTests.cpp
int libraryTest();          // Tools/LibraryTests.cpp
int libraryImport (const juce::StringArray& args); // Tools/LibraryTests.cpp
int freesoundMain (const juce::StringArray& args); // Tools/Freesound.cpp (bara verktyget: inget Freesound i pluginet)
int freesoundTest();        // Tools/Freesound.cpp
int tb303Measure();         // Tools/Tb303Tests.cpp
int tb303Unit();            // Tools/Tb303Unit.cpp
int filterModelTest();      // Tools/FilterModelTests.cpp
int filter2Test();          // Tools/AudioTests.cpp
int tb303Test();            // Tools/Tb303Tests.cpp
int boundaryTest();         // Tools/PatternTests.cpp
int includeCheck (const juce::String& root); // Tools/Tb303Unit.cpp
int renderSignaturesTb303(); // Tools/AudioTests.cpp

// Verktygets kommandon. Startas av KickbaseMcp.exe när det heter KickbaseTool.exe eller får `tool` som första argument.
int toolMain (int argc, char* argv[])
{
    // Testerna och mätningarna kör mot Init från koden (reproducerbart startläge). Skärmbilder, rendering, export, reparation av presets och
    // `rendersigreal` använder det riktiga Init (den inbakade filen).
    {
        const juce::String command (argc > 1 ? argv[1] : "");
        PluginBase::preferCodeInit = ! (command == "snapshot" || command == "render" || command == "export" || command == "repairpresets" || command == "rendersigreal" || command == "profile" || command == "uiprofile");
    }

    // Testerna som sparar presets eller rullar körs i en tillfällig mapp, aldrig i användarens Dokument\Kickbase.
    std::unique_ptr<Scenarios::TemporaryUserFolders> isolation;
    if (const juce::String command (argc > 1 ? argv[1] : ""); command == "presettest" || command == "rolltest" || command == "lengthtest"
        || command == "instrumenttest" || command == "eqtest" || command == "listentest" || command == "auditiontest" || command == "audiotest"
        || command == "analysetest" || command == "perftest" || command == "polytest" || command == "livetest" || command == "rendersig" || command == "rendersigwide" || command == "rendersigtb303")
        isolation = std::make_unique<Scenarios::TemporaryUserFolders>();

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
    if (argc == 2 && juce::String (argv[1]) == "matrixtest")
        return matrixTest();
    if (argc == 2 && juce::String (argv[1]) == "lengthtest")
        return lengthTest();
    if (argc == 2 && juce::String (argv[1]) == "livetest")
        return liveCodeTest();
    if (argc == 3 && juce::String (argv[1]) == "logoimage")
        return logoImage (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    if (argc == 2 && (juce::String (argv[1]) == "rendersig" || juce::String (argv[1]) == "rendersigreal"))
        return renderSignatures();
    if (argc >= 2 && juce::String (argv[1]) == "repairpresets")
        return repairPresets (argc > 2 && juce::String (argv[2]) == "apply");
    if (argc == 2 && juce::String (argv[1]) == "listentest")
        return listenTest();
    if (argc == 2 && juce::String (argv[1]) == "eqtest")
        return eqTest();
    if (argc == 2 && juce::String (argv[1]) == "inittest")
        return initTest();
    if (argc >= 2 && juce::String (argv[1]) == "analysetest")
        return analyseTest (argc > 2 ? juce::String (argv[2]).getIntValue() : 1);
    if (argc == 2 && juce::String (argv[1]) == "rolltest")
        return rollLengthTest (true);

    if (argc == 2 && juce::String (argv[1]) == "helpcheck")
        return helpCheck();
    if (argc == 2 && juce::String (argv[1]) == "persisttest")
        return persistTest();
    if (argc == 2 && juce::String (argv[1]) == "fulltest")
        return fullStateTest();
    if (argc == 2 && juce::String (argv[1]) == "rendertooltest")
        return renderToolTest();
    if (argc == 2 && juce::String (argv[1]) == "antest")
        return anTest();
    if (argc == 2 && juce::String (argv[1]) == "rowkeytest")
        return rowKeyTest();
    if (argc == 2 && juce::String (argv[1]) == "paramtest")
        return paramTest();
    // librarycheck [mapp]: validerar biblioteksmanifesten (läser bara). librarytest: reglerna, i en tillfällig mapp.
    if (argc >= 2 && juce::String (argv[1]) == "librarycheck")
        return libraryCheck (argc > 2 ? juce::String (argv[2]) : juce::String());
    if (argc == 2 && juce::String (argv[1]) == "librarytest")
        return libraryTest();
    if (argc == 2 && juce::String (argv[1]) == "rendersigtb303")
        return renderSignaturesTb303();
    if (argc >= 2 && juce::String (argv[1]) == "includecheck")
        return includeCheck (argc > 2 ? juce::String (argv[2]) : juce::String());
    if (argc == 2 && juce::String (argv[1]) == "boundarytest")
        return boundaryTest();
    if (argc == 2 && juce::String (argv[1]) == "tb303test")
        return tb303Test();
    if (argc == 2 && juce::String (argv[1]) == "tb303unit")
        return tb303Unit();
    if (argc == 2 && juce::String (argv[1]) == "filtermodels")
        return filterModelTest();
    if (argc == 2 && juce::String (argv[1]) == "filter2test")
        return filter2Test();
    if (argc == 2 && juce::String (argv[1]) == "tb303measure")
        return tb303Measure();
    if (argc == 2 && juce::String (argv[1]) == "freesoundtest")
        return freesoundTest();
    if (argc >= 2 && juce::String (argv[1]) == "freesound")
    {
        juce::StringArray args;
        for (int i = 2; i < argc; ++i)
            args.add (argv[i]);
        return freesoundMain (args);
    }
    if (argc >= 2 && juce::String (argv[1]) == "importlibrary")
    {
        juce::ScopedJuceInitialiser_GUI gui; // (ljudformaten)
        juce::StringArray args;
        for (int i = 2; i < argc; ++i)
            args.add (argv[i]);
        return libraryImport (args);
    }
    if (argc == 2 && juce::String (argv[1]) == "rendersigwide")
        return renderSignaturesWide();
    if (argc == 2 && juce::String (argv[1]) == "blocksizetest")
        return blockSizeTest();
    if (argc == 2 && juce::String (argv[1]) == "polytest")
        return polyTest();
    if (argc == 2 && juce::String (argv[1]) == "audiotest")
        return audioThreadTest();
    // uiprofile <flik> <init|heavy> [instrument] [sekunder]: gränssnittets CPU och var meddelandetråden lägger tiden.
    if (argc >= 4 && juce::String (argv[1]) == "uiprofile")
        return uiProfileCommand (juce::String (argv[2]).getIntValue(), juce::String (argv[3]), argc > 4 ? juce::String (argv[4]).getIntValue() : 3,
                                 argc > 5 ? juce::String (argv[5]).getDoubleValue() : 6.0);
    // profile <init|heavy|fx> [instrument] [sekunder]: var tiden går (samplande profilerare, Tools/Profile.cpp).
    if (argc >= 3 && juce::String (argv[1]) == "profile")
        return profileCommand (juce::String (argv[2]), argc > 3 ? juce::String (argv[3]).getIntValue() : 10, argc > 4 ? juce::String (argv[4]).getDoubleValue() : 5.0);

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
                   "  KickbaseTool render   instrument|fx <ut.wav> [bpm] [takter] [ton]\n"
                   "  KickbaseTool snapshot instrument|fx <ut.png>");
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
