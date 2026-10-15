#include "Effects.h"

namespace
{
    using namespace juce;
    using Knob = Effects::Knob;
    using Unit = Effects::Knob::Unit;

    Knob choice (const char* name, StringArray choices, int defaultIndex)
    {
        Knob k;
        k.name = name;
        k.min = 0.0f;
        k.max = (float) (choices.size() - 1);
        k.defaultValue = (float) defaultIndex;
        k.choices = std::move (choices);
        return k;
    }

    Knob value (const char* name, float min, float max, float centre, float defaultValue, Unit unit)
    {
        Knob k;
        k.name = name;
        k.min = min;
        k.max = max;
        k.centre = centre;
        k.defaultValue = defaultValue;
        k.unit = unit;
        return k;
    }

    Knob mixKnob (float defaultValue)
    {
        return value ("MIX", 0.0f, 1.0f, 0.0f, defaultValue, Unit::percent);
    }

    StringArray firstSyncNames (int count)
    {
        StringArray names;
        const auto all = Effects::syncNames();
        for (int i = 0; i < count && i < all.size(); ++i)
            names.add (all[i]);
        return names;
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
}

namespace Effects
{
    StringArray typeNames()
    {
        return { "Off", "Filter", "Distortion", "EQ", "Chorus", "Phaser", "Echo", "Reverb",
                 "Compressor", "Trance Gate", "Comb", "Ring Mod", "Bitcrusher", "Auto Filter" };
    }

    String name (EffectType type)
    {
        return typeNames()[(int) type].toUpperCase();
    }

    String shortName (EffectType type)
    {
        const char* names[] { "-", "FILT", "DIST", "EQ", "CHOR", "PHAS", "ECHO", "VERB",
                              "COMP", "GATE", "COMB", "RING", "CRSH", "AUTO" };
        return names[jlimit (0, numEffectTypes - 1, (int) type)];
    }

    String channelName (int channel) { return String::charToString ((juce_wchar) ('A' + channel)); }

    String slotParamID (int channel, int slot, const String& suffix) { return "fx" + channelName (channel) + String (slot + 1) + suffix; }
    String knobParamID (int channel, int slot, int knob)             { return slotParamID (channel, slot, "P" + String (knob + 1)); }
    String channelParamID (int channel, const String& suffix)       { return "fx" + channelName (channel) + suffix; }

    bool isEffectParameter (const String& paramID)
    {
        // "fxA..", "fxB..", "fxC.." (inte FX-versionens "fxKey").
        return paramID.startsWith ("fx") && paramID.length() > 2 && paramID[2] >= 'A' && paramID[2] <= 'A' + numEffectChannels - 1;
    }

    float Knob::fromNormalised (float normalised) const
    {
        normalised = jlimit (0.0f, 1.0f, normalised);
        if (isChoice())
            return (float) roundToInt (normalised * (float) (choices.size() - 1));

        NormalisableRange<float> range (min, max);
        if (centre > min && centre < max)
            range.setSkewForCentre (centre);
        return range.convertFrom0to1 (normalised);
    }

    float Knob::toNormalised (float v) const
    {
        if (isChoice())
            return choices.size() > 1 ? jlimit (0.0f, 1.0f, v / (float) (choices.size() - 1)) : 0.0f;

        NormalisableRange<float> range (min, max);
        if (centre > min && centre < max)
            range.setSkewForCentre (centre);
        return range.convertTo0to1 (jlimit (min, max, v));
    }

    String Knob::text (float v) const
    {
        if (isChoice())
            return choices[jlimit (0, choices.size() - 1, roundToInt (v))];

        switch (unit)
        {
            case Unit::percent: return String (roundToInt (v * 100.0f)) + " %";
            case Unit::hz:      return v < 10.0f ? String (v, 2) + " Hz"
                                     : v < 1000.0f ? String (roundToInt (v)) + " Hz"
                                                   : String (v / 1000.0f, 1) + " kHz";
            case Unit::ms:      return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms";
            case Unit::db:      return (v > 0.0f ? "+" : "") + String (v, 1) + " dB";
            case Unit::ratio:   return String (v, 1) + ":1";
            case Unit::bits:    return String (v, 1) + " bit";
            case Unit::times:   return String (roundToInt (v)) + "x";
            case Unit::octaves: return String (v, 1) + " oct";
            case Unit::plain:   break;
        }
        return String (v, 2);
    }

