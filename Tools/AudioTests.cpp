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
#include "../Source/Ui/FmPresets.h"
#include "../Source/Core/MixAnalysis.h"
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
 #include <psapi.h>
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

    // TB-303-modellen på de `count` första instrumenten (fas D1).
    void useTb303 (PluginBase& processor, int count, float model = 1.0f)
    {
        for (int layer = 0; layer < count; ++layer)
            Scenarios::setInstrument (processor, layer, InstrumentField::filterModel, model);
    }

    // Ladder, Formant och Comb (fas D2) på de `count` första instrumenten, reglerna (P1, P2) på mitten: instrument 1 Ladder, 2 Formant, 3 Comb, 4 Ladder ...
    void useFilterModels (PluginBase& processor, int count, int firstModel = 2)
    {
        for (int layer = 0; layer < count; ++layer)
        {
            Scenarios::setInstrument (processor, layer, InstrumentField::filterModel, (float) (2 + (firstModel - 2 + layer) % 3));
            Scenarios::setInstrument (processor, layer, InstrumentField::filterP1, 0.4f);
            Scenarios::setInstrument (processor, layer, InstrumentField::filterP2, 0.8f);
        }
    }

    // Filter 2 (fas D3) på de `count` första instrumenten: instrument 1 Serial Kickbase High, 2 Parallel Ladder med MIX 0,5, 3 Serial Comb med MIX 0,7 (sedan om från 1).
    // Fas P: flera röster per instrument (2, 4 och 8) med lång release och full notlängd, så att tonerna överlappar och röster stjäls.
    void usePolyphony (PluginBase& processor, int count)
    {
        using F = InstrumentField;
        for (int layer = 0; layer < count; ++layer)
        {
            Scenarios::setInstrument (processor, layer, F::voices, layer % 3 == 0 ? 2.0f : layer % 3 == 1 ? 4.0f : 8.0f);
            Scenarios::setInstrument (processor, layer, F::release, 300.0f);
            Scenarios::setInstrument (processor, layer, F::gate, 1.0f);
            // Ackord på varannan ton (1-3 extra toner), så att ackordtonerna också stjäls.
            std::vector<int> ticks;
            processor.pattern.forEachNote (layer, [&ticks] (int tick, const Pattern::Note&) { ticks.push_back (tick); });
            const Pattern::Chord chords[] { { 7, 0, 0 }, { 3, 7, 0 }, { -12, 4, 7 } };
            for (size_t i = 0; i < ticks.size(); i += 2)
                processor.pattern.edit (layer, ticks[i]).setChord (chords[(i / 2) % 3]);
        }
    }

    void useFilter2 (PluginBase& processor, int count)
    {
        using F = InstrumentField;
        for (int layer = 0; layer < count; ++layer)
        {
            const auto kind = layer % 3;
            Scenarios::setInstrument (processor, layer, F::filter2On, 1.0f);
            Scenarios::setInstrument (processor, layer, F::filter2Model, kind == 0 ? 0.0f : kind == 1 ? 2.0f : 4.0f);
            Scenarios::setInstrument (processor, layer, F::filter2Type, kind == 0 ? 3.0f : 0.0f);
            Scenarios::setInstrument (processor, layer, F::filter2Cutoff, kind == 0 ? 300.0f : kind == 1 ? 1200.0f : 220.0f);
            Scenarios::setInstrument (processor, layer, F::filter2Reso, 0.5f);
            Scenarios::setInstrument (processor, layer, F::filter2P1, 0.4f);
            Scenarios::setInstrument (processor, layer, F::filter2P2, 0.8f);
            Scenarios::setInstrument (processor, layer, F::filterRouting, kind == 1 ? 1.0f : 0.0f);
            Scenarios::setInstrument (processor, layer, F::filterMix, kind == 0 ? 1.0f : kind == 1 ? 0.5f : 0.7f);
        }
    }

    // ENV 2 och ENV 3 i läget Sync på de `count` första instrumenten (notvärden: attack 1/16, decay 1/4 D, release 1/8).
    void useEnvSync (PluginBase& processor, int count)
    {
        for (int layer = 0; layer < count; ++layer)
            for (int env = 0; env < 2; ++env)
            {
                Scenarios::setInstrument (processor, layer, ParamID::envModeField (env), 1.0f);
                Scenarios::setInstrument (processor, layer, ParamID::envSyncField (env, 0), 4.0f);
                Scenarios::setInstrument (processor, layer, ParamID::envSyncField (env, 1), 11.0f);
                Scenarios::setInstrument (processor, layer, ParamID::envSyncField (env, 2), 7.0f);
            }
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

// `tool rendersigtb303`: som rendersigwide men med TB-303-modellen påslagen på alla instrument (fas D1). Egna baslinjer: docs/audit/baseline-tb303.txt.
int renderSignaturesTb303()
{
    juce::ScopedJuceInitialiser_GUI gui;
    struct Case { const char* name; bool fx; int instruments; double rate; int block; };
    const Case cases[] {
        { "tb303 instr 10 @48k/512", false, 10, 48000.0, 512 },
        { "tb303 instr 3 @44.1k/128", false, 3, 44100.0, 128 },
        { "tb303 instr 1 @96k/1024", false, 1, 96000.0, 1024 },
        { "tb303 instr 4 @48k/37", false, 4, 48000.0, 37 },
                    };
    for (const auto& c : cases)
    {
        auto processor = make (c.fx);
        Scenarios::makeHeavy (*processor, c.instruments);
        useTb303 (*processor, c.instruments);
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

// Fas I: RING (B). AMOUNT 0 ändrar inget; full ring med vald källa ändrar ljudet även när källans LEVEL är 0; en avstängd källa ger tystnad.
int ringTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };
    const auto keep = make (false);
    keep->ensureTablesInUse();

    constexpr double rate = 48000.0;
    auto render = [] (const InstrumentParams& params)
    {
        InstrumentVoices v;
        v.prepare (rate);
        v.setNumVoices (1);
        v.setSeed (7);
        v.setTempo (145.0);
        v.setParams (params);
        v.reset();
        v.noteOn (0, 0.8f, (int) (rate * 10.0));
        std::vector<float> out;
        for (int i = 0; i < (int) (rate * 0.3); ++i)
        {
            const auto o = v.next();
            out.push_back (o.left * o.amplitude);
        }
        return out;
    };
    auto rms = [] (const std::vector<float>& x)
    {
        auto sum = 0.0;
        for (const auto s : x)
            sum += (double) s * s;
        return std::sqrt (sum / (double) std::max<size_t> (1, x.size()));
    };
    auto finite = [] (const std::vector<float>& x)
    {
        for (const auto s : x)
            if (! std::isfinite (s))
                return false;
        return true;
    };

    InstrumentParams base;
    base.sub = 0.0f;
    base.cutoffHz = 18000.0f;
    base.sustain = 0.8f;
    base.oscB.on = false;
    base.oscC.on = false;
    base.oscB.level = 0.0f; // källan hörs inte själv, bara som multiplikator
    base.oscC.level = 0.0f;
    base.oscB.semis = 7.0f;
    base.oscC.semis = 12.0f;
    const auto plain = render (base);
    check (rms (plain) > 0.01, "OSC 1 ensam ger ljud");

    auto ring = base;
    ring.oscA.warp = WarpMode::ring;

    ring.oscA.warpAmount = 0.0f;
    ring.oscB.on = true;
    check (render (ring) == plain, "RING AMOUNT 0 ger exakt samma ljud som utan ring");

    ring.oscA.warpAmount = 1.0f;
    for (int source = 0; source < 3; ++source)
    {
        auto p = ring;
        p.oscA.ringSource = source;
        p.oscB.on = source != 1;
        p.oscC.on = source != 0;
        const auto out = render (p);
        check (finite (out) && out != plain && rms (out) > 0.001, "RING full, källa " + juce::String (source) + " ändrar ljudet och ger ljud");
    }

    for (int source = 0; source < 3; ++source)
    {
        auto p = ring;
        p.oscA.ringSource = source;
        p.oscB.on = source == 1;  // den valda källan (eller en av dem) är av
        p.oscC.on = source == 0;
        check (rms (render (p)) < 1.0e-4, "RING full, källa " + juce::String (source) + " avstängd ger tystnad");
    }

    std::printf (failed ? "ringtest: FEL\n" : "ringtest: ok\n");
    return failed ? 1 : 0;
}

// Fas P: flera röster per instrument. Klick när en röst stjäls (mot samma toner utan stöld), att en röst som legat oanvänd medan
// rattarna ändrats låter som en ny röst, att VOICES ned tonar ut i stället för att klippa, att seed och ackordens timing är
// deterministiska, och minnet och prepareToPlay-tiden per instans (10 instrument x 8 röster = 80 röster) vid 48 och 192 kHz.
int polyTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };
    const auto keep = make (false); // (laddar de inbyggda tabellerna)
    keep->ensureTablesInUse();

    constexpr double rate = 48000.0;
    constexpr int longGate = (int) (rate * 10.0);
    auto toDb = [] (double value) { return 20.0 * std::log10 (std::max (value, 1.0e-12)); };
    auto value = [] (const InstrumentVoices::Output& o) { return o.left * o.amplitude; };
    auto setUp = [] (InstrumentVoices& v, int voices, const InstrumentParams& params, juce::int64 seed = 7)
    {
        v.prepare (rate);
        v.setNumVoices (voices);
        v.setSeed (seed);
        v.setTempo (145.0);
        v.setParams (params);
        v.reset();
    };
    auto run = [&value] (InstrumentVoices& v, std::vector<float>& out, int samples)
    {
        for (int i = 0; i < samples; ++i)
            out.push_back (value (v.next()));
    };
    // Största hoppet mellan två sampels och energin över 5 kHz (RMS efter ett högpassfilter) i [from, to).
    auto maxJump = [] (const std::vector<float>& x, int from, int to)
    {
        auto peak = 0.0f;
        for (int i = std::max (1, from); i < std::min (to, (int) x.size()); ++i)
            peak = std::max (peak, std::abs (x[(size_t) i] - x[(size_t) i - 1]));
        return peak;
    };
    auto highEnergy = [] (const std::vector<float>& x, int from, int to)
    {
        juce::IIRFilter filter;
        filter.setCoefficients (juce::IIRCoefficients::makeHighPass (rate, 5000.0));
        auto sum = 0.0;
        for (int i = 0; i < std::min (to, (int) x.size()); ++i)
        {
            const auto y = filter.processSingleSampleRaw (x[(size_t) i]);
            if (i >= from)
                sum += (double) y * y;
        }
        return std::sqrt (sum / std::max (1, to - from));
    };
    auto rmsOf = [] (const std::vector<float>& x, int from, int to)
    {
        auto sum = 0.0;
        for (int i = from; i < std::min (to, (int) x.size()); ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (sum / std::max (1, to - from));
    };

    InstrumentParams bright;
    bright.cutoffHz = 2000.0f;
    bright.releaseMs = 300.0f;
    bright.sustain = 0.8f;

    // Allpass: AMOUNT 0 ändrar inget; AMOUNT 1 roterar fasen (annan kurvform) men håller nivån.
    {
        auto render = [&] (float amount)
        {
            auto p = bright;
            p.allpassFreq = 300.0f;
            p.allpassAmount = amount;
            InstrumentVoices v;
            setUp (v, 1, p);
            v.noteOn (0, 0.8f, longGate);
            std::vector<float> out;
            run (v, out, (int) (rate * 0.5));
            return out;
        };
        const auto off = render (0.0f), full = render (1.0f);
        auto base = bright;
        InstrumentVoices v;
        setUp (v, 1, base);
        v.noteOn (0, 0.8f, longGate);
        std::vector<float> reference;
        run (v, reference, (int) (rate * 0.5));
        check (off == reference, "allpass AMOUNT 0 ger exakt samma ljud som utan allpass");
        auto diff = 0.0;
        for (size_t i = 0; i < off.size(); ++i)
            diff += std::abs (off[i] - full[i]);
        check (diff > 1.0, "allpass AMOUNT 1 ändrar kurvformen");
        check (std::abs (toDb (rmsOf (full, 4800, 24000)) - toDb (rmsOf (off, 4800, 24000))) < 1.5, "allpass AMOUNT 1 håller nivån (inom 1,5 dB)");
    }

    // INSTR-MIX: brusets pan och bredd. Pan 0 och bredd 100 % är mono; pan höger tystar vänster; bredd 200 % gör vänster och höger okorrelerade.
    {
        auto render = [&] (float pan, float width, std::vector<float>& l, std::vector<float>& r)
        {
            auto p = bright;
            p.oscA.on = false;
            p.sub = 0.0f;
            p.noise = 0.5f;
            p.noiseTone = 1.0f;
            p.cutoffHz = 18000.0f;
            p.noisePan = pan;
            p.noiseWidth = width;
            InstrumentVoices v;
            setUp (v, 1, p);
            v.noteOn (0, 0.8f, longGate);
            for (int i = 0; i < (int) (rate * 0.3); ++i)
            {
                const auto o = v.next();
                l.push_back (o.left * o.amplitude);
                r.push_back (o.right * o.amplitude);
            }
        };
        std::vector<float> l0, r0, l1, r1, l2, r2;
        render (0.0f, 1.0f, l0, r0);
        render (1.0f, 1.0f, l1, r1);
        render (0.0f, 2.0f, l2, r2);
        check (l0 == r0 && rmsOf (l0, 4800, 14400) > 0.001, "bruset är mono i mitten som standard");
        check (rmsOf (l1, 4800, 14400) < 1.0e-6 && rmsOf (r1, 4800, 14400) > 0.001, "NOISE PAN höger tystar vänster kanal");
        auto dot = 0.0, energy = 1.0e-12;
        for (size_t i = 4800; i < 14400; ++i)
        {
            dot += (double) l2[i] * r2[i];
            energy += 0.5 * ((double) l2[i] * l2[i] + (double) r2[i] * r2[i]);
        }
        check (std::abs (dot / energy) < 0.3, "NOISE WIDTH 200 % ger okorrelerat vänster och höger");
    }

    // START per källa: suben med 180 graders startfas är motsatt till 0 grader.
    {
        auto firstSamples = [&] (float deg)
        {
            auto p = bright;
            p.oscA.on = false;
            p.sub = 0.5f;
            p.subPhaseDeg = deg;
            InstrumentVoices v;
            setUp (v, 1, p);
            v.noteOn (0, 0.8f, longGate);
            std::vector<float> out;
            for (int i = 0; i < 480; ++i)
            {
                const auto o = v.next();
                out.push_back (o.left * o.amplitude);
            }
            return out;
        };
        const auto a = firstSamples (0.0f), b = firstSamples (180.0f);
        auto dot = 0.0, ea = 1.0e-12;
        for (size_t i = 0; i < a.size(); ++i)
        {
            dot += (double) a[i] * b[i];
            ea += (double) a[i] * a[i];
        }
        check (dot / ea < -0.5, "SUB START 180 grader vänder suben mot 0 grader");
    }

    // BREDD per källa: FM 200 % är okorrelerat och 100 % mono; OSC 1 med unison och bredd 0 är mono.
    {
        auto renderLR = [&] (InstrumentParams p, std::vector<float>& l, std::vector<float>& r)
        {
            InstrumentVoices v;
            setUp (v, 1, p);
            v.noteOn (0, 0.8f, longGate);
            for (int i = 0; i < 9600; ++i)
            {
                const auto o = v.next();
                l.push_back (o.left * o.amplitude);
                r.push_back (o.right * o.amplitude);
            }
        };
        auto correlation = [] (const std::vector<float>& l, const std::vector<float>& r)
        {
            auto dot = 0.0, energy = 1.0e-12;
            for (size_t i = 2400; i < l.size(); ++i)
            {
                dot += (double) l[i] * r[i];
                energy += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]);
            }
            return dot / energy;
        };
        auto fmOnly = bright;
        fmOnly.oscA.on = false;
        fmOnly.sub = 0.0f;
        fmOnly.noise = 0.0f;
        fmOnly.fmOn = true;
        fmOnly.fmLevel = 0.6f;
        fmOnly.fmRatio = 2.3f;
        fmOnly.fmIndex = 0.5f;
        std::vector<float> l0, r0, l1, r1;
        renderLR (fmOnly, l0, r0);
        fmOnly.fmWidth = 2.0f;
        renderLR (fmOnly, l1, r1);
        check (l0 == r0, "FM är mono som standard");
        check (std::abs (correlation (l1, r1)) < 0.6 && l1 != r1, "FM WIDTH 200 % ger skillnad mellan vänster och höger");

        auto oscOnly = bright;
        oscOnly.sub = 0.0f;
        oscOnly.noise = 0.0f;
        oscOnly.oscA.unison = 5;
        oscOnly.oscA.detuneCents = 30.0f;
        oscOnly.oscA.stereo = 1.0f;
        std::vector<float> l2, r2, l3, r3;
        renderLR (oscOnly, l2, r2);
        oscOnly.aWidth = 0.0f;
        renderLR (oscOnly, l3, r3);
        check (l2 != r2, "OSC 1 med unison och STEREO är bred som standard");
        check (l3 == r3, "OSC 1 WIDTH 0 gör oscillatorn mono");
    }

    // COMPARE: skillnaden mellan två rapporter.
    {
        MixAnalysis::Report before, after;
        before.valid = after.valid = true;
        MixAnalysis::SourceInfo bass;
        bass.name = "Instr 1";
        bass.present = true;
        bass.rmsDb = -20.0f;
        bass.samplePeakDb = -6.0f;
        before.sources.push_back (bass);
        bass.rmsDb = -17.0f;
        bass.samplePeakDb = -5.0f;
        after.sources.push_back (bass);
        MixAnalysis::Finding gone, same, added;
        gone.category = "collision"; gone.title = "Kick and bass collide";
        same.category = "sub"; same.title = "Sub too loud";
        added.category = "master"; added.title = "Limiter works hard";
        before.findings = { gone, same };
        after.findings = { same, added };
        const auto c = MixAnalysis::compareReports (before, after);
        check (c.valid && c.sources.size() == 1 && std::abs (c.sources[0].rmsDb - 3.0f) < 1.0e-4f, "COMPARE räknar nivåskillnaden per källa");
        check (c.resolved.size() == 1 && c.added.size() == 1 && c.unchanged == 1, "COMPARE skiljer borta, nya och oförändrade fynd");
        check (! MixAnalysis::compareReports (MixAnalysis::Report {}, after).valid, "COMPARE utan sparad rapport är ogiltig");
    }
    // a. Stöld: tre toner med 100 ms mellanrum, 2 röster (den tredje stjäl den första, som hålls) mot 3 röster (ingen stöld).
    {
        auto render = [&] (int voices)
        {
            InstrumentVoices v;
            setUp (v, voices, bright);
            std::vector<float> out;
            const int semis[] { 0, 7, 12 };
            for (const auto s : semis)
            {
                v.noteOn (s, 0.8f, longGate);
                run (v, out, (int) (rate * 0.1));
            }
            run (v, out, (int) (rate * 0.05));
            return out;
        };
        const auto stolen = render (2), reference = render (3);
        const auto at = (int) (rate * 0.2), from = at - (int) (rate * 0.002), to = at + (int) (rate * 0.01);
        const auto jumpS = maxJump (stolen, from, to), jumpR = maxJump (reference, from, to);
        const auto hfS = highEnergy (stolen, from, to), hfR = highEnergy (reference, from, to);
        std::printf ("stöld: största hopp %.4f (utan stöld %.4f), energi över 5 kHz %.1f dB (utan stöld %.1f dB)\n", jumpS, jumpR, toDb (hfS), toDb (hfR));
        check (jumpS <= jumpR * 1.5f + 0.005f && toDb (hfS) <= toDb (hfR) + 3.0, "en stulen röst klickar inte (hopp och energi över 5 kHz nära samma toner utan stöld)");
    }

    // b. Krav 1: en röst som legat oanvänd medan cutoff gick 300 -> 5000 Hz låter som en ny röst med 5000 Hz.
    {
        InstrumentParams dark = bright, open = bright;
        dark.cutoffHz = 300.0f;
        open.cutoffHz = 5000.0f;
        InstrumentVoices v;
        setUp (v, 2, dark);
        std::vector<float> ignore;
        v.noteOn (0, 0.8f, (int) (rate * 0.05));
        run (v, ignore, (int) (rate * 0.02));
        v.noteOn (5, 0.8f, (int) (rate * 0.05));
        for (int i = 0; i < (int) (rate * 2.0) && v.isActive(); ++i)
            v.next();
        check (! v.isActive(), "båda rösterna tystnar");
        v.setParams (open); // (når bara den senaste rösten; den andra ligger kvar med 300 Hz)
        std::vector<float> after, fresh;
        v.noteOn (7, 0.8f, longGate, true);
        v.noteOn (7, 0.8f, longGate);
        run (v, after, (int) (rate * 0.02));
        InstrumentVoices f;
        setUp (f, 2, open);
        f.noteOn (7, 0.8f, longGate, true);
        f.noteOn (7, 0.8f, longGate);
        run (f, fresh, (int) (rate * 0.02));
        const auto n = (int) after.size();
        const auto hfA = toDb (highEnergy (after, 0, n)), hfF = toDb (highEnergy (fresh, 0, n));
        const auto rmsA = toDb (rmsOf (after, 0, n)), rmsF = toDb (rmsOf (fresh, 0, n));
        std::printf ("färsk röst, första 20 ms: RMS %.1f dB (ny instans %.1f dB), över 5 kHz %.1f dB (ny instans %.1f dB)\n", rmsA, rmsF, hfA, hfF);
        check (std::abs (rmsA - rmsF) < 1.0 && std::abs (hfA - hfF) < 1.5, "en röst som legat oanvänd låter som en ny röst (krav 1)");
    }

    // c. Krav 2: VOICES 4 -> 1 medan fyra toner klingar: tre tonas ut på 3 ms (inget klipp), den senaste klingar vidare.
    {
        InstrumentVoices v;
        InstrumentParams soft = bright;
        soft.cutoffHz = 600.0f;
        setUp (v, 4, soft);
        std::vector<float> out;
        const int semis[] { 0, 3, 7, 12 };
        for (const auto s : semis)
            v.noteOn (s, 0.8f, longGate, s != 12);
        run (v, out, (int) (rate * 0.1));
        const auto sounding = v.countSounding();
        const auto at = (int) out.size();
        v.setNumVoices (1);
        run (v, out, (int) (rate * 0.05));
        const auto steady = maxJump (out, at - (int) (rate * 0.01), at), change = maxJump (out, at, at + (int) (rate * 0.01));
        std::printf ("VOICES 4 -> 1: klingande %d -> %d, största hopp %.4f (före ändringen %.4f)\n", sounding, v.countSounding(), change, steady);
        check (sounding == 4 && v.countSounding() == 1 && change <= steady * 1.5f + 0.005f, "VOICES ned tonar ut rösterna över gränsen (krav 2)");
    }

    // d. Krav 7: seed. Två renderingar ger samma ljud, och två röster med samma ton och slumpad fas låter olika (egna seeds).
    {
        InstrumentParams random = bright;
        random.oscA.randomPhase = 1.0f;
        random.oscA.unison = 3;
        auto render = [&] (int voices, int notes)
        {
            InstrumentVoices v;
            setUp (v, voices, random);
            std::vector<float> out;
            for (int k = 0; k < notes; ++k)
                v.noteOn (0, 0.8f, longGate, k < notes - 1);
            run (v, out, (int) (rate * 0.05));
            return out;
        };
        const auto twoA = render (2, 2), twoB = render (2, 2), one = render (1, 1);
        auto differs = 0.0f;
        for (size_t i = 0; i < one.size(); ++i)
            differs = std::max (differs, std::abs (twoA[i] - 2.0f * one[i]));
        check (twoA == twoB, "samma seed ger samma ljud med flera röster (krav 7)");
        check (differs > 1.0e-3f, "rösterna har egna seeds (två röster på samma ton är inte samma ljud x 2)");
    }

    // Ackordets timing i processorn: med swing och humanize startar ackordtonen på samma sample som grundtonen.
    {
        using F = InstrumentField;
        auto render = [] (bool chord, int voices)
        {
            auto processor = make (false);
            Scenarios::setPlain (*processor, ParamID::kickOn, 0.0f);
            for (int layer = 1; layer < ParamID::numInstrumentLayers; ++layer)
                Scenarios::setInstrument (*processor, layer, F::on, 0.0f);
            Scenarios::setPlain (*processor, ParamID::patSwing, 0.6f);
            Scenarios::setPlain (*processor, ParamID::patHumanize, 0.8f);
            Scenarios::setInstrument (*processor, 0, F::voices, (float) voices);
            Scenarios::setInstrument (*processor, 0, F::noise, 0.0f);
            for (const auto field : { F::aRandom, F::bRandom, F::cRandom })
                Scenarios::setInstrument (*processor, 0, field, 0.0f);
            for (const auto field : { F::aUnison, F::bUnison, F::cUnison })
                Scenarios::setInstrument (*processor, 0, field, 1.0f);
            processor->pattern.clearNotes (0);
            for (int tick = Pattern::ticksPerQuarter / 4; tick < Pattern::ticksPerBar; tick += Pattern::ticksPerQuarter)
                processor->pattern.addNote (0, tick, Pattern::ticksPerQuarter / 8, 0, 0.8f, false, chord ? Pattern::Chord { 12, 0, 0 } : Pattern::Chord {});
            processor->ensureTablesInUse();
            OfflineRender::Settings settings;
            settings.sampleRate = rate;
            settings.blockSize = 480;
            settings.bars = 1;
            settings.notes = { 52 };
            return OfflineRender::render (*processor, settings);
        };
        auto onsets = [] (const juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>* minus)
        {
            std::vector<int> result;
            auto silent = 0;
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                const auto x = audio.getSample (0, i) - (minus != nullptr ? minus->getSample (0, i) : 0.0f);
                if (x != 0.0f) // (första sample som inte är exakt noll: attacken börjar mjukt, en tröskel ger fel start)
                {
                    if (silent > (int) (rate * 0.05) || (result.empty() && silent == i))
                        result.push_back (i);
                    silent = 0;
                }
                else
                    ++silent;
            }
            return result;
        };
        const auto rootOnly = render (false, 2), withChord = render (true, 2);
        const auto roots = onsets (rootOnly, nullptr), chords = onsets (withChord, &rootOnly);
        juce::String list;
        for (size_t i = 0; i < std::min (roots.size(), chords.size()); ++i)
            list << roots[i] << "/" << chords[i] << " ";
        std::printf ("start grundton/ackordton (sample): %s\n", list.toRawUTF8());
        check (! roots.empty() && roots == chords, "ackordtonerna startar på samma sample som grundtonen, med swing och humanize");
    }

    // Minne och prepareToPlay-tid per instans: 10 instrument (rösterna finns alltid, 8 per instrument, oavsett VOICES).
    {
        auto privateBytes = []() -> double
        {
           #if JUCE_WINDOWS
            PROCESS_MEMORY_COUNTERS_EX counters {};
            if (K32GetProcessMemoryInfo (GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &counters, sizeof (counters)))
                return (double) counters.PrivateUsage;
           #endif
            return 0.0;
        };
        std::printf ("sizeof InstrumentVoices %d kB, InstrumentSynth %d kB\n", (int) (sizeof (InstrumentVoices) / 1024), (int) (sizeof (InstrumentSynth) / 1024));
        for (const auto sampleRate : { 48000.0, 192000.0 })
        {
            // Rösternas egen andel: 10 x 8 förberedda röster (kamfiltrens fördröjningslinjer m.m.).
            const auto start = privateBytes();
            auto voices = std::make_unique<std::array<InstrumentVoices, ParamID::numInstrumentLayers>>();
            for (auto& v : *voices)
                v.prepare (sampleRate);
            const auto bytes = privateBytes() - start;
            std::printf ("%.0f kHz: 80 röster %.1f MB\n", sampleRate / 1000.0, bytes / 1048576.0);
            check (bytes < 50.0 * 1048576.0, juce::String (sampleRate / 1000.0, 0) + " kHz: rösterna tar under 50 MB per instans");
        }
        for (const auto sampleRate : { 48000.0, 192000.0 })
        {
            const auto before = privateBytes();
            auto processor = std::make_unique<InstrumentProcessor>();
            const auto created = privateBytes();
            processor->setInstrumentCount (ParamID::numInstrumentLayers);
            for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
                Scenarios::setInstrument (*processor, layer, InstrumentField::voices, 8.0f);
            processor->setPlayConfigDetails (0, 2, sampleRate, 512);
            const auto start = juce::Time::getMillisecondCounterHiRes();
            processor->prepareToPlay (sampleRate, 512);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            const auto prepared = privateBytes();
            std::printf ("%.0f kHz: instans %.1f MB (skapad %.1f + prepareToPlay %.1f), prepareToPlay %.0f ms\n", sampleRate / 1000.0,
                         (prepared - before) / 1048576.0, (created - before) / 1048576.0, (prepared - created) / 1048576.0, ms);
            check (ms < 500.0, juce::String (sampleRate / 1000.0, 0) + " kHz: prepareToPlay under 0,5 s"); // (instansens minne utöver rösterna fanns före fas P)
        }
    }

    std::puts (failed ? "FEL: polytestet misslyckades" : "Polytestet gick igenom");
    return failed ? 1 : 0;
}

