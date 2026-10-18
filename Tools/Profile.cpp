// En enkel samplande profilerare för verktyget. En tråd stoppas ungefär 1000 gånger per sekund och dess position (instruktionspekaren och
// anropsstacken) läses. Adresserna slås upp i PDB-filen (DbgHelp), så att resultatet visas per funktion (själv och inklusive anropade) och
// per källrad.
//
//   tool profile <init|heavy|fx> [instrument] [sekunder]
//       ljudtråden: en tråd spelar pluginet block för block så fort den kan.
//   tool uiprofile <flik> <init|heavy> [instrument] [sekunder]
//       gränssnittet: editorn öppnas i ett riktigt fönster på fliken (samma siffror som snapshot, t.ex. 7 = ANALYZER) medan ljudet spelas
//       i realtid i en annan tråd. Visar hur mycket CPU meddelandetråden och ljudtråden använder och var meddelandetråden lägger tiden.
//       KB_SPECTRUM=1: live-spektrumet på. KB_HARMONICS=1: övertonerna på. KB_SPECTRUM_FPS=1-5 och KB_SPECTRUM_FILL=0|1: spektrumets bildtakt
//       och fyllning (SETTINGS > OPTIMIZATIONS), satta i en tillfällig inställningsfil.

#include "../Source/Instrument/InstrumentProcessor.h"
#include "../Source/Fx/FxProcessor.h"
#include "Scenarios.h"
#include "../Source/Core/MixAnalysis.h"
#include <thread>
#include <map>
#include <set>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment (lib, "dbghelp.lib")
#endif

#if JUCE_WINDOWS
namespace
{
    constexpr double rate = 48000.0;
    constexpr int block = 512;

    // Samlar prov från en tråd och skriver ut var tiden gick.
    class Sampler
    {
    public:
        Sampler() : process (GetCurrentProcess())
        {
            SymSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
            SymInitialize (process, nullptr, TRUE);
        }
        ~Sampler() { SymCleanup (process); }

        // Medan tråden är stoppad får provtagaren inte allokera eller ta lås som tråden kan hålla (heapen): stacken rullas upp med
        // RtlLookupFunctionEntry/RtlVirtualUnwind in i en fast buffert, och allt annat görs efter att tråden släppts.
        void sample (HANDLE thread)
        {
            constexpr int maxDepth = 48;
            DWORD64 addresses[maxDepth];
            auto depth = 0;
            if (SuspendThread (thread) == (DWORD) -1)
                return;
            CONTEXT context {};
            context.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext (thread, &context))
            {
                while (depth < maxDepth && context.Rip != 0)
                {
                    addresses[depth++] = context.Rip;
                    DWORD64 imageBase = 0;
                    auto* function = RtlLookupFunctionEntry (context.Rip, &imageBase, nullptr);
                    if (function == nullptr)
                    {
                        // En lövfunktion utan upprullningsdata: återhoppsadressen ligger överst på stacken.
                        context.Rip = *reinterpret_cast<DWORD64*> (context.Rsp);
                        context.Rsp += 8;
                        continue;
                    }
                    void* handlerData = nullptr;
                    DWORD64 establisherFrame = 0;
                    RtlVirtualUnwind (UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData, &establisherFrame, nullptr);
                }
            }
            ResumeThread (thread);
            if (depth == 0)
                return;
            ++selfCounts[addresses[0]];
            stacks.emplace_back (addresses, addresses + depth);
        }