    const std::vector<Knob>& knobs (EffectType type)
    {
        static const std::vector<std::vector<Knob>> all = []
        {
            std::vector<std::vector<Knob>> table ((size_t) numEffectTypes);
            const StringArray filterTypes { "Low pass", "High pass", "Band pass" };

            table[(size_t) EffectType::filter] = {
                choice ("TYPE", filterTypes, 0),
                value ("CUTOFF", 20.0f, 20000.0f, 1000.0f, 2000.0f, Unit::hz),
                value ("RESO", 0.0f, 1.0f, 0.0f, 0.2f, Unit::percent),
                choice ("SLOPE", { "12 dB", "24 dB" }, 1),
                mixKnob (1.0f) };
            table[(size_t) EffectType::distortion] = {
                choice ("MODE", { "Soft", "Hard", "Fold", "Crush" }, 0),
                value ("DRIVE", 0.0f, 1.0f, 0.0f, 0.4f, Unit::percent),
                value ("TONE", 500.0f, 20000.0f, 4000.0f, 8000.0f, Unit::hz),
                mixKnob (1.0f) };
            table[(size_t) EffectType::eq] = {
                value ("LOW", -15.0f, 15.0f, 0.0f, 0.0f, Unit::db),
                value ("MID FREQ", 100.0f, 8000.0f, 800.0f, 800.0f, Unit::hz),
                value ("MID", -15.0f, 15.0f, 0.0f, 0.0f, Unit::db),
                value ("HIGH", -15.0f, 15.0f, 0.0f, 0.0f, Unit::db) };
            table[(size_t) EffectType::chorus] = {
                value ("RATE", 0.05f, 10.0f, 1.0f, 0.8f, Unit::hz),
                value ("DEPTH", 0.0f, 1.0f, 0.0f, 0.3f, Unit::percent),
                value ("DELAY", 1.0f, 40.0f, 8.0f, 7.0f, Unit::ms),
                value ("FEEDBACK", -0.95f, 0.95f, 0.0f, 0.0f, Unit::percent),
                mixKnob (0.4f) };
            table[(size_t) EffectType::phaser] = {
                value ("RATE", 0.02f, 10.0f, 0.5f, 0.3f, Unit::hz),
                value ("DEPTH", 0.0f, 1.0f, 0.0f, 0.6f, Unit::percent),
                value ("FREQ", 100.0f, 6000.0f, 1000.0f, 800.0f, Unit::hz),
                value ("FEEDBACK", -0.95f, 0.95f, 0.0f, 0.4f, Unit::percent),
                mixKnob (0.5f) };
            table[(size_t) EffectType::echo] = {
                choice ("TIME", firstSyncNames (9), 5),
                value ("FEEDBACK", 0.0f, 0.95f, 0.0f, 0.4f, Unit::percent),
                choice ("PING PONG", { "Off", "On" }, 1),
                value ("LOW CUT", 20.0f, 2000.0f, 200.0f, 250.0f, Unit::hz),
                value ("HIGH CUT", 1000.0f, 20000.0f, 5000.0f, 6000.0f, Unit::hz),
                mixKnob (0.3f) };
            table[(size_t) EffectType::reverb] = {
                value ("SIZE", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("DAMPING", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("WIDTH", 0.0f, 1.0f, 0.0f, 1.0f, Unit::percent),
                value ("LOW CUT", 20.0f, 2000.0f, 300.0f, 300.0f, Unit::hz),
                mixKnob (0.25f) };
            table[(size_t) EffectType::compressor] = {
                value ("THRESHOLD", -40.0f, 0.0f, 0.0f, -18.0f, Unit::db),
                value ("RATIO", 1.0f, 20.0f, 4.0f, 4.0f, Unit::ratio),
                value ("ATTACK", 0.1f, 100.0f, 10.0f, 5.0f, Unit::ms),
                value ("RELEASE", 10.0f, 500.0f, 100.0f, 80.0f, Unit::ms),
                value ("MAKEUP", 0.0f, 24.0f, 0.0f, 3.0f, Unit::db),
                mixKnob (1.0f) };
            table[(size_t) EffectType::gate] = {
                choice ("RATE", firstSyncNames (9), 2),
                value ("LENGTH", 0.05f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, Unit::percent),
                value ("SMOOTH", 0.5f, 20.0f, 3.0f, 2.0f, Unit::ms),
                value ("SHIFT", 0.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                mixKnob (1.0f) };
            table[(size_t) EffectType::comb] = {
                value ("FREQ", 30.0f, 2000.0f, 200.0f, 110.0f, Unit::hz),
                value ("FEEDBACK", -0.98f, 0.98f, 0.0f, 0.7f, Unit::percent),
                value ("DAMPING", 0.0f, 1.0f, 0.0f, 0.3f, Unit::percent),
                mixKnob (0.5f) };
            table[(size_t) EffectType::ringMod] = {
                value ("FREQ", 1.0f, 2000.0f, 100.0f, 80.0f, Unit::hz),
                value ("DEPTH", 0.0f, 1.0f, 0.0f, 1.0f, Unit::percent),
                mixKnob (0.5f) };
            table[(size_t) EffectType::bitcrusher] = {
                value ("BITS", 1.0f, 16.0f, 0.0f, 8.0f, Unit::bits),
                value ("DOWNSAMPLE", 1.0f, 40.0f, 6.0f, 4.0f, Unit::times),
                mixKnob (1.0f) };
            table[(size_t) EffectType::autoFilter] = {
                choice ("TYPE", filterTypes, 0),
                value ("CUTOFF", 20.0f, 20000.0f, 1000.0f, 600.0f, Unit::hz),
                value ("RESO", 0.0f, 1.0f, 0.0f, 0.4f, Unit::percent),
                choice ("RATE", syncNames(), 9),
                value ("DEPTH", 0.0f, 6.0f, 0.0f, 2.0f, Unit::octaves),
                mixKnob (1.0f) };
            return table;
        }();
        return all[(size_t) jlimit (0, numEffectTypes - 1, (int) type)];
    }

    StringArray syncNames()
    {
        return { "1/32", "1/16 T", "1/16", "1/8 T", "1/8", "3/16", "1/4", "3/8", "1/2", "1 bar", "2 bars", "4 bars" };
    }

    double syncInQuarters (int index)
    {
        constexpr double quarters[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
        return quarters[jlimit (0, (int) std::size (quarters) - 1, index)];
    }

    void addParameters (AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        for (int channel = 0; channel < numEffectChannels; ++channel)
        {
            const auto channelTitle = "FX " + channelName (channel);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "Level"), 1 }, channelTitle + " Level",
                                                               NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "Duck"), 1 }, channelTitle + " Duck",
                                                               NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

            for (int slot = 0; slot < slotsPerChannel; ++slot)
            {
                const auto title = channelTitle + String (slot + 1);
                layout.add (std::make_unique<AudioParameterChoice> (ParameterID { slotParamID (channel, slot, "Type"), 1 }, title + " Type", typeNames(), 0));
                for (int k = 0; k < knobsPerSlot; ++k)
                    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { knobParamID (channel, slot, k), 1 }, title + " Knob " + String (k + 1),
                                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f));
            }
        }
    }

    EffectType slotType (const AudioProcessorValueTreeState& apvts, int channel, int slot)
    {
        if (auto* value = apvts.getRawParameterValue (slotParamID (channel, slot, "Type")))
            return (EffectType) jlimit (0, numEffectTypes - 1, (int) value->load());
        return EffectType::off;
    }

    namespace
    {
        void setNormalised (AudioProcessorValueTreeState& apvts, const String& id, float normalised)
        {
            if (auto* parameter = apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (normalised);
                parameter->endChangeGesture();
            }
        }
    }

    void setSlotType (AudioProcessorValueTreeState& apvts, int channel, int slot, EffectType type)
    {
        if (auto* typeParam = apvts.getParameter (slotParamID (channel, slot, "Type")))
            setNormalised (apvts, typeParam->getParameterID(), typeParam->convertTo0to1 ((float) type));

        const auto& list = knobs (type);
        for (int k = 0; k < knobsPerSlot; ++k)
            setNormalised (apvts, knobParamID (channel, slot, k), k < (int) list.size() ? list[(size_t) k].toNormalised (list[(size_t) k].defaultValue) : 0.5f);
    }

    void moveSlot (AudioProcessorValueTreeState& apvts, int channel, int from, int to)
    {
        if (! isPositiveAndBelow (from, slotsPerChannel) || ! isPositiveAndBelow (to, slotsPerChannel) || from == to)
            return;

        // Läs alla platsers värden (0..1), flytta raden och skriv tillbaka det som ändrats.
        auto suffixes = StringArray { "Type" };
        for (int k = 0; k < knobsPerSlot; ++k)
            suffixes.add ("P" + String (k + 1));

        std::vector<std::vector<float>> values ((size_t) slotsPerChannel);
        for (int slot = 0; slot < slotsPerChannel; ++slot)
            for (const auto& suffix : suffixes)
                values[(size_t) slot].push_back (apvts.getParameter (slotParamID (channel, slot, suffix))->getValue());

        auto moved = values[(size_t) from];
        values.erase (values.begin() + from);
        values.insert (values.begin() + to, moved);

        for (int slot = 0; slot < slotsPerChannel; ++slot)
            for (int i = 0; i < suffixes.size(); ++i)
            {
                auto* parameter = apvts.getParameter (slotParamID (channel, slot, suffixes[i]));
                if (parameter->getValue() != values[(size_t) slot][(size_t) i])
                    setNormalised (apvts, parameter->getParameterID(), values[(size_t) slot][(size_t) i]);
            }
    }
}

