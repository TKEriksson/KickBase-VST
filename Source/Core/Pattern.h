#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>
#include <atomic>

// Basens rutnät: upplösningen i stegsekvenseraren och förvalda "snap" i pianorullen.
// Själva tonerna ligger fritt i tid (se Pattern::Note), så ett lager kan ha 16-delar, 8-delar
// och 64-delar i samma mönster. Ordningen är den som visas i listan; nya rutnät läggs sist.
enum class InstrumentGrid
{
    quarter,            //  4 steg per takt
    eighth,             //  8 steg per takt
    eighthTriplet,      // 12 steg per takt: K-B-B per slag
    sixteenth,          // 16 steg per takt: rullande, galopp
    sixteenthTriplet,   // 24 steg per takt
    thirtySecond,       // 32 steg per takt
    thirtySecondTriplet, // 48 steg per takt
    sixtyFourth         // 64 steg per takt
};

// Så många instrument (baslager) kan synthen ha som mest; hur många som används just nu styrs av parametern instrumentCount.
constexpr int maxInstruments = 10;
constexpr int numPatternRows = maxInstruments + 1; // rad 0 = kicken, rad 1... = instrumenten

// En array där alla platser har samma värde (std::array kan bara fyllas med en lista).
template <typename T, size_t N>
constexpr std::array<T, N> filledArray (T value)
{
    std::array<T, N> result {};
    for (auto& item : result)
        item = value;
    return result;
}

// Mönstrets längd: hela mönstret (`total`) och varje rads egen längd (kicken är rad 0, sedan instrumenten; 0 = hela
// mönstret). En rad som är kortare än mönstret loopar (`loops`, per rad) eller spelar en gång och tystnar tills mönstret börjar om.
struct PatternLengths
{
    int total = 1;
    std::array<bool, numPatternRows> loops = [] { std::array<bool, numPatternRows> all {}; all.fill (true); return all; }(); // loopar raden? (annars spelar den en gång och tystnar)
    std::array<int, numPatternRows> rows {};

    int barsOf (int row) const
    {
        const auto r = rows[(size_t) juce::jlimit (0, numPatternRows - 1, row)];
        return r <= 0 ? total : std::min (r, total);
    }

    // Takten inom radens egen längd, eller -1 när raden är tyst (spelar en gång och är klar).
    int barInRow (int row, juce::int64 bar) const
    {
        const auto inPattern = (int) (((bar % total) + total) % total);
        const auto rowBars = barsOf (row);
        if (! loops[(size_t) juce::jlimit (0, numPatternRows - 1, row)] && inPattern >= rowBars)
            return -1;
        return inPattern % rowBars;
    }
};

enum class PatternTemplate { rolling, galloping, triplet, offbeat, clear };

// Stegen i mönstret. Gränssnittet skriver och ljudtråden läser, därför är varje fält atomiskt:
// ingen av dem behöver låsa eller vänta på den andra.
struct Pattern
{
    static constexpr int maxBars = 64; // längsta mönstret: hela partier ryms i ett mönster
    static constexpr int numGrids = 8;
    static constexpr int minPitch = -96; // halvtoner från tonarten (basens grundton med lagrets oktav)
    static constexpr int maxPitch = 96;
    static constexpr int numLayers = maxInstruments; // instrument med var sitt mönster (FX-versionen använder bara det första)

    // Kicken är ett extra "notlager" efter instrumenten (lager 10): fritt placerade toner, på samma sätt som baslagren. Tonhöjden flyttar
    // kickens svans (halvtoner från tonarten), velocity styr volymen. Kicken har ingen gate.
    static constexpr int kickLayer = numLayers;
    static constexpr int numNoteLayers = numLayers + 1;
    static constexpr float defaultKickVelocity = 1.0f;
    static float defaultVelocity (int layer) { return layer == kickLayer ? defaultKickVelocity : 0.8f; }

    // Tiden räknas i "ticks": 96 per fjärdedel, så att alla rutnät (1/4 ... 1/64 och trioler) går jämnt upp.
    static constexpr int ticksPerQuarter = 96;
    static constexpr int ticksPerBar = ticksPerQuarter * 4;
    static constexpr int maxTicks = maxBars * ticksPerBar;
    static constexpr int maxGateSteps = maxBars * 64; // GATE-stegen: högst 64 per takt (rutnätet 1/64)
    static constexpr float defaultGate = 1.0f; // en ny ton fyller hela steget (lagrets GATE kortar den vid behov)

