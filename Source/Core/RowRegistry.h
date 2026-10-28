#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

// R9b: mönsterraderna som en pool om högst maxRows. Rad 0-9 är instrumentens NOTE-rader och rad 10 är kicken (som förut, index oförändrade);
// underrader (SAMPLE, skivrader "Slice N") får ett ledigt id från 11 och uppåt. Internt används id:t (index i poolen), i filer, MCP och Live Code
// nyckeln: "i3" (NOTE-rad för instrument 3, räknat från 0 som lager), "kick", "i3.sample", "i3.slice1" (skiva 1, räknad från 1).
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

    // slice: en rad per skiva ("Slice N"); skivans index (0-baserat) ligger i slotens höga bitar. Kind 2 var den gamla SLICER-raden (borttagen).
    // slices: EN rad per instrument för alla skivor (skivindex = notens pitch). `slice` (en rad per skiva) finns kvar bara för att läsa gamla projekt.
    enum class Kind { note = 0, sample = 1, slice = 3, slices = 4 };
    static constexpr int numKinds = 5;
    // Raden Kind::slices lagrar skivindex 0..127 som pitch = skiva - 64 (Pattern klämmer pitch till -96..96, så 0..127 ryms inte rakt av).
    static constexpr int slicePitchOffset = 64;
    static constexpr int sliceOfPitch (int pitch) { return pitch + slicePitchOffset; }
    static constexpr int pitchOfSlice (int slice) { return slice - slicePitchOffset; }
    static constexpr int maxSliceRows = 16; // tak: bara de första 16 skivorna får en rad (Tobias/PL 27 okt)

    RowRegistry()
    {
        for (auto& slot : slots)
            slot = free;
        for (int row = 0; row < numBaseRows; ++row)
            slots[(size_t) row] = pack (row, Kind::note); // för kicken betyder "instrument" kickRow
    }

    // Ger radens id (skapar den om den saknas). -1: ogiltigt instrument eller sort, eller poolen är full. Kicken har inga underrader.
    int add (int instrument, Kind kind, int slice = 0)
    {
        if (kind == Kind::note)
            return find (instrument, kind);
        if (! juce::isPositiveAndBelow (instrument, numInstruments))
            return -1;
        if (kind == Kind::slice ? ! juce::isPositiveAndBelow (slice, maxSliceRows) : slice != 0)
            return -1;
        if (const auto existing = find (instrument, kind, slice); existing >= 0)
            return existing;
        for (int id = firstSubRow; id < maxRows; ++id)
            if (slots[(size_t) id].load() == free)
            {
                slots[(size_t) id] = pack (instrument, kind, slice);
                return id;
            }
        return -1;
    }

    // Radens id eller -1 om den inte finns (skapar ingenting). `slice`: 0-baserat skivindex (bara för Kind::slice).
    int find (int instrument, Kind kind, int slice = 0) const
    {
        if (kind == Kind::note)
            return juce::isPositiveAndBelow (instrument, numBaseRows) ? instrument : -1;
        const auto wanted = pack (instrument, kind, slice);
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
        instrument = (v >> 4) & 255;
        kind = (Kind) (v & 15);
        return true;
    }

    // Skivindex (0-baserat) för en slice-rad, annars -1.
    int sliceOf (int id) const
    {
        if (! isActive (id))
            return -1;
        const auto v = slots[(size_t) id].load();
        return (Kind) (v & 15) == Kind::slice ? v >> 12 : -1;
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
            if (slots[(size_t) id].load() != free && ((slots[(size_t) id].load() >> 4) & 255) == instrument)
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
        return keyOf (instrument, kind, sliceOf (id));
    }

    // Nyckeln; slice-rader: "i3.slice1" (skivan räknas från 1 i nyckeln och i gränssnittet, `slice` här är 0-baserat).
    static juce::String keyOf (int instrument, Kind kind, int slice = 0)
    {
        if (instrument == kickRow && kind == Kind::note)
            return "kick";
        if (! juce::isPositiveAndBelow (instrument, numInstruments))
            return {};
        const auto base = "i" + juce::String (instrument);
        if (kind == Kind::slices)
            return base + ".slices";
        return kind == Kind::sample ? base + ".sample" : kind == Kind::slice ? base + ".slice" + juce::String (slice + 1) : base;
    }

    // Tolkar en nyckel. false vid okänd form, ogiltigt instrument eller kick med suffix. `slice` (0-baserat) fylls för slice-nycklar.
    static bool parseKey (const juce::String& key, int& instrument, Kind& kind, int* slice = nullptr)
    {
        if (slice != nullptr)
            *slice = 0;
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
        else if (suffix == "slices")
            kind = Kind::slices;
        else if (suffix.startsWith ("slice") && suffix.substring (5).containsOnly ("0123456789") && suffix.length() > 5 && suffix.length() <= 7)
        {
            const auto number1 = suffix.substring (5).getIntValue();
            if (number1 < 1 || number1 > maxSliceRows)
                return false;
            kind = Kind::slice;
            if (slice != nullptr)
                *slice = number1 - 1;
        }
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
        int slice = 0;
        return parseKey (key, instrument, kind, &slice) ? find (instrument, kind, slice) : -1;
    }

    // Som `idOfKey` men skapar underraden vid behov (-1: okänd nyckel eller poolen full).
    int idOfKeyCreate (const juce::String& key)
    {
        int instrument = 0;
        Kind kind;
        int slice = 0;
        return parseKey (key, instrument, kind, &slice) ? add (instrument, kind, slice) : -1;
    }

    void clearSubRows()
    {
        for (int id = firstSubRow; id < maxRows; ++id)
            slots[(size_t) id] = free;
    }

private:
    static constexpr int free = -1;
    static int pack (int instrument, Kind kind, int slice = 0) { return (slice << 12) | (instrument << 4) | (int) kind; }

    std::array<std::atomic<int>, maxRows> slots {};
};