//==============================================================================
namespace
{
    // Det en effekt behöver veta om blocket utöver ljudet.
    struct Context
    {
        double sampleRate = 44100.0;
        double bpm = 140.0;
        double ppq = 0.0; // position i fjärdedelar vid buffertens första sample

        double samplesPerQuarter() const { return sampleRate * 60.0 / std::max (1.0, bpm); }
    };

    using Values = std::array<float, knobsPerSlot>;

    struct FilterFx
    {
        static constexpr int chunk = 32; // cutoff uppdateras var 32:a sample

        dsp::StateVariableTPTFilter<float> first, second;
        SmoothedValue<float, ValueSmoothingTypes::Multiplicative> smoothedCutoff;
        bool fresh = true;

        void prepare (const dsp::ProcessSpec& spec)
        {
            first.prepare (spec);
            second.prepare (spec);
            smoothedCutoff.reset (spec.sampleRate, 0.03);
            fresh = true;
        }

        void reset() { first.reset(); second.reset(); fresh = true; }

        // cutoffAt (start, end) ger cutoff för en bit av blocket (Auto Filter sveper den).
        template <typename CutoffAt>
        void run (AudioBuffer<float>& buffer, int numSamples, int type, float reso, bool steep, float wet, double sampleRate, CutoffAt cutoffAt)
        {
            const auto filterType = (dsp::StateVariableTPTFilterType) jlimit (0, 2, type);
            const auto q = 0.5f * std::pow (20.0f, reso);
            first.setType (filterType);
            second.setType (filterType);
            first.setResonance (steep ? 0.707f : q);
            second.setResonance (q);

            for (int start = 0; start < numSamples; start += chunk)
            {
                const auto end = std::min (numSamples, start + chunk);
                const auto hz = jlimit (20.0f, (float) sampleRate * 0.45f, cutoffAt (start, end));
                first.setCutoffFrequency (hz);
                second.setCutoffFrequency (hz);

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
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

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            if (fresh)
                smoothedCutoff.setCurrentAndTargetValue (k[1]);
            fresh = false;
            smoothedCutoff.setTargetValue (k[1]);
            run (buffer, numSamples, (int) k[0], k[2], k[3] >= 0.5f, k[4], context.sampleRate,
                 [this] (int start, int end) { return smoothedCutoff.skip (end - start); });
        }
    };

