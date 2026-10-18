#include "Modulation.h"

namespace Modulation
{
    juce::StringArray shapeNames()
    {
        return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random", "Custom" };
    }

    juce::StringArray rateNames()
    {
        return { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "1/4 T", "1/4", "1/2", "1 bar", "2 bars", "4 bars",
                 "3/16", "3/8", "3/4", "3 bars", "6 bars", "8 bars", "16 bars" };
    }

    juce::StringArray syncTimeNames()
    {
        return { "0", "1/64", "1/32", "1/16 T", "1/16", "1/16 D", "1/8 T", "1/8", "1/8 D", "1/4 T", "1/4", "1/4 D", "1/2", "1 bar", "2 bars", "4 bars" };
    }

    double syncTimeInQuarters (int index)
    {
        constexpr double quarters[] { 0.0, 1.0 / 16.0, 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
        return quarters[juce::jlimit (0, (int) std::size (quarters) - 1, index)];
    }

    double rateInQuarters (int index)
    {
        constexpr double quarters[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0, 0.75, 1.5, 3.0, 12.0, 24.0, 32.0, 64.0 };
        return quarters[juce::jlimit (0, (int) std::size (quarters) - 1, index)];
    }

    juce::String paramID (int lfo, const char* suffix)
    {
        return "lfo" + juce::String (lfo + 1) + suffix;
    }

    juce::String macroID (int macro)
    {
        return "macro" + juce::String (macro + 1);
    }

    juce::String kindName (int kind)
    {
        static const char* names[] { "ENV 2", "ENV 3", "VELOCITY", "RANDOM" };
        return names[juce::jlimit (0, instrumentSources - 1, kind)];
    }

    juce::String sourceName (int source)
    {
        if (isLfo (source))
            return "LFO " + juce::String (source + 1);
        if (isMacro (source))
            return "MACRO " + juce::String (source - firstMacro + 1);
        if (isInstrumentSource (source))
            return "INSTR " + juce::String (layerOfSource (source) + 1) + " " + kindName (kindOfSource (source));
        return "?";
    }

    bool isModulatable (const juce::RangedAudioParameter& parameter)
    {
        // Bara steglösa rattar för ljudet. Mastern läses utanför parametersystemet och LFO:erna
        // själva skulle bara bli förvirrande.
        if (dynamic_cast<const juce::AudioParameterFloat*> (&parameter) == nullptr)
            return false;
        const auto id = parameter.getParameterID();
        return id.startsWith ("kick") || id.startsWith ("bass") || id.startsWith ("fx") || id.startsWith ("gate");
    }

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        using namespace juce;
        for (int i = 0; i < numLfos; ++i)
        {
            const auto title = sourceName (i);
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (i, "On"), 1 }, title + " On", true));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Shape"), 1 }, title + " Shape", shapeNames(), 0));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Rate"), 1 }, title + " Rate", rateNames(), 8));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Mode"), 1 }, title + " Rate Mode", StringArray { "Sync", "Hz" }, 0));
            NormalisableRange<float> hzRange (0.02f, 40.0f, 0.001f);
            hzRange.setSkewForCentre (2.0f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramID (i, "Hz"), 1 }, title + " Free Rate", hzRange, 1.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
        }

        // Makron: fyra rattar att vrida för hand, som i Serum, och att dra till andra rattar eller sätta i matrisen.
        for (int m = 0; m < numMacros; ++m)
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { macroID (m), 1 }, "Macro " + String (m + 1),
                                                               NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    }

    float shapeValue (int shape, double phase, juce::int64 cycle)
    {
        const auto p = (float) phase;
        switch (shape)
        {
            case 0: return std::sin (juce::MathConstants<float>::twoPi * p);
            case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);  // triangel, börjar i -1
            case 2: return 2.0f * p - 1.0f;                    // såg upp
            case 3: return 1.0f - 2.0f * p;                    // såg ner
            case 4: return p < 0.5f ? 1.0f : -1.0f;            // fyrkant
            case 5:
            {
                // Slumpvärde per period, men samma värde för samma period: låter likadant varje gång låten spelas.
                juce::Random random ((juce::int64) (cycle * 2654435761LL + 12345));
                return random.nextFloat() * 2.0f - 1.0f;
            }
            default: return 0.0f;
        }
    }
}

