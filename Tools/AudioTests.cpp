// Tester av ljudtråden för verktyget (`tool rendersigwide`, `tool audiotest`):
//
// - rendersigwide: fingeravtryck (hash av varje sample) av tunga lägen i båda pluginsen och med flera blockstorlekar och samplingsfrekvenser.
//   Samma rad före och efter en ändring = exakt samma ljud. Komplement till `rendersig`, som bara täcker Init och fabriksljuden.
// - audiotest: räknar minnesallokeringar och frigöranden på ljudtråden (processBlock) i tunga lägen, också medan rattar, EQ, effekttyper
//   och modulering ändras mellan blocken, och provar gränsfall (block på 1 sample, mycket stora block, byte av samplingsfrekvens, NaN in).
//   En allokering på ljudtråden kan låsa (heapen har lås) och ge knaster: målet är noll.
//
// Allokeringarna räknas genom att programmets operator new/delete byts ut mot malloc/free med en räknare som bara är på i den tråd som mäts.

#include "../Source/Instrument/InstrumentProcessor.h"
#include "../Source/Fx/FxProcessor.h"
#include "Scenarios.h"
#include <new>
#include <cstdlib>
#include <map>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment (lib, "dbghelp.lib")
#endif

namespace
{
    thread_local bool countingAllocations = false;
    thread_local int allocationCount = 0;
    thread_local int freeCount = 0;

    // KB_ALLOC_TRACE=1: varje allokering på ljudtråden sparar sin anropsstack (i en fast tabell, utan att själv allokera).
    constexpr int maxTraces = 20000, traceDepth = 14;
    void* traces[maxTraces][traceDepth] {};
    std::atomic<int> numTraces { 0 };
    bool tracing = false;

    void noteAllocation()
    {
        ++allocationCount;
       #if JUCE_WINDOWS
        if (tracing)
            if (const auto index = numTraces.fetch_add (1); index < maxTraces)
                RtlCaptureStackBackTrace (2, traceDepth, traces[index], nullptr);
       #endif
    }

    void* allocate (std::size_t size)
    {
        if (countingAllocations)
            noteAllocation();
        if (auto* p = std::malloc (size > 0 ? size : 1))
            return p;
        throw std::bad_alloc();
    }

    void* allocateAligned (std::size_t size, std::align_val_t alignment)
    {
        if (countingAllocations)
            noteAllocation();
       #if JUCE_WINDOWS
        if (auto* p = _aligned_malloc (size > 0 ? size : 1, (std::size_t) alignment))
            return p;
       #else
        if (auto* p = std::aligned_alloc ((std::size_t) alignment, ((size + (std::size_t) alignment - 1) / (std::size_t) alignment) * (std::size_t) alignment))
            return p;
       #endif
        throw std::bad_alloc();
    }

    void release (void* p) noexcept
    {
        if (p != nullptr && countingAllocations)
            ++freeCount;
        std::free (p);
    }

    void releaseAligned (void* p) noexcept
    {
        if (p != nullptr && countingAllocations)
            ++freeCount;
       #if JUCE_WINDOWS
        _aligned_free (p);
       #else
        std::free (p);
       #endif
    }
}