    struct AutoFilterFx
    {
        FilterFx filter;

        void prepare (const dsp::ProcessSpec& spec) { filter.prepare (spec); }
        void reset() { filter.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            // Triangel-LFO synkad till takten: cutoff sveper ±DEPTH/2 oktaver runt CUTOFF.
            const auto quartersPerSample = 1.0 / context.samplesPerQuarter();
            const auto period = Effects::syncInQuarters ((int) k[3]);
            const auto cutoff = k[1], depth = k[4];

            filter.run (buffer, numSamples, (int) k[0], k[2], true, k[5], context.sampleRate, [&] (int start, int end)
            {
                const auto ppq = context.ppq + (start + end) * 0.5 * quartersPerSample;
                const auto phase = ppq / period - std::floor (ppq / period);
                const auto triangle = (float) (phase < 0.5 ? phase * 4.0 - 1.0 : 3.0 - phase * 4.0); // -1..1
                return cutoff * std::exp2 (0.5f * depth * triangle);
            });
        }
    };

    struct DistortionFx
    {
        std::array<OnePole, 2> toneFilters;
        std::array<float, 2> held {};
        int holdCounter = 0;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { toneFilters = {}; held = {}; holdCounter = 0; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto mode = (int) k[0];
            const auto amount = k[1];
            const auto wet = k[3];
            const auto gain = std::pow (10.0f, amount * 1.5f); // upp till ~30 gånger (+30 dB)
            const auto toneCoefficient = onePoleCoefficient (k[2], context.sampleRate);

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

                    switch (mode)
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

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            using Array = dsp::IIR::ArrayCoefficients<float>;
            const auto l = k[0], mf = k[1], m = k[2], h = k[3];

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
        dsp::Chorus<float> chorus;

        void prepare (const dsp::ProcessSpec& spec) { chorus.prepare (spec); }
        void reset() { chorus.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            chorus.setRate (k[0]);
            chorus.setDepth (k[1]);
            chorus.setCentreDelay (k[2]);
            chorus.setFeedback (k[3]);
            chorus.setMix (k[4]);
            auto block = dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples);
            chorus.process (dsp::ProcessContextReplacing<float> (block));
        }
    };