//==============================================================================
const juce::Identifier ModRoutes::treeType { "ModRoutes" };

int ModRoutes::find (int source, int target) const
{
    for (int i = 0; i < (int) routes.size(); ++i)
        if (routes[(size_t) i].source.load() == source && routes[(size_t) i].target.load() == target)
            return i;
    return -1;
}

int ModRoutes::add (int source, int target, float amount)
{
    if (const auto existing = find (source, target); existing >= 0)
        return existing;

    for (int i = 0; i < (int) routes.size(); ++i)
    {
        auto& route = routes[(size_t) i];
        if (route.source.load() < 0)
        {
            route.amount = amount;
            route.target = target;
            route.source = source; // sist: nu blir kopplingen aktiv
            ++version;
            return i;
        }
    }
    return -1;
}

void ModRoutes::remove (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) routes.size()))
        return;
    routes[(size_t) index].source = -1; // först: kopplingen slutar verka direkt
    routes[(size_t) index].target = -1;
    routes[(size_t) index].amount = 0.0f;
    ++version;
}

void ModRoutes::clear()
{
    for (int i = 0; i < (int) routes.size(); ++i)
        remove (i);
}

juce::ValueTree ModRoutes::toValueTree (const juce::Array<juce::AudioProcessorParameter*>& parameters) const
{
    juce::ValueTree tree (treeType);
    for (const auto& route : routes)
    {
        const auto source = route.source.load(), target = route.target.load();
        if (source < 0 || ! juce::isPositiveAndBelow (target, parameters.size()))
            continue;
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[target]))
        {
            juce::ValueTree child ("Route");
            child.setProperty ("source", source, nullptr);
            child.setProperty ("target", withId->paramID, nullptr);
            child.setProperty ("amount", (double) route.amount.load(), nullptr);
            tree.appendChild (child, nullptr);
        }
    }
    return tree;
}

void ModRoutes::fromValueTree (const juce::ValueTree& tree, const juce::Array<juce::AudioProcessorParameter*>& parameters)
{
    clear();
    for (const auto& child : tree)
    {
        const auto id = child.getProperty ("target").toString();
        for (int i = 0; i < parameters.size(); ++i)
        {
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == id)
            {
                add (juce::jlimit (0, Modulation::numSources - 1, (int) child.getProperty ("source")), i,
                     (float) (double) child.getProperty ("amount"));
                break;
            }
        }
    }
    ++version;
}

//==============================================================================
Modulator::Modulator (juce::AudioProcessorValueTreeState& apvts, const ModRoutes& routesIn) : routes (routesIn)
{
    using namespace Modulation;
    for (int i = 0; i < numLfos; ++i)
    {
        auto& lfo = lfos[(size_t) i];
        lfo.on = apvts.getRawParameterValue (paramID (i, "On"));
        lfo.shape = apvts.getRawParameterValue (paramID (i, "Shape"));
        lfo.rate = apvts.getRawParameterValue (paramID (i, "Rate"));
        lfo.mode = apvts.getRawParameterValue (paramID (i, "Mode"));
        lfo.hz = apvts.getRawParameterValue (paramID (i, "Hz"));
    }
    for (int m = 0; m < numMacros; ++m)
        macros[(size_t) m] = apvts.getRawParameterValue (macroID (m));
    for (auto& display : sourceDisplay)
        display = 0.0f;

    // Samma ordning som processorns parameterlista, så att index stämmer överallt.
    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        parameters.push_back (ranged);
        raws.push_back (ranged != nullptr ? apvts.getRawParameterValue (ranged->getParameterID()) : nullptr);
    }
    offsets.assign (parameters.size(), 0.0f);
    displayOffsets = std::make_unique<std::atomic<float>[]> (parameters.size());
    for (size_t i = 0; i < parameters.size(); ++i)
        displayOffsets[i] = 0.0f;
    touched.reserve ((size_t) maxRoutes);
}

