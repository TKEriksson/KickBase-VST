#include "Effects.h"

namespace
{
    using namespace juce;

    String formatMs (float ms) { return ms < 10.0f ? String (ms, 1) + " ms" : String (roundToInt (ms)) + " ms"; }
    String formatHz (float hz) { return hz >= 1000.0f ? String (hz / 1000.0f, 1) + " kHz" : String (roundToInt (hz)) + " Hz"; }
    String formatPercent (float v) { return String (roundToInt (v * 100.0f)) + " %"; }
    String formatDb (float db) { return (db > 0.0f ? "+" : "") + String (db, 1) + " dB"; }

    NormalisableRange<float> skewed (float min, float max, float centre)
    {
        NormalisableRange<float> range (min, max);
        range.setSkewForCentre (centre);
        return range;
    }

    template <typename Formatter>
    std::unique_ptr<AudioParameterFloat> floatParam (const String& id, const String& name, NormalisableRange<float> range,
                                                     float defaultValue, Formatter format)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, defaultValue,
            AudioParameterFloatAttributes().withStringFromValueFunction ([format] (float v, int) { return format (v); }));
    }

    // En-pols filter, används för ton och för echots återkoppling.
    struct OnePole
    {
        float state = 0.0f;
        float lowpass (float x, float coefficient)  { state += (x - state) * coefficient; return state; }
        float highpass (float x, float coefficient) { return x - lowpass (x, coefficient); }
    };

    float onePoleCoefficient (float hz, double sampleRate)
    {
        return (float) (1.0 - std::exp (-MathConstants<double>::twoPi * hz / sampleRate));
    }

    // Läser en parameter som skapats av Effects::addParameters.
    std::atomic<float>* raw (AudioProcessorValueTreeState& apvts, EffectType type, const char* suffix)
    {
        auto* value = apvts.getRawParameterValue (Effects::parameterPrefix (type) + suffix);
        jassert (value != nullptr);
        return value;
    }
}

namespace Effects
{
    String name (EffectType type)
    {
        switch (type)
        {
            case EffectType::filter:     return "FILTER";
            case EffectType::distortion: return "DISTORTION";
            case EffectType::eq:         return "EQ";
            case EffectType::chorus:     return "CHORUS";
            case EffectType::phaser:     return "PHASER";
            case EffectType::echo:       return "ECHO";
            case EffectType::reverb:     return "REVERB";
        }
        return {};
    }

    String parameterPrefix (EffectType type)
    {
        switch (type)
        {
            case EffectType::filter:     return "fxFilter";
            case EffectType::distortion: return "fxDist";
            case EffectType::eq:         return "fxEq";
            case EffectType::chorus:     return "fxChorus";
            case EffectType::phaser:     return "fxPhaser";
            case EffectType::echo:       return "fxEcho";
            case EffectType::reverb:     return "fxReverb";
        }
        return {};
    }

    bool isEffectParameter (const String& paramID)
    {
        for (int i = 0; i < numEffects; ++i)
            if (paramID.startsWith (parameterPrefix ((EffectType) i)))
                return true;
        return false;
    }

    StringArray echoTimeNames()
    {
        return { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "3/16", "1/4", "3/8", "1/2" };
    }

    double echoTimeInQuarters (int index)
    {
        constexpr double quarters[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };
        return quarters[jlimit (0, (int) std::size (quarters) - 1, index)];
    }