    struct PhaserFx
    {
        dsp::Phaser<float> phaser;

        void prepare (const dsp::ProcessSpec& spec) { phaser.prepare (spec); }
        void reset() { phaser.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            phaser.setRate (k[0]);
            phaser.setDepth (k[1]);
            phaser.setCentreFrequency (k[2]);
            phaser.setFeedback (k[3]);
            phaser.setMix (k[4]);
            auto block = dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples);
            phaser.process (dsp::ProcessContextReplacing<float> (block));
        }
    };

    struct EchoFx
    {
        static constexpr double maxSeconds = 2.2; // 1/2 not vid 55 BPM

        dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::Linear> line;
        SmoothedValue<float> delaySamples;
        std::array<OnePole, 2> lowCutFilters, highCutFilters;
        bool fresh = true;

        void prepare (const dsp::ProcessSpec& spec)
        {
            line.prepare ({ spec.sampleRate, spec.maximumBlockSize, 2 });
            line.setMaximumDelayInSamples ((int) (spec.sampleRate * maxSeconds) + 4);
            delaySamples.reset (spec.sampleRate, 0.08);
            reset();
        }

        void reset() { line.reset(); lowCutFilters = {}; highCutFilters = {}; fresh = true; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto target = (float) jlimit (1.0, context.sampleRate * (maxSeconds - 0.05),
                                                Effects::syncInQuarters ((int) k[0]) * context.samplesPerQuarter());
            if (fresh)
                delaySamples.setCurrentAndTargetValue (target);
            fresh = false;
            delaySamples.setTargetValue (target);

            const auto fb = k[1];
            const auto pong = k[2] >= 0.5f;
            const auto lowCoefficient = onePoleCoefficient (k[3], context.sampleRate);
            const auto highCoefficient = onePoleCoefficient (k[4], context.sampleRate);
            const auto wet = k[5];
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

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            dsp::Reverb::Parameters p;
            p.roomSize = k[0];
            p.damping = k[1];
            p.width = k[2];
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            reverb.setParameters (p);
            lowCutFilter.setCutoffFrequency (k[3]);

            // Bara det som går in i reverbet låg-kapas: basens botten förblir torr och tight.
            const auto channels = std::min (buffer.getNumChannels(), wetBuffer.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
                wetBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            auto block = dsp::AudioBlock<float> (wetBuffer).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) channels);
            lowCutFilter.process (dsp::ProcessContextReplacing<float> (block));
            reverb.process (dsp::ProcessContextReplacing<float> (block));