        void report (int selfLines, int inclusiveLines, int sourceLines)
        {
            std::map<juce::String, int> self, inclusive, lines;
            auto total = 0;
            for (const auto& [address, count] : selfCounts)
            {
                self[functionOf (address)] += count;
                lines[lineOf (address) + "  " + functionOf (address).substring (0, 60)] += count;
                total += count;
            }
            for (const auto& stack : stacks)
            {
                std::set<juce::String> seen;
                for (auto address : stack)
                    if (seen.insert (functionOf (address)).second)
                        ++inclusive[functionOf (address)];
            }

            auto printTop = [total] (const char* title, const std::map<juce::String, int>& counts, int howMany)
            {
                std::vector<std::pair<int, juce::String>> sorted;
                for (const auto& [name, count] : counts)
                    sorted.push_back ({ count, name });
                std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.first > b.first; });
                std::printf ("\n%s\n", title);
                for (int i = 0; i < std::min (howMany, (int) sorted.size()); ++i)
                    std::printf ("%6.2f %%  %s\n", 100.0 * sorted[(size_t) i].first / std::max (1, total), sorted[(size_t) i].second.substring (0, 150).toRawUTF8());
            };
            std::printf ("%d prov\n", total);
            printTop ("Själv (där tiden faktiskt går):", self, selfLines);

            // Vem som anropar de dyraste funktionerna (första ramen utanför funktionen själv), med källrad.
            {
                std::vector<std::pair<int, juce::String>> sorted;
                for (const auto& [name, count] : self)
                    sorted.push_back ({ count, name });
                std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.first > b.first; });
                std::printf ("\nAnropas från (de tio dyraste):\n");
                for (int i = 0; i < std::min (10, (int) sorted.size()); ++i)
                {
                    std::map<juce::String, int> callers;
                    for (const auto& stack : stacks)
                    {
                        if (stack.empty() || functionOf (stack[0]) != sorted[(size_t) i].second)
                            continue;
                        for (size_t f = 1; f < stack.size(); ++f)
                            if (functionOf (stack[f]) != sorted[(size_t) i].second)
                            {
                                ++callers[functionOf (stack[f]).substring (0, 70) + " (" + lineOf (stack[f]) + ")"];
                                break;
                            }
                    }
                    std::vector<std::pair<int, juce::String>> top;
                    for (const auto& [name, count] : callers)
                        top.push_back ({ count, name });
                    std::sort (top.begin(), top.end(), [] (auto& a, auto& b) { return a.first > b.first; });
                    std::printf ("  %s:\n", sorted[(size_t) i].second.substring (0, 70).toRawUTF8());
                    for (int c = 0; c < std::min (3, (int) top.size()); ++c)
                        std::printf ("      %5.1f %%  %s\n", 100.0 * top[(size_t) c].first / std::max (1, total), top[(size_t) c].second.toRawUTF8());
                }
            }
            printTop ("Inklusive anropade funktioner:", inclusive, inclusiveLines);
            printTop ("Källrader:", lines, sourceLines);
        }

    private:
        juce::String functionOf (DWORD64 address)
        {
            if (auto found = functionCache.find (address); found != functionCache.end())
                return found->second;
            alignas (SYMBOL_INFO) char storage[sizeof (SYMBOL_INFO) + 512] {};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*> (storage);
            symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
            symbol->MaxNameLen = 511;
            DWORD64 displacement = 0;
            juce::String name = SymFromAddr (process, address, &displacement, symbol) ? juce::String (symbol->Name) : juce::String ("?");
            functionCache[address] = name;
            return name;
        }

        juce::String lineOf (DWORD64 address)
        {
            IMAGEHLP_LINE64 line {};
            line.SizeOfStruct = sizeof (line);
            DWORD displacement = 0;
            if (SymGetLineFromAddr64 (process, address, &displacement, &line))
                return juce::File (juce::String (line.FileName)).getFileName() + ":" + juce::String ((int) line.LineNumber);
            return "?";
        }

        HANDLE process;
        std::map<DWORD64, int> selfCounts;
        std::vector<std::vector<DWORD64>> stacks;
        std::map<DWORD64, juce::String> functionCache;
    };

    double cpuSecondsOf (HANDLE thread)
    {
        FILETIME created, exited, kernel, user;
        if (! GetThreadTimes (thread, &created, &exited, &kernel, &user))
            return 0.0;
        auto toSeconds = [] (FILETIME t) { return (double) (((juce::uint64) t.dwHighDateTime << 32) | t.dwLowDateTime) * 1.0e-7; };
        return toSeconds (kernel) + toSeconds (user);
    }

    std::unique_ptr<PluginBase> makeProcessor (const juce::String& scenario, int instruments)
    {
        std::unique_ptr<PluginBase> processor;
        if (scenario == "fx")
            processor = std::make_unique<FxProcessor>();
        else
            processor = std::make_unique<InstrumentProcessor>();
        const auto fx = processor->isEffectVersion();
        processor->setPlayConfigDetails (fx ? 2 : 0, 2, rate, block);
        processor->prepareToPlay (rate, block);
        if (scenario == "heavy" || fx)
            Scenarios::makeHeavy (*processor, instruments);
        else
        {
            processor->setInstrumentCount (instruments);
            for (int layer = 3; layer < instruments; ++layer)
                processor->pattern.applyLayerTemplate (layer, PatternTemplate::rolling);
        }
        processor->ensureTablesInUse();
        return processor;
    }

    // En DAW som spelar i 145 BPM.
    struct Head : juce::AudioPlayHead
    {
        std::atomic<juce::int64> samples { 0 };
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            const auto now = samples.load();
            info.setBpm (145.0);
            info.setIsPlaying (true);
            info.setTimeInSamples (now);
            info.setPpqPosition ((double) now / rate * 145.0 / 60.0);
            info.setPpqPositionOfLastBarStart (std::floor ((double) now / rate * 145.0 / 60.0 / 4.0) * 4.0);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    };

    // Ljudtråden: spelar med en hållen ton, så fort den kan eller i realtid.
    void audioLoop (PluginBase& processor, Head& head, std::atomic<bool>& stop, std::atomic<juce::int64>& blocksDone, bool realTime)
    {
        const auto fx = processor.isEffectVersion();
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer first, none;
        first.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
        double phase = 0.0;
        auto firstBlock = true;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        while (! stop.load())
        {
            buffer.clear();
            if (fx)
                for (int i = 0; i < block; ++i)
                {
                    const auto s = (float) (2.0 * phase - 1.0) * 0.5f;
                    buffer.setSample (0, i, s);
                    buffer.setSample (1, i, s);
                    phase += 55.0 / rate;
                    phase -= std::floor (phase);
                }
            processor.processBlock (buffer, firstBlock ? first : none);
            firstBlock = false;
            head.samples += block;
            ++blocksDone;
            if (realTime)
            {
                const auto due = start + (double) head.samples.load() / rate * 1000.0;
                while (juce::Time::getMillisecondCounterHiRes() < due && ! stop.load())
                    juce::Thread::sleep (1);
            }
        }
    }
}
#endif

