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
    for (int layer = 0; layer < numRowSlots; ++layer)
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
            {
                n.on = false;
                n.clearExtras();
                n.clearLocks();
            }
            else
            {
                if (tick + n.length.load() > limit)
                    n.length = limit - tick; // en ton som går över slutet kortas
                for (auto& e : n.extra)
                    if (e.on.load() && tick + e.length.load() > limit)
                        e.length = limit - tick;
            }
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
    if (! n.on.load())
        n.clearLocks(); // en ny ton ärver inga lås (en ändrad ton behåller sina)
    n.length = juce::jlimit (1, maxTicks, length);
    n.pitch = juce::jlimit (minPitch, maxPitch, pitch);
    n.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    n.slide = slide;
    n.clearExtras(); // ... och inga gamla staplar
    n.setChord (chord); // en ny ton ärver inte ett gammalt ackord (sätts före on, så att ljudtråden aldrig ser tonen utan sitt ackord)
    n.on = true;
    return n;
}

bool Pattern::setLock (int layer, int tick, int param, float value)
{
    auto* n = existing (layer, tick);
    if (n == nullptr || ! n->on.load() || ! juce::isPositiveAndBelow (param, (int) numLocks))
        return false;
    n->lock[(size_t) param] = juce::jlimit (0.0f, 1.0f, value);
    return true;
}

void Pattern::clearLock (int layer, int tick, int param)
{
    if (auto* n = existing (layer, tick); n != nullptr && juce::isPositiveAndBelow (param, (int) numLocks))
        n->lock[(size_t) param] = lockUnset;
}

float Pattern::getLock (int layer, int tick, int param) const
{
    if (! juce::isPositiveAndBelow (param, (int) numLocks))
        return lockUnset;
    const auto& n = note (layer, tick);
    return n.on.load() ? n.lock[(size_t) param].load() : lockUnset;
}

int Pattern::stackSize (int layer, int tick) const
{
    const auto& n = note (layer, tick);
    if (! n.on.load())
        return 0;
    auto count = 1;
    for (const auto& e : n.extra)
        count += e.on.load() ? 1 : 0;
    return count;
}

bool Pattern::getStack (int layer, int tick, int index, StackNote& out) const
{
    const auto& n = note (layer, tick);
    if (! n.on.load() || index < 0 || index >= maxStack)
        return false;
    if (index == 0)
    {
        out = { n.length.load(), n.pitch.load(), n.velocity.load(), n.slide.load() };
        return true;
    }
    const auto& e = n.extra[(size_t) index - 1];
    if (! e.on.load())
        return false;
    out = { e.length.load(), e.pitch.load(), e.velocity.load(), e.slide.load() };
    return true;
}

int Pattern::addExtra (int layer, int tick, const StackNote& s)
{
    auto* n = existing (layer, tick);
    if (n == nullptr || ! n->on.load())
        return -1;
    for (size_t i = 0; i < n->extra.size(); ++i)
    {
        auto& e = n->extra[i];
        if (e.on.load())
            continue;
        e.length = juce::jlimit (1, maxTicks, s.length);
        e.pitch = juce::jlimit (minPitch, maxPitch, s.pitch);
        e.velocity = juce::jlimit (0.0f, 1.0f, s.velocity);
        e.slide = s.slide;
        e.on = true; // sist: ljudtråden ser aldrig en halvfärdig ton
        return (int) i + 1;
    }
    return -1;
}

void Pattern::setStack (int layer, int tick, int index, const StackNote& s)
{
    auto* n = existing (layer, tick);
    if (n == nullptr || ! n->on.load() || index < 0 || index >= maxStack)
        return;
    if (index == 0)
    {
        n->length = juce::jlimit (1, maxTicks, s.length);
        n->pitch = juce::jlimit (minPitch, maxPitch, s.pitch);
        n->velocity = juce::jlimit (0.0f, 1.0f, s.velocity);
        n->slide = s.slide;
        return;
    }
    auto& e = n->extra[(size_t) index - 1];
    if (! e.on.load())
        return;
    e.length = juce::jlimit (1, maxTicks, s.length);
    e.pitch = juce::jlimit (minPitch, maxPitch, s.pitch);
    e.velocity = juce::jlimit (0.0f, 1.0f, s.velocity);
    e.slide = s.slide;
}

void Pattern::removeStack (int layer, int tick, int index)
{
    if (index <= 0)
        removeNote (layer, tick);
    else if (index < maxStack)
        if (auto* n = existing (layer, tick))
            n->extra[(size_t) index - 1].on = false;
}