            for (int ch = 0; ch < channels; ++ch)
                buffer.addFrom (ch, 0, wetBuffer, ch, 0, numSamples, k[4]);
        }
    };

    struct CompressorFx
    {
        dsp::Compressor<float> compressor;
        AudioBuffer<float> dry;

        void prepare (const dsp::ProcessSpec& spec)
        {
            compressor.prepare (spec);
            dry.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        }

        void reset() { compressor.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            compressor.setThreshold (k[0]);
            compressor.setRatio (k[1]);
            compressor.setAttack (k[2]);
            compressor.setRelease (k[3]);
            const auto makeup = Decibels::decibelsToGain (k[4]);
            const auto wet = k[5];

            const auto channels = std::min (buffer.getNumChannels(), dry.getNumChannels());
            for (int ch = 0; ch < channels; ++ch)
                dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            auto block = dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) channels);
            compressor.process (dsp::ProcessContextReplacing<float> (block));

            for (int ch = 0; ch < channels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                const auto* original = dry.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = original[i] + (data[i] * makeup - original[i]) * wet;
            }
        }
    };

    // Trance gate: hackar ljudet i takt (på under LENGTH av varje period, annars ned med DEPTH).
    struct GateFx
    {
        float gain = 1.0f;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { gain = 1.0f; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto period = Effects::syncInQuarters ((int) k[0]);
            const auto length = k[1], depth = k[2], shift = k[4], wet = k[5];
            const auto smoothing = 1.0f - std::exp (-1.0f / (k[3] * 0.001f * (float) context.sampleRate));
            const auto quartersPerSample = 1.0 / context.samplesPerQuarter();

            for (int i = 0; i < numSamples; ++i)
            {
                const auto position = (context.ppq + i * quartersPerSample) / period + shift;
                const auto phase = (float) (position - std::floor (position));
                const auto target = phase < length ? 1.0f : 1.0f - depth;
                gain += (target - gain) * smoothing;

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    buffer.setSample (ch, i, x + (x * gain - x) * wet);
                }
            }
        }
    };

    // Kamfilter med återkoppling: en resonans på FREQ och dess övertoner (metalliskt).
    struct CombFx
    {
        std::array<std::vector<float>, 2> lines;
        std::array<OnePole, 2> damping;
        int writeIndex = 0;

        void prepare (const dsp::ProcessSpec& spec)
        {
            for (auto& line : lines)
                line.assign ((size_t) (spec.sampleRate / 25.0) + 4, 0.0f);
            reset();
        }

        void reset()
        {
            for (auto& line : lines)
                std::fill (line.begin(), line.end(), 0.0f);
            damping = {};
            writeIndex = 0;
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto size = (int) lines[0].size();
            const auto delay = jlimit (2.0, (double) size - 2.0, context.sampleRate / std::max (1.0f, k[0]));
            const auto fb = k[1];
            const auto dampingCoefficient = onePoleCoefficient (20000.0f * std::pow (0.02f, k[2]), context.sampleRate);
            const auto normalise = 1.0f - std::abs (fb); // resonanstoppen hamnar runt 0 dB
            const auto wet = k[3];

            for (int i = 0; i < numSamples; ++i)
            {
                auto read = (double) writeIndex - delay;
                if (read < 0.0)
                    read += size;
                const auto index = (int) read;
                const auto fraction = (float) (read - index);

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                {
                    auto& line = lines[(size_t) ch];
                    const auto delayed = line[(size_t) index] + (line[(size_t) ((index + 1) % size)] - line[(size_t) index]) * fraction;
                    const auto x = buffer.getSample (ch, i);
                    const auto y = x + fb * damping[(size_t) ch].lowpass (delayed, dampingCoefficient);
                    line[(size_t) writeIndex] = y;
                    buffer.setSample (ch, i, x + (y * normalise - x) * wet);
                }
                writeIndex = (writeIndex + 1) % size;
            }
        }
    };

    struct RingModFx
    {
        double phase = 0.0;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { phase = 0.0; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto step = k[0] / context.sampleRate;
            const auto depth = k[1], wet = k[2];

            for (int i = 0; i < numSamples; ++i)
            {
                const auto carrier = (float) std::sin (MathConstants<double>::twoPi * phase);
                const auto gain = 1.0f - depth + depth * carrier;
                phase += step;
                phase -= std::floor (phase);

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    buffer.setSample (ch, i, x + (x * gain - x) * wet);
                }
            }
        }
    };

    struct BitcrusherFx
    {
        std::array<float, 2> held {};
        int holdCounter = 0;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { held = {}; holdCounter = 0; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            const auto levels = std::exp2 (k[0] - 1.0f);
            const auto holdLength = std::max (1, roundToInt (k[1]));
            const auto wet = k[2];

            for (int i = 0; i < numSamples; ++i)
            {
                const auto takeNew = holdCounter == 0;
                holdCounter = (holdCounter + 1) % holdLength;

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    if (takeNew)
                        held[(size_t) ch] = std::round (x * levels) / levels;
                    buffer.setSample (ch, i, x + (held[(size_t) ch] - x) * wet);
                }
            }
        }
    };
}

//==============================================================================
struct EffectChain::Impl
{
    struct Slot
    {
        std::atomic<float>* type = nullptr;
        std::array<std::atomic<float>*, knobsPerSlot> knobs {};

        EffectType current = EffectType::off;
        Values values {};

        FilterFx filter;
        DistortionFx distortion;
        EqFx eq;
        ChorusFx chorus;
        PhaserFx phaser;
        EchoFx echo;
        ReverbFx reverb;
        CompressorFx compressor;
        GateFx gate;
        CombFx comb;
        RingModFx ringMod;
        BitcrusherFx bitcrusher;
        AutoFilterFx autoFilter;

        template <typename Function>
        void forEachEffect (Function&& function)
        {
            function (filter); function (distortion); function (eq); function (chorus); function (phaser);
            function (echo); function (reverb); function (compressor); function (gate); function (comb);
            function (ringMod); function (bitcrusher); function (autoFilter);
        }

