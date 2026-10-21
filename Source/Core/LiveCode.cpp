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
        HostObject (const std::map<int, double>& values, const std::map<juce::String, double>& params, juce::Array<juce::var>& writes, juce::StringArray& console)
        {
            // log(level, text): console.* i koden; hamnar i konsolen i Live Code-sidan (aldrig skrivet från ljudtråden)
            setMethod ("log", [&console] (const juce::var::NativeFunctionArgs& a) -> juce::var
            {
                if (a.numArguments >= 2 && console.size() < LiveCode::maxConsoleLines)
                    console.add (a.arguments[0].toString() + ": " + a.arguments[1].toString());
                return {};
            });
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
            // lock(mål, art): F2. Noteras som {lock, kind} i samma lista; LiveCodePage delar upp dem från parameterskrivningarna.
            setMethod ("lock", [&writes] (const juce::var::NativeFunctionArgs& a) -> juce::var
            {
                if (a.numArguments < 2)
                    return {};
                auto* w = new juce::DynamicObject();
                w->setProperty ("lock", a.arguments[0].toString());
                w->setProperty ("kind", a.arguments[1].toString());
                writes.add (juce::var (w));
                return {};
            });
            // effect(kanal, plats, typ, ratt, värde): F3. Typ-byte har ratt "" ; en ratt har typen som den har efter ev. byte i samma körning.
            setMethod ("effect", [&writes] (const juce::var::NativeFunctionArgs& a) -> juce::var
            {
                if (a.numArguments < 5)
                    return {};
                auto* w = new juce::DynamicObject();
                w->setProperty ("fx", a.arguments[0].toString());
                w->setProperty ("slot", (int) a.arguments[1]);
                w->setProperty ("type", a.arguments[2].toString());
                w->setProperty ("knob", a.arguments[3].toString());
                w->setProperty ("value", a.arguments[4]);
                writes.add (juce::var (w));
                return {};
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
    engine.registerNativeObject ("host", new HostObject (sliderValues, paramValues, paramWrites, consoleLines));
}

void LiveCode::setSliderValues (std::map<int, double> values) { sliderValues = std::move (values); }
void LiveCode::setParams (std::map<juce::String, double> values) { paramValues = std::move (values); }

juce::var LiveCode::takeParamWrites()
{
    juce::var out (paramWrites);
    paramWrites.clear();
    return out;
}

juce::StringArray LiveCode::takeConsole()
{
    auto out = consoleLines;
    consoleLines.clear();
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
        juce::Array<juce::var> ids, meters;
        for (auto& kv : paramValues)
            if (kv.first.startsWith ("meter.")) // F4: mätare bygger inte parameterträdet
                meters.add (kv.first.fromFirstOccurrenceOf (".", false, false));
            else
                ids.add (kv.first);
        juce::Result built = juce::Result::ok();
        engine.evaluate ("buildParams(" + juce::JSON::toString (juce::var (ids), true) + ")", &built);
        if (built.failed()) return built;
        engine.evaluate ("buildMeters(" + juce::JSON::toString (juce::var (meters), true) + ")", &built);
        if (built.failed()) return built;
    }
    if (effectCatalog.isObject())
    {
        juce::Result built = juce::Result::ok();
        engine.evaluate ("buildEffects(" + juce::JSON::toString (juce::var (juce::JSON::toString (effectCatalog, true))) + ")", &built);
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

juce::var LiveCode::collectMods (const std::vector<Hap>& haps)
{
    juce::Array<juce::var> out;
    juce::StringArray seen; // nyckel lager|param|sort: första posten gäller
    for (auto& h : haps)
    {
        const auto list = h.value["_mod"];
        if (list.getArray() == nullptr) continue;
        const auto s = h.value["s"].toString().toLowerCase();
        int layer = 0; // som toPatternArgs: bd/kick = kicken, iN = instrument N, annars instrument 1
        if (s == "bd" || s == "kick") layer = -1;
        else if (s.length() > 1 && s.startsWithChar ('i') && s.substring (1).containsOnly ("0123456789"))
            layer = juce::jlimit (1, 10, s.substring (1).getIntValue()) - 1;

        for (auto& m : *list.getArray())
        {
            const auto key = juce::String (layer) + "|" + m["param"].toString() + "|" + m["kind"].toString();
            if (seen.contains (key)) continue;
            seen.add (key);
            auto* o = new juce::DynamicObject();
            o->setProperty ("layer", layer);
            for (const auto* name : { "param", "kind", "rate", "shape", "attack", "decay", "depth" })
                if (m.hasProperty (name)) o->setProperty (name, m[name]);
            out.add (juce::var (o));
        }
    }
    return out;
}

juce::var LiveCode::create (const juce::String& code, int cycles, juce::Result& result)
{
    mods = juce::var (juce::Array<juce::var>{});
    result = setCode (code);
    if (result.failed()) return {};
    auto haps = query (0, cycles, result);
    if (result.failed()) return {};
    mods = collectMods (haps);
    return toPatternArgs (haps, cycles);
}
