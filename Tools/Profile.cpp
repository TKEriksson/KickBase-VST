// `tool profile <scenario> [instrument] [sekunder]`: en enkel samplande profilerare. En tråd spelar pluginet (som en DAW, block för block)
// medan huvudtråden stoppar den ungefär 1000 gånger per sekund och läser var den är (instruktionspekaren och anropsstacken). Adresserna
// slås upp i PDB-filen (DbgHelp), så att resultatet visas per funktion (själv och inklusive anropade) och per källrad.
// Scenario: init (det inbakade Init), heavy (Scenarios::makeHeavy: allt på), fx (FX-versionen, heavy).

#include "../Source/Instrument/InstrumentProcessor.h"
#include "../Source/Fx/FxProcessor.h"
#include "Scenarios.h"
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

int profileCommand (const juce::String& scenario, int instruments, double seconds)
{
   #if ! JUCE_WINDOWS
    juce::ignoreUnused (scenario, instruments, seconds);
    std::puts ("profile finns bara på Windows");
    return 1;
   #else
    juce::ScopedJuceInitialiser_GUI gui;
    constexpr double rate = 48000.0;
    constexpr int block = 512;

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
    else if (! fx)
    {
        processor->setInstrumentCount (instruments);
        for (int layer = 3; layer < instruments; ++layer)
            processor->pattern.applyLayerTemplate (layer, PatternTemplate::rolling);
    }
    processor->ensureTablesInUse();

    // Arbetstråden: spelar med en hållen ton. FX-versionen får en sågtand in och en DAW som spelar (annars gör den inget).
    std::atomic<bool> stop { false };
    std::atomic<juce::int64> blocksDone { 0 };
    struct Head : juce::AudioPlayHead
    {
        juce::int64 samples = 0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (145.0);
            info.setIsPlaying (true);
            info.setTimeInSamples (samples);
            info.setPpqPosition ((double) samples / rate * 145.0 / 60.0);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    } head;
    if (fx)
        processor->setPlayHead (&head);

    std::thread worker ([&]
    {
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer first, none;
        first.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
        double phase = 0.0;
        auto firstBlock = true;
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
            processor->processBlock (buffer, firstBlock ? first : none);
            firstBlock = false;
            head.samples += block;
            ++blocksDone;
        }
    });

    auto* thread = (HANDLE) worker.native_handle();
    const auto process = GetCurrentProcess();
    SymSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize (process, nullptr, TRUE);

    std::map<DWORD64, int> selfCounts;
    std::vector<std::vector<DWORD64>> stacks;
    juce::Thread::sleep (300); // uppvärmning: tabeller, första blocken
    const auto blocksAtStart = blocksDone.load();
    const auto start = juce::Time::getMillisecondCounterHiRes();
    while (juce::Time::getMillisecondCounterHiRes() - start < seconds * 1000.0)
    {
        if (SuspendThread (thread) != (DWORD) -1)
        {
            CONTEXT context {};
            context.ContextFlags = CONTEXT_FULL;
            if (GetThreadContext (thread, &context))
            {
                ++selfCounts[context.Rip];
                std::vector<DWORD64> stack;
                STACKFRAME64 frame {};
                frame.AddrPC.Offset = context.Rip;
                frame.AddrPC.Mode = AddrModeFlat;
                frame.AddrFrame.Offset = context.Rbp;
                frame.AddrFrame.Mode = AddrModeFlat;
                frame.AddrStack.Offset = context.Rsp;
                frame.AddrStack.Mode = AddrModeFlat;
                for (int depth = 0; depth < 40; ++depth)
                {
                    if (! StackWalk64 (IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
                        || frame.AddrPC.Offset == 0)
                        break;
                    stack.push_back (frame.AddrPC.Offset);
                }
                stacks.push_back (std::move (stack));
            }
            ResumeThread (thread);
        }
        juce::Thread::sleep (1);
    }
    const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
    const auto blocks = blocksDone.load() - blocksAtStart;
    stop = true;
    worker.join();

    // Uppslag: funktion och källrad för en adress.
    std::map<DWORD64, juce::String> functionCache;
    auto functionOf = [&] (DWORD64 address) -> juce::String
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
    };
    auto lineOf = [&] (DWORD64 address) -> juce::String
    {
        IMAGEHLP_LINE64 line {};
        line.SizeOfStruct = sizeof (line);
        DWORD displacement = 0;
        if (SymGetLineFromAddr64 (process, address, &displacement, &line))
            return juce::File (juce::String (line.FileName)).getFileName() + ":" + juce::String ((int) line.LineNumber);
        return "?";
    };

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

    const auto audioSeconds = (double) blocks * block / rate;
    std::printf ("Scenario %s, %d instrument: %d prov på %.1f s; %.3f s CPU per sekund ljud (en kärna, med profileringen)\n",
                 scenario.toRawUTF8(), instruments, total, elapsed, audioSeconds > 0.0 ? elapsed / audioSeconds : 0.0);
    printTop ("Själv (där tiden faktiskt går):", self, 35);
    printTop ("Inklusive anropade funktioner:", inclusive, 35);
    printTop ("Källrader:", lines, 45);
    SymCleanup (process);
    return 0;
   #endif
}