        void resetCurrent()
        {
            switch (current)
            {
                case EffectType::filter:     filter.reset(); break;
                case EffectType::distortion: distortion.reset(); break;
                case EffectType::eq:         eq.reset(); break;
                case EffectType::chorus:     chorus.reset(); break;
                case EffectType::phaser:     phaser.reset(); break;
                case EffectType::echo:       echo.reset(); break;
                case EffectType::reverb:     reverb.reset(); break;
                case EffectType::compressor: compressor.reset(); break;
                case EffectType::gate:       gate.reset(); break;
                case EffectType::comb:       comb.reset(); break;
                case EffectType::ringMod:    ringMod.reset(); break;
                case EffectType::bitcrusher: bitcrusher.reset(); break;
                case EffectType::autoFilter: autoFilter.reset(); break;
                case EffectType::off:
                case EffectType::count:      break;
            }
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Context& context)
        {
            switch (current)
            {
                case EffectType::filter:     filter.process (buffer, numSamples, values, context); break;
                case EffectType::distortion: distortion.process (buffer, numSamples, values, context); break;
                case EffectType::eq:         eq.process (buffer, numSamples, values, context); break;
                case EffectType::chorus:     chorus.process (buffer, numSamples, values, context); break;
                case EffectType::phaser:     phaser.process (buffer, numSamples, values, context); break;
                case EffectType::echo:       echo.process (buffer, numSamples, values, context); break;
                case EffectType::reverb:     reverb.process (buffer, numSamples, values, context); break;
                case EffectType::compressor: compressor.process (buffer, numSamples, values, context); break;
                case EffectType::gate:       gate.process (buffer, numSamples, values, context); break;
                case EffectType::comb:       comb.process (buffer, numSamples, values, context); break;
                case EffectType::ringMod:    ringMod.process (buffer, numSamples, values, context); break;
                case EffectType::bitcrusher: bitcrusher.process (buffer, numSamples, values, context); break;
                case EffectType::autoFilter: autoFilter.process (buffer, numSamples, values, context); break;
                case EffectType::off:
                case EffectType::count:      break;
            }
        }

        bool isActive() const { return current != EffectType::off; }
    };

    Impl (AudioProcessorValueTreeState& apvts, int channelIn) : channel (channelIn)
    {
        for (int i = 0; i < slotsPerChannel; ++i)
        {
            auto& slot = slots[(size_t) i];
            slot.type = apvts.getRawParameterValue (Effects::slotParamID (channel, i, "Type"));
            for (int k = 0; k < knobsPerSlot; ++k)
                slot.knobs[(size_t) k] = apvts.getRawParameterValue (Effects::knobParamID (channel, i, k));
            jassert (slot.type != nullptr && slot.knobs.back() != nullptr);
        }
    }

    const int channel;
    std::array<Slot, slotsPerChannel> slots;
    const Modulator* modulator = nullptr;
    Context context;
    int maxBlockSize = 512;
};

EffectChain::EffectChain (AudioProcessorValueTreeState& apvts, int channel) : impl (std::make_unique<Impl> (apvts, channel)) {}
EffectChain::~EffectChain() = default;

void EffectChain::prepare (double sampleRate, int maxBlockSize)
{
    impl->maxBlockSize = std::max (1, maxBlockSize);
    impl->context.sampleRate = sampleRate;
    const dsp::ProcessSpec spec { sampleRate, (uint32) impl->maxBlockSize, 2 };
    for (auto& slot : impl->slots)
        slot.forEachEffect ([&spec] (auto& effect) { effect.prepare (spec); });
}

void EffectChain::reset()
{
    for (auto& slot : impl->slots)
        slot.forEachEffect ([] (auto& effect) { effect.reset(); });
}

void EffectChain::setModulator (const Modulator* modulator)
{
    impl->modulator = modulator;
}

void EffectChain::update (double bpm, double ppq)
{
    impl->context.bpm = bpm;
    impl->context.ppq = ppq;

    for (auto& slot : impl->slots)
    {
        // En effekt som väljs (eller slås på) börjar tyst, utan rester från förra gången.
        const auto type = (EffectType) jlimit (0, numEffectTypes - 1, (int) slot.type->load());
        if (type != slot.current)
        {
            slot.current = type;
            slot.resetCurrent();
        }

        const auto& list = Effects::knobs (type);
        for (size_t k = 0; k < list.size() && k < (size_t) knobsPerSlot; ++k)
        {
            auto normalised = slot.knobs[k]->load();
            if (impl->modulator != nullptr)
                normalised = impl->modulator->applyRaw (slot.knobs[k], normalised); // LFO:er dragna till ratten
            slot.values[k] = list[k].fromNormalised (normalised);
        }
    }
}

bool EffectChain::hasActive() const
{
    for (const auto& slot : impl->slots)
        if (slot.isActive())
            return true;
    return false;
}

