#include "LiveCode.h"

#include <BinaryData.h>
#include "Pattern.h"
#include <algorithm>
#include <map>

namespace
{
    // host.slider(id, v, min, max): id = ordningsnumret på slider()-anropet i koden. Har användaren dragit i reglaget under LIVE svarar
    // värdekartan (begränsad till min..max), annars startvärdet. host.get(id) är reserverad för reglage utan omkörning.
    struct HostObject : juce::DynamicObject
    {
        HostObject (const std::map<int, double>& values, const std::map<juce::String, double>& params, juce::Array<juce::var>& writes)
        {
            // paramSet(id, v): bara noteras här; själva skrivningen görs på message thread efter körningen. paramGet(id): värdet från före körningen.
            setMethod ("paramSet", [&writes] (const juce::var::NativeFunctionArgs& a) -> juce::var
            {
                if (a.numArguments < 2)
                    return {};
                const auto id = a.arguments[0].toString();
                for (auto& w : writes)
                    if (w["id"].toString() == id)
                    {
                        w.getDynamicObject()->setProperty ("value", a.arguments[1]);
                        return a.arguments[1];
                    }
                auto* w = new juce::DynamicObject();
                w->setProperty ("id", id);
                w->setProperty ("value", a.arguments[1]);
                writes.add (juce::var (w));
                return a.arguments[1];
            });
            setMethod ("paramGet", [&params] (const juce::var::NativeFunctionArgs& a) -> juce::var
            {
                if (a.numArguments < 1)
                    return 0.0;
                const auto it = params.find (a.arguments[0].toString());
                return it != params.end() ? it->second : 0.0;
            });
            setMethod ("slider", [&values] (const juce::var::NativeFunctionArgs& a) -> juce::var
            {
                if (a.numArguments < 2)
                    return juce::var (0.0);
                if (a.numArguments >= 4)
                    if (const auto it = values.find ((int) a.arguments[0]); it != values.end())
                        return juce::jlimit ((double) a.arguments[2], (double) a.arguments[3], it->second);
                return a.arguments[1];
            });
            setMethod ("get", [] (const juce::var::NativeFunctionArgs&) -> juce::var { return 0.0; });
        }
    };
}

LiveCode::LiveCode()
{
    engine.maximumExecutionTime = juce::RelativeTime::milliseconds (500);
    engine.registerNativeObject ("host", new HostObject (sliderValues, paramValues, paramWrites));
}

void LiveCode::setSliderValues (std::map<int, double> values) { sliderValues = std::move (values); }
void LiveCode::setParams (std::map<juce::String, double> values) { paramValues = std::move (values); }

juce::var LiveCode::takeParamWrites()
{
    juce::var out (paramWrites);
    paramWrites.clear();
    return out;
}

juce::Result LiveCode::load()
{
    if (loaded) return juce::Result::ok();
    engine.maximumExecutionTime = juce::RelativeTime::seconds (10); // bunten tar ~0.2 s
    auto r = engine.execute (juce::String::fromUTF8 (BinaryData::strudel_all_js, BinaryData::strudel_all_jsSize));
    engine.maximumExecutionTime = juce::RelativeTime::milliseconds (500);
    loaded = r.wasOk();
    return r;
}

juce::Result LiveCode::setCode (const juce::String& code)
{
    if (auto r = load(); r.failed()) return r;
    if (! paramValues.empty())
    {
        juce::Array<juce::var> ids;
        for (auto& kv : paramValues)
            ids.add (kv.first);
        juce::Result built = juce::Result::ok();
        engine.evaluate ("buildParams(" + juce::JSON::toString (juce::var (ids), true) + ")", &built);
        if (built.failed()) return built;
    }
    juce::Result res = juce::Result::ok();
    const auto message = engine.evaluate ("setPattern(" + juce::JSON::toString (code) + ")", &res); // glue.js svarar med felmeddelandet, "" = ok
    if (res.failed()) return res;
    if (message.toString().isNotEmpty()) return juce::Result::fail (message.toString());
    return res;
}

std::vector<LiveCode::Hap> LiveCode::query (double from, double to, juce::Result& result)
{
    std::vector<Hap> out;
    result = juce::Result::ok();
    auto json = engine.evaluate ("queryJson(" + juce::String (from, 6) + "," + juce::String (to, 6) + ")", &result);
    if (result.failed()) return out;
    const auto parsed = juce::JSON::parse (json.toString()); // måste leva under loopen (getArray pekar in i den)
    if (auto* arr = parsed.getArray())
        for (auto& h : *arr)
        {
            if (! (bool) h["hasOnset"] || h["b"].isVoid()) continue;
            Hap hap;
            hap.begin = h["b"];
            hap.end = h["e"];
            hap.value = h["v"];
            if (hap.begin >= from && hap.begin < to) out.push_back (std::move (hap));
        }
    return out;
}