    void addParameters (AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        // På/av och före/efter gaten för varje effekt. Standard: av; filter, distortion och
        // EQ före gaten (formar tonen), resten efter (svansar som klingar ut).
        for (int i = 0; i < numEffects; ++i)
        {
            const auto type = (EffectType) i;
            const auto prefix = parameterPrefix (type);
            const auto title = name (type).substring (0, 1) + name (type).substring (1).toLowerCase();
            const auto preByDefault = type == EffectType::filter || type == EffectType::distortion || type == EffectType::eq;

            layout.add (std::make_unique<AudioParameterBool> (ParameterID { prefix + "On", 1 }, title + " On", false));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { prefix + "Post", 1 }, title + " Position",
                                                                StringArray { "Pre gate", "Post gate" }, preByDefault ? 0 : 1));
        }

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxFilterType", 1 }, "Filter Type", StringArray { "Low pass", "High pass", "Band pass" }, 0));
        layout.add (floatParam ("fxFilterCutoff", "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 2000.0f, formatHz));
        layout.add (floatParam ("fxFilterReso", "Filter Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, formatPercent));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxFilterSlope", 1 }, "Filter Slope", StringArray { "12 dB", "24 dB" }, 1));
        layout.add (floatParam ("fxFilterMix", "Filter Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, formatPercent));

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxDistMode", 1 }, "Distortion Mode", StringArray { "Soft", "Hard", "Fold", "Crush" }, 0));
        layout.add (floatParam ("fxDistDrive", "Distortion Drive", NormalisableRange<float> (0.0f, 1.0f), 0.4f, formatPercent));
        layout.add (floatParam ("fxDistTone", "Distortion Tone", skewed (500.0f, 20000.0f, 4000.0f), 8000.0f, formatHz));
        layout.add (floatParam ("fxDistMix", "Distortion Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, formatPercent));

        layout.add (floatParam ("fxEqLow", "EQ Low", NormalisableRange<float> (-15.0f, 15.0f), 0.0f, formatDb));
        layout.add (floatParam ("fxEqMidFreq", "EQ Mid Freq", skewed (100.0f, 8000.0f, 800.0f), 800.0f, formatHz));
        layout.add (floatParam ("fxEqMid", "EQ Mid", NormalisableRange<float> (-15.0f, 15.0f), 0.0f, formatDb));
        layout.add (floatParam ("fxEqHigh", "EQ High", NormalisableRange<float> (-15.0f, 15.0f), 0.0f, formatDb));

        layout.add (floatParam ("fxChorusRate", "Chorus Rate", skewed (0.05f, 10.0f, 1.0f), 0.8f, [] (float v) { return String (v, 2) + " Hz"; }));
        layout.add (floatParam ("fxChorusDepth", "Chorus Depth", NormalisableRange<float> (0.0f, 1.0f), 0.3f, formatPercent));
        layout.add (floatParam ("fxChorusDelay", "Chorus Delay", skewed (1.0f, 40.0f, 8.0f), 7.0f, formatMs));
        layout.add (floatParam ("fxChorusFeedback", "Chorus Feedback", NormalisableRange<float> (-0.95f, 0.95f), 0.0f, formatPercent));
        layout.add (floatParam ("fxChorusMix", "Chorus Mix", NormalisableRange<float> (0.0f, 1.0f), 0.4f, formatPercent));

        layout.add (floatParam ("fxPhaserRate", "Phaser Rate", skewed (0.02f, 10.0f, 0.5f), 0.3f, [] (float v) { return String (v, 2) + " Hz"; }));
        layout.add (floatParam ("fxPhaserDepth", "Phaser Depth", NormalisableRange<float> (0.0f, 1.0f), 0.6f, formatPercent));
        layout.add (floatParam ("fxPhaserFreq", "Phaser Freq", skewed (100.0f, 6000.0f, 1000.0f), 800.0f, formatHz));
        layout.add (floatParam ("fxPhaserFeedback", "Phaser Feedback", NormalisableRange<float> (-0.95f, 0.95f), 0.4f, formatPercent));
        layout.add (floatParam ("fxPhaserMix", "Phaser Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, formatPercent));

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "fxEchoTime", 1 }, "Echo Time", echoTimeNames(), 5));
        layout.add (floatParam ("fxEchoFeedback", "Echo Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.4f, formatPercent));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "fxEchoPingPong", 1 }, "Echo Ping Pong", true));
        layout.add (floatParam ("fxEchoLowCut", "Echo Low Cut", skewed (20.0f, 2000.0f, 200.0f), 250.0f, formatHz));
        layout.add (floatParam ("fxEchoHighCut", "Echo High Cut", skewed (1000.0f, 20000.0f, 5000.0f), 6000.0f, formatHz));
        layout.add (floatParam ("fxEchoMix", "Echo Mix", NormalisableRange<float> (0.0f, 1.0f), 0.3f, formatPercent));

        layout.add (floatParam ("fxReverbSize", "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.5f, formatPercent));
        layout.add (floatParam ("fxReverbDamping", "Reverb Damping", NormalisableRange<float> (0.0f, 1.0f), 0.5f, formatPercent));
        layout.add (floatParam ("fxReverbWidth", "Reverb Width", NormalisableRange<float> (0.0f, 1.0f), 1.0f, formatPercent));
        layout.add (floatParam ("fxReverbLowCut", "Reverb Low Cut", skewed (20.0f, 2000.0f, 300.0f), 300.0f, formatHz));
        layout.add (floatParam ("fxReverbMix", "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), 0.25f, formatPercent));
    }
}

//==============================================================================
void EffectOrder::move (int from, int to)
{
    if (! isPositiveAndBelow (from, numEffects) || ! isPositiveAndBelow (to, numEffects) || from == to)
        return;

    std::array<int, numEffects> current;
    for (int i = 0; i < numEffects; ++i)
        current[(size_t) i] = slots[(size_t) i].load();

    const auto moved = current[(size_t) from];
    const auto step = from < to ? 1 : -1;
    for (int i = from; i != to; i += step)
        current[(size_t) i] = current[(size_t) (i + step)];
    current[(size_t) to] = moved;

    for (int i = 0; i < numEffects; ++i)
        slots[(size_t) i] = current[(size_t) i];
}

String EffectOrder::toString() const
{
    StringArray parts;
    for (const auto& slot : slots)
        parts.add (String (slot.load()));
    return parts.joinIntoString (",");
}

void EffectOrder::fromString (const String& text)
{
    // Godta bara en fullständig permutation, annars standardordningen.
    const auto parts = StringArray::fromTokens (text, ",", {});
    std::array<bool, numEffects> seen {};
    std::array<int, numEffects> values {};

    if (parts.size() != numEffects)
    {
        reset();
        return;
    }

    for (int i = 0; i < numEffects; ++i)
    {
        const auto value = parts[i].getIntValue();
        if (! isPositiveAndBelow (value, numEffects) || seen[(size_t) value])
        {
            reset();
            return;
        }
        seen[(size_t) value] = true;
        values[(size_t) i] = value;
    }

    for (int i = 0; i < numEffects; ++i)
        slots[(size_t) i] = values[(size_t) i];
}

//==============================================================================
namespace
{
    struct FilterFx
    {
        static constexpr int chunk = 32; // cutoff uppdateras var 32:a sample

        std::atomic<float>* type; std::atomic<float>* cutoff; std::atomic<float>* reso;
        std::atomic<float>* slope; std::atomic<float>* mix;

        dsp::StateVariableTPTFilter<float> first, second;
        SmoothedValue<float, ValueSmoothingTypes::Multiplicative> smoothedCutoff;
        double sampleRate = 44100.0;

        void prepare (const dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            first.prepare (spec);
            second.prepare (spec);
            smoothedCutoff.reset (spec.sampleRate, 0.03);
            smoothedCutoff.setCurrentAndTargetValue (cutoff->load());
        }

        void reset() { first.reset(); second.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            const auto filterType = (dsp::StateVariableTPTFilterType) jlimit (0, 2, (int) type->load());
            const auto steep = slope->load() >= 0.5f;
            const auto q = 0.5f * std::pow (20.0f, reso->load());
            const auto wet = mix->load();

            first.setType (filterType);
            second.setType (filterType);
            first.setResonance (steep ? 0.707f : q);
            second.setResonance (q);
            smoothedCutoff.setTargetValue (cutoff->load());

            for (int start = 0; start < numSamples; start += chunk)
            {
                const auto end = std::min (numSamples, start + chunk);
                const auto hz = jlimit (20.0f, (float) sampleRate * 0.45f, smoothedCutoff.skip (end - start));
                first.setCutoffFrequency (hz);
                second.setCutoffFrequency (hz);

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);
                    for (int i = start; i < end; ++i)
                    {
                        auto y = first.processSample (ch, data[i]);
                        if (steep)
                            y = second.processSample (ch, y);
                        data[i] += (y - data[i]) * wet;
                    }
                }
            }
        }
    };

    struct DistortionFx
    {
        std::atomic<float>* mode; std::atomic<float>* drive; std::atomic<float>* tone; std::atomic<float>* mix;
        std::array<OnePole, 2> toneFilters;
        std::array<float, 2> held {};
        int holdCounter = 0;
        double sampleRate = 44100.0;

        void prepare (const dsp::ProcessSpec& spec) { sampleRate = spec.sampleRate; reset(); }
        void reset() { toneFilters = {}; held = {}; holdCounter = 0; }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            const auto m = (int) mode->load();
            const auto amount = drive->load();
            const auto wet = mix->load();
            const auto gain = std::pow (10.0f, amount * 1.5f); // upp till ~30 gånger (+30 dB)
            const auto toneCoefficient = onePoleCoefficient (tone->load(), sampleRate);

            // Crush: färre bitar och lägre samplingsfrekvens ju mer drive.
            const auto levels = std::pow (2.0f, 16.0f - amount * 13.0f);
            const auto holdLength = 1 + (int) (amount * 24.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto takeNew = holdCounter == 0;
                holdCounter = (holdCounter + 1) % holdLength;

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    float y = x;

                    switch (m)
                    {
                        case 0: y = std::tanh (gain * x) / std::tanh (gain); break;              // soft
                        case 1: y = jlimit (-1.0f, 1.0f, gain * x) / std::min (gain, 1.0f); break; // hard
                        case 2: y = std::sin (gain * x * MathConstants<float>::halfPi); break;    // fold
                        case 3:                                                                   // crush
                            if (takeNew)
                                held[(size_t) ch] = std::round (x * levels) / levels;
                            y = held[(size_t) ch];
                            break;
                        default: break;
                    }

                    y = toneFilters[(size_t) ch].lowpass (y, toneCoefficient);
                    buffer.setSample (ch, i, x + (y - x) * wet);
                }
            }
        }
    };

    struct EqFx
    {
        std::atomic<float>* low; std::atomic<float>* midFreq; std::atomic<float>* mid; std::atomic<float>* high;
        std::array<dsp::IIR::Filter<float>, 2> lowFilters, midFilters, highFilters;
        double sampleRate = 44100.0;
        float lastLow = -100.0f, lastMidFreq = -1.0f, lastMid = -100.0f, lastHigh = -100.0f;

        void prepare (const dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            const dsp::ProcessSpec mono { spec.sampleRate, spec.maximumBlockSize, 1 };
            for (auto* filters : { &lowFilters, &midFilters, &highFilters })
            {
                for (auto& filter : *filters)
                {
                    // Koefficientobjekten skapas här; ljudtråden skriver bara över värdena.
                    filter.coefficients = new dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
                    filter.prepare (mono);
                }
            }
            lastLow = lastMidFreq = lastMid = lastHigh = -100.0f;
        }

        void reset()
        {
            for (auto* filters : { &lowFilters, &midFilters, &highFilters })
                for (auto& filter : *filters)
                    filter.reset();
        }

        void updateCoefficients()
        {
            using Array = dsp::IIR::ArrayCoefficients<float>;
            const auto l = low->load(), mf = midFreq->load(), m = mid->load(), h = high->load();

            if (l != lastLow)
                for (auto& f : lowFilters)
                    *f.coefficients = Array::makeLowShelf (sampleRate, 120.0f, 0.7f, Decibels::decibelsToGain (l));
            if (mf != lastMidFreq || m != lastMid)
                for (auto& f : midFilters)
                    *f.coefficients = Array::makePeakFilter (sampleRate, mf, 0.9f, Decibels::decibelsToGain (m));
            if (h != lastHigh)
                for (auto& f : highFilters)
                    *f.coefficients = Array::makeHighShelf (sampleRate, 6000.0f, 0.7f, Decibels::decibelsToGain (h));

            lastLow = l; lastMidFreq = mf; lastMid = m; lastHigh = h;
        }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            updateCoefficients();
            for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = highFilters[(size_t) ch].processSample (midFilters[(size_t) ch].processSample (lowFilters[(size_t) ch].processSample (data[i])));
            }
        }
    };

    struct ChorusFx
    {
        std::atomic<float>* rate; std::atomic<float>* depth; std::atomic<float>* delay; std::atomic<float>* feedback; std::atomic<float>* mix;
        dsp::Chorus<float> chorus;

        void prepare (const dsp::ProcessSpec& spec) { chorus.prepare (spec); }
        void reset() { chorus.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            chorus.setRate (rate->load());
            chorus.setDepth (depth->load());
            chorus.setCentreDelay (delay->load());
            chorus.setFeedback (feedback->load());
            chorus.setMix (mix->load());
            auto block = dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples);
            chorus.process (dsp::ProcessContextReplacing<float> (block));
        }
    };

    struct PhaserFx
    {
        std::atomic<float>* rate; std::atomic<float>* depth; std::atomic<float>* freq; std::atomic<float>* feedback; std::atomic<float>* mix;
        dsp::Phaser<float> phaser;

        void prepare (const dsp::ProcessSpec& spec) { phaser.prepare (spec); }
        void reset() { phaser.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            phaser.setRate (rate->load());
            phaser.setDepth (depth->load());
            phaser.setCentreFrequency (freq->load());
            phaser.setFeedback (feedback->load());
            phaser.setMix (mix->load());
            auto block = dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples);
            phaser.process (dsp::ProcessContextReplacing<float> (block));
        }
    };

    struct EchoFx
    {
        std::atomic<float>* time; std::atomic<float>* feedback; std::atomic<float>* pingPong;
        std::atomic<float>* lowCut; std::atomic<float>* highCut; std::atomic<float>* mix;

        dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::Linear> line;
        SmoothedValue<float> delaySamples;
        std::array<OnePole, 2> lowCutFilters, highCutFilters;
        double sampleRate = 44100.0, bpm = 140.0;

        void prepare (const dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            line.prepare ({ spec.sampleRate, spec.maximumBlockSize, 2 });
            line.setMaximumDelayInSamples ((int) (spec.sampleRate * 4.0));
            delaySamples.reset (spec.sampleRate, 0.08);
            delaySamples.setCurrentAndTargetValue (targetDelay());
            reset();
        }

        void reset() { line.reset(); lowCutFilters = {}; highCutFilters = {}; }

        float targetDelay() const
        {
            const auto seconds = Effects::echoTimeInQuarters ((int) time->load()) * 60.0 / bpm;
            return (float) jlimit (1.0, sampleRate * 3.9, seconds * sampleRate);
        }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            delaySamples.setTargetValue (targetDelay());
            const auto fb = feedback->load();
            const auto wet = mix->load();
            const auto pong = pingPong->load() >= 0.5f;
            const auto lowCoefficient = onePoleCoefficient (lowCut->load(), sampleRate);
            const auto highCoefficient = onePoleCoefficient (highCut->load(), sampleRate);
            const auto stereo = buffer.getNumChannels() > 1;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto d = delaySamples.getNextValue();
                std::array<float, 2> echo { line.popSample (0, d), line.popSample (1, d) };

                // Filtrera ekot, så att varje upprepning blir tunnare och mörkare.
                for (size_t ch = 0; ch < 2; ++ch)
                    echo[ch] = highCutFilters[ch].lowpass (lowCutFilters[ch].highpass (echo[ch], lowCoefficient), highCoefficient);

                const auto inL = buffer.getSample (0, i);
                const auto inR = stereo ? buffer.getSample (1, i) : inL;

                if (pong)
                {
                    // Ping-pong: ingången går in till vänster, ekot studsar mellan sidorna.
                    line.pushSample (0, (inL + inR) * 0.5f + echo[1] * fb);
                    line.pushSample (1, echo[0] * fb);
                }
                else
                {
                    line.pushSample (0, inL + echo[0] * fb);
                    line.pushSample (1, inR + echo[1] * fb);
                }

                buffer.setSample (0, i, inL + echo[0] * wet);
                if (stereo)
                    buffer.setSample (1, i, inR + echo[1] * wet);
            }
        }
    };

    struct ReverbFx
    {
        std::atomic<float>* size; std::atomic<float>* damping; std::atomic<float>* width; std::atomic<float>* lowCut; std::atomic<float>* mix;

        dsp::Reverb reverb;
        dsp::StateVariableTPTFilter<float> lowCutFilter;
        AudioBuffer<float> wetBuffer;

        void prepare (const dsp::ProcessSpec& spec)
        {
            reverb.prepare (spec);
            lowCutFilter.prepare (spec);
            lowCutFilter.setType (dsp::StateVariableTPTFilterType::highpass);
            wetBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        }

        void reset() { reverb.reset(); lowCutFilter.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples)
        {
            dsp::Reverb::Parameters p;
            p.roomSize = size->load();
            p.damping = damping->load();
            p.width = width->load();
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            reverb.setParameters (p);
            lowCutFilter.setCutoffFrequency (lowCut->load());

            // Bara det som går in i reverbet låg-kapas: basens botten förblir torr och tight.
            const auto channels = std::min (buffer.getNumChannels(), wetBuffer.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
                wetBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            auto block = dsp::AudioBlock<float> (wetBuffer).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) channels);
            lowCutFilter.process (dsp::ProcessContextReplacing<float> (block));
            reverb.process (dsp::ProcessContextReplacing<float> (block));

            for (int ch = 0; ch < channels; ++ch)
                buffer.addFrom (ch, 0, wetBuffer, ch, 0, numSamples, mix->load());
        }
    };
}

