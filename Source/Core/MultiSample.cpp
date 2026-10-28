#include "MultiSample.h"

int MultiSampling::parseRootKey (const juce::String& name)
{
    // Heltal sist i namnet (efter sista icke-siffra).
    int end = name.length();
    int begin = end;
    while (begin > 0 && juce::CharacterFunctions::isDigit (name[begin - 1]))
        --begin;

    // Tonnamn: bokstav A-G, valfri # eller b, valfritt minustecken, oktav. Letas från slutet så att "Piano_C#3" hittas.
    if (begin < end && begin >= 1)
    {
        auto octaveBegin = begin;
        if (octaveBegin > 0 && name[octaveBegin - 1] == '-')
            --octaveBegin;
        auto p = octaveBegin;
        int accidental = 0;
        if (p > 0 && (name[p - 1] == '#' || name[p - 1] == 'b'))
        {
            accidental = name[p - 1] == '#' ? 1 : -1;
            --p;
        }
        if (p > 0)
        {
            const auto letter = juce::CharacterFunctions::toUpperCase (name[p - 1]);
            static const int semitone[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
            // Bokstaven får inte sitta mitt i ett ord ("Bass3" är inte en ton): tecknet före ska vara en avgränsare eller början.
            const auto before = p >= 2 ? name[p - 2] : (juce::juce_wchar) '_';
            if (letter >= 'A' && letter <= 'G' && ! juce::CharacterFunctions::isLetterOrDigit (before))
            {
                const auto octave = name.substring (octaveBegin, end).getIntValue();
                const auto key = 12 * (octave + 1) + semitone[letter - 'A'] + accidental;
                if (key >= 0 && key <= 127)
                    return key;
            }
        }
    }

    if (begin < end)
    {
        const auto number = name.substring (begin, end).getIntValue();
        if (number >= 0 && number <= 127 && end - begin <= 3)
            return number;
    }
    return -1;
}

namespace
{
    // SFZ-tonvärde: heltal eller namn (c4 = 60, c#4, db3, a-1). -1 om det inte går att tolka.
    int sfzNote (const juce::String& value)
    {
        const auto v = value.trim();
        if (v.isEmpty())
            return -1;
        if (v.containsOnly ("0123456789"))
            return juce::jlimit (0, 127, v.getIntValue());
        const auto letter = juce::CharacterFunctions::toUpperCase (v[0]);
        if (letter < 'A' || letter > 'G')
            return -1;
        static const int semitone[7] = { 9, 11, 0, 2, 4, 5, 7 };
        int p = 1, accidental = 0;
        if (v[p] == '#') { accidental = 1; ++p; }
        else if (v[p] == 'b' || v[p] == 'B') { accidental = -1; ++p; }
        const auto octave = v.substring (p);
        if (octave.isEmpty() || ! octave.removeCharacters ("-").containsOnly ("0123456789"))
            return -1;
        return juce::jlimit (0, 127, 12 * (octave.getIntValue() + 1) + semitone[letter - 'A'] + accidental);
    }

    bool isOpcodeStart (const juce::String& text, int i)
    {
        // "namn=" där namnet är bokstäver, siffror och understreck
        auto j = i;
        while (j < text.length() && (juce::CharacterFunctions::isLetterOrDigit (text[j]) || text[j] == '_'))
            ++j;
        return j > i && j < text.length() && text[j] == '=';
    }

    using Opcodes = std::map<juce::String, juce::String>;

    void applyOpcodes (MultiSampling::SfzRegion& region, const Opcodes& ops, juce::String defaultPath)
    {
        auto note = [&ops] (const char* key) { auto it = ops.find (key); return it != ops.end() ? sfzNote (it->second) : -1; };
        auto integer = [&ops] (const char* key, int fallback) { auto it = ops.find (key); return it != ops.end() ? it->second.getIntValue() : fallback; };
        auto& m = region.mapping;
        if (auto it = ops.find ("sample"); it != ops.end())
            region.samplePath = defaultPath + it->second.replaceCharacter ('\\', '/');
        if (const auto k = note ("key"); k >= 0)
            m.keyLow = m.keyHigh = m.rootKey = k;
        if (const auto k = note ("lokey"); k >= 0) m.keyLow = k;
        if (const auto k = note ("hikey"); k >= 0) m.keyHigh = k;
        if (const auto k = note ("pitch_keycenter"); k >= 0) m.rootKey = k;
        m.velocityLow = juce::jlimit (1, 127, integer ("lovel", 1));
        m.velocityHigh = juce::jlimit (1, 127, integer ("hivel", 127));
        m.tuneCents = (float) integer ("tune", 0);
        m.rootKey = juce::jlimit (0, 127, m.rootKey - integer ("transpose", 0));
    }
}

std::vector<MultiSampling::SfzRegion> MultiSampling::parseSfz (const juce::String& rawText)
{
    // Radkommentarer bort
    juce::StringArray lines;
    lines.addLines (rawText);
    juce::String text;
    for (auto line : lines)
    {
        if (const auto c = line.indexOf ("//"); c >= 0)
            line = line.substring (0, c);
        text << line << ' ';
    }

    std::vector<SfzRegion> regions;
    Opcodes global, master, group, region, control;
    Opcodes* current = nullptr;
    enum class Level { none, control, global, master, group, region } level = Level::none;

    auto flushRegion = [&]
    {
        if (level != Level::region)
            return;
        Opcodes merged = global;
        for (const auto* layer : { &master, &group, &region })
            for (const auto& [k, v] : *layer)
                merged[k] = v;
        SfzRegion r;
        auto path = control.count ("default_path") ? control["default_path"].replaceCharacter ('\\', '/') : juce::String();
        applyOpcodes (r, merged, path);
        if (r.samplePath.isNotEmpty())
            regions.push_back (std::move (r));
    };

    int i = 0;
    const auto n = text.length();
    while (i < n)
    {
        const auto c = text[i];
        if (juce::CharacterFunctions::isWhitespace (c)) { ++i; continue; }
        if (c == '<')
        {
            const auto close = text.indexOfChar (i, '>');
            if (close < 0) break;
            const auto header = text.substring (i + 1, close).trim().toLowerCase();
            flushRegion();
            if (header == "region")      { level = Level::region;  region.clear(); current = &region; }
            else if (header == "group")  { level = Level::group;   group.clear(); region.clear(); current = &group; }
            else if (header == "master") { level = Level::master;  master.clear(); group.clear(); region.clear(); current = &master; }
            else if (header == "global") { level = Level::global;  global.clear(); master.clear(); group.clear(); region.clear(); current = &global; }
            else if (header == "control"){ level = Level::control; current = &control; }
            else                         { level = Level::none;    current = nullptr; }
            i = close + 1;
            continue;
        }
        if (! isOpcodeStart (text, i)) { ++i; continue; }
        const auto eq = text.indexOfChar (i, '=');
        const auto key = text.substring (i, eq).toLowerCase();
        auto j = eq + 1;
        // Värdet går fram till nästa "namn=" eller "<" (sample-sökvägar kan innehålla blanksteg)
        auto end = j;
        while (end < n)
        {
            if (text[end] == '<') break;
            if (juce::CharacterFunctions::isWhitespace (text[end]) && end + 1 < n)
            {
                auto k = end + 1;
                while (k < n && juce::CharacterFunctions::isWhitespace (text[k])) ++k;
                if (k < n && (text[k] == '<' || isOpcodeStart (text, k))) break;
            }
            ++end;
        }
        if (current != nullptr)
            (*current)[key] = text.substring (j, end).trim();
        i = end;
    }
    flushRegion();
    return regions;
}

int MultiSampleSet::addSfz (const juce::File& sfzFile)
{
    const auto regions = MultiSampling::parseSfz (sfzFile.loadFileAsString());
    auto added = 0;
    for (const auto& r : regions)
    {
        MultiSampleMapping m;
        m.rootKey = r.mapping.rootKey;
        m.keyLow = r.mapping.keyLow;
        m.keyHigh = r.mapping.keyHigh;
        m.velocityLow = r.mapping.velocityLow;
        m.velocityHigh = r.mapping.velocityHigh;
        m.tuneCents = r.mapping.tuneCents;
        if (addFile (sfzFile.getParentDirectory().getChildFile (r.samplePath), &m))
            ++added;
    }
    return added;
}

int MultiSampleBank::select (int key, int velocity) const noexcept
{
    int best = -1;
    int bestScore = 0;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& m = zones[i].mapping;
        const auto keyDistance = key < m.keyLow ? m.keyLow - key : (key > m.keyHigh ? key - m.keyHigh : 0);
        const auto velocityDistance = velocity < m.velocityLow ? m.velocityLow - velocity : (velocity > m.velocityHigh ? velocity - m.velocityHigh : 0);
        const auto score = keyDistance * 1000 + velocityDistance;
        if (best < 0 || score < bestScore)
        {
            best = (int) i;
            bestScore = score;
        }
    }
    return best;
}

