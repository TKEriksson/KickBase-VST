#include "Pattern.h"
#include <algorithm>

int Pattern::instrumentStepsPerBar (InstrumentGrid grid)
{
    switch (grid)
    {
        case InstrumentGrid::quarter:             return 4;
        case InstrumentGrid::eighth:              return 8;
        case InstrumentGrid::eighthTriplet:       return 12;
        case InstrumentGrid::sixteenth:           return 16;
        case InstrumentGrid::sixteenthTriplet:    return 24;
        case InstrumentGrid::thirtySecond:        return 32;
        case InstrumentGrid::thirtySecondTriplet: return 48;
        case InstrumentGrid::sixtyFourth:         return 64;
    }
    return 16;
}

double Pattern::instrumentStepsPerQuarter (InstrumentGrid grid)
{
    return instrumentStepsPerBar (grid) / 4.0;
}

Pattern::~Pattern()
{
    for (auto& row : noteBlocks)
        for (auto& block : row)
            delete block.load();
    for (auto& row : gateBlocks)
        for (auto& block : row)
            delete block.load();
}

const Pattern::Note& Pattern::note (int layer, int tick) const
{
    tick = clampedTick (tick);
    const auto* block = blockOf (layer, tick / ticksPerBar);
    return block != nullptr ? block->notes[(size_t) (tick % ticksPerBar)] : emptyNote;
}

Pattern::Note& Pattern::edit (int layer, int tick)
{
    layer = clampedLayer (layer);
    tick = clampedTick (tick);
    auto& slot = noteBlocks[(size_t) layer][(size_t) (tick / ticksPerBar)];
    auto* block = slot.load (std::memory_order_acquire);
    if (block == nullptr)
    {
        block = new NoteBlock();
        slot.store (block, std::memory_order_release); // bara meddelandetråden skriver pekaren
    }
    return block->notes[(size_t) (tick % ticksPerBar)];
}

Pattern::Note* Pattern::existing (int layer, int tick)
{
    tick = clampedTick (tick);
    auto* block = noteBlocks[(size_t) clampedLayer (layer)][(size_t) (tick / ticksPerBar)].load (std::memory_order_acquire);
    return block != nullptr ? &block->notes[(size_t) (tick % ticksPerBar)] : nullptr;
}

bool Pattern::hasBar (int layer, int bar) const
{
    return juce::isPositiveAndBelow (bar, maxBars) && blockOf (layer, bar) != nullptr;
}

void Pattern::releaseNoteBlocks (int layer, int fromBar)
{
    for (int bar = std::max (0, fromBar); bar < maxBars; ++bar)
        if (auto* block = noteBlocks[(size_t) clampedLayer (layer)][(size_t) bar].exchange (nullptr, std::memory_order_acq_rel))
            retiredNoteBlocks.retire (std::unique_ptr<NoteBlock> (block)); // ljudtråden kan läsa blocket just nu: det frigörs först senare
}

bool Pattern::isGateStepOn (int layer, int index) const
{
    index = juce::jlimit (0, maxGateSteps - 1, index);
    const auto* block = gateBlocks[(size_t) juce::jlimit (0, numLayers - 1, layer)][(size_t) (index / gateBlockSteps)].load (std::memory_order_acquire);
    return block == nullptr || block->steps[(size_t) (index % gateBlockSteps)].load();
}

void Pattern::setGateStep (int layer, int index, bool open)
{
    layer = juce::jlimit (0, numLayers - 1, layer);
    index = juce::jlimit (0, maxGateSteps - 1, index);
    auto& slot = gateBlocks[(size_t) layer][(size_t) (index / gateBlockSteps)];
    auto* block = slot.load (std::memory_order_acquire);
    if (block == nullptr)
    {
        if (open)
            return; // ett block som saknas är redan öppet
        block = new GateBlock();
        slot.store (block, std::memory_order_release);
    }
    block->steps[(size_t) (index % gateBlockSteps)] = open;
}