//==============================================================================
struct EffectChain::Impl
{
    explicit Impl (AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < numEffects; ++i)
        {
            on[(size_t) i] = raw (apvts, (EffectType) i, "On");
            post[(size_t) i] = raw (apvts, (EffectType) i, "Post");
        }

        filter.type = raw (apvts, EffectType::filter, "Type");
        filter.cutoff = raw (apvts, EffectType::filter, "Cutoff");
        filter.reso = raw (apvts, EffectType::filter, "Reso");
        filter.slope = raw (apvts, EffectType::filter, "Slope");
        filter.mix = raw (apvts, EffectType::filter, "Mix");

        distortion.mode = raw (apvts, EffectType::distortion, "Mode");
        distortion.drive = raw (apvts, EffectType::distortion, "Drive");
        distortion.tone = raw (apvts, EffectType::distortion, "Tone");
        distortion.mix = raw (apvts, EffectType::distortion, "Mix");

        eq.low = raw (apvts, EffectType::eq, "Low");
        eq.midFreq = raw (apvts, EffectType::eq, "MidFreq");
        eq.mid = raw (apvts, EffectType::eq, "Mid");
        eq.high = raw (apvts, EffectType::eq, "High");

        chorus.rate = raw (apvts, EffectType::chorus, "Rate");
        chorus.depth = raw (apvts, EffectType::chorus, "Depth");
        chorus.delay = raw (apvts, EffectType::chorus, "Delay");
        chorus.feedback = raw (apvts, EffectType::chorus, "Feedback");
        chorus.mix = raw (apvts, EffectType::chorus, "Mix");