int Pattern::maxSimultaneous (int layer) const
{
    std::vector<int> active;
    auto most = 0;
    forEachNote (layer, [&] (int tick, const Note& n)
    {
        active.erase (std::remove_if (active.begin(), active.end(), [tick] (int end) { return end <= tick; }), active.end());
        if (n.slide.load() && ! active.empty())
            return; // en glide-ton återanvänder den klingande rösten (extras av en glide-ton också)
        active.push_back (tick + std::max (1, n.length.load()));
        for (const auto& c : n.chord)
            if (c.load() != 0)
                active.push_back (tick + std::max (1, n.length.load()));
        for (const auto& e : n.extra)
            if (e.on.load())
                active.push_back (tick + std::max (1, e.length.load()));
        most = std::max (most, (int) active.size());
    });
    return most;
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
    layer = juce::jlimit (0, numRowSlots - 1, layer);
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
    // R9b: därefter de underrader som finns (id 11 och uppåt). De märks med `row` (nyckeln, t.ex. "i3.sample") och `loop`; grundraderna får inget
    // `row` (filerna blir lika som förut när inga underrader finns) och en äldre version hoppar över lager över 10.
    for (int layer = 0; layer < numRowSlots; ++layer)
    {
        if (layer >= numNoteLayers && ! rows.isActive (layer))
            continue;
        juce::StringArray entries, lockEntries;
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
            auto hasExtras = false;
            for (const auto& e : n.extra)
                hasExtras = hasExtras || e.on.load();
            if (hasExtras) // polyfoni: ackordet paddas till tre fält så att /e börjar på en fast plats
                for (const auto interval : n.getChord())
                    entry << "/" << interval;
            else
                for (const auto interval : n.getChord()) // fas P: /c1/c2/c3 bara om tonen har ett ackord (äldre versioner läser bara de fem första)
                    if (interval != 0)
                        entry << "/" << interval;
            for (const auto& e : n.extra) // polyfoni: /e/pitch/length/velocity/slide per staplad ton
                if (e.on.load())
                    entry << "/e/" << e.pitch.load() << "/" << e.length.load() << "/" << juce::String (e.velocity.load(), 3) << "/" << (e.slide.load() ? 1 : 0);
            entries.add (entry);
            if (n.hasAnyLock()) // parameterlås: tick/param/värde, bara satta (egenskapen saknas helt när inga lås finns)
                for (int p = 0; p < numLocks; ++p)
                    if (n.hasLock (p))
                        lockEntries.add (juce::String (tick) + "/" + juce::String (p) + "/" + juce::String (n.lock[(size_t) p].load(), 4));
        }

        juce::ValueTree child ("Notes");
        child.setProperty ("layer", layer, nullptr);
        child.setProperty ("notes", entries.joinIntoString (";"), nullptr);
        if (lockEntries.size() > 0)
            child.setProperty ("locks", lockEntries.joinIntoString (";"), nullptr);
        child.setProperty ("custom", isCustom (layer), nullptr);
        if (layer >= numNoteLayers)
        {
            child.setProperty ("row", rows.keyOf (layer), nullptr);
            child.setProperty ("loop", loopsRow (layer), nullptr);
        }
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
    if (! tree.hasType (treeType) || ! juce::isPositiveAndBelow (layer, numRowSlots) || (layer >= numNoteLayers && ! rows.isActive (layer)))
        return;
    auto copy = tree.createCopy();
    for (auto child : copy)
    {
        child.setProperty ("layer", layer, nullptr);
        child.removeProperty ("row", nullptr); // raden är redan vald (layer); nyckeln i en sparad underrad ska inte flytta den
    }
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

    // R9b: en full laddning ersätter underraderna också (en rad finns bara om trädet nämner den).
    if (clearUnmentionedLayers)
    {
        for (int id = numNoteLayers; id < numRowSlots; ++id)
            if (rows.isActive (id))
            {
                clearNotes (id);
                manualOf[(size_t) id] = extraOf[(size_t) id] = 0;
                setLoopsRow (id, true);
            }
        rows.clearSubRows();
    }

    std::array<bool, numRowSlots> cleared {};
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
        if (child.hasProperty ("row")) // R9b: nyckeln gäller före indexet; en okänd nyckel eller full pool hoppas över
        {
            layer = rows.idOfKeyCreate (child["row"].toString());
            if (layer < 0)
                continue;
        }
        if (! juce::isPositiveAndBelow (layer, numRowSlots) || (layer >= numNoteLayers && ! rows.isActive (layer)))
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
            if (layer >= numNoteLayers)
                setLoopsRow (layer, ! child.hasProperty ("loop") || (bool) child["loop"]);
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
                // Fält 5-7 är ackordet (saknade blir 0) tills ett "e" börjar staplarna; ett trasigt eller avkapat /e hoppas över utan fel.
                auto firstExtra = parts.size();
                for (int i = 5; i < parts.size(); ++i)
                    if (parts[i] == "e") { firstExtra = i; break; }
                if (firstExtra > 5)
                    n.setChord ({ parts[5].getIntValue(), firstExtra > 6 ? parts[6].getIntValue() : 0, firstExtra > 7 ? parts[7].getIntValue() : 0 });
                for (int i = firstExtra, slot = 0; i + 4 < parts.size() && parts[i] == "e" && slot < maxExtraNotes; i += 5)
                {
                    StackNote s;
                    s.pitch = parts[i + 1].getIntValue();
                    s.length = parts[i + 2].getIntValue();
                    s.velocity = parts[i + 3].getFloatValue();
                    s.slide = parts[i + 4].getIntValue() != 0;
                    if (addExtra (layer, tick, s) >= 0)
                        ++slot;
                }
            }
            for (const auto& entry : split (child["locks"], ";")) // version 8: tick/param/värde; ett trasigt eller okänt lås hoppas över
            {
                const auto parts = juce::StringArray::fromTokens (entry, "/", {});
                if (parts.size() == 3 && juce::isPositiveAndBelow (parts[0].getIntValue(), maxTicks))
                    setLock (layer, parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getFloatValue());
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