bool MultiSampleSet::addFile (const juce::File& file, const MultiSampleMapping* explicitMapping)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return false;

    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (MultiSampling::maxSecondsPerZone * reader->sampleRate));
    juce::AudioBuffer<float> fileAudio ((int) reader->numChannels, length);
    reader->read (&fileAudio, 0, length, 0, true, true);

    juce::AudioBuffer<float> mono (1, length);
    mono.clear();
    for (int ch = 0; ch < fileAudio.getNumChannels(); ++ch)
        mono.addFrom (0, 0, fileAudio, ch, 0, length, 1.0f / (float) fileAudio.getNumChannels());

    MultiSampleMapping mapping;
    const auto name = file.getFileNameWithoutExtension();
    if (explicitMapping != nullptr)
        mapping = *explicitMapping;
    else if (const auto root = MultiSampling::parseRootKey (name); root >= 0)
        mapping.rootKey = root;

    return addZone (std::move (mono), reader->sampleRate, name, mapping);
}

bool MultiSampleSet::addZone (juce::AudioBuffer<float> mono, double rate, const juce::String& name, const MultiSampleMapping& mapping)
{
    if (mono.getNumSamples() <= 0 || rate <= 0.0)
        return false;

    {
        const juce::ScopedLock scoped (lock);
        if ((int) originals.size() >= MultiSampling::maxZones)
            return false;
        if (totalSecondsLocked() + (double) mono.getNumSamples() / rate > MultiSampling::maxTotalSeconds)
            return false;

        Original o;
        o.mono = std::move (mono);
        o.rate = rate;
        o.name = name;
        o.mapping = mapping;
        o.mapping.rootKey = juce::jlimit (0, 127, o.mapping.rootKey);
        o.mapping.keyLow = juce::jlimit (0, 127, o.mapping.keyLow);
        o.mapping.keyHigh = juce::jlimit (o.mapping.keyLow, 127, o.mapping.keyHigh);
        o.mapping.velocityLow = juce::jlimit (1, 127, o.mapping.velocityLow);
        o.mapping.velocityHigh = juce::jlimit (o.mapping.velocityLow, 127, o.mapping.velocityHigh);
        o.mapping.tuneCents = juce::jlimit (-100.0f, 100.0f, o.mapping.tuneCents);
        originals.push_back (std::move (o));
    }
    rebuild();
    return true;
}