        phaser.rate = raw (apvts, EffectType::phaser, "Rate");
        phaser.depth = raw (apvts, EffectType::phaser, "Depth");
        phaser.freq = raw (apvts, EffectType::phaser, "Freq");
        phaser.feedback = raw (apvts, EffectType::phaser, "Feedback");
        phaser.mix = raw (apvts, EffectType::phaser, "Mix");

        echo.time = raw (apvts, EffectType::echo, "Time");
        echo.feedback = raw (apvts, EffectType::echo, "Feedback");
        echo.pingPong = raw (apvts, EffectType::echo, "PingPong");
        echo.lowCut = raw (apvts, EffectType::echo, "LowCut");
        echo.highCut = raw (apvts, EffectType::echo, "HighCut");
        echo.mix = raw (apvts, EffectType::echo, "Mix");

        reverb.size = raw (apvts, EffectType::reverb, "Size");
        reverb.damping = raw (apvts, EffectType::reverb, "Damping");
        reverb.width = raw (apvts, EffectType::reverb, "Width");
        reverb.lowCut = raw (apvts, EffectType::reverb, "LowCut");
        reverb.mix = raw (apvts, EffectType::reverb, "Mix");
    }

    void processEffect (EffectType type, AudioBuffer<float>& buffer, int numSamples)
    {
        switch (type)
        {
            case EffectType::filter:     filter.process (buffer, numSamples); break;
            case EffectType::distortion: distortion.process (buffer, numSamples); break;
            case EffectType::eq:         eq.process (buffer, numSamples); break;
            case EffectType::chorus:     chorus.process (buffer, numSamples); break;
            case EffectType::phaser:     phaser.process (buffer, numSamples); break;
            case EffectType::echo:       echo.process (buffer, numSamples); break;
            case EffectType::reverb:     reverb.process (buffer, numSamples); break;
        }
    }

    std::array<std::atomic<float>*, numEffects> on {}, post {};
    int maxBlockSize = 512;
    std::array<bool, numEffects> wasOn {};

    FilterFx filter;
    DistortionFx distortion;
    EqFx eq;
    ChorusFx chorus;
    PhaserFx phaser;
    EchoFx echo;
    ReverbFx reverb;
};

