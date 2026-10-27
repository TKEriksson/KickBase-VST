// `tool rowkeytest`: radpoolen (R9b, Source/Core/RowRegistry.h): nyckel <-> id, okända nycklar, full pool, radering av ett instruments underrader.

#include "../Source/Core/Pattern.h"
#include "../Source/Core/RowRegistry.h"

static_assert (RowRegistry::numInstruments == maxInstruments, "RowRegistry och Pattern måste ha samma antal instrument");
static_assert (RowRegistry::kickRow == Pattern::kickLayer, "kicken ska ligga på samma rad som i Pattern");

int rowKeyTest()
{
    using Kind = RowRegistry::Kind;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what)
    {
        std::puts ((juce::String (ok ? "ok:  " : "FEL: ") + what).toRawUTF8());
        failed = failed || ! ok;
    };

    RowRegistry rows;
    check (rows.numActive() == RowRegistry::numBaseRows, "start: bara NOTE-raderna och kicken finns");
    check (rows.idOfKey ("i3") == 3 && rows.idOfKey ("kick") == 10 && rows.idOfKey ("i0") == 0, "grundraderna behåller sina index (0-9, kick = 10)");
    check (rows.idOfKey ("i3.sample") == -1, "en underrad som inte skapats har inget id");

    const auto sample = rows.idOfKeyCreate ("i3.sample");
    const auto slicer = rows.idOfKeyCreate ("I3.Slice1");
    check (sample == RowRegistry::firstSubRow && slicer == RowRegistry::firstSubRow + 1, "underrader får första lediga id från 11");
    check (rows.idOfKeyCreate ("i3.sample") == sample, "samma nyckel ger samma id (skapar inte dubbletter)");
    check (rows.keyOf (sample) == "i3.sample" && rows.keyOf (slicer) == "i3.slice1" && rows.keyOf (10) == "kick" && rows.keyOf (4) == "i4", "id -> nyckel");
    check (rows.idOfKey ("i3.slice1") == slicer && rows.instrumentOf (slicer) == 3 && rows.sliceOf (slicer) == 0 && rows.sliceOf (sample) == -1,
           "nyckel -> id, radens instrument och skivindex (0-baserat)");
    check (rows.idOfKey ("i3.slice0") == -1 && rows.idOfKey ("i3.slice17") == -1 && rows.idOfKey ("i3.slice16") == -1
               && rows.idOfKeyCreate ("i3.slice16") >= 0 && rows.sliceOf (rows.idOfKey ("i3.slice16")) == RowRegistry::maxSliceRows - 1,
           "skivnycklar går från slice1 till slice16");
    rows.remove (rows.idOfKey ("i3.slice16"));

    for (auto bad : { "", "i", "i10", "i3.", "i3.foo", "kick.sample", "x3", "i-1", "i3.sample.x", "i 3" })
        check (rows.idOfKey (bad) == -1 && rows.idOfKeyCreate (bad) == -1, juce::String ("okänd nyckel ger -1: '") + bad + "'");
    check (rows.numActive() == RowRegistry::numBaseRows + 2, "okända nycklar skapade ingenting");
    check (rows.add (RowRegistry::kickRow, Kind::sample) == -1, "kicken har inga underrader");

    check (! rows.remove (3) && ! rows.remove (10) && rows.isActive (3), "NOTE-rader och kick kan inte tas bort");
    check (rows.remove (sample) && ! rows.isActive (sample) && rows.keyOf (sample).isEmpty(), "en underrad kan tas bort");
    check (rows.idOfKeyCreate ("i5.sample") == sample, "ett frigjort id återanvänds");

    // Instrument 3 raderas: dess underrader följer med, andras ligger kvar.
    rows.add (3, Kind::sample);
    check (rows.removeInstrument (3) == 2 && rows.idOfKey ("i3.sample") == -1 && rows.idOfKey ("i3.slice1") == -1 && rows.idOfKey ("i5.sample") == sample,
           "instrumentets underrader tas bort, andra instruments ligger kvar");

    // Full pool: 10 instrument x (SAMPLE + 16 skivor) = 170 vill ha rad men poolen har bara maxRows - firstSubRow platser; resten nekas utan följder.
    rows.clearSubRows();
    auto created = 0;
    for (int i = 0; i < RowRegistry::numInstruments; ++i)
    {
        created += rows.add (i, Kind::sample) >= 0 ? 1 : 0;
        for (int s = 0; s < RowRegistry::maxSliceRows; ++s)
            created += rows.add (i, Kind::slice, s) >= 0 ? 1 : 0;
    }
    const auto poolSize = RowRegistry::maxRows - RowRegistry::firstSubRow;
    check (created == poolSize && rows.numActive() == RowRegistry::numBaseRows + poolSize, "poolen fylls exakt och inget mer ryms: " + juce::String (created) + " av " + juce::String (poolSize));
    check (rows.add (0, Kind::slice, RowRegistry::maxSliceRows) == -1, "skiva 17 nekas (tak " + juce::String (RowRegistry::maxSliceRows) + ")");
    check (rows.add (11, Kind::sample) == -1 && rows.numActive() == RowRegistry::numBaseRows + poolSize, "ogiltigt instrument nekas utan följder");

    std::puts (failed ? "FEL: rowkeytest" : "radtestet gick igenom");
    return failed ? 1 : 0;
}
