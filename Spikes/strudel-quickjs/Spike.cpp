// Spike: går Strudel att köra i JUCE:s JavascriptEngine (QuickJS) på en bakgrundstråd, och stoppar det ljudtråden?
// Strudel (js/strudel.all.js): Copyright (C) Strudel contributors, AGPL-3.0-or-later, https://codeberg.org/uzu/strudel
#include <juce_core/juce_core.h>
#include <juce_javascript/juce_javascript.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <thread>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#endif

using Clock = std::chrono::steady_clock;
static double msSince (Clock::time_point t) { return std::chrono::duration<double, std::milli> (Clock::now() - t).count(); }

static double workingSetMB()
{
   #if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc {};
    GetProcessMemoryInfo (GetCurrentProcess(), &pmc, sizeof (pmc));
    return (double) pmc.WorkingSetSize / (1024.0 * 1024.0);
   #else
    return 0;
   #endif
}

// ---- C++-sidan av en händelse (det Strudel-mönstret ger oss) ----
struct Hap
{
    double begin = 0, end = 0;
    bool hasOnset = false;
    std::map<juce::String, juce::var> fields; // s, note, n, cutoff, gain ...
};

static std::vector<Hap> parseHaps (const juce::String& json)
{
    std::vector<Hap> out;
    auto arr = juce::JSON::parse (json);
    if (auto* a = arr.getArray())
        for (auto& h : *a)
        {
            Hap hap;
            hap.begin = h["b"]; hap.end = h["e"]; hap.hasOnset = (bool) h["hasOnset"];
            if (auto* obj = h["v"].getDynamicObject())
                for (auto& p : obj->getProperties())
                    hap.fields[p.name.toString()] = p.value;
            out.push_back (std::move (hap));
        }
    return out;
}

// ---- host-objektet som JS ser (slider-registrering) ----
struct SliderState
{
    std::mutex m;
    std::map<int, std::array<double, 4>> sliders; // id -> {värde, min, max, ändrat}
};

struct HostObject : juce::DynamicObject
{
    SliderState& state;
    explicit HostObject (SliderState& s) : state (s)
    {
        // slider(id, v, min, max): registrerar/uppdaterar reglaget och svarar med nuvarande värde.
        setMethod ("slider", [this] (const juce::var::NativeFunctionArgs& a) -> juce::var
        {
            if (a.numArguments < 4) return {};
            std::lock_guard<std::mutex> l (state.m);
            int id = a.arguments[0];
            auto it = state.sliders.find (id);
            if (it == state.sliders.end())
                it = state.sliders.emplace (id, std::array<double, 4> { a.arguments[1], a.arguments[2], a.arguments[3], 0 }).first;
            return it->second[0];
        });
        setMethod ("get", [this] (const juce::var::NativeFunctionArgs& a) -> juce::var
        {
            if (a.numArguments < 1) return 0.0;
            std::lock_guard<std::mutex> l (state.m);
            auto it = state.sliders.find ((int) a.arguments[0]);
            if (it == state.sliders.end()) return 0.0;
            return (it->second[0] - it->second[1]) / (it->second[2] - it->second[1]); // normaliserat 0..1
        });
    }
};

// ---- hjälpare ----
static bool setPattern (juce::JavascriptEngine& js, const juce::String& code, juce::String& err)
{
    juce::Result res = juce::Result::ok();
    js.evaluate ("setPattern(" + juce::JSON::toString (code) + ")", &res);
    err = res.getErrorMessage();
    return res.wasOk();
}

static juce::String queryJson (juce::JavascriptEngine& js, double from, double to, juce::Result* r = nullptr)
{
    juce::Result res = juce::Result::ok();
    auto v = js.evaluate ("queryJson(" + juce::String (from, 6) + "," + juce::String (to, 6) + ")", &res);
    if (r != nullptr) *r = res;
    return v.toString();
}

struct Stats { double max = 0, p999 = 0, mean = 0; int n = 0, late = 0; };

// Simulerad ljudtråd: 256 samplingar @48 kHz = 5.33 ms per block. Mäter hur sent varje block vaknar.
static Stats runAudioSim (double seconds, const std::function<void()>& background)
{
    std::atomic<bool> stop { false };
    std::vector<double> lateness;
    std::thread audio ([&]
    {
        const auto period = std::chrono::microseconds (5333);
        auto next = Clock::now() + period;
        while (! stop)
        {
            std::this_thread::sleep_until (next);
            lateness.push_back (std::chrono::duration<double, std::milli> (Clock::now() - next).count());
            volatile double x = 0; for (int i = 0; i < 20000; ++i) x = x + i * 0.5; // låtsas-DSP
            next += period;
        }
    });
    std::thread bg (background);
    std::this_thread::sleep_for (std::chrono::duration<double> (seconds));
    stop = true; bg.join(); audio.join();
    Stats s; s.n = (int) lateness.size();
    auto sorted = lateness; std::sort (sorted.begin(), sorted.end());
    s.max = sorted.back(); s.p999 = sorted[(size_t) ((double) sorted.size() * 0.999)];
    for (auto l : lateness) { s.mean += l; if (l > 2.0) ++s.late; }
    s.mean /= s.n;
    return s;
}