void Pattern::refreshLengths()
{
    for (int layer = 0; layer < numNoteLayers; ++layer)
    {
        auto content = 0;
        for (int bar = 0; bar < maxBars; ++bar)
        {
            auto& slot = noteBlocks[(size_t) layer][(size_t) bar];
            const auto* block = slot.load (std::memory_order_acquire);
            if (block == nullptr)
                continue;

            auto any = false;
            for (int i = 0; i < ticksPerBar; ++i)
            {
                const auto& n = block->notes[(size_t) i];
                if (! n.on.load())
                    continue;
                any = true;
                const auto lastTick = bar * ticksPerBar + i + std::max (1, n.length.load()) - 1; // tonens sista tick: en lång ton täcker takter
                content = std::max (content, lastTick / ticksPerBar + 1);
            }
            if (! any) // ett block utan toner behövs inte: frigörs (ljudtråden kan läsa det just nu, därför DeferredRelease)
                if (auto* empty = slot.exchange (nullptr, std::memory_order_acq_rel))
                    retiredNoteBlocks.retire (std::unique_ptr<NoteBlock> (empty));
        }
        content = juce::jlimit (1, maxBars, content);

        if (extraOf[(size_t) layer].load() > 0 && content >= extraOf[(size_t) layer].load())
            extraOf[(size_t) layer] = 0; // innehållet har nått takten som "+" lade till: raden följer innehållet igen

        contentOf[(size_t) layer] = content;
        const auto manual = manualOf[(size_t) layer].load();
        lengthOf[(size_t) layer] = manual > 0 ? manual : std::max (content, extraOf[(size_t) layer].load());
    }
}

void Pattern::setManualBars (int layer, int bars)
{
    manualOf[(size_t) clampedLayer (layer)] = juce::jlimit (0, maxBars, bars);
    refreshLengths();
}

void Pattern::setExtraBars (int layer, int bars)
{
    extraOf[(size_t) clampedLayer (layer)] = juce::jlimit (0, maxBars, bars);
    refreshLengths();
}

void Pattern::trimRow (int layer, int bars)
{
    layer = clampedLayer (layer);
    const auto limit = std::max (1, bars) * ticksPerBar;
    for (int bar = 0; bar < maxBars; ++bar) // från takt 0: en lång ton som startar tidigare kortas också vid slutet
    {
        auto* block = noteBlocks[(size_t) layer][(size_t) bar].load (std::memory_order_acquire);
        if (block == nullptr)
            continue;
        for (int i = 0; i < ticksPerBar; ++i)
        {
            auto& n = block->notes[(size_t) i];
            const auto tick = bar * ticksPerBar + i;
            if (! n.on.load())
                continue;
            if (tick >= limit)
                n.on = false;
            else if (tick + n.length.load() > limit)
                n.length = limit - tick; // en ton som går över slutet kortas
        }
    }
    refreshLengths();
}

void Pattern::applyLegacyLengths (int total, const std::array<int, numPatternRows>& rows, int onlyLayer)
{
    // Gamla regeln: totalen är minst så lång som den längsta raden, en rad med 0 följer totalen, och rader kortare än totalen loopar.
    auto longest = std::max (1, total);
    for (const auto rowBars : rows)
        longest = std::max (longest, rowBars);
    for (int row = 0; row < numPatternRows; ++row)
    {
        const auto layer = row == 0 ? kickLayer : row - 1;
        if (onlyLayer >= 0 && layer != onlyLayer)
            continue;
        const auto bars = rows[(size_t) row] > 0 ? std::min (rows[(size_t) row], longest) : longest;
        trimRow (layer, bars);
        manualOf[(size_t) layer] = contentBars (layer) < bars ? bars : 0; // innehållet fyller längden: AUTO räcker
        extraOf[(size_t) layer] = 0;
    }
    refreshLengths();
}

int Pattern::maxSimultaneous (int layer) const
{
    // forEachNote ger tonerna i tidsordning: `active` håller slutticken för de toner som fortfarande klingar (en per röst).
    std::vector<int> active;
    int most = 0;
    forEachNote (layer, [&] (int tick, const Note& n)
    {
        active.erase (std::remove_if (active.begin(), active.end(), [tick] (int end) { return end <= tick; }), active.end());
        if (n.slide.load() && ! active.empty())
            return; // glide: återanvänder rösten som klingar
        const auto end = tick + std::max (1, n.length.load());
        const auto count = 1 + (int) std::count_if (n.chord.begin(), n.chord.end(), [] (const auto& c) { return c.load() != 0; });
        for (int i = 0; i < count; ++i)
            active.push_back (end);
        most = std::max (most, (int) active.size());
    });
    return most;
}