void MultiSampleSet::removeZone (int index)
{
    {
        const juce::ScopedLock scoped (lock);
        if (index < 0 || (size_t) index >= originals.size())
            return;
        originals.erase (originals.begin() + index);
    }
    rebuild();
}

void MultiSampleSet::setMapping (int index, const MultiSampleMapping& mapping)
{
    {
        const juce::ScopedLock scoped (lock);
        if (index < 0 || (size_t) index >= originals.size())
            return;
        auto& m = originals[(size_t) index].mapping;
        m = mapping;
        m.rootKey = juce::jlimit (0, 127, m.rootKey);
        m.keyLow = juce::jlimit (0, 127, m.keyLow);
        m.keyHigh = juce::jlimit (m.keyLow, 127, m.keyHigh);
        m.velocityLow = juce::jlimit (1, 127, m.velocityLow);
        m.velocityHigh = juce::jlimit (m.velocityLow, 127, m.velocityHigh);
        m.tuneCents = juce::jlimit (-100.0f, 100.0f, m.tuneCents);
    }
    rebuild();
}

void MultiSampleSet::clear()
{
    {
        const juce::ScopedLock scoped (lock);
        originals.clear();
    }
    rebuild();
}

void MultiSampleSet::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;
    rebuild();
}

void MultiSampleSet::autoMapKeyRanges()
{
    {
        const juce::ScopedLock scoped (lock);
        // Zonerna ordnas efter rotton (stabilt), gränsen läggs halvvägs; zoner med samma rotton delar område (olika anslag får man själv ställa in).
        std::vector<size_t> order (originals.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::stable_sort (order.begin(), order.end(), [this] (size_t a, size_t b) { return originals[a].mapping.rootKey < originals[b].mapping.rootKey; });

        for (size_t n = 0; n < order.size(); ++n)
        {
            auto& m = originals[order[n]].mapping;
            // Närmaste lägre och högre rotton som skiljer sig från den egna.
            int lower = -1, upper = -1;
            for (size_t k = n; k-- > 0;)
                if (originals[order[k]].mapping.rootKey < m.rootKey) { lower = originals[order[k]].mapping.rootKey; break; }
            for (size_t k = n + 1; k < order.size(); ++k)
                if (originals[order[k]].mapping.rootKey > m.rootKey) { upper = originals[order[k]].mapping.rootKey; break; }
            m.keyLow = lower < 0 ? 0 : (lower + m.rootKey) / 2 + 1;
            m.keyHigh = upper < 0 ? 127 : (m.rootKey + upper) / 2;
        }
    }
    rebuild();
}

int MultiSampleSet::zoneCount() const
{
    const juce::ScopedLock scoped (lock);
    return (int) originals.size();
}

juce::String MultiSampleSet::zoneName (int index) const
{
    const juce::ScopedLock scoped (lock);
    return index >= 0 && (size_t) index < originals.size() ? originals[(size_t) index].name : juce::String();
}

MultiSampleMapping MultiSampleSet::mapping (int index) const
{
    const juce::ScopedLock scoped (lock);
    return index >= 0 && (size_t) index < originals.size() ? originals[(size_t) index].mapping : MultiSampleMapping();
}

double MultiSampleSet::totalSecondsLocked() const
{
    double total = 0.0;
    for (const auto& o : originals)
        total += o.rate > 0.0 ? (double) o.mono.getNumSamples() / o.rate : 0.0;
    return total;
}

void MultiSampleSet::rebuild()
{
    std::unique_ptr<MultiSampleBank> fresh;
    const juce::ScopedLock scoped (lock);

    if (! originals.empty())
    {
        fresh = std::make_unique<MultiSampleBank>();
        fresh->zones.reserve (originals.size());
        for (const auto& o : originals)
        {
            MultiSampleBank::Zone zone;
            zone.sample.name = o.name;
            zone.mapping = o.mapping;
            zone.rootHz = 440.0f * std::exp2 (((float) o.mapping.rootKey - 69.0f + o.mapping.tuneCents / 100.0f) / 12.0f);

            const auto ratio = o.rate / currentSampleRate;
            const auto outLength = std::max (1, (int) (o.mono.getNumSamples() / ratio));
            zone.sample.audio.setSize (1, outLength);
            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio, o.mono.getReadPointer (0), zone.sample.audio.getWritePointer (0), outLength);
            fresh->zones.push_back (std::move (zone));
        }
    }

    // Som SampleSlot: byt atomiskt, frigör den gamla först efter en stund.
    activeBank.store (fresh.get());
    retired.retire (std::move (owned));
    owned = std::move (fresh);
}