void* operator new (std::size_t size)                                            { return allocate (size); }
void* operator new[] (std::size_t size)                                          { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept            { try { return allocate (size); } catch (...) { return nullptr; } }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept          { try { return allocate (size); } catch (...) { return nullptr; } }
void* operator new (std::size_t size, std::align_val_t a)                        { return allocateAligned (size, a); }
void* operator new[] (std::size_t size, std::align_val_t a)                      { return allocateAligned (size, a); }
void* operator new (std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept   { try { return allocateAligned (size, a); } catch (...) { return nullptr; } }
void* operator new[] (std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { try { return allocateAligned (size, a); } catch (...) { return nullptr; } }
void operator delete (void* p) noexcept                                          { release (p); }
void operator delete[] (void* p) noexcept                                        { release (p); }
void operator delete (void* p, std::size_t) noexcept                             { release (p); }
void operator delete[] (void* p, std::size_t) noexcept                           { release (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept                   { release (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept                 { release (p); }
void operator delete (void* p, std::align_val_t) noexcept                        { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept                      { releaseAligned (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept           { releaseAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept         { releaseAligned (p); }
void operator delete (void* p, std::align_val_t, const std::nothrow_t&) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t, const std::nothrow_t&) noexcept { releaseAligned (p); }

namespace
{
    juce::uint64 hashOf (const juce::AudioBuffer<float>& audio)
    {
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
    }

    bool allFiniteIn (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;
        return true;
    }

    std::unique_ptr<PluginBase> make (bool fx)
    {
        if (fx)
            return std::make_unique<FxProcessor>();
        return std::make_unique<InstrumentProcessor>();
    }

    // FX-versionen: en sågtand i 55 Hz in (som en annan synth).
    void sawInput (juce::AudioBuffer<float>& block, int startSample, double rate)
    {
        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            const auto phase = std::fmod ((double) (startSample + i) * 55.0 / rate, 1.0);
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
                block.setSample (ch, i, (float) (2.0 * phase - 1.0) * 0.5f);
        }
    }
}

int renderSignaturesWide()
{
    juce::ScopedJuceInitialiser_GUI gui;
    struct Case { const char* name; bool fx; int instruments; double rate; int block; };
    const Case cases[] {
        { "heavy instr 10 @48k/512", false, 10, 48000.0, 512 },
        { "heavy instr 3 @44.1k/128", false, 3, 44100.0, 128 },
        { "heavy instr 1 @96k/1024", false, 1, 96000.0, 1024 },
        { "heavy instr 4 @48k/37", false, 4, 48000.0, 37 },
        { "heavy fx @48k/512", true, 1, 48000.0, 512 },
        { "heavy fx @44.1k/64", true, 1, 44100.0, 64 },
    };
    for (const auto& c : cases)
    {
        auto processor = make (c.fx);
        Scenarios::makeHeavy (*processor, c.instruments);
        OfflineRender::Settings settings;
        settings.sampleRate = c.rate;
        settings.blockSize = c.block;
        settings.bpm = 145.0;
        settings.bars = 2;
        settings.notes = { 52, 55 };
        settings.extraBars = 1;
        const auto rate = c.rate;
        settings.fillInput = [rate] (juce::AudioBuffer<float>& block, int start) { sawInput (block, start, rate); };
        processor->ensureTablesInUse();
        const auto audio = OfflineRender::render (*processor, settings);
        auto finite = true;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            for (int i = 0; i < audio.getNumSamples(); ++i)
                finite = finite && std::isfinite (audio.getSample (ch, i));
        std::printf ("%-28s %016llx peak %.4f rms %.4f%s\n", c.name, (unsigned long long) hashOf (audio), audio.getMagnitude (0, audio.getNumSamples()),
                     audio.getRMSLevel (0, 0, audio.getNumSamples()), finite ? "" : "  ICKE-ÄNDLIGA VÄRDEN");
    }
    return 0;
}

int audioThreadTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    tracing = juce::SystemStats::getEnvironmentVariable ("KB_ALLOC_TRACE", "0") == "1";
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // Kör `blocks` block och räknar allokeringar i de block som kommer efter uppvärmningen. `between` körs på "meddelandetråden"
    // (utanför räkningen) före varje block, som en användare som vrider på rattar.
    auto run = [&] (PluginBase& processor, double rate, int blockSize, int blocks, const std::function<void (int)>& between, bool fx)
    {
        processor.setPlayConfigDetails (fx ? 2 : 0, 2, rate, blockSize);
        processor.prepareToPlay (rate, blockSize);
        struct Head : juce::AudioPlayHead
        {
            juce::int64 samples = 0;
            double rate = 48000.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setBpm (145.0);
                info.setIsPlaying (true);
                info.setTimeInSamples (samples);
                info.setPpqPosition ((double) samples / rate * 145.0 / 60.0);
                info.setPpqPositionOfLastBarStart (std::floor ((double) samples / rate * 145.0 / 60.0 / 4.0) * 4.0);
                info.setTimeSignature (TimeSignature { 4, 4 });
                return info;
            }
        } head;
        head.rate = rate;
        processor.setPlayHead (&head);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.ensureSize (64);
        auto counted = 0, freed = 0, nonFinite = 0;
        for (int block = 0; block < blocks; ++block)
        {
            if (between)
                between (block);
            midi.clear();
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
            if (block == blocks / 2)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 52), blockSize / 2);
                midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 90), blockSize / 2);
            }
            buffer.clear();
            if (fx)
                sawInput (buffer, (int) head.samples, rate);

            allocationCount = freeCount = 0;
            countingAllocations = block >= 8; // de första blocken får fylla på (statiska tabeller, första gången)
            processor.processBlock (buffer, midi);
            countingAllocations = false;
            counted += allocationCount;
            freed += freeCount;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    nonFinite += std::isfinite (buffer.getSample (ch, i)) ? 0 : 1;
            head.samples += blockSize;
        }
        processor.setPlayHead (nullptr);
        return std::array<int, 3> { counted, freed, nonFinite };
    };

    auto report = [&] (const juce::String& name, std::array<int, 3> result)
    {
        check (result[0] == 0 && result[1] == 0, name + ": " + juce::String (result[0]) + " allokeringar, " + juce::String (result[1]) + " frigöranden på ljudtråden");
        check (result[2] == 0, name + ": inga icke-ändliga värden (" + juce::String (result[2]) + ")");
    };

    for (const auto fx : { false, true })
    {
        auto processor = make (fx);
        Scenarios::makeHeavy (*processor, 10);
        report (juce::String (fx ? "fx" : "instrument") + " tungt läge, 48k/512", run (*processor, 48000.0, 512, 400, {}, fx));
    }

    // Rattar, EQ, kompressor, effekttyper och modulering som ändras mellan blocken (som automation eller en användare).
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 6);
        auto& p = *processor;
        auto between = [&p] (int block)
        {
            const auto t = (float) (block % 50) / 50.0f;
            Scenarios::setPlain (p, ParamID::bass (0, BassField::cutoff), 200.0f + 2000.0f * t);
            Scenarios::setPlain (p, "kickEq2G", -6.0f + 12.0f * t);
            Scenarios::setPlain (p, "bass2Eq3G", 6.0f * t);
            Scenarios::setPlain (p, "masterEq1F", 60.0f + 100.0f * t);
            Scenarios::setPlain (p, Effects::channelParamID (2, "Eq1G"), 8.0f * t);
            Scenarios::setPlain (p, Effects::channelParamID (3, "CompThr"), -30.0f * t);
            Scenarios::setPlain (p, Effects::knobParamID (1, 0, 1), t);
            Scenarios::setPlain (p, "masterDrive", 6.0f * t);
            Scenarios::setPlain (p, ParamID::bass (3, BassField::aUnison), (float) (1 + block % 7));
            if (block % 40 == 20)
                Scenarios::setPlain (p, Effects::slotParamID (4, 2, "Type"), (float) (1 + (block / 40) % (numEffectTypes - 1)));
        };
        report ("rattar, EQ och effekttyper som ändras", run (p, 48000.0, 256, 400, between, false));
    }

    // Gränsfall: block på 1 sample, ett mycket stort block (större än utlovat i prepareToPlay), byte av samplingsfrekvens.
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 3);
        report ("block på 1 sample", run (*processor, 48000.0, 1, 3000, {}, false));
        report ("byte till 96 kHz, block 2048", run (*processor, 96000.0, 2048, 60, {}, false));
        report ("tillbaka till 44.1 kHz, block 64", run (*processor, 44100.0, 64, 300, {}, false));

        // Ett block som är större än det som utlovades: tillåtet att allokera (en gång), men inte krascha eller ge icke-ändliga värden.
        processor->setPlayConfigDetails (0, 2, 48000.0, 256);
        processor->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> big (2, 8192);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
        big.clear();
        processor->processBlock (big, midi);
        auto finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < big.getNumSamples(); ++i)
                finite = finite && std::isfinite (big.getSample (ch, i));
        check (finite, "ett block på 8192 samples efter prepareToPlay med 256: inga icke-ändliga värden");
    }

    // FX-versionen med NaN och oändliga värden in: ingenting får spridas till utgången för alltid (en sjuk synth före i kedjan).
    {
        auto processor = make (true);
        Scenarios::makeHeavy (*processor, 1);
        processor->setPlayConfigDetails (2, 2, 48000.0, 256);
        processor->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer none;
        auto lastBlockFinite = true;
        for (int block = 0; block < 200; ++block)
        {
            sawInput (buffer, block * 256, 48000.0);
            if (block == 10)
                buffer.setSample (0, 5, std::numeric_limits<float>::quiet_NaN());
            if (block == 11)
                buffer.setSample (1, 7, std::numeric_limits<float>::infinity());
            processor->processBlock (buffer, none);
            lastBlockFinite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    lastBlockFinite = lastBlockFinite && std::isfinite (buffer.getSample (ch, i));
        }
        check (lastBlockFinite, "FX-versionen återhämtar sig efter NaN/inf i insignalen");
    }

    // Instrumentet när DAW:en skickar NaN som parametervärde (trasig automation) en kort stund: tyst under tiden, sedan ljud igen.
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 3);
        processor->setPlayConfigDetails (0, 2, 48000.0, 256);
        processor->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        auto* cutoff = processor->apvts.getParameter (ParamID::bass (0, BassField::cutoff));
        auto* knob = processor->apvts.getParameter (Effects::knobParamID (0, 0, 1));
        const auto cutoffBefore = cutoff->getValue(), knobBefore = knob->getValue();
        auto lastFinite = true;
        auto peakAfter = 0.0f;
        for (int block = 0; block < 400; ++block)
        {
            midi.clear();
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
            if (block == 50)
            {
                cutoff->setValueNotifyingHost (std::numeric_limits<float>::quiet_NaN());
                knob->setValueNotifyingHost (std::numeric_limits<float>::quiet_NaN());
            }
            if (block == 55)
            {
                cutoff->setValueNotifyingHost (cutoffBefore);
                knob->setValueNotifyingHost (knobBefore);
            }
            buffer.clear();
            processor->processBlock (buffer, midi);
            lastFinite = allFiniteIn (buffer);
            if (block > 300)
                peakAfter = std::max (peakAfter, buffer.getMagnitude (0, 256));
        }
        check (lastFinite && peakAfter > 0.01f, "instrumentet ger ljud igen efter NaN i parametrarna (topp " + juce::String (peakAfter, 3) + ", "
                                                    + juce::String (processor->nonFiniteRecoveries.load()) + " återställningar)");
    }

   #if JUCE_WINDOWS
    if (tracing && numTraces.load() > 0)
    {
        // Var allokeringarna kom ifrån: de vanligaste stackarna (funktion:rad för de översta ramarna utanför allokatorn).
        const auto process = GetCurrentProcess();
        SymSetOptions (SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize (process, nullptr, TRUE);
        auto describe = [process] (void* address)
        {
            alignas (SYMBOL_INFO) char storage[sizeof (SYMBOL_INFO) + 512] {};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*> (storage);
            symbol->SizeOfStruct = sizeof (SYMBOL_INFO);
            symbol->MaxNameLen = 511;
            DWORD64 displacement = 0;
            juce::String text = SymFromAddr (process, (DWORD64) address, &displacement, symbol) ? juce::String (symbol->Name).substring (0, 70) : juce::String ("?");
            IMAGEHLP_LINE64 line {};
            line.SizeOfStruct = sizeof (line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64 (process, (DWORD64) address, &lineDisplacement, &line))
                text << " (" << juce::File (juce::String (line.FileName)).getFileName() << ":" << (int) line.LineNumber << ")";
            return text;
        };
        std::map<juce::String, int> counts;
        for (int t = 0; t < std::min (numTraces.load(), maxTraces); ++t)
        {
            juce::String key;
            for (int d = 0; d < 7 && traces[t][d] != nullptr; ++d)
                key << describe (traces[t][d]) << "\n      ";
            ++counts[key];
        }
        std::vector<std::pair<int, juce::String>> sorted;
        for (const auto& [key, count] : counts)
            sorted.push_back ({ count, key });
        std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        std::printf ("\nAllokeringar på ljudtråden (%d spårade), vanligaste stackarna:\n", numTraces.load());
        for (int i = 0; i < std::min (25, (int) sorted.size()); ++i)
            std::printf ("%6d  %s\n", sorted[(size_t) i].first, sorted[(size_t) i].second.toRawUTF8());
        SymCleanup (process);
    }
   #endif
    std::puts (failed ? "FEL: ljudtrådstestet misslyckades" : "Ljudtrådstestet gick igenom");
    return failed ? 1 : 0;
}
