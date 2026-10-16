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
    const auto& n = note (layer, tick);
    const auto length = (double) n.length.load();
    if (n.slide.load())
        return std::max (length, (double) ticksToNextNote (layer, tick, patternTicks, 2 * ticksPerQuarter)) * slideOverlap;
    return std::max (1.0, length * (double) gateScale);
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

void Pattern::clearNotes (int layer)
{
    for (auto& n : notes[(size_t) layer])
    {
        n.on = false;
        n.length = 17;
        n.pitch = 0;
        n.slide = false;
        n.velocity = 0.8f;
    }
}

BassGrid Pattern::applyTemplate (PatternTemplate t)
{
    // Kick på varje fjärdedel i alla mallar utom Clear.
    for (int i = 0; i < maxKickSteps; ++i)
        kick[(size_t) i] = t != PatternTemplate::clear && i % 4 == 0;

    for (int layer = 1; layer < numLayers; ++layer)
        clearNotes (layer);
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
            case PatternTemplate::offbeat:   on = inBeat == 2; gate = 0.9f; break;         // K . B .
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

    juce::StringArray kicks;
    for (int i = 0; i < maxKickSteps; ++i)
        kicks.add (kick[(size_t) i].load() ? "1" : "0");
    tree.setProperty ("kick", kicks.joinIntoString (","), nullptr);

    // En rad per lager: start/längd/ton/slide/velocity för varje ton, åtskilda med semikolon.
    for (int layer = 0; layer < numLayers; ++layer)
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
        tree.appendChild (child, nullptr);
    }

    return tree;
}

void Pattern::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (treeType))
        return;

    const auto kicks = split (tree["kick"], ",");
    for (int i = 0; i < maxKickSteps && i < kicks.size(); ++i)
        kick[(size_t) i] = kicks[i].getIntValue() != 0;

    // Version 1 hade bara rutnäten 1/16, 1/8 T och 1/16 T, i den ordningen.
    const auto version = (int) tree.getProperty ("version", 1);
    auto gridIndex = [version] (int stored)
    {
        if (version >= 2)
            return stored;
        constexpr BassGrid oldOrder[] { BassGrid::sixteenth, BassGrid::eighthTriplet, BassGrid::sixteenthTriplet };
        return juce::isPositiveAndBelow (stored, 3) ? (int) oldOrder[stored] : -1;
    };

    std::array<bool, numLayers> cleared {};
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
        const auto layer = (int) child.getProperty ("layer", 0);
        if (! juce::isPositiveAndBelow (layer, numLayers))
            continue;

        if (child.hasType ("Notes"))
        {
            clearOnce (layer);
            for (const auto& entry : split (child["notes"], ";"))
            {
                const auto parts = juce::StringArray::fromTokens (entry, "/", {});
                const auto tick = parts[0].getIntValue();
                if (parts.size() >= 5 && juce::isPositiveAndBelow (tick, maxTicks))
                    addNote (layer, tick, parts[1].getIntValue(), parts[2].getIntValue(), parts[4].getFloatValue(), parts[3].getIntValue() != 0);
            }
        }
        else if (child.hasType ("Bass"))
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
}