static void spike (const juce::File& bundle)
{
    const juce::String src = bundle.loadFileAsString();

    // 1. Ladda
    const double mem0 = workingSetMB();
    SliderState sliders;
    juce::JavascriptEngine js;
    js.maximumExecutionTime = juce::RelativeTime::seconds (10);
    js.registerNativeObject ("host", new HostObject (sliders));
    auto t0 = Clock::now();
    auto r = js.execute (src);
    const double loadMs = msSince (t0);
    const double mem1 = workingSetMB();
    std::printf ("[1] ladda bunten (%lld kB): %s, %.1f ms, minne %.1f -> %.1f MB (+%.1f)\n", bundle.getSize() / 1024,
                 r.wasOk() ? "OK" : r.getErrorMessage().toRawUTF8(), loadMs, mem0, mem1, mem1 - mem0);
    if (r.failed()) return;

    // 2/3/4. Fem mönster: eval, queryArc(0,4) till C++-structar, 1000 frågor à en cykel
    struct P { const char* code; } pats[] = {
        { "s(\"bd*4, ~ hh\")" },
        { "note(\"c2 [e2 g2]*2\").s(\"bass\")" },
        { "n(\"0 3 5 <7 10>\")" },
        { "s(\"bd*4\").every(3, x=>x.fast(2)).sometimes(x=>x.speed(2))" },
        { "note(\"c2 e2 g2 b2\").s(\"saw\").cutoff(sine.range(200,2000))" },
    };
    int idx = 0;
    for (auto& p : pats)
    {
        ++idx; juce::String err;
        const double m0 = workingSetMB();
        auto te = Clock::now();
        bool ok = setPattern (js, p.code, err);
        const double evalMs = msSince (te);
        if (! ok) { std::printf ("[%d] %s: setPattern FEL %s\n", idx, p.code, err.toRawUTF8()); continue; }

        auto tq = Clock::now();
        auto json = queryJson (js, 0, 4);
        const double q4Ms = msSince (tq);
        auto tp = Clock::now();
        auto haps = parseHaps (json);
        const double parseMs = msSince (tp);

        std::vector<double> times, timesParse;
        for (int i = 0; i < 1000; ++i)
        {
            auto a = Clock::now();
            auto s = queryJson (js, i, i + 1);
            times.push_back (msSince (a));
            auto b = Clock::now();
            auto hs = parseHaps (s);
            timesParse.push_back (msSince (b));
        }
        std::sort (times.begin(), times.end()); std::sort (timesParse.begin(), timesParse.end());
        std::printf ("[%d] %s\n    eval %.2f ms | queryArc(0,4): %.2f ms, %zu haps (+%.2f ms JSON->C++)\n"
                     "    1000 x 1 cykel: median %.3f ms, max %.3f ms (JS+JSON) | parse till C++ median %.3f ms | minne +%.1f MB\n",
                     idx, p.code, evalMs, q4Ms, haps.size(), parseMs,
                     times[500], times.back(), timesParse[500], workingSetMB() - m0);
        for (size_t i = 0; i < std::min<size_t> (3, haps.size()); ++i)
        {
            std::printf ("      hap %.3f-%.3f onset=%d {", haps[i].begin, haps[i].end, (int) haps[i].hasOnset);
            for (auto& f : haps[i].fields) std::printf (" %s=%s", f.first.toRawUTF8(), f.second.toString().toRawUTF8());
            std::printf (" }\n");
        }
    }

    // 5. Avbrott: oändlig slinga, och är motorn användbar efteråt?
    {
        juce::String err;
        setPattern (js, pats[0].code, err);
        js.maximumExecutionTime = juce::RelativeTime::milliseconds (500);
        auto a = Clock::now();
        auto rr = js.execute ("while(true){}");
        std::printf ("[5] maximumExecutionTime 500 ms: slingan avbröts efter %.0f ms: %s\n", msSince (a),
                     rr.failed() ? rr.getErrorMessage().toRawUTF8() : "AVBRÖTS INTE");
        js.maximumExecutionTime = juce::RelativeTime::seconds (10);
        std::printf ("    motorn efteråt: %zu haps, state kvar: %s\n", parseHaps (queryJson (js, 0, 1)).size(),
                     js.evaluate ("typeof Strudel").toString().toRawUTF8());

        std::thread stopper ([&] { std::this_thread::sleep_for (std::chrono::milliseconds (300)); js.stop(); });
        a = Clock::now();
        rr = js.execute ("while(true){}");
        stopper.join();
        std::printf ("    stop() från annan tråd: avbröts efter %.0f ms: %s\n", msSince (a), rr.failed() ? rr.getErrorMessage().toRawUTF8() : "AVBRÖTS INTE");
        std::printf ("    motorn efter stop(): %zu haps\n", parseHaps (queryJson (js, 0, 1)).size());

        // tung (inte oändlig) mönsterberäkning inuti Strudel, inte bara rå slinga
        js.maximumExecutionTime = juce::RelativeTime::milliseconds (500);
        setPattern (js, "s(\"bd*4\").fast(1000000)", err);
        a = Clock::now();
        auto qr = juce::Result::ok(); queryJson (js, 0, 1, &qr);
        std::printf ("    explosiv fråga (fast(1e6)): %s efter %.0f ms\n", qr.failed() ? "avbröts" : "KLAR", msSince (a));
        js.maximumExecutionTime = juce::RelativeTime::seconds (10);

        bool bad = setPattern (js, "s(\"bd\").nonexistent(3)", err);
        std::printf ("    felaktig kod: %s (%s)\n", bad ? "ACCEPTERADES" : "avvisades", err.toRawUTF8());
        setPattern (js, pats[0].code, err);
        std::printf ("    motorn efter felaktig kod: %zu haps\n", parseHaps (queryJson (js, 0, 1)).size());
    }

    // 6. slider(v,min,max) från C++
    {
        juce::String err;
        sliders.sliders.clear();
        bool ok = setPattern (js, "note(\"c2 e2\").cutoff(slider(800, 200, 2000))", err);
        auto h1 = parseHaps (queryJson (js, 0, 1));
        std::printf ("[6] slider(800,200,2000): %s, registrerade reglage: %zu", ok ? "OK" : err.toRawUTF8(), sliders.sliders.size());
        if (! sliders.sliders.empty()) std::printf (" (id0 v=%g min=%g max=%g)", sliders.sliders[0][0], sliders.sliders[0][1], sliders.sliders[0][2]);
        std::printf ("\n    cutoff i hap 0 = %s", h1.empty() ? "?" : h1[0].fields["cutoff"].toString().toRawUTF8());
        sliders.sliders[0][0] = 1500; // C++ ändrar reglaget (användaren drar)
        auto h2 = parseHaps (queryJson (js, 0, 1));
        std::printf ("; efter reglagedrag (utan ny eval) = %s (konstant tills ny eval, väntat)\n", h2.empty() ? "?" : h2[0].fields["cutoff"].toString().toRawUTF8());
        setPattern (js, "note(\"c2 e2\").cutoff(sliderLive(0, 200, 2000))", err);
        sliders.sliders[0][0] = 200; auto h3 = parseHaps (queryJson (js, 0, 1));
        sliders.sliders[0][0] = 2000; auto h4 = parseHaps (queryJson (js, 0, 1));
        std::printf ("    sliderLive (läser host.get vid varje fråga): vid 200 -> %s, vid 2000 -> %s\n",
                     h3.empty() ? "?" : h3[0].fields["cutoff"].toString().toRawUTF8(), h4.empty() ? "?" : h4[0].fields["cutoff"].toString().toRawUTF8());
    }

    // 7. Ljudtråd: utan JS som baslinje, sedan JS på bakgrundstråd (egen motor, nytt mönster var 20:e fråga)
    auto base = runAudioSim (5, [] {});
    std::printf ("[7] ljudtråd utan JS: %d block, snitt-sen %.3f ms, p99.9 %.3f, max %.3f, >2 ms: %d\n", base.n, base.mean, base.p999, base.max, base.late);
    auto with = runAudioSim (5, [&]
    {
        juce::JavascriptEngine bg; bg.maximumExecutionTime = juce::RelativeTime::seconds (10);
        bg.execute (src);
        juce::String e; int i = 0;
        const auto end = Clock::now() + std::chrono::milliseconds (5200);
        while (Clock::now() < end)
        {
            if (i % 20 == 0) setPattern (bg, pats[3].code, e);
            queryJson (bg, i, i + 2); ++i;
        }
        std::printf ("    (bakgrunden hann %d frågor, inkl. omladdning av mönster)\n", i);
    });
    std::printf ("[7] ljudtråd med JS:  %d block, snitt-sen %.3f ms, p99.9 %.3f, max %.3f, >2 ms: %d\n", with.n, with.mean, with.p999, with.max, with.late);
}

int main (int argc, char** argv)
{
    juce::File bundle (argc > 1 ? juce::String (argv[1]) : "js/strudel.all.js");
    if (! bundle.existsAsFile()) { std::printf ("hittar inte %s\n", bundle.getFullPathName().toRawUTF8()); return 2; }
    std::thread worker ([&] { spike (bundle); }); // allt JS på bakgrundstråd, inte huvudtråden
    worker.join();
    return 0;
}