int Pattern::firstNoteIn (int layer, int fromTick, int toTick) const
{
    toTick = std::min (maxTicks, toTick);
    for (int tick = std::max (0, fromTick); tick < toTick;)
    {
        const auto* block = blockOf (layer, tick / ticksPerBar);
        const auto barEnd = (tick / ticksPerBar + 1) * ticksPerBar;
        if (block == nullptr)
        {
            tick = barEnd; // en takt utan block har inga toner
            continue;
        }
        for (; tick < barEnd && tick < toTick; ++tick)
            if (block->notes[(size_t) (tick % ticksPerBar)].on.load())
                return tick;
    }
    return -1;
}

int Pattern::ticksToNextNote (int layer, int tick, int patternTicks, int limit) const
{
    patternTicks = std::max (1, std::min (patternTicks, maxTicks));
    for (int distance = 1; distance <= std::min (limit, patternTicks - 1); ++distance)
        if (note (layer, (tick + distance) % patternTicks).on.load())
            return distance;
    return limit;
}

double Pattern::holdTicks (int layer, int tick, int patternTicks, float gateScale) const
{
    juce::ignoreUnused (patternTicks);
    return std::max (1.0, (double) note (layer, tick).length.load() * (double) gateScale);
}

Pattern::Note& Pattern::addNote (int layer, int tick, int length, int pitch, float velocity, bool slide, const Chord& chord)
{
    auto& n = edit (layer, tick);
    n.length = juce::jlimit (1, maxTicks, length);
    n.pitch = juce::jlimit (minPitch, maxPitch, pitch);
    n.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    n.slide = slide;
    n.setChord (chord); // en ny ton ärver inte ett gammalt ackord (sätts före on, så att ljudtråden aldrig ser tonen utan sitt ackord)
    n.on = true;
    return n;
}

void Pattern::setGateSteps (int layer, const juce::String& steps)
{
    // Block för block, på plats: ljudtråden ser aldrig en halvfärdig rad (och inget "alla öppna" mellan två laddningar). Steg som strängen inte
    // nämner är öppna.
    layer = juce::jlimit (0, numLayers - 1, layer);
    const auto text = steps.toStdString(); // bara '0' och '1': ett byte per steg
    for (int chunk = 0; chunk < maxGateSteps / gateBlockSteps; ++chunk)
    {
        std::array<bool, gateBlockSteps> wanted;
        auto allOpen = true;
        for (int i = 0; i < gateBlockSteps; ++i)
        {
            const auto index = (size_t) (chunk * gateBlockSteps + i);
            wanted[(size_t) i] = index >= text.size() || text[index] != '0';
            allOpen = allOpen && wanted[(size_t) i];
        }

        auto& slot = gateBlocks[(size_t) layer][(size_t) chunk];
        if (allOpen)
        {
            if (auto* block = slot.exchange (nullptr, std::memory_order_acq_rel))
                retiredGateBlocks.retire (std::unique_ptr<GateBlock> (block));
            continue;
        }
        auto* block = slot.load (std::memory_order_acquire);
        if (block == nullptr)
        {
            block = new GateBlock();
            slot.store (block, std::memory_order_release);
        }
        for (int i = 0; i < gateBlockSteps; ++i)
            block->steps[(size_t) i] = wanted[(size_t) i];
    }
}

void Pattern::setGate (int layer, bool open)
{
    layer = juce::jlimit (0, numLayers - 1, layer);
    for (auto& slot : gateBlocks[(size_t) layer])
    {
        if (open)
        {
            // Öppen = inga block (ett block som saknas är öppet). Ljudtråden kan läsa dem just nu: DeferredRelease.
            if (auto* block = slot.exchange (nullptr, std::memory_order_acq_rel))
                retiredGateBlocks.retire (std::unique_ptr<GateBlock> (block));
        }
        else if (slot.load() == nullptr)
            slot.store (new GateBlock(), std::memory_order_release);

        if (! open)
            for (auto& step : slot.load()->steps)
                step = false;
    }
}