int Modulator::indexOf (const juce::String& paramID) const
{
    for (size_t i = 0; i < parameters.size(); ++i)
        if (parameters[i] != nullptr && parameters[i]->getParameterID() == paramID)
            return (int) i;
    return -1;
}

void Modulator::update (double ppq, double bpm)
{
    using namespace Modulation;

    std::array<float, numSources> sourceValues {};
    std::array<bool, numSources> sourceOn {};
    sourceOn.fill (true);
    auto& lfoValues = sourceValues;
    auto& lfoOn = sourceOn;
    for (int i = 0; i < numLfos; ++i)
    {
        const auto& lfo = lfos[(size_t) i];
        // Tempo-synkad (en period i fjärdedelar) eller fri hastighet i Hz (sekunder räknade ur positionen och tempot).
        const auto position = lfo.mode->load() >= 0.5f ? ppq * 60.0 / std::max (20.0, bpm) * (double) lfo.hz->load()
                                                       : ppq / rateInQuarters ((int) lfo.rate->load());
        const auto cycle = (juce::int64) std::floor (position);
        const auto lfoPhase = position - std::floor (position);
        const auto shapeIndex = (int) lfo.shape->load();
        lfoValues[(size_t) i] = shapeIndex == 6 ? (lfoCurves != nullptr ? 1.0f - 2.0f * (*lfoCurves)[(size_t) i].lookup (lfoPhase) : 0.0f)
                                                : shapeValue (shapeIndex, lfoPhase, cycle);
        lfoOn[(size_t) i] = lfo.on->load() >= 0.5f;

        phase[(size_t) i] = (float) lfoPhase;
        value[(size_t) i] = lfoValues[(size_t) i];
    }
    for (int m = 0; m < numMacros; ++m)
        sourceValues[(size_t) (firstMacro + m)] = macros[(size_t) m] != nullptr ? macros[(size_t) m]->load() : 0.0f;
    for (int layer = 0; layer < maxInstruments; ++layer)
        for (int kind = 0; kind < instrumentSources; ++kind)
            sourceValues[(size_t) instrumentSource (layer, kind)] = layerSources[(size_t) layer][(size_t) kind];
    for (int s = 0; s < numSources; ++s)
        sourceDisplay[(size_t) s] = sourceValues[(size_t) s];

    // Nollställ det som modulerades förra blocket och lägg på kopplingarna.
    for (auto index : touched)
    {
        offsets[(size_t) index] = 0.0f;
        displayOffsets[(size_t) index] = 0.0f;
    }
    touched.clear();

    for (const auto& route : routes.routes)
    {
        const auto source = route.source.load();
        const auto target = route.target.load();
        if (source < 0 || source >= numSources || ! sourceOn[(size_t) source] || target < 0 || target >= (int) offsets.size())
            continue;

        offsets[(size_t) target] += sourceValues[(size_t) source] * route.amount.load();
        if (touched.size() < touched.capacity())
            touched.push_back (target);
    }

    for (auto index : touched)
        displayOffsets[(size_t) index] = offsets[(size_t) index];
    anyActive = ! touched.empty();
}

float Modulator::apply (int index, float valueIn) const
{
    if (! anyActive || index < 0 || index >= (int) offsets.size() || offsets[(size_t) index] == 0.0f)
        return valueIn;

    auto* parameter = parameters[(size_t) index];
    if (parameter == nullptr)
        return valueIn;
    const auto normalised = juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (valueIn) + offsets[(size_t) index]);
    return parameter->convertFrom0to1 (normalised);
}

float Modulator::applyRaw (const std::atomic<float>* raw, float valueIn) const
{
    if (! anyActive)
        return valueIn;
    for (auto index : touched)
        if (raws[(size_t) index] == raw)
            return apply (index, valueIn);
    return valueIn;
}

float Modulator::currentOffset (int index) const
{
    return index >= 0 && index < (int) parameters.size() ? displayOffsets[(size_t) index].load() : 0.0f;
}
