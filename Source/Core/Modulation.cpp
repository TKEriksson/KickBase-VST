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
        static const char* names[] { "ENV 2", "ENV 3", "VELOCITY", "RANDOM", "AMP ENV", "FILT ENV" };
        return names[juce::jlimit (0, instrumentKinds - 1, kind)];
    }

    juce::String sourceName (int source)
    {
        if (isLfoSource (source))
            return "LFO " + juce::String (lfoIndexOfSource (source) + 1);
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

    // LFO:ns grundparametrar (On, Shape, Rate, Mode, Hz) och SNAP-paret. LFO 1-3 har SNAP i ett eget block (se addParameters).
    static void addLfoBaseParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, int i)
    {
        using namespace juce;
        {
            const auto title = sourceName (lfoSource (i));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (i, "On"), 1 }, title + " On", true));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Shape"), 1 }, title + " Shape", shapeNames(), 0));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Rate"), 1 }, title + " Rate", rateNames(), 8));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Mode"), 1 }, title + " Rate Mode", StringArray { "Sync", "Hz" }, 0));
            NormalisableRange<float> hzRange (0.02f, 40.0f, 0.001f);
            hzRange.setSkewForCentre (2.0f);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramID (i, "Hz"), 1 }, title + " Free Rate", hzRange, 1.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
        }
    }

    static void addLfoSnapParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout, int i)
    {
        using namespace juce;
        {
            const auto title = sourceName (lfoSource (i));
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (i, "Snap"), 1 }, title + " Snap", false));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { paramID (i, "SnapRange"), 1 }, title + " Snap Range",
                                                               NormalisableRange<float> (1.0f, 25.0f, 0.1f), 5.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String::fromUTF8 ("\xC2\xB1") + String (v, 1) + " %"; })));
        }
    }

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        using namespace juce;
        for (int i = 0; i < numLfos; ++i)
            addLfoBaseParameters (layout, i);

        // SNAP i Hz-läget (R): nya parametrar sist bland LFO-parametrarna (före makrona är ordningen som förut för allt före dem). Standard av.
        for (int i = 0; i < numLfos; ++i)
            addLfoSnapParameters (layout, i);

        // Makron: fyra rattar att vrida för hand, som i Serum, och att dra till andra rattar eller sätta i matrisen.
        for (int m = 0; m < numMacros; ++m)
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { macroID (m), 1 }, "Macro " + String (m + 1),
                                                               NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    }

    void addExtraLfoParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        for (int i = numLfos; i < maxLfos; ++i)
        {
            addLfoBaseParameters (layout, i);
            addLfoSnapParameters (layout, i);
        }
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
    for (int i = 0; i < maxLfos; ++i) // FX har bara LFO 1-3: där blir pekarna för 4-10 nullptr och LFO:n räknas aldrig (se update)
    {
        auto& lfo = lfos[(size_t) i];
        lfo.on = apvts.getRawParameterValue (paramID (i, "On"));
        lfo.shape = apvts.getRawParameterValue (paramID (i, "Shape"));
        lfo.rate = apvts.getRawParameterValue (paramID (i, "Rate"));
        lfo.mode = apvts.getRawParameterValue (paramID (i, "Mode"));
        lfo.hz = apvts.getRawParameterValue (paramID (i, "Hz"));
        lfo.snap = apvts.getRawParameterValue (paramID (i, "Snap"));
        lfo.snapRange = apvts.getRawParameterValue (paramID (i, "SnapRange"));
        snapNote[(size_t) i] = -1;
        snapLocked[(size_t) i] = false;
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
    const auto mask = activeMask.load (std::memory_order_relaxed); // en gång per block (LESSONS R7)
    for (int i = 0; i < maxLfos; ++i)
    {
        const auto& lfo = lfos[(size_t) i];
        const auto source = (size_t) lfoSource (i);
        if ((mask & (1u << i)) == 0 || lfo.on == nullptr)
        {
            // Inaktiv (borttagen) LFO: ingen beräkning alls. Fältens nollställning här gör att fasen startar rent när den läggs till igen
            // (bara ljudtråden skriver dem).
            lfoOn[source] = false;
            lfoValues[source] = 0.0f;
            phase[(size_t) i] = 0.0f;
            value[(size_t) i] = 0.0f;
            snapNote[(size_t) i] = -1;
            snapLocked[(size_t) i] = false;
            phaseOffset[(size_t) i] = 0.0;
            lastCyclesPerQuarter[(size_t) i] = 0.0;
            lastPpq[(size_t) i] = 0.0;
            continue;
        }
        // Tempo-synkad (en period i fjärdedelar) eller fri hastighet i Hz (sekunder räknade ur positionen och tempot).
        // Perioder per fjärdedel: tempo-synkat (rate) eller fri hastighet (Hz). Byter hastigheten (rattar, automation, Sync <-> Hz) ska fasen
        // fortsätta där den är i stället för att hoppa till ppq * nya hastigheten: `phaseOffset` bär skillnaden så att positionen är kontinuerlig
        // (p * gamla + offset = p * nya + ny offset). Ändras inget är offseten 0 och positionen exakt som förr (x + 0.0 är x).
        // Mode, Hz, Rate och SNAP läses en gång här och används i båda uttrycken (en ratt som vrids mellan två läsningar ger annars en offset
        // som inte stämmer med positionen, och fasen hoppar).
        const auto freeRate = lfo.mode->load() >= 0.5f;
        const auto hz = (double) lfo.hz->load();
        const auto rateQuarters = rateInQuarters ((int) lfo.rate->load());
        const auto tempoFactor = 60.0 / std::max (20.0, bpm);

        // SNAP (bara i Hz-läget): det slutliga Hz-värdet jämförs med alla notvärden. Närmaste visas ("≈ 1/8 T"), och ligger det inom ± SNAP RANGE
        // (procent av notvärdets hastighet) och SNAP är på låses hastigheten exakt mot notvärdet, så att den ligger på takten.
        auto snappedCycles = 0.0; // > 0: låst hastighet (perioder per fjärdedel)
        if (freeRate)
        {
            const auto wanted = tempoFactor * hz;
            auto nearest = -1;
            auto nearestRelative = 1.0e9;
            for (int k = 0; k < 18; ++k) // de 18 notvärdena (rateNames().size(); rateNames() allokerar och får inte anropas här)
            {
                const auto noteCycles = 1.0 / rateInQuarters (k);
                const auto relative = std::abs (wanted - noteCycles) / noteCycles;
                if (relative < nearestRelative)
                {
                    nearestRelative = relative;
                    nearest = k;
                }
            }
            const auto locked = lfo.snap->load() >= 0.5f && nearestRelative <= (double) lfo.snapRange->load() * 0.01;
            if (locked)
                snappedCycles = 1.0 / rateInQuarters (nearest);
            snapNote[(size_t) i] = nearest;
            snapLocked[(size_t) i] = locked;
        }
        else
        {
            snapNote[(size_t) i] = -1;
            snapLocked[(size_t) i] = false;
        }

        const auto cyclesPerQuarter = snappedCycles > 0.0 ? snappedCycles : freeRate ? tempoFactor * hz : 1.0 / rateQuarters;
        auto& offset = phaseOffset[(size_t) i];
        auto& lastCycles = lastCyclesPerQuarter[(size_t) i];
        auto& lastPosition = lastPpq[(size_t) i];
        if (ppq < lastPosition - 1.0e-6)
            offset = 0.0; // transporten startade om eller loopade: LFO:n följer takten igen
        else if (lastCycles > 0.0 && cyclesPerQuarter != lastCycles)
            offset += ppq * (lastCycles - cyclesPerQuarter);
        lastCycles = cyclesPerQuarter;
        lastPosition = ppq;
        const auto position = snappedCycles > 0.0 ? ppq * snappedCycles + offset
                            : freeRate            ? ppq * 60.0 / std::max (20.0, bpm) * hz + offset // (samma uttrycksform som förr: bit-identiskt)
                                                  : ppq / rateQuarters + offset;
        const auto cycle = (juce::int64) std::floor (position);
        const auto lfoPhase = position - std::floor (position);
        const auto shapeIndex = (int) lfo.shape->load();
        lfoValues[source] = shapeIndex == 6 ? (lfoCurves != nullptr ? 1.0f - 2.0f * (*lfoCurves)[(size_t) i].lookup (lfoPhase) : 0.0f)
                                                : shapeValue (shapeIndex, lfoPhase, cycle);
        lfoOn[source] = lfo.on->load() >= 0.5f;

        phase[(size_t) i] = (float) lfoPhase;
        value[(size_t) i] = lfoValues[source];
    }
    for (int m = 0; m < numMacros; ++m)
        sourceValues[(size_t) (firstMacro + m)] = macros[(size_t) m] != nullptr ? macros[(size_t) m]->load() : 0.0f;
    for (int layer = 0; layer < maxInstruments; ++layer)
        for (int kind = 0; kind < instrumentKinds; ++kind)
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
