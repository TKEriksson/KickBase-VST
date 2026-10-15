#include "Pattern.h"

int Pattern::bassStepsPerBar (BassGrid grid)
{
    switch (grid)
    {
        case BassGrid::quarter:          return 4;
        case BassGrid::eighth:           return 8;
        case BassGrid::eighthTriplet:    return 12;
        case BassGrid::sixteenth:        return 16;
        case BassGrid::sixteenthTriplet: return 24;
        case BassGrid::thirtySecond:     return 32;
    }
    return 16;
}

double Pattern::bassStepsPerQuarter (BassGrid grid)
{
    return bassStepsPerBar (grid) / 4.0;
}

BassGrid Pattern::applyTemplate (PatternTemplate t)
{
    // Kick på varje fjärdedel i alla mallar utom Clear.
    for (int i = 0; i < maxKickSteps; ++i)
        kick[(size_t) i] = t != PatternTemplate::clear && i % 4 == 0;

    auto grid = BassGrid::sixteenth;
    if (t == PatternTemplate::triplet)
        grid = BassGrid::eighthTriplet;

    // Nollställ alla rutnät, och fyll sedan mallens.
    for (auto& steps : bass)
    {
        for (auto& step : steps)
        {
            step.on = false;
            step.pitch = 0;
            step.slide = false;
            step.gate = 0.7f;
            step.velocity = 0.8f;
        }
    }

    const auto perQuarter = (int) bassStepsPerQuarter (grid);
    auto& steps = bass[(size_t) grid];

    for (int i = 0; i < maxBars * bassStepsPerBar (grid); ++i)
    {
        const auto inBeat = i % perQuarter;
        auto& step = steps[(size_t) i];

        switch (t)
        {
            case PatternTemplate::rolling:   step.on = inBeat != 0; break;                 // K B B B
            case PatternTemplate::galloping: step.on = inBeat >= 2; break;                 // K . B B
            case PatternTemplate::triplet:   step.on = inBeat != 0; break;                 // K B B
            case PatternTemplate::offbeat:   step.on = inBeat == 2; step.gate = 0.9f; break; // K . B .
            case PatternTemplate::clear:     break;
        }
    }

    return grid;
}

namespace
{
    template <typename T, typename Get>
    juce::String join (int count, Get&& get)
    {
        juce::StringArray parts;
        for (int i = 0; i < count; ++i)
            parts.add (juce::String (get (i)));
        return parts.joinIntoString (",");
    }

    juce::StringArray split (const juce::var& value)
    {
        return juce::StringArray::fromTokens (value.toString(), ",", {});
    }
}

juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree tree (treeType);
    tree.setProperty ("version", currentVersion, nullptr);
    tree.setProperty ("kick", join<int> (maxKickSteps, [this] (int i) { return kick[(size_t) i].load() ? 1 : 0; }), nullptr);

    for (int g = 0; g < numGrids; ++g)
    {
        const auto& steps = bass[(size_t) g];
        juce::ValueTree child ("Bass");
        child.setProperty ("grid", g, nullptr);
        child.setProperty ("on",       join<int>   (maxBassSteps, [&] (int i) { return steps[(size_t) i].on.load() ? 1 : 0; }), nullptr);
        child.setProperty ("pitch",    join<int>   (maxBassSteps, [&] (int i) { return steps[(size_t) i].pitch.load(); }), nullptr);
        child.setProperty ("slide",    join<int>   (maxBassSteps, [&] (int i) { return steps[(size_t) i].slide.load() ? 1 : 0; }), nullptr);
        child.setProperty ("gate",     join<float> (maxBassSteps, [&] (int i) { return steps[(size_t) i].gate.load(); }), nullptr);
        child.setProperty ("velocity", join<float> (maxBassSteps, [&] (int i) { return steps[(size_t) i].velocity.load(); }), nullptr);
        tree.appendChild (child, nullptr);
    }

    return tree;
}

void Pattern::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (treeType))
        return;

    const auto kicks = split (tree["kick"]);
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

    for (const auto& child : tree)
    {
        const auto g = gridIndex ((int) child["grid"]);
        if (! child.hasType ("Bass") || g < 0 || g >= numGrids)
            continue;

        auto& steps = bass[(size_t) g];
        const auto on = split (child["on"]), pitch = split (child["pitch"]), slide = split (child["slide"]);
        const auto gate = split (child["gate"]), velocity = split (child["velocity"]);

        for (int i = 0; i < maxBassSteps; ++i)
        {
            auto& step = steps[(size_t) i];
            if (i < on.size())       step.on = on[i].getIntValue() != 0;
            if (i < pitch.size())    step.pitch = juce::jlimit (minPitch, maxPitch, pitch[i].getIntValue());
            if (i < slide.size())    step.slide = slide[i].getIntValue() != 0;
            if (i < gate.size())     step.gate = juce::jlimit (0.05f, 1.0f, gate[i].getFloatValue());
            if (i < velocity.size()) step.velocity = juce::jlimit (0.0f, 1.0f, velocity[i].getFloatValue());
        }
    }
}
