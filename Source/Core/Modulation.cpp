#include "Modulation.h"

namespace Modulation
{
    juce::StringArray destinationNames()
    {
        return { "-", "Bass Cutoff", "Bass Resonance", "Bass Env Amount", "Bass Drive", "Bass Level", "Bass Pitch",
                 "Kick Pitch", "Kick Decay",
                 "FX Filter Cutoff", "FX Filter Reso", "FX Dist Drive", "FX Echo Mix", "FX Echo Feedback",
                 "FX Reverb Mix", "FX Phaser Mix", "FX Chorus Mix",
                 "Duck Depth" };
    }

    const char* destinationParamID (Destination destination)
    {
        switch (destination)
        {
            case Destination::bassCutoff:    return "bassCutoff";
            case Destination::bassResonance: return "bassResonance";
            case Destination::bassEnvAmount: return "bassEnvAmount";
            case Destination::bassDrive:     return "bassDrive";
            case Destination::bassLevel:     return "bassLevel";
            case Destination::bassTune:      return "bassTune";
            case Destination::kickPitch:     return "kickPitch";
            case Destination::kickDecay:     return "kickDecay";
            case Destination::filterCutoff:  return "fxFilterCutoff";
            case Destination::filterReso:    return "fxFilterReso";
            case Destination::distDrive:     return "fxDistDrive";
            case Destination::echoMix:       return "fxEchoMix";
            case Destination::echoFeedback:  return "fxEchoFeedback";
            case Destination::reverbMix:     return "fxReverbMix";
            case Destination::phaserMix:     return "fxPhaserMix";
            case Destination::chorusMix:     return "fxChorusMix";
            case Destination::duckAmount:    return "duckAmount";
            case Destination::none:
            case Destination::count:         break;
        }
        return nullptr;
    }

    juce::StringArray shapeNames()
    {
        return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" };
    }

    juce::StringArray rateNames()
    {
        return { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "1/4 T", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
    }

    double rateInQuarters (int index)
    {
        constexpr double quarters[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0 };
        return quarters[juce::jlimit (0, (int) std::size (quarters) - 1, index)];
    }

    juce::String paramID (int lfo, const char* suffix)
    {
        return "lfo" + juce::String (lfo + 1) + suffix;
    }

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        using namespace juce;
        for (int i = 0; i < numLfos; ++i)
        {
            const auto title = "LFO " + String (i + 1);
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { paramID (i, "On"), 1 }, title + " On", false));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Shape"), 1 }, title + " Shape", shapeNames(), 0));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, "Rate"), 1 }, title + " Rate", rateNames(), 4));

            for (int r = 0; r < routesPerLfo; ++r)
            {
                const auto letter = String::charToString ((juce_wchar) ('A' + r));
                layout.add (std::make_unique<AudioParameterChoice> (ParameterID { paramID (i, ("Dest" + letter).toRawUTF8()), 1 },
                                                                    title + " Destination " + letter, destinationNames(), 0));
                layout.add (std::make_unique<AudioParameterFloat> (
                    ParameterID { paramID (i, ("Amount" + letter).toRawUTF8()), 1 }, title + " Amount " + letter,
                    NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                    {
                        return (v > 0.0f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %";
                    })));
            }
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

Modulator::Modulator (juce::AudioProcessorValueTreeState& apvts)
{
    using namespace Modulation;
    for (int i = 0; i < numLfos; ++i)
    {
        auto& lfo = lfos[(size_t) i];
        lfo.on = apvts.getRawParameterValue (paramID (i, "On"));
        lfo.shape = apvts.getRawParameterValue (paramID (i, "Shape"));
        lfo.rate = apvts.getRawParameterValue (paramID (i, "Rate"));
        for (int r = 0; r < routesPerLfo; ++r)
        {
            const auto letter = juce::String::charToString ((juce::juce_wchar) ('A' + r));
            lfo.destination[(size_t) r] = apvts.getRawParameterValue (paramID (i, ("Dest" + letter).toRawUTF8()));
            lfo.amount[(size_t) r] = apvts.getRawParameterValue (paramID (i, ("Amount" + letter).toRawUTF8()));
        }
    }

    // Parametrar som inte finns i det här pluginet (t.ex. bassynten i FX) blir nullptr och hoppas över.
    for (int d = 1; d < (int) Destination::count; ++d)
        if (const auto* id = destinationParamID ((Destination) d))
            targets[(size_t) d] = apvts.getParameter (id);
}

void Modulator::update (double ppq)
{
    using namespace Modulation;
    offsets.fill (0.0f);

    for (int i = 0; i < numLfos; ++i)
    {
        const auto& lfo = lfos[(size_t) i];
        const auto period = rateInQuarters ((int) lfo.rate->load());
        const auto position = ppq / period;
        const auto cycle = (juce::int64) std::floor (position);
        const auto lfoPhase = position - std::floor (position);
        const auto lfoValue = shapeValue ((int) lfo.shape->load(), lfoPhase, cycle);

        phase[(size_t) i] = (float) lfoPhase;
        value[(size_t) i] = lfoValue;

        if (lfo.on->load() < 0.5f)
            continue;

        for (int r = 0; r < routesPerLfo; ++r)
        {
            const auto destination = (int) lfo.destination[(size_t) r]->load();
            if (destination > 0 && destination < (int) Destination::count)
                offsets[(size_t) destination] += lfoValue * lfo.amount[(size_t) r]->load();
        }
    }
}

bool Modulator::isModulated (Modulation::Destination destination) const
{
    return offsets[(size_t) destination] != 0.0f && targets[(size_t) destination] != nullptr;
}

float Modulator::apply (Modulation::Destination destination, float fallback) const
{
    if (! isModulated (destination))
        return fallback;

    auto* parameter = targets[(size_t) destination];
    const auto normalised = juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (fallback) + offsets[(size_t) destination]);
    return parameter->convertFrom0to1 (normalised);
}
