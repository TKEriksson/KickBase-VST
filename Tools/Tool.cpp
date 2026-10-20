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

#include "../Source/Core/MixAnalysis.h"
#include "../Source/Ui/LogoPainter.h"
#include "../Source/Ui/InstrumentMenu.h"
#include "../Source/Ui/ListenButton.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/Fx/FxProcessor.h"
#include "../Source/Core/OfflineRender.h"
#include "../Source/Core/ToolLayer.h"
#include "../Source/Core/LiveCode.h"
#include "../Source/Ui/LiveCodePage.h"
#include "../Source/Core/AiChat.h"
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
                    if (b->getButtonText() == "LIVE") play = b;
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

                // Projekt/preset laddas medan LIVE spelar: LIVE stoppas, den nya koden tas in, och gammal kod skriver inte över det nya mönstret.
                {
                    stop->triggerClick();
                    runFor (100);
                    play->triggerClick();
                    runFor (600);
                    check (play->getToggleState(), "laddning under LIVE: LIVE spelar före laddningen");
                    juce::ValueTree loaded ("LiveCode");
                    loaded.setProperty ("code", "note(\"a2\").s(\"i1\")", nullptr);
                    loaded.setProperty ("locks", 0, nullptr);
                    processor.liveCodeFromValueTree (loaded);
                    ++processor.liveCodeLoads;
                    runFor (700);
                    check (! play->getToggleState(), "laddning under LIVE: LIVE stoppas");
                    check (code->getDocument().getAllContent() == "note(\"a2\").s(\"i1\")", "laddning under LIVE: den nya koden visas");
                    pattern.clearNotes (0);
                    runFor (1200);
                    check (pattern.stackSize (0, 0) == 0, "laddning under LIVE: gammal kod skriver inte över mönstret efteråt");
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

            auto described = false;
            for (auto& t : ToolLayer::tools())
                if (t.name == "live_code_create")
                    described = t.description.contains ("overwrite") && t.description.contains ("live_code_lock") && t.description.contains ("$:");
            check (described, "live_code_create beskriver overwrite, live_code_lock och $:");
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
int helpCheck();            // Tools/StateTests.cpp
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
    if (argc >= 2 && juce::String (argv[1]) == "analysetest")
        return analyseTest (argc > 2 ? juce::String (argv[2]).getIntValue() : 1);
    if (argc == 2 && juce::String (argv[1]) == "rolltest")
        return rollLengthTest (true);

    if (argc == 2 && juce::String (argv[1]) == "helpcheck")
        return helpCheck();
    if (argc == 2 && juce::String (argv[1]) == "persisttest")
        return persistTest();
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