int profileCommand (const juce::String& scenario, int instruments, double seconds)
{
   #if ! JUCE_WINDOWS
    juce::ignoreUnused (scenario, instruments, seconds);
    std::puts ("profile finns bara på Windows");
    return 1;
   #else
    juce::ScopedJuceInitialiser_GUI gui;
    const auto analysis = scenario.startsWith ("analysis"); // analysis-init / analysis-heavy: ANALYSE (MixAnalysis::run) i stället för ljudtråden
    auto processor = makeProcessor (analysis ? scenario.fromFirstOccurrenceOf ("-", false, false) : scenario, instruments);
    Head head;
    processor->setPlayHead (&head);

    std::atomic<bool> stop { false };
    std::atomic<juce::int64> blocksDone { 0 };
    std::atomic<int> analysesDone { 0 };
    std::atomic<double> analysisSeconds { 0.0 };
    std::thread worker ([&]
    {
        if (! analysis)
        {
            audioLoop (*processor, head, stop, blocksDone, false);
            return;
        }
        processor->setPlayHead (nullptr);
        while (! stop.load())
        {
            const auto begin = juce::Time::getMillisecondCounterHiRes();
            auto copy = MixAnalysis::prepareCopy (*processor);
            MixAnalysis::run (*copy, 145.0, 52, {}, nullptr, {}, [&stop] { return stop.load(); });
            if (! stop.load())
            {
                analysisSeconds = analysisSeconds.load() + (juce::Time::getMillisecondCounterHiRes() - begin) / 1000.0;
                ++analysesDone;
            }
        }
    });

    Sampler sampler;
    auto* thread = (HANDLE) worker.native_handle();
    juce::Thread::sleep (300); // uppvärmning: tabeller, första blocken
    const auto blocksAtStart = blocksDone.load();
    const auto start = juce::Time::getMillisecondCounterHiRes();
    while (juce::Time::getMillisecondCounterHiRes() - start < seconds * 1000.0)
    {
        sampler.sample (thread);
        juce::Thread::sleep (1);
    }
    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    const auto blocks = blocksDone.load() - blocksAtStart;
    stop = true;
    worker.join();
    processor->setPlayHead (nullptr);

    const auto audioSeconds = (double) blocks * block / rate;
    if (analysis)
        std::printf ("Scenario %s, %d instrument: %d analyser klara, %.2f s per analys (med profileringen)\n", scenario.toRawUTF8(), instruments,
                     analysesDone.load(), analysesDone.load() > 0 ? analysisSeconds.load() / analysesDone.load() : 0.0);
    else
        std::printf ("Scenario %s, %d instrument: %.3f s CPU per sekund ljud (en kärna, med profileringen)\n",
                     scenario.toRawUTF8(), instruments, audioSeconds > 0.0 ? elapsed / audioSeconds : 0.0);
    sampler.report (35, 35, 45);
    return 0;
   #endif
}