    static int instrumentStepsPerBar (InstrumentGrid grid);
    static double instrumentStepsPerQuarter (InstrumentGrid grid);
    static int ticksPerStep (InstrumentGrid grid) { return ticksPerBar / instrumentStepsPerBar (grid); }
    static juce::StringArray gridNames()     { return { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32", "1/32 T", "1/64" }; }
    static juce::StringArray templateNames() { return { "Rolling", "Galloping", "Triplet", "Offbeat", "Clear" }; }

    // En ton i basens mönster. Den ligger på sin starttick; ett lager har högst en ton per tick.
    struct Note
    {
        std::atomic<bool> on { false };
        std::atomic<int> length { 24 };       // i ticks
        std::atomic<int> pitch { 0 };         // halvtoner från tonarten
        std::atomic<bool> slide { false };    // glide-ton (som i FL Studio): glider den klingande tonen hit under sin längd
        std::atomic<float> velocity { 0.8f };
    };

    Pattern() { applyTemplate (PatternTemplate::rolling); }

    // Index utanför mönstret hamnar vid närmaste kant i stället för utanför minnet.
    Note& note (int layer, int tick)             { return notes[noteIndex (layer, tick)]; }
    const Note& note (int layer, int tick) const { return notes[noteIndex (layer, tick)]; }

    // Första tonen som startar i [fromTick, toTick), eller -1.
    int firstNoteIn (int layer, int fromTick, int toTick) const;

    // Antal ticks till nästa ton efter `tick` (cirkulärt i ett mönster på `patternTicks`), högst `limit`.
    int ticksToNextNote (int layer, int tick, int patternTicks, int limit) const;

    // Hur länge tonen hålls, i ticks: tonens längd skalad med lagrets LENGTH (bassNGate).
    double holdTicks (int layer, int tick, int patternTicks, float gateScale) const;

    // GATE: en egen av/på-rytm per lager (eget rutnät, bassNGateGrid) som hackar lagrets ljud. Öppet = true.
    std::atomic<bool>& gateStep (int layer, int index)
    {
        return gate[(size_t) juce::jlimit (0, numLayers - 1, layer)][(size_t) juce::jlimit (0, maxGateSteps - 1, index)];
    }
    bool isGateStepOn (int layer, int index) const
    {
        return gate[(size_t) juce::jlimit (0, numLayers - 1, layer)][(size_t) juce::jlimit (0, maxGateSteps - 1, index)].load();
    }
    void setGate (int layer, bool open);

    // Ett lager med "pianorulle": tonerna redigeras i pianorullen (frihand i tid, höjd och glide) och visas som en
    // miniatyr i stället för stegknappar, som i FL Studio. Sätts när man redigerar i pianorullen, och nollställs
    // när lagrets toner rensas eller fylls om.
    bool isCustom (int layer) const { return custom[(size_t) juce::jlimit (0, numNoteLayers - 1, layer)].load(); }
    void setCustom (int layer, bool value) { custom[(size_t) juce::jlimit (0, numNoteLayers - 1, layer)] = value; }

    // Lägger en ny ton (eller ändrar den som redan ligger där). Returnerar den.
    Note& addNote (int layer, int tick, int length, int pitch = 0, float velocity = 0.8f, bool slide = false);

    // Kicken som en rad ettor och punkter, `ticksPerStep` mellan stegen (för verktyg och tester).
    void setKickSteps (const juce::String& steps, int ticksPerStep);
    void removeNote (int layer, int tick) { note (layer, tick).on = false; }
    void clearNotes (int layer);

    // Fyller hela mönstret enligt en mall: kicken och första baslagret, övriga lager töms.
    // Returnerar det rutnät mallen är skriven för.
    // `keepOtherInstruments`: bara kicken och instrument 1 skrivs om; de övriga instrumentens toner och gate rörs inte.
    InstrumentGrid applyTemplate (PatternTemplate t, bool keepOtherInstruments = false);

    // Fyller bara ett baslager enligt en mall (kicken och andra lager orörda).
    InstrumentGrid applyLayerTemplate (int layer, PatternTemplate t);

    juce::ValueTree toValueTree() const;

    // En enda rad (instrument 0-9, eller kicken = kickLayer): dess toner (med pianorullens flagga) och GATE-steg. `rowFromValueTree` skriver raden
    // på det lager man anger (en rad sparad från ett instrument kan laddas i ett annat).
    juce::ValueTree rowToValueTree (int layer) const;
    void rowFromValueTree (int layer, const juce::ValueTree& tree);
    void fromValueTree (const juce::ValueTree& tree, bool clearUnmentionedLayers = true); // false: lager som trädet inte nämner lämnas orörda (en enskild rad)

    static inline const juce::Identifier treeType { "Pattern" };
    static constexpr int currentVersion = 6; // 6: upp till 10 instrument, kicken är lager 10 (före det lager 3); 5: kicken som notlager; 4: fria toner (ticks); 3: baslager med steg per rutnät

private:
    static size_t noteIndex (int layer, int tick)
    {
        return (size_t) juce::jlimit (0, numNoteLayers - 1, layer) * (size_t) maxTicks + (size_t) juce::jlimit (0, maxTicks - 1, tick);
    }

    std::vector<Note> notes = std::vector<Note> ((size_t) numNoteLayers * (size_t) maxTicks); // på heapen: många MB på stacken räcker inte
    std::array<std::array<std::atomic<bool>, maxGateSteps>, numLayers> gate {};
    std::array<std::atomic<bool>, numNoteLayers> custom {};
};
