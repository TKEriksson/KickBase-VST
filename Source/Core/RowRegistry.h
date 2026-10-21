#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

// R9b: mönsterraderna som en pool om högst maxRows. Rad 0-9 är instrumentens NOTE-rader och rad 10 är kicken (som förut, index oförändrade);
// underrader (SAMPLE, SLICER) får ett ledigt id från 11 och uppåt. Internt används id:t (index i poolen), i filer, MCP och Live Code
// nyckeln: "i3" (NOTE-rad för instrument 3, räknat från 0 som lager), "kick", "i3.sample", "i3.slicer".
//
// Tråd: bara meddelandetråden skapar och tar bort rader (`add`, `remove`, `removeInstrument`). Poolen är en fast array av atomer, så
// ljudtråden kan läsa (`get`, `isActive`) utan lås och utan allokering.
class RowRegistry
{
public:
    static constexpr int maxRows = 64;
    static constexpr int numInstruments = 10;   // samma som maxInstruments (kontrolleras med static_assert i Pattern.h)
    static constexpr int kickRow = numInstruments;
    static constexpr int numBaseRows = numInstruments + 1;
    static constexpr int firstSubRow = numBaseRows;

    enum class Kind { note = 0, sample = 1, slicer = 2 };
    static constexpr int numKinds = 3;

    RowRegistry()
    {
        for (auto& slot : slots)
            slot = free;
        for (int row = 0; row < numBaseRows; ++row)
            slots[(size_t) row] = pack (row, Kind::note); // för kicken betyder "instrument" kickRow
    }

    // Ger radens id (skapar den om den saknas). -1: ogiltigt instrument eller sort, eller poolen är full. Kicken har inga underrader.
    int add (int instrument, Kind kind)
    {
        if (kind == Kind::note)
            return find (instrument, kind);
        if (! juce::isPositiveAndBelow (instrument, numInstruments))
            return -1;
        if (const auto existing = find (instrument, kind); existing >= 0)
            return existing;
        for (int id = firstSubRow; id < maxRows; ++id)
            if (slots[(size_t) id].load() == free)
            {
                slots[(size_t) id] = pack (instrument, kind);
                return id;
            }
        return -1;
    }

    // Radens id eller -1 om den inte finns (skapar ingenting).
    int find (int instrument, Kind kind) const
    {
        if (kind == Kind::note)
            return juce::isPositiveAndBelow (instrument, numBaseRows) ? instrument : -1;
        const auto wanted = pack (instrument, kind);
        for (int id = firstSubRow; id < maxRows; ++id)
            if (slots[(size_t) id].load() == wanted)
                return id;
        return -1;
    }

    bool isActive (int id) const { return juce::isPositiveAndBelow (id, maxRows) && slots[(size_t) id].load() != free; }
    bool isSubRow (int id) const { return id >= firstSubRow && isActive (id); }

    // Instrument och sort för ett id. false om id:t inte används.
    bool get (int id, int& instrument, Kind& kind) const
    {
        if (! isActive (id))
            return false;
        const auto v = slots[(size_t) id].load();
        instrument = v >> 4;
        kind = (Kind) (v & 15);
        return true;
    }

    // Instrumentet en rad hör till (kicken: kickRow), eller -1.
    int instrumentOf (int id) const
    {
        int instrument = 0;
        Kind kind;
        return get (id, instrument, kind) ? instrument : -1;
    }

    // Tar bort en underrad (NOTE-raderna och kicken kan inte tas bort). Returnerar true om en rad togs bort.
    bool remove (int id)
    {
        if (! isSubRow (id))
            return false;
        slots[(size_t) id] = free;
        return true;
    }

    // Alla underrader som hör till instrumentet (när instrumentet raderas). Returnerar antalet.
    int removeInstrument (int instrument)
    {
        auto removed = 0;
        for (int id = firstSubRow; id < maxRows; ++id)
            if (slots[(size_t) id].load() != free && (slots[(size_t) id].load() >> 4) == instrument)
            {
                slots[(size_t) id] = free;
                ++removed;
            }
        return removed;
    }

    int numActive() const
    {
        auto n = 0;
        for (int id = 0; id < maxRows; ++id)
            n += isActive (id) ? 1 : 0;
        return n;
    }

    // Nyckeln för ett id ("" om id:t inte används).
    juce::String keyOf (int id) const
    {
        int instrument = 0;
        Kind kind;
        if (! get (id, instrument, kind))
            return {};
        return keyOf (instrument, kind);
    }

    static juce::String keyOf (int instrument, Kind kind)
    {
        if (instrument == kickRow && kind == Kind::note)
            return "kick";
        if (! juce::isPositiveAndBelow (instrument, numInstruments))
            return {};
        const auto base = "i" + juce::String (instrument);
        return kind == Kind::sample ? base + ".sample" : kind == Kind::slicer ? base + ".slicer" : base;
    }

    // Tolkar en nyckel. false vid okänd form, ogiltigt instrument eller kick med suffix.
    static bool parseKey (const juce::String& key, int& instrument, Kind& kind)
    {
        const auto text = key.trim().toLowerCase();
        if (text == "kick")
        {
            instrument = kickRow;
            kind = Kind::note;
            return true;
        }
        if (! text.startsWithChar ('i'))
            return false;
        const auto dot = text.indexOfChar ('.');
        const auto number = text.substring (1, dot < 0 ? text.length() : dot);
        if (number.isEmpty() || ! number.containsOnly ("0123456789") || number.length() > 2)
            return false;
        const auto n = number.getIntValue();
        if (n >= numInstruments)
            return false;
        const auto suffix = dot < 0 ? juce::String() : text.substring (dot + 1);
        if (dot < 0)
            kind = Kind::note;
        else if (suffix == "sample")
            kind = Kind::sample;
        else if (suffix == "slicer")
            kind = Kind::slicer;
        else
            return false;
        instrument = n;
        return true;
    }

    // Id för en nyckel utan att skapa raden: -1 om nyckeln är okänd eller raden inte finns.
    int idOfKey (const juce::String& key) const
    {
        int instrument = 0;
        Kind kind;
        return parseKey (key, instrument, kind) ? find (instrument, kind) : -1;
    }

    // Som `idOfKey` men skapar underraden vid behov (-1: okänd nyckel eller poolen full).
    int idOfKeyCreate (const juce::String& key)
    {
        int instrument = 0;
        Kind kind;
        return parseKey (key, instrument, kind) ? add (instrument, kind) : -1;
    }

    void clearSubRows()
    {
        for (int id = firstSubRow; id < maxRows; ++id)
            slots[(size_t) id] = free;
    }

private:
    static constexpr int free = -1;
    static int pack (int instrument, Kind kind) { return (instrument << 4) | (int) kind; }

    std::array<std::atomic<int>, maxRows> slots {};
};