bool LiveCode::pitchFrom (const juce::var& note, int& pitch)
{
    if (note.isInt() || note.isInt64() || note.isDouble())
    {
        pitch = juce::roundToInt ((double) note);
        return true;
    }
    auto s = note.toString().trim().toLowerCase();
    if (s.isEmpty()) return false;
    static const int semis[] = { 9, 11, 0, 2, 4, 5, 7 }; // a b c d e f g
    const auto letter = s[0];
    if (letter < 'a' || letter > 'g') return false;
    int semi = semis[letter - 'a'], i = 1;
    for (; i < s.length() && (s[i] == '#' || s[i] == 's' || s[i] == 'b' || s[i] == 'f'); ++i)
        semi += (s[i] == '#' || s[i] == 's') ? 1 : -1;
    int octave = 3; // Strudel: utan oktav är det oktav 3
    if (i < s.length())
    {
        auto rest = s.substring (i);
        if (! rest.containsOnly ("-0123456789")) return false;
        octave = rest.getIntValue();
    }
    pitch = (octave + 1) * 12 + semi - 36; // MIDI - 36: C2 = 0
    return true;
}

juce::var LiveCode::toPatternArgs (const std::vector<Hap>& haps, int cycles) const
{
    // Lager per hap: s("bd"/"kick") = kicken, s("i1".."i10") = instrument 1-10, annars instrument 1.
    struct Layer { juce::Array<juce::var> notes; std::map<int, std::vector<int>> ticks; }; // ticks: tonhöjder redan på tickens stapel
    std::map<int, Layer> layers;
    std::vector<int> kickHits;

    for (auto& h : haps)
    {
        const auto s = h.value["s"].toString().toLowerCase();
        const int tick = juce::roundToInt (h.begin * ticksPerCycle);
        if (tick < 0 || tick >= cycles * ticksPerCycle) continue;

        if (s == "bd" || s == "kick")
        {
            kickHits.push_back (juce::roundToInt (h.begin * 16)); // steg i en 16-delsrad, räknat över alla cykler
            continue;
        }

        int layer = 0;
        if (s.length() > 1 && s.startsWithChar ('i') && s.substring (1).containsOnly ("0123456789"))
            layer = juce::jlimit (1, 10, s.substring (1).getIntValue()) - 1;

        int pitch = 0;
        const auto noteVal = h.value.hasProperty ("note") ? h.value["note"] : h.value["n"];
        if (! noteVal.isVoid() && ! pitchFrom (noteVal, pitch)) continue;

        auto& l = layers[layer];
        auto& stacked = l.ticks[tick];
        if (std::find (stacked.begin(), stacked.end(), pitch) != stacked.end()) continue; // samma tonhöjd på samma tick hoppas över
        if ((int) stacked.size() >= Pattern::maxStack) continue; // full stapel
        const bool isExtra = ! stacked.empty(); // första tonen = huvudton, nästa = staplad ton (set_pattern: "stack")
        stacked.push_back (pitch);
        const double gain = h.value.hasProperty ("gain") ? (double) h.value["gain"] : 0.8;
        auto* n = new juce::DynamicObject();
        n->setProperty ("tick", tick);
        n->setProperty ("length", std::max (1, juce::roundToInt ((h.end - h.begin) * ticksPerCycle)));
        n->setProperty ("pitch", juce::jlimit (-48, 48, pitch));
        n->setProperty ("velocity", juce::jlimit (0.05, 1.0, gain));
        if (isExtra) n->setProperty ("stack", true);
        l.notes.add (juce::var (n));
    }

    juce::Array<juce::var> result;
    for (auto& [layer, l] : layers)
    {
        auto* a = new juce::DynamicObject();
        a->setProperty ("layer", layer);
        a->setProperty ("notes", l.notes);
        a->setProperty ("length", cycles);
        result.add (juce::var (a));
    }
    if (! kickHits.empty())
    {
        juce::Array<juce::var> bars;
        for (int c = 0; c < cycles; ++c)
        {
            juce::String row = juce::String::repeatedString (".", 16);
            for (int step : kickHits)
                if (step / 16 == c) row = row.replaceSection (step % 16, 1, "x");
            bars.add (row);
        }
        auto* k = new juce::DynamicObject();
        k->setProperty ("layer", "kick");
        k->setProperty ("bars", bars);
        result.add (juce::var (k));
    }
    return result;
}

juce::var LiveCode::create (const juce::String& code, int cycles, juce::Result& result)
{
    result = setCode (code);
    if (result.failed()) return {};
    auto haps = query (0, cycles, result);
    if (result.failed()) return {};
    return toPatternArgs (haps, cycles);
}