// Att ljudet inte beror på DAW:ens blockstorlek: nolltest mot block 32, också med udda storlekar. Sedan fråga 1 räknas moduleringen
// och blockbehandlingen var 32:a sample (PluginBase::processInControlChunks), så skillnaden ska vara exakt noll; förut var den
// upp till -1,7 dBFS. Testet fallerar om något skiljer.
int blockSizeTest()
{
    auto failed = false;
    juce::ScopedJuceInitialiser_GUI gui;
    // läge 0 = instrumentets Init, 1 = instrumentet i tungt läge, 2 = Kickbase FX i tungt läge (med en sågtand som insignal),
    // 3 = instrumentet i tungt läge med TB-303-modellen (fas D1), 4 = instrumentet i tungt läge med ENV 2/3 i läget Sync, 5 = tungt läge med Ladder, Formant och Comb (fas D2)
    auto renderWith = [] (int mode, int blockSize)
    {
        const auto heavy = mode > 0;
        auto processor = make (mode == 2);
        if (heavy)
        {
            Scenarios::makeHeavy (*processor, mode == 2 ? 1 : 3);
            if (mode == 3)
                useTb303 (*processor, 3);
            if (mode == 4)
                useEnvSync (*processor, 3);
            if (mode == 5)
                useFilterModels (*processor, 3);
            if (mode == 7)
                usePolyphony (*processor, 3);
            if (mode == 6)
            {
                useFilterModels (*processor, 3, 3); // (Filter 1 med en annan modell än Filter 2, så att kombinationerna är olika)
                useFilter2 (*processor, 3);
            }
            // Felsökning: KB_BS_RESET=ord1,ord2 återställer de parametrar vars ID innehåller något av orden (ringar in vad som beror på blocken).
            const auto tokens = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("KB_BS_RESET", {}), ",", {});
            for (auto* p : processor->getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                    for (const auto& token : tokens)
                        if (token.isNotEmpty() && ranged->getParameterID().contains (token))
                            ranged->setValueNotifyingHost (ranged->getDefaultValue());
        }
        processor->ensureTablesInUse();
        OfflineRender::Settings settings;
        settings.sampleRate = 48000.0;
        settings.blockSize = blockSize;
        settings.bpm = 145.0;
        settings.bars = 2;
        settings.notes = { 52 };
        settings.extraBars = 1;
        settings.fillInput = [] (juce::AudioBuffer<float>& block, int start) { sawInput (block, start, 48000.0); };
        return OfflineRender::render (*processor, settings);
    };
    const char* names[] { "Init", "tungt läge (LFO, ENV 2/3)", "Kickbase FX, tungt läge", "tungt läge med TB-303", "tungt läge med ENV Sync", "tungt läge: Ladder/Formant/Comb", "tungt läge med Filter 2",
                          "tungt läge med 2/4/8 röster (stealing)" };
    juce::AudioBuffer<float> monoReference;
    for (const auto mode : { 0, 1, 2, 3, 4, 5, 6, 7 })
    {
        const auto reference = renderWith (mode, 32);
        if (mode == 1)
            monoReference = reference;
        if (mode == 7 && hashOf (reference) == hashOf (monoReference))
        {
            std::puts ("FEL: flera röster låter som en röst");
            failed = true;
        }
        const auto referenceRms = reference.getRMSLevel (0, 0, reference.getNumSamples());
        for (const auto blockSize : { 37, 64, 100, 256, 512, 1024, 2048 })
        {
            const auto audio = renderWith (mode, blockSize);
            auto peak = 0.0f;
            auto sum = 0.0;
            auto first = -1;
            for (int i = 0; i < std::min (audio.getNumSamples(), reference.getNumSamples()); ++i)
            {
                const auto d = audio.getSample (0, i) - reference.getSample (0, i);
                if (d != 0.0f && first < 0)
                    first = i;
                peak = std::max (peak, std::abs (d));
                sum += (double) d * d;
            }
            const auto rms = std::sqrt (sum / std::max (1, audio.getNumSamples()));
            failed |= peak > 0.0f || audio.getNumSamples() != reference.getNumSamples();
            if (first >= 0)
                std::printf ("  första skillnaden vid sample %d (%.3f s)\n", first, first / 48000.0);
            std::printf ("%s, block %4d mot 32: skillnad topp %.1f dBFS, rms %.1f dB under signalen\n", names[mode], blockSize,
                         juce::Decibels::gainToDecibels (peak, -200.0f), juce::Decibels::gainToDecibels (referenceRms, -200.0f) - juce::Decibels::gainToDecibels ((float) rms, -200.0f));
        }
    }
    std::puts (failed ? "FEL: ljudet beror på blockstorleken" : "Blockstorlekstestet gick igenom (identiskt ljud)");
    return failed ? 1 : 0;
}