juce::ValueTree MultiSampleSet::toValueTree (const juce::Identifier& type) const
{
    juce::ValueTree tree (type);
    const juce::ScopedLock scoped (lock);
    for (const auto& o : originals)
    {
        juce::ValueTree zone ("Zone");
        juce::MemoryBlock block (o.mono.getReadPointer (0), (size_t) o.mono.getNumSamples() * sizeof (float));
        zone.setProperty ("name", o.name, nullptr);
        zone.setProperty ("rate", o.rate, nullptr);
        zone.setProperty ("data", block.toBase64Encoding(), nullptr);
        zone.setProperty ("root", o.mapping.rootKey, nullptr);
        zone.setProperty ("keyLow", o.mapping.keyLow, nullptr);
        zone.setProperty ("keyHigh", o.mapping.keyHigh, nullptr);
        zone.setProperty ("velLow", o.mapping.velocityLow, nullptr);
        zone.setProperty ("velHigh", o.mapping.velocityHigh, nullptr);
        zone.setProperty ("tune", (double) o.mapping.tuneCents, nullptr);
        tree.appendChild (zone, nullptr);
    }
    return tree;
}

void MultiSampleSet::fromValueTree (const juce::ValueTree& tree)
{
    std::vector<Original> loaded;
    double total = 0.0;
    for (const auto& zone : tree)
    {
        juce::MemoryBlock block;
        if (! zone.hasProperty ("data") || ! block.fromBase64Encoding (zone["data"].toString()) || (int) loaded.size() >= MultiSampling::maxZones)
            continue;

        const auto numSamples = (int) (block.getSize() / sizeof (float));
        const auto rate = (double) zone["rate"];
        if (numSamples <= 0 || rate <= 0.0)
            continue;
        total += (double) numSamples / rate;
        if (total > MultiSampling::maxTotalSeconds)
            break;

        Original o;
        o.mono.setSize (1, numSamples);
        std::memcpy (o.mono.getWritePointer (0), block.getData(), (size_t) numSamples * sizeof (float));
        o.rate = rate;
        o.name = zone["name"].toString();
        o.mapping.rootKey = juce::jlimit (0, 127, (int) zone.getProperty ("root", 60));
        o.mapping.keyLow = juce::jlimit (0, 127, (int) zone.getProperty ("keyLow", 0));
        o.mapping.keyHigh = juce::jlimit (o.mapping.keyLow, 127, (int) zone.getProperty ("keyHigh", 127));
        o.mapping.velocityLow = juce::jlimit (1, 127, (int) zone.getProperty ("velLow", 1));
        o.mapping.velocityHigh = juce::jlimit (o.mapping.velocityLow, 127, (int) zone.getProperty ("velHigh", 127));
        o.mapping.tuneCents = juce::jlimit (-100.0f, 100.0f, (float) (double) zone.getProperty ("tune", 0.0));
        loaded.push_back (std::move (o));
    }

    {
        const juce::ScopedLock scoped (lock);
        originals = std::move (loaded);
    }
    rebuild();
}