void Pattern::setKickSteps (const juce::String& steps, int stepTicks)
{
    clearNotes (kickLayer);
    for (int i = 0; i < steps.length() && (i + 1) * stepTicks <= maxTicks; ++i)
        if (steps[i] != '.')
            addNote (kickLayer, i * stepTicks, stepTicks, 0, steps[i] == 'X' ? 1.0f : defaultKickVelocity);
}

void Pattern::clearNotes (int layer)
{
    layer = juce::jlimit (0, numNoteLayers - 1, layer);
    custom[(size_t) layer] = false;
    releaseNoteBlocks (layer, 0); // en rad utan toner har inga block
}

InstrumentGrid Pattern::applyTemplate (PatternTemplate t, bool keepOtherInstruments, int fillBars)
{
    fillBars = juce::jlimit (1, maxBars, fillBars);

    // Kick på varje fjärdedel i alla mallar utom Clear.
    clearNotes (kickLayer);
    if (t != PatternTemplate::clear)
        for (int tick = 0; tick < fillBars * ticksPerBar; tick += ticksPerQuarter)
            addNote (kickLayer, tick, ticksPerQuarter / 4, 0, defaultKickVelocity);

    if (! keepOtherInstruments)
        for (int layer = 1; layer < numLayers; ++layer)
            clearNotes (layer);
    for (int layer = 0; layer < (keepOtherInstruments ? 1 : numLayers); ++layer)
        setGate (layer, true);
    const auto grid = applyLayerTemplate (0, t, fillBars);
    for (int layer = 0; layer < (keepOtherInstruments ? 1 : numNoteLayers); ++layer)
        manualOf[(size_t) layer] = extraOf[(size_t) layer] = 0; // längden följer innehållet igen
    refreshLengths();
    return grid;
}

InstrumentGrid Pattern::applyLayerTemplate (int layer, PatternTemplate t, int fillBars)
{
    fillBars = juce::jlimit (1, maxBars, fillBars);
    auto grid = InstrumentGrid::sixteenth;
    if (t == PatternTemplate::triplet)
        grid = InstrumentGrid::eighthTriplet;

    clearNotes (layer);

    const auto perQuarter = (int) instrumentStepsPerQuarter (grid);
    const auto stepTicks = ticksPerStep (grid);

    for (int i = 0; i < fillBars * instrumentStepsPerBar (grid); ++i)
    {
        const auto inBeat = i % perQuarter;
        auto on = false;
        const auto noteLength = defaultGate;

        switch (t)
        {
            case PatternTemplate::rolling:   on = inBeat != 0; break;                      // K B B B
            case PatternTemplate::galloping: on = inBeat >= 2; break;                      // K . B B
            case PatternTemplate::triplet:   on = inBeat != 0; break;                      // K B B
            case PatternTemplate::offbeat:   on = inBeat == 2; break;                      // K . B .
            case PatternTemplate::clear:     break;
        }

        if (on)
            addNote (layer, i * stepTicks, juce::roundToInt (noteLength * (float) stepTicks));
    }

    refreshLengths();
    return grid;
}

namespace
{
    juce::StringArray split (const juce::var& value, const juce::String& separator)
    {
        return juce::StringArray::fromTokens (value.toString(), separator, {});
    }
}

juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree tree (treeType);
    tree.setProperty ("version", currentVersion, nullptr);

    // En rad per lager: start/längd/ton/slide/velocity för varje ton, åtskilda med semikolon.
    for (int layer = 0; layer < numNoteLayers; ++layer)
    {
        juce::StringArray entries;
        for (int tick = 0; tick < maxTicks; ++tick)
        {
            if (! hasBar (layer, tick / ticksPerBar))
            {
                tick = (tick / ticksPerBar + 1) * ticksPerBar - 1; // en takt utan block har inga toner
                continue;
            }
            const auto& n = note (layer, tick);
            if (! n.on.load())
                continue;
            auto entry = juce::String (tick) + "/" + juce::String (n.length.load()) + "/" + juce::String (n.pitch.load()) + "/"
                         + (n.slide.load() ? "1" : "0") + "/" + juce::String (n.velocity.load(), 3);
            for (const auto interval : n.getChord()) // fas P: /c1/c2/c3 bara om tonen har ett ackord (äldre versioner läser bara de fem första)
                if (interval != 0)
                    entry << "/" << interval;
            entries.add (entry);
        }

        juce::ValueTree child ("Notes");
        child.setProperty ("layer", layer, nullptr);
        child.setProperty ("notes", entries.joinIntoString (";"), nullptr);
        child.setProperty ("custom", isCustom (layer), nullptr);
        child.setProperty ("length", manualBars (layer) > 0 ? juce::String (manualBars (layer)) : juce::String ("auto"), nullptr); // AUTO eller valt antal takter
        if (extraBars (layer) > 0)
            child.setProperty ("extra", extraBars (layer), nullptr);
        tree.appendChild (child, nullptr);

        if (layer >= numLayers)
            continue; // kicken har ingen gate

        // GATE-stegen: en siffra per steg (1 = öppet).
        // (bara fram till sista stängda steget: det som inte nämns är öppet)
        auto lastClosed = -1;
        for (int chunk = maxGateSteps / gateBlockSteps - 1; chunk >= 0 && lastClosed < 0; --chunk)
            if (gateBlocks[(size_t) layer][(size_t) chunk].load() != nullptr)
                for (int i = gateBlockSteps - 1; i >= 0; --i)
                    if (! isGateStepOn (layer, chunk * gateBlockSteps + i))
                    {
                        lastClosed = chunk * gateBlockSteps + i;
                        break;
                    }
        juce::String steps;
        for (int i = 0; i <= lastClosed; ++i)
            steps << (isGateStepOn (layer, i) ? '1' : '0');
        juce::ValueTree gateChild ("Gate");
        gateChild.setProperty ("layer", layer, nullptr);
        gateChild.setProperty ("steps", steps, nullptr);
        tree.appendChild (gateChild, nullptr);
    }

    return tree;
}

juce::ValueTree Pattern::rowToValueTree (int layer) const
{
    const auto full = toValueTree();
    juce::ValueTree row (treeType);
    row.setProperty ("version", currentVersion, nullptr);
    for (const auto& child : full)
        if ((int) child.getProperty ("layer", -1) == layer)
            row.appendChild (child.createCopy(), nullptr);
    return row;
}

void Pattern::rowFromValueTree (int layer, const juce::ValueTree& tree)
{
    if (! tree.hasType (treeType) || ! juce::isPositiveAndBelow (layer, numNoteLayers))
        return;
    auto copy = tree.createCopy();
    for (auto child : copy)
        child.setProperty ("layer", layer, nullptr);
    // Det finns alltid en Notes-rad (även utan toner) i en sparad rad: raden töms först, så att inget gammalt blir kvar.
    clearNotes (layer);
    fromValueTree (copy, false);
}