// Filter 2 (fas D3) i synten: riktiga renderingar av ett instrument (kicken avstängd) med olika inställningar.
int filter2Test()
{
    juce::ScopedJuceInitialiser_GUI gui;
    using F = InstrumentField;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    auto render = [] (const std::function<void (PluginBase&)>& setup)
    {
        auto processor = make (false);
        Scenarios::setPlain (*processor, ParamID::kickOn, 0.0f);
        Scenarios::setPlain (*processor, ParamID::kickLevel, 0.0f);
        for (int layer = 1; layer < ParamID::numInstrumentLayers; ++layer)
            Scenarios::setInstrument (*processor, layer, F::on, 0.0f);
        setup (*processor);
        processor->ensureTablesInUse();
        OfflineRender::Settings settings;
        settings.sampleRate = 48000.0;
        settings.blockSize = 480;
        settings.bpm = 145.0;
        settings.bars = 1;
        settings.notes = { 52 };
        settings.extraBars = 1;
        return OfflineRender::render (*processor, settings);
    };
    auto set = [] (PluginBase& processor, F field, float value) { Scenarios::setInstrument (processor, 0, field, value); };
    auto rms = [] (const juce::AudioBuffer<float>& audio) { return audio.getRMSLevel (0, 0, audio.getNumSamples()); };
    auto maxDifference = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        auto peak = 0.0f;
        for (int i = 0; i < std::min (a.getNumSamples(), b.getNumSamples()); ++i)
            peak = std::max (peak, std::abs (a.getSample (0, i) - b.getSample (0, i)));
        return a.getNumSamples() == b.getNumSamples() ? peak : 1.0e9f;
    };

    // Grundljudet: Filter 1 = LP 12 vid 3 kHz.
    auto base = [&] (PluginBase& processor)
    {
        set (processor, F::aTable, (float) Wavetables::classicIndex (2)); // sågtand: gott om övertoner att filtrera
        set (processor, F::envAmount, 0.0f); // (filter-envelopen öppnar annars cutoff och döljer skillnaden)
        set (processor, F::keyTrack, 0.0f);
        set (processor, F::filterType, 1.0f);
        set (processor, F::cutoff, 3000.0f);
        set (processor, F::resonance, 0.2f);
    };
    const auto off = render (base);
    check (rms (off) > 0.01f, "grundljudet hörs (rms " + juce::String (rms (off), 3) + ")");
    const auto silent = render ([&] (PluginBase& processor) { base (processor); set (processor, F::on, 0.0f); });
    check (rms (silent) < 1.0e-4f, "med instrumentet av är det tyst (testet mäter bara instrumentet, rms " + juce::String (rms (silent), 5) + ")");

    // Filter 2 med MIX 0: Serial och Parallel lämnar ljudet som med Filter 1 ensamt (exakt: sample += 0 * ...).
    for (const auto routing : { 0.0f, 1.0f })
    {
        const auto audio = render ([&] (PluginBase& processor)
        {
            base (processor);
            set (processor, F::filter2On, 1.0f);
            set (processor, F::filter2Model, 2.0f);
            set (processor, F::filter2Cutoff, 400.0f);
            set (processor, F::filterRouting, routing);
            set (processor, F::filterMix, 0.0f);
        });
        check (maxDifference (off, audio) == 0.0f, juce::String (routing == 0.0f ? "Serial" : "Parallel") + ", MIX 0: ljudet är exakt som med Filter 1 ensamt");
    }

    // Serial: Filter 1 = LP 12 vid 300 Hz följt av Filter 2 = High vid 4 kHz: nästan ingenting kommer igenom bandet 300 Hz - 4 kHz.
    const auto lowOnly = render ([&] (PluginBase& processor) { base (processor); set (processor, F::cutoff, 300.0f); });
    const auto narrow = render ([&] (PluginBase& processor)
    {
        base (processor);
        set (processor, F::cutoff, 300.0f);
        set (processor, F::filter2On, 1.0f);
        set (processor, F::filter2Model, 0.0f);
        set (processor, F::filter2Type, 3.0f);
        set (processor, F::filter2Cutoff, 4000.0f);
        set (processor, F::filterMix, 1.0f);
    });
    check (rms (narrow) < 0.1f * rms (lowOnly), "Serial: LP 300 Hz följt av High 4 kHz tar nästan bort allt (rms " + juce::String (rms (narrow), 4) + " mot " + juce::String (rms (lowOnly), 4) + ")");

    // Parallel med MIX 1: bara Filter 2 hörs, som om Filter 2:s inställning låg i Filter 1 (inom avrundning).
    const auto parallel = render ([&] (PluginBase& processor)
    {
        base (processor);
        set (processor, F::cutoff, 300.0f);
        set (processor, F::filter2On, 1.0f);
        set (processor, F::filter2Model, 0.0f);
        set (processor, F::filter2Type, 2.0f);
        set (processor, F::filter2Cutoff, 1500.0f);
        set (processor, F::filter2Reso, 0.2f);
        set (processor, F::filterRouting, 1.0f);
        set (processor, F::filterMix, 1.0f);
    });
    const auto filter2Alone = render ([&] (PluginBase& processor)
    {
        base (processor);
        set (processor, F::filterType, 2.0f);
        set (processor, F::cutoff, 1500.0f);
        set (processor, F::resonance, 0.2f);
    });
    check (maxDifference (parallel, filter2Alone) < 1.0e-4f, "Parallel, MIX 1: bara Filter 2 hörs (största skillnad " + juce::String (maxDifference (parallel, filter2Alone), 7) + ")");

    // Båda på med resonans 100 %, alla modellkombinationer: inga NaN och ingen skenande nivå.
    auto worst = 0.0f;
    auto finite = true;
    for (int model1 = 0; model1 < 5; ++model1)
        for (int model2 = 0; model2 < 5; ++model2)
        {
            const auto audio = render ([&] (PluginBase& processor)
            {
                base (processor);
                set (processor, F::filterModel, (float) model1);
                set (processor, F::resonance, 1.0f);
                set (processor, F::filter2On, 1.0f);
                set (processor, F::filter2Model, (float) model2);
                set (processor, F::filter2Reso, 1.0f);
                set (processor, F::filter2P1, 1.0f);
                set (processor, F::filterRouting, (float) ((model1 + model2) % 2));
            });
            finite = finite && allFiniteIn (audio);
            worst = std::max (worst, audio.getMagnitude (0, audio.getNumSamples()));
        }
    check (finite && worst < 20.0f, "alla 25 modellkombinationer vid resonans 100 %: inga NaN, största topp " + juce::String (worst, 2));

    // S2: FM-källan. På med nivå 0 = exakt som av; på med nivå > 0 hörs; ratio x index x oktav (även ±24) ger inga NaN och ingen skenande nivå.
    const auto fmSilent = render ([&] (PluginBase& processor) { base (processor); set (processor, F::fmOn, 1.0f); set (processor, F::fmLevel, 0.0f); });
    check (maxDifference (off, fmSilent) == 0.0f, "FM på med nivå 0: ljudet är exakt som med FM av");
    const auto fmAudible = render ([&] (PluginBase& processor) { base (processor); set (processor, F::fmOn, 1.0f); set (processor, F::fmLevel, 0.8f); });
    check (maxDifference (off, fmAudible) > 0.05f, "FM på med nivå 0.8 hörs");
    auto fmWorst = 0.0f;
    auto fmFinite = true;
    for (const auto ratio : { 1.0f, 2.0f, 3.5f, 8.0f })
        for (const auto index : { 0.0f, 0.5f, 1.0f })
            for (const auto semis : { -24.0f, 0.0f, 24.0f })
            {
                const auto audio = render ([&] (PluginBase& processor)
                {
                    base (processor);
                    set (processor, F::fmOn, 1.0f);
                    set (processor, F::fmLevel, 1.0f);
                    set (processor, F::fmRatio, ratio);
                    set (processor, F::fmIndex, index);
                    set (processor, F::fmSemis, semis);
                    set (processor, F::fmEnvAmount, 0.5f);
                });
                fmFinite = fmFinite && allFiniteIn (audio);
                fmWorst = std::max (fmWorst, audio.getMagnitude (0, audio.getNumSamples()));
            }
    check (fmFinite && fmWorst < 4.0f, "FM, ratio 1-8 x index 0-1 x +-24 halvtoner: inga NaN, största topp " + juce::String (fmWorst, 2));

    // S2c: varje FM-preset ger en hörbar, ändlig signal, och namnlistan följer FM Preset-parameterns val.
    check (FmPresets::names().size() == FmPresets::count(), "FM-presettabellen och namnlistan har lika många poster");
    for (int preset = 1; preset < FmPresets::count(); ++preset)
    {
        const auto audio = render ([&] (PluginBase& processor) { base (processor); FmPresets::apply (processor.apvts, 0, preset); });
        check (allFiniteIn (audio) && rms (audio) > 0.001f && audio.getMagnitude (0, audio.getNumSamples()) < 4.0f,
               "FM-preset " + juce::String (FmPresets::get (preset).name) + ": hörbar och ändlig signal");
    }

    std::puts (failed ? "Filter 2: FEL" : "Filter 2 gick igenom");
    return failed ? 1 : 0;
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

    // TB-303-modellen (fas D1): tungt läge med 10 instrument, och modellen som byts mellan blocken medan resonansen står på 100 % (inga allokeringar,
    // inga icke-ändliga värden). Halvbandsdesignen skapas i konstruktionen/prepare, aldrig på ljudtråden.
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 10);
        useTb303 (*processor, 10);
        report ("TB-303, tungt läge, 10 instrument, 48k/512", run (*processor, 48000.0, 512, 400, {}, false));

        auto& p = *processor;
        for (int layer = 0; layer < 10; ++layer)
            Scenarios::setInstrument (p, layer, InstrumentField::resonance, 1.0f);
        auto between = [&p] (int block) { useTb303 (p, 10, (block / 7) % 2 == 0 ? 1.0f : 0.0f); };
        report ("modellbyte TB-303 <-> Kickbase var sjunde block, resonans 100 %", run (p, 48000.0, 256, 400, between, false));
    }

    // Fas D2: Ladder, Formant och Comb, med byte mellan alla fem modellerna var sjätte block (och olika ratter), resonans 100 %.
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 10);
        auto& p = *processor;
        for (int layer = 0; layer < 10; ++layer)
            Scenarios::setInstrument (p, layer, InstrumentField::resonance, 1.0f);
        auto between = [&p] (int block)
        {
            for (int layer = 0; layer < 10; ++layer)
            {
                Scenarios::setInstrument (p, layer, InstrumentField::filterModel, (float) (((block / 6) + layer) % 5));
                Scenarios::setInstrument (p, layer, InstrumentField::filterP1, (float) (block % 11) / 10.0f);
                Scenarios::setInstrument (p, layer, InstrumentField::filterP2, (float) ((block / 3) % 5) / 4.0f);
            }
        };
        report ("filtermodellerna byts var sjätte block (Kickbase, TB-303, Ladder, Formant, Comb), resonans 100 %", run (p, 48000.0, 256, 400, between, false));
    }

    // Fas D3: Filter 2 på alla tio instrument; modellerna i båda filtren, routing, MIX och på/av ändras var femte block, resonans 100 %.
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 10);
        auto& p = *processor;
        useFilter2 (p, 10);
        for (int layer = 0; layer < 10; ++layer)
        {
            Scenarios::setInstrument (p, layer, InstrumentField::resonance, 1.0f);
            Scenarios::setInstrument (p, layer, InstrumentField::filter2Reso, 1.0f);
        }
        auto between = [&p] (int block)
        {
            for (int layer = 0; layer < 10; ++layer)
            {
                Scenarios::setInstrument (p, layer, InstrumentField::filterModel, (float) (((block / 5) + layer) % 5));
                Scenarios::setInstrument (p, layer, InstrumentField::filter2Model, (float) (((block / 5) + 2 * layer) % 5));
                Scenarios::setInstrument (p, layer, InstrumentField::filterRouting, (float) ((block / 5 + layer) % 2));
                Scenarios::setInstrument (p, layer, InstrumentField::filterMix, (float) (block % 5) / 4.0f);
                Scenarios::setInstrument (p, layer, InstrumentField::filter2On, (block / 7 + layer) % 3 == 0 ? 0.0f : 1.0f);
            }
        };
        report ("Filter 2 på alla instrument, modeller, routing och MIX ändras var femte block, resonans 100 %", run (p, 48000.0, 256, 400, between, false));
    }

    // Rattar, EQ, kompressor, effekttyper och modulering som ändras mellan blocken (som automation eller en användare).
    {
        auto processor = make (false);
        Scenarios::makeHeavy (*processor, 6);
        auto& p = *processor;
        auto between = [&p] (int block)
        {
            const auto t = (float) (block % 50) / 50.0f;
            Scenarios::setPlain (p, ParamID::bass (0, InstrumentField::cutoff), 200.0f + 2000.0f * t);
            Scenarios::setPlain (p, "kickEq2G", -6.0f + 12.0f * t);
            Scenarios::setPlain (p, "bass2Eq3G", 6.0f * t);
            Scenarios::setPlain (p, "masterEq1F", 60.0f + 100.0f * t);
            Scenarios::setPlain (p, Effects::channelParamID (2, "Eq1G"), 8.0f * t);
            Scenarios::setPlain (p, Effects::channelParamID (3, "CompThr"), -30.0f * t);
            Scenarios::setPlain (p, Effects::knobParamID (1, 0, 1), t);
            Scenarios::setPlain (p, "masterDrive", 6.0f * t);
            Scenarios::setPlain (p, ParamID::bass (3, InstrumentField::aUnison), (float) (1 + block % 7));
            if (block % 40 == 20)
                Scenarios::setPlain (p, Effects::slotParamID (4, 2, "Type"), (float) (1 + (block / 40) % (numEffectTypes - 2)));
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
        auto* cutoff = processor->apvts.getParameter (ParamID::bass (0, InstrumentField::cutoff));
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
