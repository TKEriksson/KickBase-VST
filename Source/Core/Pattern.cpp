#include "Pattern.h"

int Pattern::bassStepsPerBar (BassGrid grid)
{
    switch (grid)
    {
        case BassGrid::quarter:             return 4;
        case BassGrid::eighth:              return 8;
        case BassGrid::eighthTriplet:       return 12;
        case BassGrid::sixteenth:           return 16;
        case BassGrid::sixteenthTriplet:    return 24;
        case BassGrid::thirtySecond:        return 32;
        case BassGrid::thirtySecondTriplet: return 48;
        case BassGrid::sixtyFourth:         return 64;
    }
    return 16;
}

double Pattern::bassStepsPerQuarter (BassGrid grid)
{
    return bassStepsPerBar (grid) / 4.0;
}

int Pattern::firstNoteIn (int layer, int fromTick, int toTick) const
{
    for (int tick = std::max (0, fromTick); tick < std::min (maxTicks, toTick); ++tick)
        if (note (layer, tick).on.load())
            return tick;
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

Pattern::Note& Pattern::addNote (int layer, int tick, int length, int pitch, float velocity, bool slide)
{
    auto& n = note (layer, tick);
    n.length = juce::jlimit (1, maxTicks, length);
    n.pitch = juce::jlimit (minPitch, maxPitch, pitch);
    n.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    n.slide = slide;
    n.on = true;
    return n;
}

void Pattern::setGate (int layer, bool open)
{
    for (auto& step : gate[(size_t) juce::jlimit (0, numLayers - 1, layer)])
        step = open;
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
    for (int tick = 0; tick < maxTicks; ++tick)
    {
        auto& n = note (layer, tick);
        n.on = false;
        n.length = 24;
        n.pitch = 0;
        n.slide = false;
        n.velocity = 0.8f;
    }
}

BassGrid Pattern::applyTemplate (PatternTemplate t, bool keepOtherInstruments)
{
    // Kick på varje fjärdedel i alla mallar utom Clear.
    clearNotes (kickLayer);
    if (t != PatternTemplate::clear)
        for (int tick = 0; tick < maxTicks; tick += ticksPerQuarter)
            addNote (kickLayer, tick, ticksPerQuarter / 4, 0, defaultKickVelocity);

    if (! keepOtherInstruments)
        for (int layer = 1; layer < numLayers; ++layer)
            clearNotes (layer);
    for (int layer = 0; layer < (keepOtherInstruments ? 1 : numLayers); ++layer)
        setGate (layer, true);
    return applyLayerTemplate (0, t);
}

BassGrid Pattern::applyLayerTemplate (int layer, PatternTemplate t)
{
    auto grid = BassGrid::sixteenth;
    if (t == PatternTemplate::triplet)
        grid = BassGrid::eighthTriplet;

    clearNotes (layer);

    const auto perQuarter = (int) bassStepsPerQuarter (grid);
    const auto stepTicks = ticksPerStep (grid);

    for (int i = 0; i < maxBars * bassStepsPerBar (grid); ++i)
    {
        const auto inBeat = i % perQuarter;
        auto on = false;
        auto gate = defaultGate;

        switch (t)
        {
            case PatternTemplate::rolling:   on = inBeat != 0; break;                      // K B B B
            case PatternTemplate::galloping: on = inBeat >= 2; break;                      // K . B B
            case PatternTemplate::triplet:   on = inBeat != 0; break;                      // K B B
            case PatternTemplate::offbeat:   on = inBeat == 2; break;                      // K . B .
            case PatternTemplate::clear:     break;
        }

        if (on)
            addNote (layer, i * stepTicks, juce::roundToInt (gate * (float) stepTicks));
    }

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
            const auto& n = note (layer, tick);
            if (n.on.load())
                entries.add (juce::String (tick) + "/" + juce::String (n.length.load()) + "/" + juce::String (n.pitch.load()) + "/"
                             + (n.slide.load() ? "1" : "0") + "/" + juce::String (n.velocity.load(), 3));
        }

        juce::ValueTree child ("Notes");
        child.setProperty ("layer", layer, nullptr);
        child.setProperty ("notes", entries.joinIntoString (";"), nullptr);
        child.setProperty ("custom", isCustom (layer), nullptr);
        tree.appendChild (child, nullptr);

        if (layer >= numLayers)
            continue; // kicken har ingen gate

        // GATE-stegen: en siffra per steg (1 = öppet).
        juce::String steps;
        for (int i = 0; i < maxGateSteps; ++i)
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
        constexpr BassGrid oldOrder[] { BassGrid::sixteenth, BassGrid::eighthTriplet, BassGrid::sixteenthTriplet };
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
            const auto steps = child["steps"].toString();
            auto character = steps.getCharPointer();
            for (int i = 0; i < maxGateSteps && ! character.isEmpty(); ++i)
                gateStep (layer, i) = character.getAndAdvance() != '0';
        }
        else if (child.hasType ("Notes"))
        {
            clearOnce (layer);
            setCustom (layer, (bool) child["custom"]);
            for (const auto& entry : split (child["notes"], ";"))
            {
                const auto parts = juce::StringArray::fromTokens (entry, "/", {});
                const auto tick = parts[0].getIntValue();
                if (parts.size() >= 5 && juce::isPositiveAndBelow (tick, maxTicks))
                    addNote (layer, tick, parts[1].getIntValue(), parts[2].getIntValue(), parts[4].getFloatValue(), parts[3].getIntValue() != 0);
            }
        }
        else if (child.hasType ("Bass") && layer < numLayers)
        {
            // Äldre projekt: ett steg per rutnät. Varje på-steg blir en ton på sin plats i tiden.
            const auto g = gridIndex ((int) child["grid"]);
            if (g < 0 || g >= numGrids)
                continue;

            clearOnce (layer);
            const auto grid = (BassGrid) g;
            const auto stepTicks = ticksPerStep (grid);
            const auto on = split (child["on"], ","), pitch = split (child["pitch"], ","), slide = split (child["slide"], ",");
            const auto gate = split (child["gate"], ","), velocity = split (child["velocity"], ",");

            for (int i = 0; i < on.size(); ++i)
            {
                const auto tick = i * stepTicks;
                if (on[i].getIntValue() == 0 || tick >= maxTicks)
                    continue;

                const auto gateFraction = i < gate.size() ? juce::jlimit (0.05f, 1.0f, gate[i].getFloatValue()) : defaultGate;
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
            if (layer < numLayers)
                setGate (layer, true);
        }
}