void EffectChain::process (AudioBuffer<float>& buffer, int numSamples)
{
    // Chorus, phaser, reverb och compressor klarar högst maxBlockSize åt gången, så längre block delas.
    for (int start = 0; start < numSamples; start += impl->maxBlockSize)
    {
        const auto length = std::min (impl->maxBlockSize, numSamples - start);
        AudioBuffer<float> part (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);

        auto context = impl->context;
        context.ppq += start / context.samplesPerQuarter();

        for (auto& slot : impl->slots)
            if (slot.isActive())
                slot.process (part, length, context);
    }
}

//==============================================================================
struct EffectRack::Impl
{
    explicit Impl (AudioProcessorValueTreeState& apvts)
    {
        for (int ch = 0; ch < numEffectChannels; ++ch)
        {
            chains[(size_t) ch] = std::make_unique<EffectChain> (apvts, ch);
            level[(size_t) ch] = apvts.getRawParameterValue (Effects::channelParamID (ch, "Level"));
            duck[(size_t) ch] = apvts.getRawParameterValue (Effects::channelParamID (ch, "Duck"));
        }
    }

    std::array<std::unique_ptr<EffectChain>, numEffectChannels> chains;
    std::array<std::atomic<float>*, numEffectChannels> level {}, duck {};
    std::array<AudioBuffer<float>, numEffectChannels> buffers;
    std::array<float, numEffectChannels> gains {}, duckAmounts {};
    std::array<bool, numEffectChannels> active {};
    std::array<float, numEffectChannels> duckState {};
    float duckCoefficient = 0.02f;
    const Modulator* modulator = nullptr;
};

EffectRack::EffectRack (AudioProcessorValueTreeState& apvts) : impl (std::make_unique<Impl> (apvts)) {}
EffectRack::~EffectRack() = default;

void EffectRack::prepare (double sampleRate, int maxBlockSize)
{
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        impl->chains[(size_t) ch]->prepare (sampleRate, maxBlockSize);
        impl->buffers[(size_t) ch].setSize (2, std::max (1, maxBlockSize), false, true, true);
        impl->duckState[(size_t) ch] = 1.0f;
    }
    impl->duckCoefficient = (float) (1.0 - std::exp (-1.0 / (0.001 * sampleRate)));
}

void EffectRack::setModulator (const Modulator* modulator)
{
    impl->modulator = modulator;
    for (auto& chain : impl->chains)
        chain->setModulator (modulator);
}

void EffectRack::beginBlock (int numSamples, double bpm, double ppq)
{
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        auto& buffer = impl->buffers[(size_t) ch];
        if (buffer.getNumSamples() < numSamples)
            buffer.setSize (2, numSamples, false, true, true); // bara om DAW:en skickar ett större block än utlovat
        buffer.clear (0, numSamples);

        auto& chain = *impl->chains[(size_t) ch];
        chain.update (bpm, ppq);
        impl->active[(size_t) ch] = chain.hasActive();

        auto read = [this] (std::atomic<float>* raw)
        {
            const auto value = raw->load();
            return impl->modulator != nullptr ? impl->modulator->applyRaw (raw, value) : value;
        };
        impl->gains[(size_t) ch] = read (impl->level[(size_t) ch]);
        impl->duckAmounts[(size_t) ch] = read (impl->duck[(size_t) ch]);
    }
}

bool EffectRack::isActive (int channel) const
{
    return jlimit (0, numEffectChannels - 1, channel) == channel && impl->active[(size_t) channel];
}

void EffectRack::send (int channel, const float* left, const float* right, int numSamples, float amount)
{
    auto& buffer = impl->buffers[(size_t) channel];
    buffer.addFrom (0, 0, left, numSamples, amount);
    buffer.addFrom (1, 0, right != nullptr ? right : left, numSamples, amount);
}

void EffectRack::process (int numSamples, const float* kickEnvelope, AudioBuffer<float>& mix)
{
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        // En kanal utan effekter är inte en del av ljudvägen (lagren skickar inget dit).
        if (! impl->active[(size_t) ch])
            continue;

        auto& buffer = impl->buffers[(size_t) ch];
        impl->chains[(size_t) ch]->process (buffer, numSamples);

        // Kanalens duckning följer kickens envelope (efter effekterna, så att svansar duckas också).
        auto& state = impl->duckState[(size_t) ch];
        const auto amount = impl->duckAmounts[(size_t) ch];
        const auto gain = impl->gains[(size_t) ch];
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            state += (1.0f - amount * kickEnvelope[i] - state) * impl->duckCoefficient;
            const auto g = gain * state;
            left[i] *= g;
            right[i] *= g;
        }

        mix.addFrom (0, 0, buffer, 0, 0, numSamples);
        mix.addFrom (1, 0, buffer, 1, 0, numSamples);
    }
}