int uiProfileCommand (int tab, const juce::String& scenario, int instruments, double seconds)
{
   #if ! JUCE_WINDOWS
    juce::ignoreUnused (tab, scenario, instruments, seconds);
    std::puts ("uiprofile finns bara på Windows");
    return 1;
   #else
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<Scenarios::TemporaryUserFolders> folders; // användarens settings.xml rörs aldrig
    const auto fpsStep = juce::SystemStats::getEnvironmentVariable ("KB_SPECTRUM_FPS", {}), fill = juce::SystemStats::getEnvironmentVariable ("KB_SPECTRUM_FILL", {});
    if (fpsStep.isNotEmpty() || fill.isNotEmpty())
    {
        folders = std::make_unique<Scenarios::TemporaryUserFolders>();
        if (fpsStep.isNotEmpty())
            Settings::setVisualFpsStep (Settings::Visual::analyzer, fpsStep.getIntValue());
        if (fill.isNotEmpty())
            Settings::setVisualFill (Settings::Visual::analyzer, fill.getIntValue() != 0);
    }
    auto processor = makeProcessor (scenario, instruments);
    processor->selectedTab = tab % 10;
    processor->selectedSynthTab = tab / 10 % 10;
    if (juce::SystemStats::getEnvironmentVariable ("KB_SPECTRUM", "0").getIntValue() != 0)
        processor->snapshotSpectrum = true;
    Head head;
    processor->setPlayHead (&head);

    std::atomic<bool> stop { false };
    std::atomic<juce::int64> blocksDone { 0 };
    std::thread worker ([&] { audioLoop (*processor, head, stop, blocksDone, true); });

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    editor->setTopLeftPosition (40, 40);
    editor->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
    editor->setVisible (true);

    HANDLE mainThread = nullptr;
    DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    auto* audioThread = (HANDLE) worker.native_handle();

    // Provtagningen sker i en egen tråd medan meddelandetråden kör som vanligt.
    Sampler sampler;
    std::atomic<bool> sampling { false };
    std::thread samplerThread ([&]
    {
        while (! stop.load())
        {
            if (sampling.load())
                sampler.sample (mainThread);
            juce::Thread::sleep (1);
        }
    });

    juce::MessageManager::getInstance()->runDispatchLoopUntil (1500); // fönstret öppnas, analysen kommer igång
    const auto uiBefore = cpuSecondsOf (mainThread), audioBefore = cpuSecondsOf (audioThread);
    const auto start = juce::Time::getMillisecondCounterHiRes();
    sampling = true;
    juce::MessageManager::getInstance()->runDispatchLoopUntil ((int) (seconds * 1000.0));
    sampling = false;
    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    const auto uiCpu = cpuSecondsOf (mainThread) - uiBefore, audioCpu = cpuSecondsOf (audioThread) - audioBefore;

    stop = true;
    samplerThread.join();
    worker.join();
    editor.reset();
    processor->setPlayHead (nullptr);
    CloseHandle (mainThread);

    std::printf ("Flik %d, scenario %s, %d instrument, %.1f s: meddelandetråden %.1f %% av en kärna, ljudtråden %.1f %% (realtid)\n",
                 tab, scenario.toRawUTF8(), instruments, elapsed, 100.0 * uiCpu / elapsed, 100.0 * audioCpu / elapsed);
    sampler.report (20, 70, 10);
    return 0;
   #endif
}