void Pattern::fromValueTree (const juce::ValueTree& tree, bool clearUnmentionedLayers)
{
    if (! tree.hasType (treeType))
        return;

    // Äldre projekt (version < 5) hade kicken som 16 steg per takt; varje på-steg blir en ton på sin plats i tiden.
    if (tree.hasProperty ("kick"))
    {
        const auto kicks = split (tree["kick"], ",");
        clearNotes (kickLayer);
        const auto stepTicks = ticksPerBar / 16;
        for (int i = 0; i < kicks.size() && (i + 1) * stepTicks <= maxTicks; ++i)
            if (kicks[i].getIntValue() != 0)
                addNote (kickLayer, i * stepTicks, stepTicks, 0, defaultKickVelocity);
    }

    // Version 1 hade bara rutnäten 1/16, 1/8 T och 1/16 T, i den ordningen.
    const auto version = (int) tree.getProperty ("version", 1);
    auto gridIndex = [version] (int stored)
    {
        if (version >= 2)
            return stored;
        constexpr InstrumentGrid oldOrder[] { InstrumentGrid::sixteenth, InstrumentGrid::eighthTriplet, InstrumentGrid::sixteenthTriplet };
        return juce::isPositiveAndBelow (stored, 3) ? (int) oldOrder[stored] : -1;
    };

    std::array<bool, numNoteLayers> cleared {};
    auto clearOnce = [&] (int layer)
    {
        if (! cleared[(size_t) layer])
        {
            clearNotes (layer);
            cleared[(size_t) layer] = true;
        }
    };

    for (const auto& child : tree)
    {
        auto layer = (int) child.getProperty ("layer", 0);
        if (version <= 5 && child.hasType ("Notes") && layer == 3)
            layer = kickLayer; // före version 6 var kicken lager 3 (det fanns tre instrument)
        if (! juce::isPositiveAndBelow (layer, numNoteLayers))
            continue;

        if (child.hasType ("Gate") && layer < numLayers)
        {
            // Tecken för tecken med en pekare: String::operator[] och length() går igenom UTF-8-texten från början varje gång, vilket gjorde
            // det här kvadratiskt (4096 steg per lager) och tog nära en sekund varje gång ett tillstånd laddades.
            setGateSteps (layer, child["steps"].toString());
        }
        else if (child.hasType ("Notes"))
        {
            clearOnce (layer);
            setCustom (layer, (bool) child["custom"]);
            const auto lengthText = child["length"].toString(); // saknas i projekt före version 7 (då AUTO; gamla längder: applyLegacyLengths)
            manualOf[(size_t) layer] = juce::jlimit (0, maxBars, lengthText.containsOnly ("0123456789") ? lengthText.getIntValue() : 0);
            extraOf[(size_t) layer] = juce::jlimit (0, maxBars, (int) child["extra"]);
            for (const auto& entry : split (child["notes"], ";"))
            {
                const auto parts = juce::StringArray::fromTokens (entry, "/", {});
                const auto tick = parts[0].getIntValue();
                if (parts.size() < 5 || ! juce::isPositiveAndBelow (tick, maxTicks))
                    continue;
                auto& n = addNote (layer, tick, parts[1].getIntValue(), parts[2].getIntValue(), parts[4].getFloatValue(), parts[3].getIntValue() != 0);
                if (parts.size() > 5)
                    n.setChord ({ parts[5].getIntValue(), parts[6].getIntValue(), parts[7].getIntValue() }); // (saknade fält blir 0)
            }
        }
        else if (child.hasType ("Bass") && layer < numLayers)
        {
            // Äldre projekt: ett steg per rutnät. Varje på-steg blir en ton på sin plats i tiden.
            const auto g = gridIndex ((int) child["grid"]);
            if (g < 0 || g >= numGrids)
                continue;

            clearOnce (layer);
            const auto grid = (InstrumentGrid) g;
            const auto stepTicks = ticksPerStep (grid);
            const auto on = split (child["on"], ","), pitch = split (child["pitch"], ","), slide = split (child["slide"], ",");
            const auto gates = split (child["gate"], ","), velocity = split (child["velocity"], ",");

            for (int i = 0; i < on.size(); ++i)
            {
                const auto tick = i * stepTicks;
                if (on[i].getIntValue() == 0 || tick >= maxTicks)
                    continue;

                const auto gateFraction = i < gates.size() ? juce::jlimit (0.05f, 1.0f, gates[i].getFloatValue()) : defaultGate;
                addNote (layer, tick, juce::roundToInt (gateFraction * (float) stepTicks), i < pitch.size() ? pitch[i].getIntValue() : 0,
                         i < velocity.size() ? velocity[i].getFloatValue() : 0.8f, i < slide.size() && slide[i].getIntValue() != 0);
            }
        }
    }

    // Lager som projektet inte nämner (t.ex. instrument 4-10 i ett äldre projekt) töms, så att inget gammalt blir kvar.
    for (int layer = 0; layer < numNoteLayers && clearUnmentionedLayers; ++layer)
        if (! cleared[(size_t) layer] && ! (layer == kickLayer && tree.hasProperty ("kick")))
        {
            clearNotes (layer);
            manualOf[(size_t) layer] = extraOf[(size_t) layer] = 0;
            if (layer < numLayers)
                setGate (layer, true);
        }

    refreshLengths();
}