EffectChain::EffectChain (AudioProcessorValueTreeState& apvts) : impl (std::make_unique<Impl> (apvts)) {}
EffectChain::~EffectChain() = default;

void EffectChain::prepare (double sampleRate, int maxBlockSize)
{
    impl->maxBlockSize = std::max (1, maxBlockSize);
    const dsp::ProcessSpec spec { sampleRate, (uint32) impl->maxBlockSize, 2 };
    impl->filter.prepare (spec);
    impl->distortion.prepare (spec);
    impl->eq.prepare (spec);
    impl->chorus.prepare (spec);
    impl->phaser.prepare (spec);
    impl->echo.prepare (spec);
    impl->reverb.prepare (spec);
}

void EffectChain::reset()
{
    impl->filter.reset();
    impl->distortion.reset();
    impl->eq.reset();
    impl->chorus.reset();
    impl->phaser.reset();
    impl->echo.reset();
    impl->reverb.reset();
}

void EffectChain::update (double bpm)
{
    impl->echo.bpm = bpm;

    // En effekt som slås på börjar tyst, utan rester från förra gången den användes.
    for (int i = 0; i < numEffects; ++i)
    {
        const auto isOn = impl->on[(size_t) i]->load() >= 0.5f;
        if (isOn && ! impl->wasOn[(size_t) i])
        {
            switch ((EffectType) i)
            {
                case EffectType::filter:     impl->filter.reset(); break;
                case EffectType::distortion: impl->distortion.reset(); break;
                case EffectType::eq:         impl->eq.reset(); break;
                case EffectType::chorus:     impl->chorus.reset(); break;
                case EffectType::phaser:     impl->phaser.reset(); break;
                case EffectType::echo:       impl->echo.reset(); break;
                case EffectType::reverb:     impl->reverb.reset(); break;
            }
        }
        impl->wasOn[(size_t) i] = isOn;
    }
}

bool EffectChain::hasActive (bool postGate) const
{
    for (int i = 0; i < numEffects; ++i)
        if (impl->on[(size_t) i]->load() >= 0.5f && (impl->post[(size_t) i]->load() >= 0.5f) == postGate)
            return true;
    return false;
}

void EffectChain::process (AudioBuffer<float>& buffer, int numSamples, bool postGate, const EffectOrder& order)
{
    // Chorus, phaser och reverb klarar högst maxBlockSize åt gången, så längre block delas.
    for (int start = 0; start < numSamples; start += impl->maxBlockSize)
    {
        const auto length = std::min (impl->maxBlockSize, numSamples - start);
        AudioBuffer<float> part (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);

        for (int slot = 0; slot < numEffects; ++slot)
        {
            const auto type = order.at (slot);
            const auto index = (size_t) type;
            if (impl->on[index]->load() >= 0.5f && (impl->post[index]->load() >= 0.5f) == postGate)
                impl->processEffect (type, part, length);
        }
    }
}
