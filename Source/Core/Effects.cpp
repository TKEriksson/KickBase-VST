#include "Effects.h"
#include "Biquad.h"

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
                 "Compressor", "Trance Gate", "Comb", "Ring Mod", "Bitcrusher", "Auto Filter", "Maximizer", "Sidechain", "Disperser", "Widener", "Flanger", "Transient", "Dynamic EQ", "Octaver", "Band Ducker", "Shimmer", "Low Multiplier" };
    }

    String name (EffectType type)
    {
        return typeNames()[(int) type].toUpperCase();
    }

    String shortName (EffectType type)
    {
        const char* names[] { "-", "FILT", "DIST", "EQ", "CHOR", "PHAS", "ECHO", "VERB",
                              "COMP", "GATE", "COMB", "RING", "CRSH", "AUTO", "MAX", "SIDE", "DISP", "WIDE", "FLNG", "TRAN", "DEQ", "OCT", "BDUC" };
        return names[jlimit (0, numEffectTypes - 1, (int) type)];
    }

    String channelName (int channel) { return String::charToString ((juce_wchar) ('A' + channel)); }
    String channelLabel (int channel) { return "FX" + String (channel + 1); }

    String slotParamID (int channel, int slot, const String& suffix) { return "fx" + channelName (channel) + String (slot + 1) + suffix; }
    String knobParamID (int channel, int slot, int knob)             { return slotParamID (channel, slot, "P" + String (knob + 1)); }
    String channelParamID (int channel, const String& suffix)       { return "fx" + channelName (channel) + suffix; }

    bool isEffectParameter (const String& paramID)
    {
        // "fxA..", "fxB.." ... "fxK.." (inte FX-versionens "fxKey").
        return paramID.startsWith ("fx") && paramID.length() > 2 && paramID[2] >= 'A' && paramID[2] <= 'A' + numEffectChannels - 1 && paramID != "fxKey";
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
                mixKnob (1.0f),
                // FX1 (sist): kurvor som ersätter MODE, samt 3-bandsdistortion (LR4) där DRIVE styr mellanbandet.
                choice ("CURVE", { "Mode", "Tape", "Soft clip", "Tube" }, 0),
                choice ("BANDS", { "Off", "3-band" }, 0),
                value ("XOVER LO", 60.0f, 400.0f, 150.0f, 150.0f, Unit::hz),
                value ("XOVER HI", 1000.0f, 8000.0f, 2500.0f, 2500.0f, Unit::hz),
                value ("LOW DRIVE", 0.0f, 1.0f, 0.0f, 0.4f, Unit::percent),
                value ("HIGH DRIVE", 0.0f, 1.0f, 0.0f, 0.4f, Unit::percent) };
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
                value ("OFFSET", -0.5f, 0.5f, 0.0f, 0.0f, Unit::percent),
                value ("IN PAN", -1.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                value ("IN VOL", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                choice ("FB MODE", { "Normal", "Invert", "Ping-pong" }, 2),
                value ("FEEDBACK", 0.0f, 0.95f, 0.0f, 0.4f, Unit::percent),
                value ("CUT", 1000.0f, 20000.0f, 5000.0f, 6000.0f, Unit::hz),
                value ("DRY VOL", 0.0f, 1.0f, 0.0f, 1.0f, Unit::percent),
                choice ("SYNC", { "Straight", "Dotted", "Triplet" }, 0), // FX1 (sist)
                mixKnob (1.0f) };                                         // ekots nivå (DRY VOL är ingången)
            table[(size_t) EffectType::reverb] = {
                value ("SIZE", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("DAMPING", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("WIDTH", 0.0f, 1.0f, 0.0f, 1.0f, Unit::percent),
                value ("LOW CUT", 20.0f, 2000.0f, 300.0f, 300.0f, Unit::hz),
                mixKnob (0.25f),
                value ("PRE-DELAY", 0.0f, 200.0f, 40.0f, 0.0f, Unit::ms) }; // FX1 (sist)
            table[(size_t) EffectType::compressor] = {
                value ("THRESHOLD", -40.0f, 0.0f, 0.0f, -18.0f, Unit::db),
                value ("RATIO", 1.0f, 20.0f, 4.0f, 4.0f, Unit::ratio),
                value ("ATTACK", 0.1f, 100.0f, 10.0f, 5.0f, Unit::ms),
                value ("RELEASE", 10.0f, 500.0f, 100.0f, 80.0f, Unit::ms),
                value ("MAKEUP", 0.0f, 24.0f, 0.0f, 3.0f, Unit::db),
                mixKnob (1.0f),
                value ("DET HPF", 20.0f, 1000.0f, 150.0f, 20.0f, Unit::hz) }; // FX1 (sist): 20 Hz = av
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
            table[(size_t) EffectType::maximizer] = {
                value ("GAIN", 0.0f, 24.0f, 0.0f, 6.0f, Unit::db),
                value ("CLIP", 0.0f, 1.0f, 0.0f, 0.3f, Unit::percent),
                value ("CEILING", -12.0f, 0.0f, 0.0f, -0.3f, Unit::db),
                value ("RELEASE", 5.0f, 500.0f, 60.0f, 60.0f, Unit::ms),
                mixKnob (1.0f),
                choice ("MODE", { "Classic", "Smooth" }, 0) }; // läge 2 (sist)
            table[(size_t) EffectType::ducker] = {
                choice ("SOURCE", { "Kick", "Instr 1", "Instr 2", "Instr 3", "Instr 4", "Instr 5", "Instr 6", "Instr 7", "Instr 8", "Instr 9", "Instr 10" }, 1),
                value ("THRESHOLD", -48.0f, 0.0f, -20.0f, -24.0f, Unit::db),
                value ("DEPTH", 0.0f, 1.0f, 0.0f, 0.8f, Unit::percent),
                value ("ATTACK", 0.5f, 50.0f, 5.0f, 3.0f, Unit::ms),
                value ("RELEASE", 20.0f, 800.0f, 150.0f, 200.0f, Unit::ms),
                mixKnob (1.0f) };
            table[(size_t) EffectType::disperser] = {
                choice ("STAGES", { "4", "6", "8", "10", "12", "14", "16" }, 2),
                value ("FREQ", 50.0f, 8000.0f, 400.0f, 300.0f, Unit::hz),
                value ("SPREAD", 0.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                value ("Q", 0.3f, 4.0f, 0.7f, 0.7f, Unit::plain),
                mixKnob (1.0f) };
            // FX2 (sist): stereobredd med mono-bas, flanger, transient shaper.
            table[(size_t) EffectType::widener] = {
                value ("WIDTH", 0.0f, 2.0f, 1.0f, 1.0f, Unit::percent),
                value ("BASS MONO", 20.0f, 500.0f, 120.0f, 20.0f, Unit::hz), // 20 Hz = av
                value ("PAN", -1.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                mixKnob (1.0f) };
            table[(size_t) EffectType::flanger] = {
                value ("RATE", 0.02f, 10.0f, 0.5f, 0.3f, Unit::hz),
                value ("DEPTH", 0.0f, 1.0f, 0.0f, 0.7f, Unit::percent),
                value ("DELAY", 0.1f, 10.0f, 2.0f, 1.5f, Unit::ms),
                value ("FEEDBACK", -0.95f, 0.95f, 0.0f, 0.5f, Unit::percent),
                mixKnob (0.5f) };
            table[(size_t) EffectType::transient] = {
                value ("ATTACK", -1.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                value ("SUSTAIN", -1.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                mixKnob (1.0f) };
            table[(size_t) EffectType::dynamicEq] = {
                value ("FREQ", 50.0f, 8000.0f, 400.0f, 300.0f, Unit::hz),
                value ("Q", 0.3f, 8.0f, 1.0f, 1.5f, Unit::plain),
                value ("THRESHOLD", -60.0f, 0.0f, -24.0f, -24.0f, Unit::db),
                value ("RANGE", -18.0f, 18.0f, 0.0f, -6.0f, Unit::db), // minus = sänker bandet när det är högt, plus = höjer
                value ("ATTACK", 0.5f, 50.0f, 5.0f, 5.0f, Unit::ms),
                value ("RELEASE", 20.0f, 800.0f, 150.0f, 150.0f, Unit::ms),
                mixKnob (1.0f) };
            table[(size_t) EffectType::octaver] = {
                value ("OCT DOWN", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("OCT UP", 0.0f, 1.0f, 0.0f, 0.0f, Unit::percent),
                value ("TONE", 60.0f, 600.0f, 200.0f, 200.0f, Unit::hz), // lågpass före oktaven ner (och på den)
                value ("TRANSIENT", 0.0f, 1.0f, 0.0f, 0.0f, Unit::percent), // 0 = oktaven ligger alltid på, 1 = bara på anslagen
                mixKnob (1.0f) };
            table[(size_t) EffectType::bandDucker] = {
                choice ("SOURCE", { "Kick", "Instr 1", "Instr 2", "Instr 3", "Instr 4", "Instr 5", "Instr 6", "Instr 7", "Instr 8", "Instr 9", "Instr 10" }, 0),
                value ("SPLIT", 60.0f, 500.0f, 150.0f, 150.0f, Unit::hz),
                value ("LOW DUCK", 0.0f, 1.0f, 0.0f, 0.9f, Unit::percent),
                value ("HIGH DUCK", 0.0f, 1.0f, 0.0f, 0.3f, Unit::percent),
                value ("THRESHOLD", -48.0f, 0.0f, -20.0f, -24.0f, Unit::db),
                value ("ATTACK", 0.5f, 50.0f, 5.0f, 3.0f, Unit::ms),
                value ("RELEASE", 20.0f, 800.0f, 150.0f, 200.0f, Unit::ms),
                mixKnob (1.0f) };
            table[(size_t) EffectType::shimmer] = {
                value ("SIZE", 0.0f, 1.0f, 0.0f, 0.7f, Unit::percent),
                value ("DAMPING", 0.0f, 1.0f, 0.0f, 0.4f, Unit::percent),
                value ("SHIMMER", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent), // hur mycket av oktavtonen som matas tillbaka in i reverbet
                value ("LOW CUT", 20.0f, 2000.0f, 300.0f, 300.0f, Unit::hz),
                mixKnob (0.3f),
                choice ("INTERVAL", { "+12", "+7", "+19", "+24" }, 0) };
            table[(size_t) EffectType::lowMultiplier] = {
                value ("LOW", 30.0f, 200.0f, 80.0f, 40.0f, Unit::hz),
                value ("HIGH", 100.0f, 400.0f, 200.0f, 200.0f, Unit::hz),
                value ("DRIVE", 0.0f, 1.0f, 0.0f, 0.5f, Unit::percent),
                value ("MONO", 0.0f, 1.0f, 0.0f, 1.0f, Unit::percent),
                mixKnob (0.5f) };
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
            const auto channelTitle = channelLabel (channel);
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "Level"), 1 }, channelTitle + " Level",
                                                               NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "Duck"), 1 }, channelTitle + " Duck",
                                                               NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));

            // EQ: högpass, fyra band (frekvens, gain, Q), lågpass.
            auto skewedRange = [] (float lo, float hi, float centre)
            {
                NormalisableRange<float> range (lo, hi, 0.1f);
                range.setSkewForCentre (centre);
                return range;
            };
            auto hzText = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "EqHp"), 1 }, channelTitle + " EQ High Pass",
                                                               skewedRange (20.0f, 600.0f, 90.0f), 20.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 21.0f ? String ("Off") : String (roundToInt (v)) + " Hz"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "EqLp"), 1 }, channelTitle + " EQ Low Pass",
                                                               skewedRange (1500.0f, 20000.0f, 6000.0f), 20000.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 19500.0f ? String ("Off") : (v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"); })));
            const float bandLow[] { 30.0f, 60.0f, 400.0f, 2000.0f }, bandHigh[] { 400.0f, 2000.0f, 8000.0f, 16000.0f };
            const float bandDefault[] { 100.0f, 400.0f, 2500.0f, 8000.0f }, bandCentre[] { 100.0f, 400.0f, 2000.0f, 6000.0f };
            for (int b = 0; b < eqBands; ++b)
            {
                const auto name = channelTitle + " EQ " + String (b + 1) + " ";
                const auto key = "Eq" + String (b + 1);
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, key + "F"), 1 }, name + "Freq",
                                                                   skewedRange (bandLow[b], bandHigh[b], bandCentre[b]), bandDefault[b],
                                                                   AudioParameterFloatAttributes().withStringFromValueFunction (hzText)));
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, key + "G"), 1 }, name + "Gain",
                                                                   NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f,
                                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; })));
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, key + "Q"), 1 }, name + "Q",
                                                                   skewedRange (0.3f, 8.0f, 1.2f), b == 1 || b == 2 ? 1.0f : 0.7f,
                                                                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
            }

            // Kompressor: neutral som standard (ratio 1:1, ingen make-up).
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "CompThr"), 1 }, channelTitle + " Comp Threshold",
                                                               NormalisableRange<float> (-40.0f, 0.0f, 0.1f), 0.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "CompRatio"), 1 }, channelTitle + " Comp Ratio",
                                                               skewedRange (1.0f, 20.0f, 4.0f), 1.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "CompAtt"), 1 }, channelTitle + " Comp Attack",
                                                               skewedRange (0.1f, 100.0f, 10.0f), 10.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "CompRel"), 1 }, channelTitle + " Comp Release",
                                                               skewedRange (10.0f, 500.0f, 100.0f), 100.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { channelParamID (channel, "CompMakeup"), 1 }, channelTitle + " Comp Makeup",
                                                               NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f,
                                                               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "+" + String (v, 1) + " dB"; })));

            for (int slot = 0; slot < slotsPerChannel; ++slot)
            {
                const auto title = channelTitle + " Slot " + String (slot + 1); // (förr "FX11" för FX1 plats 1: samma som kanalen FX11)
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

    EqSettings readEq (const AudioProcessorValueTreeState& apvts, int channel)
    {
        auto get = [&] (const String& suffix, float fallback)
        {
            const auto* value = apvts.getRawParameterValue (channelParamID (channel, suffix));
            return value != nullptr ? value->load() : fallback;
        };
        EqSettings s;
        s.hp = get ("EqHp", 20.0f);
        s.lp = get ("EqLp", 20000.0f);
        for (int b = 0; b < eqBands; ++b)
        {
            const auto key = "Eq" + String (b + 1);
            s.freq[b] = get (key + "F", s.freq[b]);
            s.gain[b] = get (key + "G", 0.0f);
            s.q[b] = get (key + "Q", s.q[b]);
        }
        return s;
    }

    EqRefs::EqRefs (const AudioProcessorValueTreeState& apvts, int channel)
    {
        values[0] = apvts.getRawParameterValue (channelParamID (channel, "EqHp"));
        values[1] = apvts.getRawParameterValue (channelParamID (channel, "EqLp"));
        for (int b = 0; b < eqBands; ++b)
        {
            const auto key = "Eq" + String (b + 1);
            values[(size_t) (2 + b * 3)] = apvts.getRawParameterValue (channelParamID (channel, key + "F"));
            values[(size_t) (3 + b * 3)] = apvts.getRawParameterValue (channelParamID (channel, key + "G"));
            values[(size_t) (4 + b * 3)] = apvts.getRawParameterValue (channelParamID (channel, key + "Q"));
        }
    }

    EqSettings EqRefs::read() const
    {
        // Samma värden och reservvärden som readEq.
        auto get = [] (const std::atomic<float>* value, float fallback) { return value != nullptr ? value->load() : fallback; };
        EqSettings s;
        s.hp = get (values[0], 20.0f);
        s.lp = get (values[1], 20000.0f);
        for (int b = 0; b < eqBands; ++b)
        {
            s.freq[b] = get (values[(size_t) (2 + b * 3)], s.freq[b]);
            s.gain[b] = get (values[(size_t) (3 + b * 3)], 0.0f);
            s.q[b] = get (values[(size_t) (4 + b * 3)], s.q[b]);
        }
        return s;
    }

    std::array<std::array<float, 6>, eqBands + 2> eqArrayCoefficients (double rate, const EqSettings& s)
    {
        using Array = dsp::IIR::ArrayCoefficients<float>;
        const auto limit = (float) rate * 0.45f;
        return { Array::makeHighPass (rate, s.hp <= 21.0f ? 5.0f : jmin (s.hp, limit), 0.7071f),
                 Array::makeLowShelf (rate, jmin (s.freq[0], limit), 0.7f, Decibels::decibelsToGain (s.gain[0])),
                 Array::makePeakFilter (rate, jmin (s.freq[1], limit), s.q[1], Decibels::decibelsToGain (s.gain[1])),
                 Array::makePeakFilter (rate, jmin (s.freq[2], limit), s.q[2], Decibels::decibelsToGain (s.gain[2])),
                 Array::makeHighShelf (rate, jmin (s.freq[3], limit), 0.7f, Decibels::decibelsToGain (s.gain[3])),
                 Array::makeLowPass (rate, s.lp >= 19500.0f ? limit : jmin (s.lp, limit), 0.7071f) };
    }

    std::array<dsp::IIR::Coefficients<float>::Ptr, eqBands + 2> eqCoefficients (double rate, const EqSettings& s)
    {
        // Samma som Coefficients::makeHighPass osv. (JUCE bygger dem av ArrayCoefficients på samma sätt).
        const auto arrays = eqArrayCoefficients (rate, s);
        std::array<dsp::IIR::Coefficients<float>::Ptr, eqBands + 2> result;
        for (size_t f = 0; f < result.size(); ++f)
            result[f] = new dsp::IIR::Coefficients<float> (arrays[f]);
        return result;
    }

    float eqResponseDb (double rate, const EqSettings& s, float hz)
    {
        float magnitude = 1.0f;
        for (auto& c : eqCoefficients (rate, s))
            magnitude = (float) (magnitude * c->getMagnitudeForFrequency (hz, rate));
        return Decibels::gainToDecibels (magnitude, -60.0f);
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

    bool newKnobDefault (EffectType type, int k, float& normalised)
    {
        // Antal rattar före FX1: de nya rattarna (därefter) ska ha sitt standardvärde i stället för parameterns 0.5.
        const auto legacyCount = type == EffectType::distortion ? 4 : type == EffectType::echo ? 8 : type == EffectType::reverb ? 5
                               : type == EffectType::maximizer ? 5 : type == EffectType::compressor ? 6 : knobsPerSlot;
        const auto& list = knobs (type);
        if (k < legacyCount || k >= (int) list.size() || k >= knobsPerSlot)
            return false;
        normalised = list[(size_t) k].toNormalised (list[(size_t) k].defaultValue);
        return true;
    }

    void migrateLegacyKnobs (ValueTree& state)
    {
        if ((int) state.getProperty ("fxKnobsV", 0) >= knobsVersion)
            return;
        state.setProperty ("fxKnobsV", knobsVersion, nullptr);

        const auto read = [&] (const String& id, float fallback)
        {
            const auto child = state.getChildWithProperty ("id", id);
            return child.isValid() ? (float) (double) child.getProperty ("value", fallback) : fallback;
        };
        const auto write = [&] (const String& id, float v)
        {
            auto child = state.getChildWithProperty ("id", id);
            if (child.isValid())
                return; // en sparad rattväder ska aldrig skrivas över
            child = ValueTree ("PARAM");
            child.setProperty ("id", id, nullptr);
            child.setProperty ("value", v, nullptr);
            state.appendChild (child, nullptr);
        };

        for (int channel = 0; channel < numEffectChannels; ++channel)
            for (int slot = 0; slot < slotsPerChannel; ++slot)
            {
                const auto typeIndex = roundToInt (read (slotParamID (channel, slot, "Type"), 0.0f));
                if (! isPositiveAndBelow (typeIndex, numEffectTypes))
                    continue;
                const auto type = (EffectType) typeIndex;
                float value = 0.0f;
                for (int k = 0; k < knobsPerSlot; ++k)
                    if (newKnobDefault (type, k, value))
                        write (knobParamID (channel, slot, k), value);
            }
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
        int controlPhase = 0; // hur långt in i sitt kontrollfönster (Modulation::controlInterval) buffertens första sample ligger
        const SourceSignals* sources = nullptr; // kickens och instrumentens ljud (för SIDECHAIN)

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

        // cutoffAt (start, newWindow) ger cutoff för en bit av blocket (Auto Filter sveper den). Bitarna följer kontrolltaktens
        // 32-rutnät (controlPhase = buffertens början i sitt fönster), så att en bit som DAW:ens block delat får samma cutoff som ett
        // helt fönster: newWindow är sant när biten börjar ett nytt fönster.
        template <typename CutoffAt>
        void run (AudioBuffer<float>& buffer, int numSamples, int type, float reso, bool steep, float wet, double sampleRate, int controlPhase, CutoffAt cutoffAt)
        {
            const auto filterType = (dsp::StateVariableTPTFilterType) jlimit (0, 2, type);
            const auto q = 0.5f * std::pow (20.0f, reso);
            first.setType (filterType);
            second.setType (filterType);
            first.setResonance (steep ? 0.707f : q);
            second.setResonance (q);

            for (int start = 0; start < numSamples;)
            {
                const auto phase = (controlPhase + start) % chunk;
                const auto end = std::min (numSamples, start + chunk - phase);
                const auto hz = jlimit (20.0f, (float) sampleRate * 0.45f, cutoffAt (start, phase == 0));
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
                start = end;
            }
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            if (fresh)
                smoothedCutoff.setCurrentAndTargetValue (k[1]);
            fresh = false;
            smoothedCutoff.setTargetValue (k[1]);
            // Utjämningen stegas ett helt fönster i taget när ett fönster börjar; en bit mitt i ett fönster använder fönstrets värde
            // (förut stegades den med bitens längd: en bit på 5 + en på 27 gav andra värden än en på 32).
            run (buffer, numSamples, (int) k[0], k[2], k[3] >= 0.5f, k[4], context.sampleRate, context.controlPhase,
                 [this] (int, bool newWindow)
                 {
                     if (newWindow)
                         windowCutoff = smoothedCutoff.skip (chunk);
                     return windowCutoff;
                 });
        }

        float windowCutoff = 1000.0f; // utjämnad cutoff för fönstret som pågår
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

            filter.run (buffer, numSamples, (int) k[0], k[2], true, k[5], context.sampleRate, context.controlPhase, [&] (int start, bool)
            {
                // Svepet räknas i mitten av kontrollfönstret som biten ligger i (inte bitens egen mitt): en bit som DAW:ens block delat
                // mitt i ett fönster får samma cutoff som ett helt fönster, så ljudet beror inte på blockstorleken.
                constexpr int window = Modulation::controlInterval;
                const auto windowStart = ((start + context.controlPhase) / window) * window - context.controlPhase;
                const auto ppq = context.ppq + (windowStart + window * 0.5) * quartersPerSample;
                const auto phase = ppq / period - std::floor (ppq / period);
                const auto triangle = (float) (phase < 0.5 ? phase * 4.0 - 1.0 : 3.0 - phase * 4.0); // -1..1
                return cutoff * std::exp2 (0.5f * depth * triangle);
            });
        }
    };

    struct DistortionFx
    {
        std::array<OnePole, 2> toneFilters;
        std::array<std::array<float, 2>, 3> held {}; // crush: [band][kanal]
        int holdCounter = 0;
        // LR4-delning: lågt | (mitt | högt). Lågbandet går också genom ett allpass vid övre delningen, så att summan blir fasriktig.
        dsp::LinkwitzRileyFilter<float> splitLow, splitHigh, allpassLow;

        void prepare (const dsp::ProcessSpec& spec)
        {
            for (auto* filter : { &splitLow, &splitHigh, &allpassLow })
                filter->prepare (spec);
            reset();
        }

        void reset()
        {
            toneFilters = {}; held = {}; holdCounter = 0;
            splitLow.reset(); splitHigh.reset(); allpassLow.reset();
        }

        static float shape (int mode, int curve, float x, float gain, float levels, float& heldValue, bool takeNew)
        {
            switch (curve)
            {
                case 1: return std::atan (1.2f * gain * x) / std::atan (1.2f * gain);                    // tape: mjukt knä
                case 2: { const auto z = jlimit (-1.5f, 1.5f, gain * x); return z - z * z * z / 6.75f; } // soft clip (kubisk)
                case 3: return (std::tanh (gain * x + 0.2f) - std::tanh (0.2f)) / (std::tanh (gain + 0.2f) - std::tanh (0.2f)); // tube: asymmetrisk
                default: break;
            }

            switch (mode)
            {
                case 0: return std::tanh (gain * x) / std::tanh (gain);                // soft
                case 1: return jlimit (-1.0f, 1.0f, gain * x) / std::min (gain, 1.0f); // hard
                case 2: return std::sin (gain * x * MathConstants<float>::halfPi);     // fold
                case 3:                                                                // crush
                    if (takeNew)
                        heldValue = std::round (x * levels) / levels;
                    return heldValue;
                default: return x;
            }
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto mode = (int) k[0];
            const auto amount = k[1];
            const auto wet = k[3];
            const auto curve = (int) k[4];
            const auto multiband = (int) k[5] == 1;
            const auto toneCoefficient = onePoleCoefficient (k[2], context.sampleRate);

            // Crush: färre bitar och lägre samplingsfrekvens ju mer drive.
            const auto holdLength = 1 + (int) (amount * 24.0f);

            // Lågt band = LOW DRIVE, mitt = DRIVE, högt = HIGH DRIVE (utan band gäller bara DRIVE).
            const std::array<float, 3> amounts { multiband ? k[8] : amount, amount, multiband ? k[9] : amount };
            std::array<float, 3> gains, levels;
            for (size_t b = 0; b < 3; ++b)
            {
                gains[b] = std::pow (10.0f, amounts[b] * 1.5f); // upp till ~30 gånger (+30 dB)
                levels[b] = std::pow (2.0f, 16.0f - amounts[b] * 13.0f);
            }

            if (multiband)
            {
                const auto low = jlimit (20.0f, (float) context.sampleRate * 0.4f, k[6]);
                const auto high = jlimit (low * 1.5f, (float) context.sampleRate * 0.45f, k[7]);
                splitLow.setCutoffFrequency (low);
                splitHigh.setCutoffFrequency (high);
                allpassLow.setCutoffFrequency (high);
            }

            for (int i = 0; i < numSamples; ++i)
            {
                const auto takeNew = holdCounter == 0;
                holdCounter = (holdCounter + 1) % holdLength;

                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    float y;

                    if (! multiband)
                        y = shape (mode, curve, x, gains[1], levels[1], held[0][(size_t) ch], takeNew);
                    else
                    {
                        float low, rest, mid, high, lowLow, lowHigh;
                        splitLow.processSample (ch, x, low, rest);
                        splitHigh.processSample (ch, rest, mid, high);
                        allpassLow.processSample (ch, low, lowLow, lowHigh);
                        y = shape (mode, curve, lowLow + lowHigh, gains[0], levels[0], held[0][(size_t) ch], takeNew)
                          + shape (mode, curve, mid, gains[1], levels[1], held[1][(size_t) ch], takeNew)
                          + shape (mode, curve, high, gains[2], levels[2], held[2][(size_t) ch], takeNew);
                    }

                    y = toneFilters[(size_t) ch].lowpass (y, toneCoefficient);
                    buffer.setSample (ch, i, x + (y - x) * wet);
                }
            }
        }
    };

    struct EqFx
    {
        std::array<Biquad, 2> lowFilters, midFilters, highFilters; // räknar som dsp::IIR::Filter, bit för bit
        double sampleRate = 44100.0;
        float lastLow = -100.0f, lastMidFreq = -1.0f, lastMid = -100.0f, lastHigh = -100.0f;

        void prepare (const dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto* filters : { &lowFilters, &midFilters, &highFilters })
                for (auto& filter : *filters)
                {
                    filter.setIdentity();
                    filter.reset();
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
                    f.setCoefficients (Array::makeLowShelf (sampleRate, 120.0f, 0.7f, Decibels::decibelsToGain (l)));
            if (mf != lastMidFreq || m != lastMid)
                for (auto& f : midFilters)
                    f.setCoefficients (Array::makePeakFilter (sampleRate, mf, 0.9f, Decibels::decibelsToGain (m)));
            if (h != lastHigh)
                for (auto& f : highFilters)
                    f.setCoefficients (Array::makeHighShelf (sampleRate, 6000.0f, 0.7f, Decibels::decibelsToGain (h)));
            lastLow = l; lastMidFreq = mf; lastMid = m; lastHigh = h;

            for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    data[i] = highFilters[(size_t) ch].process (midFilters[(size_t) ch].process (lowFilters[(size_t) ch].process (data[i])));
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

    // Som FL Studios Fruity Delay 2: ingången (pan, volym) matas in i två fördröjningar, ekot går
    // tillbaka med återkoppling (normal, inverterad eller ping-pong) och ett filter (CUT), höger
    // sida kan ligga före eller efter vänster (OFFSET), och DRY VOL är det orörda ljudet.
    struct EchoFx
    {
        static constexpr double maxSeconds = 3.4; // 1/2 not vid 55 BPM, med offset på +50 %

        dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::Linear> line;
        SmoothedValue<float> delayLeft, delayRight;
        std::array<OnePole, 2> lowCutFilters, highCutFilters;
        bool fresh = true;

        void prepare (const dsp::ProcessSpec& spec)
        {
            line.prepare ({ spec.sampleRate, spec.maximumBlockSize, 2 });
            line.setMaximumDelayInSamples ((int) (spec.sampleRate * maxSeconds) + 4);
            delayLeft.reset (spec.sampleRate, 0.08);
            delayRight.reset (spec.sampleRate, 0.08);
            reset();
        }

        void reset() { line.reset(); lowCutFilters = {}; highCutFilters = {}; fresh = true; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto longest = context.sampleRate * (maxSeconds - 0.05);
            const auto syncFactor = (int) k[8] == 1 ? 1.5 : (int) k[8] == 2 ? 2.0 / 3.0 : 1.0; // punkterad / triol
            const auto time = Effects::syncInQuarters ((int) k[0]) * syncFactor * context.samplesPerQuarter();
            const auto targetLeft = (float) jlimit (1.0, longest, time);
            const auto targetRight = (float) jlimit (1.0, longest, time * (1.0 + k[1]));
            if (fresh)
            {
                delayLeft.setCurrentAndTargetValue (targetLeft);
                delayRight.setCurrentAndTargetValue (targetRight);
            }
            fresh = false;
            delayLeft.setTargetValue (targetLeft);
            delayRight.setTargetValue (targetRight);

            const auto pan = k[2];
            const auto gainLeft = pan > 0.0f ? 1.0f - pan : 1.0f, gainRight = pan < 0.0f ? 1.0f + pan : 1.0f;
            const auto inputVolume = k[3];
            const auto mode = (int) k[4];
            const auto pong = mode == 2;
            const auto feedback = k[5] * (mode == 1 ? -1.0f : 1.0f);
            const auto lowCoefficient = onePoleCoefficient (60.0f, context.sampleRate); // släpper inte in dc och sub i slingan
            const auto highCoefficient = onePoleCoefficient (k[6], context.sampleRate);
            const auto dry = k[7];
            const auto echoMix = k[9];
            const auto stereo = buffer.getNumChannels() > 1;

            for (int i = 0; i < numSamples; ++i)
            {
                std::array<float, 2> echo { line.popSample (0, delayLeft.getNextValue()), line.popSample (1, delayRight.getNextValue()) };

                // Filtrera ekot, så att varje upprepning blir mörkare.
                for (size_t ch = 0; ch < 2; ++ch)
                    echo[ch] = highCutFilters[ch].lowpass (lowCutFilters[ch].highpass (echo[ch], lowCoefficient), highCoefficient);

                const auto inL = buffer.getSample (0, i);
                const auto inR = stereo ? buffer.getSample (1, i) : inL;

                if (pong)
                {
                    // Ping-pong: ingången går in på ena sidan (åt det håll du panorerar, annars vänster)
                    // och ekot studsar mellan sidorna.
                    const auto mono = (inL + inR) * 0.5f * inputVolume;
                    line.pushSample (0, (pan > 0.0f ? 0.0f : mono) + echo[1] * feedback);
                    line.pushSample (1, (pan > 0.0f ? mono : 0.0f) + echo[0] * feedback);
                }
                else
                {
                    line.pushSample (0, inL * gainLeft * inputVolume + echo[0] * feedback);
                    line.pushSample (1, inR * gainRight * inputVolume + echo[1] * feedback);
                }

                buffer.setSample (0, i, inL * dry + echo[0] * echoMix);
                if (stereo)
                    buffer.setSample (1, i, inR * dry + echo[1] * echoMix);
            }
        }
    };

    struct ReverbFx
    {
        dsp::Reverb reverb;
        dsp::StateVariableTPTFilter<float> lowCutFilter;
        dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::Linear> preDelay;
        AudioBuffer<float> wetBuffer;
        double sampleRate = 44100.0;
        bool preDelayActive = false;

        void prepare (const dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            preDelay.prepare (spec);
            preDelay.setMaximumDelayInSamples ((int) (spec.sampleRate * 0.2) + 4);
            reverb.prepare (spec);
            lowCutFilter.prepare (spec);
            lowCutFilter.setType (dsp::StateVariableTPTFilterType::highpass);
            wetBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        }

        void reset() { reverb.reset(); lowCutFilter.reset(); preDelay.reset(); preDelayActive = false; }

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

            // PRE-DELAY: fördröjer det som går in i reverbet (0 ms = signalen rörs inte).
            const auto preDelaySamples = (float) (k[5] * 0.001 * sampleRate);
            if (preDelaySamples >= 0.5f)
            {
                preDelayActive = true;
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto* data = wetBuffer.getWritePointer (ch);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        preDelay.pushSample (ch, data[i]);
                        data[i] = preDelay.popSample (ch, preDelaySamples);
                    }
                }
            }
            else if (preDelayActive)
            {
                preDelay.reset();
                preDelayActive = false;
            }

            auto block = dsp::AudioBlock<float> (wetBuffer).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) channels);
            lowCutFilter.process (dsp::ProcessContextReplacing<float> (block));
            reverb.process (dsp::ProcessContextReplacing<float> (block));

            for (int ch = 0; ch < channels; ++ch)
                buffer.addFrom (ch, 0, wetBuffer, ch, 0, numSamples, k[4]);
        }
    };

    // SHIMMER: ett reverb vars svans matas tillbaka genom en tonhöjdshöjning (en oktav som standard). Varje varv lägger svansen högre,
    // så den "glittrar" uppåt. Tonhöjdshöjaren är två korsutjämnade avläsningar av en fördröjningslinje. Återkopplingen går i block om
    // 64 samplar så att ljudet inte beror på värdens blockstorlek.
    struct ShimmerFx
    {
        static constexpr int chunk = 64;

        struct Shifter
        {
            std::vector<float> line;
            int writePosition = 0;
            double phase = 0.0;

            float process (float input, double ratio, int windowSamples)
            {
                const auto mask = (int) line.size() - 1;
                line[(size_t) writePosition] = input;
                writePosition = (writePosition + 1) & mask;
                phase += (ratio - 1.0) / (double) windowSamples;
                phase -= std::floor (phase);

                float out = 0.0f;
                for (int tap = 0; tap < 2; ++tap)
                {
                    const auto p = std::fmod (phase + 0.5 * tap, 1.0);
                    const auto delay = (1.0 - p) * (double) windowSamples;
                    const auto position = (double) writePosition - 1.0 - delay;
                    const auto base = (int) std::floor (position);
                    const auto fraction = (float) (position - (double) base);
                    const auto a = line[(size_t) (base & mask)], b = line[(size_t) ((base + 1) & mask)];
                    const auto gain = (float) (0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * p));
                    out += gain * (a + (b - a) * fraction);
                }
                return out;
            }
        };

        dsp::Reverb reverb;
        dsp::StateVariableTPTFilter<float> lowCutFilter;
        AudioBuffer<float> wetBuffer, feedback;
        std::array<Shifter, 2> shifters;
        int windowSamples = 2400;

        void prepare (const dsp::ProcessSpec& spec)
        {
            reverb.prepare (spec);
            lowCutFilter.prepare (spec);
            lowCutFilter.setType (dsp::StateVariableTPTFilterType::highpass);
            wetBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
            feedback.setSize ((int) spec.numChannels, chunk);
            windowSamples = std::max (64, (int) (spec.sampleRate * 0.05));
            for (auto& shifter : shifters)
                shifter.line.assign ((size_t) nextPowerOfTwo (windowSamples + 8), 0.0f);
            reset();
        }

        void reset()
        {
            reverb.reset();
            lowCutFilter.reset();
            feedback.clear();
            for (auto& shifter : shifters)
            {
                std::fill (shifter.line.begin(), shifter.line.end(), 0.0f);
                shifter.writePosition = 0;
                shifter.phase = 0.0;
            }
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            dsp::Reverb::Parameters p;
            p.roomSize = k[0];
            p.damping = k[1];
            p.width = 1.0f;
            p.wetLevel = 1.0f;
            p.dryLevel = 0.0f;
            reverb.setParameters (p);
            lowCutFilter.setCutoffFrequency (k[3]);
            const auto amount = k[2] * 0.8f;
            const auto semitones = std::array<double, 4> { 12.0, 7.0, 19.0, 24.0 }[(size_t) jlimit (0, 3, (int) k[5])];
            const auto ratio = std::exp2 (semitones / 12.0);

            const auto channels = std::min ({ buffer.getNumChannels(), wetBuffer.getNumChannels(), (int) shifters.size() });
            for (int ch = 0; ch < channels; ++ch)
                wetBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

            for (int start = 0; start < numSamples; start += chunk)
            {
                const auto length = std::min (chunk, numSamples - start);
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto* wet = wetBuffer.getWritePointer (ch, start);
                    const auto* fed = feedback.getReadPointer (ch);
                    for (int i = 0; i < length; ++i)
                        wet[i] += fed[i] * amount;
                }

                auto block = dsp::AudioBlock<float> (wetBuffer).getSubBlock ((size_t) start, (size_t) length).getSubsetChannelBlock (0, (size_t) channels);
                lowCutFilter.process (dsp::ProcessContextReplacing<float> (block));
                reverb.process (dsp::ProcessContextReplacing<float> (block));

                feedback.clear();
                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto* wet = wetBuffer.getReadPointer (ch, start);
                    auto* fed = feedback.getWritePointer (ch);
                    for (int i = 0; i < length; ++i)
                        fed[i] = std::tanh (shifters[(size_t) ch].process (wet[i], ratio, windowSamples));
                }
            }

            for (int ch = 0; ch < channels; ++ch)
                buffer.addFrom (ch, 0, wetBuffer, ch, 0, numSamples, k[4]);
        }
    };
    // LOW MULTIPLIER: bara bandet LOW-HIGH (30-400 Hz) formas om (mjuk klippning) så att det får övertoner som hörs på små högtalare. Det som
    // läggs till (formad minus original) kan göras mono (MONO), så att lågändan inte blir bred. Bandet själv och allt utanför rörs inte.
    struct LowMultiplierFx
    {
        std::array<dsp::StateVariableTPTFilter<float>, 2> highpass, lowpass;
        bool prepared = false;

        void prepare (const dsp::ProcessSpec& spec)
        {
            dsp::ProcessSpec mono = spec;
            mono.numChannels = 1;
            for (auto& f : highpass) { f.prepare (mono); f.setType (dsp::StateVariableTPTFilterType::highpass); }
            for (auto& f : lowpass) { f.prepare (mono); f.setType (dsp::StateVariableTPTFilterType::lowpass); }
            prepared = true;
        }

        void reset()
        {
            for (auto& f : highpass) f.reset();
            for (auto& f : lowpass) f.reset();
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context&)
        {
            const auto low = k[0], high = std::max (k[1], k[0] + 20.0f);
            const auto drive = 1.0f + 12.0f * k[2];
            const auto norm = 1.0f / std::tanh (drive);
            const auto mono = k[3], wet = k[4];
            const auto channels = std::min (2, buffer.getNumChannels());
            for (auto& f : highpass) f.setCutoffFrequency (low);
            for (auto& f : lowpass) f.setCutoffFrequency (high);

            for (int i = 0; i < numSamples; ++i)
            {
                std::array<float, 2> added {};
                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto band = lowpass[(size_t) ch].processSample (0, highpass[(size_t) ch].processSample (0, buffer.getSample (ch, i)));
                    added[(size_t) ch] = std::tanh (band * drive) * norm - band;
                }
                if (channels == 2)
                {
                    const auto mid = 0.5f * (added[0] + added[1]);
                    for (auto& a : added)
                        a += (mid - a) * mono;
                }
                for (int ch = 0; ch < channels; ++ch)
                    buffer.setSample (ch, i, buffer.getSample (ch, i) + added[(size_t) ch] * wet);
            }
        }
    };
    struct CompressorFx
    {
        dsp::Compressor<float> compressor;
        AudioBuffer<float> dry;
        std::array<OnePole, 2> detectorFilters;
        float envelope = 0.0f; // detektorns nivå när DET HPF används

        void prepare (const dsp::ProcessSpec& spec)
        {
            compressor.prepare (spec);
            dry.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        }

        void reset() { compressor.reset(); detectorFilters = {}; envelope = 0.0f; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
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

            if (k[6] > 20.5f)
            {
                // DET HPF: detektorn lyssnar på högpassat ljud så att basen inte pumpar kompressorn; förstärkningen läggs på det ofiltrerade ljudet.
                const auto hpf = onePoleCoefficient (k[6], context.sampleRate);
                const auto attack = 1.0f - std::exp (-1.0f / (std::max (0.1f, k[2]) * 0.001f * (float) context.sampleRate));
                const auto release = 1.0f - std::exp (-1.0f / (std::max (1.0f, k[3]) * 0.001f * (float) context.sampleRate));
                const auto slope = 1.0f - 1.0f / std::max (1.0f, k[1]);
                for (int i = 0; i < numSamples; ++i)
                {
                    float level = 0.0f;
                    for (int ch = 0; ch < channels; ++ch)
                        level = std::max (level, std::abs (detectorFilters[(size_t) ch].highpass (buffer.getSample (ch, i), hpf)));
                    envelope += (level - envelope) * (level > envelope ? attack : release);
                    const auto over = Decibels::gainToDecibels (envelope, -100.0f) - k[0];
                    const auto gain = over > 0.0f ? Decibels::decibelsToGain (-over * slope) : 1.0f;
                    for (int ch = 0; ch < channels; ++ch)
                        buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
                }
            }
            else
            {
                auto block = dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples).getSubsetChannelBlock (0, (size_t) channels);
                compressor.process (dsp::ProcessContextReplacing<float> (block));
            }

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

    // Maximizer utan fördröjning: förstärkning in, mjuk klippning och en limiter med omedelbar attack (ingenting går över
    // taket) och en release. Utan lookahead, så att den kan ligga i en effektkanal utan att flytta ljudet i tiden.
    // Ett bra sätt att göra kicken högljudd och jämn för sig: skicka kicken till en kanal med en Maximizer (AMOUNT 100 %).
    struct MaximizerFx
    {
        float envelope = 1.0f; // förstärkningsminskning som tal >= 1 (länkat mellan kanalerna)
        float fastEnvelope = 1.0f, slowEnvelope = 1.0f; // läge 2

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { envelope = fastEnvelope = slowEnvelope = 1.0f; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto gain = Decibels::decibelsToGain (k[0]);
            const auto clipAmount = k[1];
            const auto ceiling = std::max (0.05f, Decibels::decibelsToGain (k[2]));
            const auto release = (float) std::exp (-1.0 / (std::max (1.0, (double) k[3]) * 0.001 * context.sampleRate));
            const auto wet = k[4];
            const auto smooth = k[5] > 0.5f;
            const auto channels = std::min (2, buffer.getNumChannels());
            const auto fastRelease = release * release * release * release;
            const auto slowRelease = std::sqrt (release);

            for (int i = 0; i < numSamples; ++i)
            {
                std::array<float, 2> dry {}, processed {};
                auto peak = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                {
                    dry[(size_t) ch] = buffer.getSample (ch, i);
                    const auto x = dry[(size_t) ch] * gain;
                    const auto clipped = smooth ? x / std::sqrt (1.0f + x * x) : std::tanh (x);
                    processed[(size_t) ch] = x + (clipped - x) * clipAmount;
                    peak = std::max (peak, std::abs (processed[(size_t) ch]));
                }

                const auto needed = std::max (1.0f, peak / ceiling);
                if (smooth)
                {
                    // LÄGE 2: släpper programberoende, snabb del och långsam del var för sig; medelvärdet är aldrig under det som behövs, så taket håller.
                    fastEnvelope = std::max (needed, 1.0f + (fastEnvelope - 1.0f) * fastRelease);
                    slowEnvelope = std::max (needed, 1.0f + (slowEnvelope - 1.0f) * slowRelease);
                    envelope = 0.5f * (fastEnvelope + slowEnvelope);
                }
                else
                {
                    envelope = std::max (needed, 1.0f + (envelope - 1.0f) * release);
                    fastEnvelope = slowEnvelope = envelope;
                }

                for (int ch = 0; ch < channels; ++ch)
                    buffer.setSample (ch, i, dry[(size_t) ch] + (processed[(size_t) ch] / envelope - dry[(size_t) ch]) * wet);
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

    // SIDECHAIN: kanalens ljud sänks av en annan källas (kickens eller ett instruments) nivå. Källans nivå följs med snabb attack och
    // valbar release; en nivå THRESHOLD och 12 dB över ger full effekt (DEPTH).
    struct DuckerFx
    {
        float envelope = 0.0f;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { envelope = 0.0f; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto source = jlimit (0, SourceSignals::count - 1, (int) k[0]);
            if (context.sources == nullptr || context.sources->left[(size_t) source] == nullptr)
                return; // källan spelar inte (instrumentet finns inte): inget att ducka efter
            const auto* keyLeft = context.sources->left[(size_t) source];
            const auto* keyRight = context.sources->right[(size_t) source];

            const auto thresholdDb = k[1], depth = k[2], wet = k[5];
            const auto attack = 1.0f - std::exp (-1.0f / (std::max (0.1f, k[3]) * 0.001f * (float) context.sampleRate));
            const auto release = 1.0f - std::exp (-1.0f / (std::max (1.0f, k[4]) * 0.001f * (float) context.sampleRate));

            for (int i = 0; i < numSamples; ++i)
            {
                auto level = std::abs (keyLeft[i]);
                if (keyRight != nullptr)
                    level = std::max (level, std::abs (keyRight[i]));
                envelope += (level - envelope) * (level > envelope ? attack : release);
                const auto db = 20.0f * std::log10 (envelope + 1.0e-5f);
                const auto amount = jlimit (0.0f, 1.0f, (db - thresholdDb) / 12.0f);
                const auto gain = 1.0f + (-depth * amount) * wet;
                for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
                    buffer.setSample (ch, i, buffer.getSample (ch, i) * gain);
            }
        }
    };

    // BAND DUCKER: som SIDECHAIN, men ljudet delas först i låg och hög del (LR4 vid SPLIT) som duckas olika mycket: låga delen djupt
    // (kickens plats), höga delen lite eller inte alls. Delen summeras tillbaka (LR4 ger flat summa, ändrad bara i fas).
    struct BandDuckerFx
    {
        // [kanal][steg]: två Butterworth-steg i serie = Linkwitz-Riley 4:e ordningen
        std::array<std::array<Biquad, 2>, 2> low, high;
        float envelope = 0.0f, lastSplit = 0.0f;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset()
        {
            for (auto& channel : low) for (auto& f : channel) f.reset();
            for (auto& channel : high) for (auto& f : channel) f.reset();
            envelope = 0.0f;
            lastSplit = 0.0f;
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto source = jlimit (0, SourceSignals::count - 1, (int) k[0]);
            if (context.sources == nullptr || context.sources->left[(size_t) source] == nullptr)
                return; // källan spelar inte: inget att ducka efter
            const auto* keyLeft = context.sources->left[(size_t) source];
            const auto* keyRight = context.sources->right[(size_t) source];

            using Array = dsp::IIR::ArrayCoefficients<float>;
            const auto split = k[1], lowDepth = k[2], highDepth = k[3], thresholdDb = k[4], wet = k[7];
            if (std::abs (split - lastSplit) > 0.01f)
            {
                lastSplit = split;
                const auto lowCoefficients = Array::makeLowPass (context.sampleRate, split, 0.70710678f);
                const auto highCoefficients = Array::makeHighPass (context.sampleRate, split, 0.70710678f);
                for (int ch = 0; ch < 2; ++ch)
                    for (int s = 0; s < 2; ++s)
                    {
                        low[(size_t) ch][(size_t) s].setCoefficients (lowCoefficients);
                        high[(size_t) ch][(size_t) s].setCoefficients (highCoefficients);
                    }
            }
            const auto attack = 1.0f - std::exp (-1.0f / (std::max (0.1f, k[5]) * 0.001f * (float) context.sampleRate));
            const auto release = 1.0f - std::exp (-1.0f / (std::max (1.0f, k[6]) * 0.001f * (float) context.sampleRate));
            const auto channels = std::min (2, buffer.getNumChannels());

            for (int i = 0; i < numSamples; ++i)
            {
                auto level = std::abs (keyLeft[i]);
                if (keyRight != nullptr)
                    level = std::max (level, std::abs (keyRight[i]));
                envelope += (level - envelope) * (level > envelope ? attack : release);
                const auto db = 20.0f * std::log10 (envelope + 1.0e-5f);
                const auto amount = jlimit (0.0f, 1.0f, (db - thresholdDb) / 12.0f);
                const auto lowGain = 1.0f - lowDepth * amount;
                const auto highGain = 1.0f - highDepth * amount;
                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto x = buffer.getSample (ch, i);
                    auto& l = low[(size_t) ch];
                    auto& h = high[(size_t) ch];
                    const auto lowPart = l[1].process (l[0].process (x));
                    const auto highPart = h[1].process (h[0].process (x));
                    const auto ducked = lowPart * lowGain + highPart * highGain;
                    buffer.setSample (ch, i, x + (ducked - x) * wet);
                }
            }
        }
    };

    // DISPERSER: en kedja av all-pass-filter (2:a ordningen, STAGES st) som bara vrider fasen och sprider gruppfördröjningen runt FREQ,
    // så att en kick får "chirp" och sin transient utsmetad utan att tonen ändras. SPREAD sprider filtrens frekvenser (0 = alla på FREQ,
    // 100 % = +-2 oktaver); MIX 100 % är ren fasvridning. Koefficienterna räknas om en gång per block.
    struct DisperserFx
    {
        static constexpr int maxStages = 16;
        std::array<std::array<Biquad, maxStages>, 2> filters;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { for (auto& channel : filters) for (auto& f : channel) f.reset(); }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            using Array = dsp::IIR::ArrayCoefficients<float>;
            const auto stages = 4 + 2 * jlimit (0, 6, (int) k[0]);
            const auto freq = k[1], spread = k[2], q = k[3], wet = k[4];
            const auto top = (float) context.sampleRate * 0.45f;
            for (int s = 0; s < stages; ++s)
            {
                const auto position = stages > 1 ? (float) s / (float) (stages - 1) * 2.0f - 1.0f : 0.0f; // -1 ... +1
                const auto hz = jlimit (20.0f, top, freq * std::pow (2.0f, 2.0f * spread * position));
                const auto coefficients = Array::makeAllPass ((float) context.sampleRate, hz, q);
                for (auto& channel : filters)
                    channel[(size_t) s].setCoefficients (coefficients);
            }
            for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                auto& chain = filters[(size_t) ch];
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto dry = data[i];
                    auto x = dry;
                    for (int s = 0; s < stages; ++s)
                        x = chain[(size_t) s].process (x);
                    data[i] = dry + (x - dry) * wet;
                }
            }
        }
    };

    // WIDENER: bredd på sida/mitt-signalen (0 = mono, 100 % = orört, 200 % = dubbel sida), BASS MONO gör allt under frekvensen mono
    // (så att basen håller ihop), PAN är en balans. Delningen är ett en-pols lågpass; höga delen är resten, så summan är exakt.
    struct WidenerFx
    {
        std::array<OnePole, 2> low;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { low = {}; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            if (buffer.getNumChannels() < 2)
                return;
            const auto width = k[0], pan = k[2], wet = k[3];
            const auto bassMono = k[1] > 21.0f;
            const auto coefficient = onePoleCoefficient (k[1], context.sampleRate);
            const auto gainLeft = pan > 0.0f ? 1.0f - pan : 1.0f, gainRight = pan < 0.0f ? 1.0f + pan : 1.0f;
            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto dryLeft = left[i], dryRight = right[i];
                auto lowLeft = 0.0f, lowRight = 0.0f;
                if (bassMono)
                {
                    lowLeft = low[0].lowpass (dryLeft, coefficient);
                    lowRight = low[1].lowpass (dryRight, coefficient);
                }
                const auto highLeft = dryLeft - lowLeft, highRight = dryRight - lowRight;
                const auto mid = 0.5f * (highLeft + highRight), side = 0.5f * (highLeft - highRight) * width;
                const auto lowMid = 0.5f * (lowLeft + lowRight);
                const auto outLeft = (mid + side + lowMid) * gainLeft, outRight = (mid - side + lowMid) * gainRight;
                left[i] = dryLeft + (outLeft - dryLeft) * wet;
                right[i] = dryRight + (outRight - dryRight) * wet;
            }
        }
    };

    // FLANGER: kort fördröjning (DELAY) som en sinus-LFO svänger med DEPTH, återkopplad (FEEDBACK, negativ ger ihåligt ljud).
    // Höger kanals LFO ligger ett kvarts varv efter vänsters, så bilden blir bred.
    struct FlangerFx
    {
        dsp::DelayLine<float, dsp::DelayLineInterpolationTypes::Linear> line;
        std::array<float, 2> feedbackState {};
        double phase = 0.0;

        void prepare (const dsp::ProcessSpec& spec)
        {
            line.prepare ({ spec.sampleRate, spec.maximumBlockSize, 2 });
            line.setMaximumDelayInSamples ((int) (spec.sampleRate * 0.025) + 4);
            reset();
        }

        void reset() { line.reset(); feedbackState = {}; phase = 0.0; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto rate = k[0], depth = k[1], centre = k[2] * 0.001f * (float) context.sampleRate, feedback = k[3], wet = k[4];
            const auto increment = (double) rate / context.sampleRate;
            const auto channels = std::min (2, buffer.getNumChannels());
            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto lfo = (float) std::sin (MathConstants<double>::twoPi * (phase + 0.25 * ch));
                    const auto delay = jmax (1.0f, centre * (1.0f + depth * 0.9f * lfo));
                    auto* data = buffer.getWritePointer (ch);
                    const auto dry = data[i];
                    line.pushSample (ch, dry + feedbackState[(size_t) ch] * feedback);
                    const auto delayed = line.popSample (ch, delay, true);
                    feedbackState[(size_t) ch] = delayed;
                    data[i] = dry + (delayed - dry) * wet;
                }
                phase += increment;
                if (phase >= 1.0) phase -= 1.0;
            }
        }
    };

    // TRANSIENT: jämför en snabb och en långsam envelopp. Snabb över långsam = anslag (ATTACK höjer/sänker det), tvärtom = släpp
    // (SUSTAIN). Upp till +-12 dB. Avkänningen är länkad mellan kanalerna, så bilden håller.
    struct TransientFx
    {
        float fast = 0.0f, slow = 0.0f, gain = 1.0f;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { fast = slow = 0.0f; gain = 1.0f; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto attack = k[0], sustain = k[1], wet = k[2];
            const auto fastUp = onePoleCoefficient (400.0f, context.sampleRate), fastDown = onePoleCoefficient (25.0f, context.sampleRate);
            const auto slowUp = onePoleCoefficient (15.0f, context.sampleRate), slowDown = onePoleCoefficient (15.0f, context.sampleRate);
            const auto smooth = onePoleCoefficient (200.0f, context.sampleRate);
            const auto channels = std::min (2, buffer.getNumChannels());
            for (int i = 0; i < numSamples; ++i)
            {
                auto level = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    level = jmax (level, std::abs (buffer.getSample (ch, i)));
                fast += (level - fast) * (level > fast ? fastUp : fastDown);
                slow += (level - slow) * (level > slow ? slowUp : slowDown);
                const auto ratio = (fast + 1.0e-5f) / (slow + 1.0e-5f);
                const auto octaves = std::log2 (ratio);
                const auto db = octaves > 0.0f ? attack * 12.0f * jmin (octaves, 2.0f) * 0.5f
                                               : sustain * 12.0f * jmin (-octaves, 2.0f) * 0.5f;
                gain += (Decibels::decibelsToGain (db) - gain) * smooth;
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);
                    data[i] += (data[i] * gain - data[i]) * wet;
                }
            }
        }
    };

    // OCTAVER: oktav ner = delare (vänder tecken vid varannan nollgenomgång av det lågpassade ljudet, fyrkant med ljudets nivå, sedan
    // lågpass). Oktav upp = helvågslikriktning (|x|) utan likspänning. TRANSIENT släpper bara igenom oktaverna på anslag (snabb
    // envelope över långsam). Mono-summan används, och oktaven läggs lika på båda kanalerna.
    struct OctaverFx
    {
        float tone = 0.0f, envelope = 0.0f, previous = 0.0f, flip = 1.0f, divided = 0.0f, downSmooth = 0.0f;
        float upDc = 0.0f, fast = 0.0f, slow = 0.0f, gate = 1.0f;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset() { tone = envelope = previous = divided = downSmooth = upDc = fast = slow = 0.0f; flip = 1.0f; gate = 1.0f; }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            const auto down = k[0], up = k[1], transient = k[3], wet = k[4];
            const auto toneCoefficient = onePoleCoefficient (k[2], context.sampleRate);
            const auto outCoefficient = onePoleCoefficient (k[2] * 0.7f, context.sampleRate);
            const auto envelopeCoefficient = onePoleCoefficient (30.0f, context.sampleRate);
            const auto dcCoefficient = onePoleCoefficient (60.0f, context.sampleRate);
            const auto fastUp = onePoleCoefficient (400.0f, context.sampleRate), fastDown = onePoleCoefficient (25.0f, context.sampleRate);
            const auto slowCoefficient = onePoleCoefficient (15.0f, context.sampleRate);
            const auto gateCoefficient = onePoleCoefficient (150.0f, context.sampleRate);
            const auto channels = std::min (2, buffer.getNumChannels());
            for (int i = 0; i < numSamples; ++i)
            {
                auto mono = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    mono += buffer.getSample (ch, i);
                mono /= (float) jmax (1, channels);

                tone += (mono - tone) * toneCoefficient;
                envelope += (std::abs (tone) - envelope) * envelopeCoefficient;
                if (previous <= 0.0f && tone > 0.0f)
                    flip = -flip; // en nollgenomgång uppåt per period: vartannat tecken ger halva frekvensen
                previous = tone;
                divided = flip * envelope * 1.6f;
                downSmooth += (divided - downSmooth) * outCoefficient;

                const auto rectified = std::abs (mono);
                upDc += (rectified - upDc) * dcCoefficient;
                const auto upSignal = rectified - upDc;

                fast += (rectified - fast) * (rectified > fast ? fastUp : fastDown);
                slow += (rectified - slow) * slowCoefficient;
                const auto target = jlimit (0.0f, 1.0f, (fast / (slow + 1.0e-5f) - 1.0f));
                gate += (target - gate) * gateCoefficient;
                const auto amount = 1.0f - transient + transient * gate;

                const auto octave = (downSmooth * down + upSignal * up) * amount;
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);
                    data[i] += octave * wet;
                }
            }
        }
    };

    // DYNAMIC EQ: ett band (klocka på FREQ) vars förstärkning följer hur högt just det bandet är. Avkännaren är ett bandpass på
    // samma frekvens (länkat mellan kanalerna); över THRESHOLD går bandet mot RANGE dB (minus sänker, plus höjer), full effekt 6 dB över.
    struct DynamicEqFx
    {
        std::array<Biquad, 2> detector, band; // detektorn är mono i praktiken (kanal 0), bandet går per kanal
        float envelope = 0.0f, appliedDb = 0.0f;
        int counter = 0;

        void prepare (const dsp::ProcessSpec&) { reset(); }
        void reset()
        {
            for (auto& f : detector) f.reset();
            for (auto& f : band) { f.reset(); f.setIdentity(); }
            envelope = 0.0f; appliedDb = 0.0f; counter = 0;
        }

        void process (AudioBuffer<float>& buffer, int numSamples, const Values& k, const Context& context)
        {
            using Array = dsp::IIR::ArrayCoefficients<float>;
            const auto sampleRate = context.sampleRate;
            const auto freq = jmin (k[0], (float) (sampleRate * 0.45)), q = k[1], threshold = k[2], range = k[3], wet = k[6];
            const auto attack = 1.0f - std::exp (-1.0f / (float) (jmax (0.1f, k[4]) * 0.001 * sampleRate));
            const auto release = 1.0f - std::exp (-1.0f / (float) (jmax (1.0f, k[5]) * 0.001 * sampleRate));
            const auto channels = std::min (2, buffer.getNumChannels());
            const auto detectorCoefficients = Array::makeBandPass (sampleRate, freq, q);
            for (auto& f : detector) f.setCoefficients (detectorCoefficients);
            for (int i = 0; i < numSamples; ++i)
            {
                auto level = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    level = jmax (level, std::abs (detector[(size_t) ch].process (buffer.getSample (ch, i))));
                envelope += (level - envelope) * (level > envelope ? attack : release);
                const auto over = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (envelope + 1.0e-6f) - threshold) / 6.0f);
                const auto targetDb = range * over;
                if (++counter >= 16) // koefficienterna räknas om var 16:e sample, bara om förstärkningen ändrats märkbart
                {
                    counter = 0;
                    if (std::abs (targetDb - appliedDb) > 0.02f)
                    {
                        appliedDb = targetDb;
                        const auto coefficients = Array::makePeakFilter (sampleRate, freq, q, Decibels::decibelsToGain (appliedDb));
                        for (auto& f : band) f.setCoefficients (coefficients);
                    }
                }
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto* data = buffer.getWritePointer (ch);
                    const auto filtered = band[(size_t) ch].process (data[i]);
                    data[i] += (filtered - data[i]) * wet;
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
        std::array<float, knobsPerSlot> lastNormalised {}; // värdena ovan räknades från dessa (NaN = inte räknat än)
        EffectType valuesType = EffectType::off;

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
        MaximizerFx maximizer;
        DuckerFx ducker;
        DisperserFx disperser;
        WidenerFx widener;
        FlangerFx flanger;
        TransientFx transient;
        DynamicEqFx dynamicEq;
        OctaverFx octaver;
        BandDuckerFx bandDucker;
        ShimmerFx shimmer;
        LowMultiplierFx lowMultiplier;

        template <typename Function>
        void forEachEffect (Function&& function)
        {
            function (filter); function (distortion); function (eq); function (chorus); function (phaser);
            function (echo); function (reverb); function (compressor); function (gate); function (comb);
            function (ringMod); function (bitcrusher); function (autoFilter); function (maximizer); function (ducker); function (disperser);
            function (widener); function (flanger); function (transient); function (dynamicEq); function (octaver); function (bandDucker); function (shimmer); function (lowMultiplier);
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
                case EffectType::maximizer:  maximizer.reset(); break;
                case EffectType::ducker:     ducker.reset(); break;
                case EffectType::disperser:  disperser.reset(); break;
                case EffectType::widener:    widener.reset(); break;
                case EffectType::flanger:    flanger.reset(); break;
                case EffectType::transient:  transient.reset(); break;
                case EffectType::dynamicEq:  dynamicEq.reset(); break;
                case EffectType::octaver:    octaver.reset(); break;
                case EffectType::bandDucker: bandDucker.reset(); break;
                case EffectType::shimmer:    shimmer.reset(); break;
                case EffectType::lowMultiplier: lowMultiplier.reset(); break;
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
                case EffectType::maximizer:  maximizer.process (buffer, numSamples, values, context); break;
                case EffectType::ducker:     ducker.process (buffer, numSamples, values, context); break;
                case EffectType::disperser:  disperser.process (buffer, numSamples, values, context); break;
                case EffectType::widener:    widener.process (buffer, numSamples, values, context); break;
                case EffectType::flanger:    flanger.process (buffer, numSamples, values, context); break;
                case EffectType::transient:  transient.process (buffer, numSamples, values, context); break;
                case EffectType::dynamicEq:  dynamicEq.process (buffer, numSamples, values, context); break;
                case EffectType::octaver:    octaver.process (buffer, numSamples, values, context); break;
                case EffectType::bandDucker: bandDucker.process (buffer, numSamples, values, context); break;
                case EffectType::shimmer:    shimmer.process (buffer, numSamples, values, context); break;
                case EffectType::lowMultiplier: lowMultiplier.process (buffer, numSamples, values, context); break;
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

void EffectChain::setSources (const SourceSignals* sources)
{
    impl->context.sources = sources;
}

void EffectChain::update (double bpm, double ppq, int controlPhase)
{
    impl->context.bpm = bpm;
    impl->context.ppq = ppq;
    impl->context.controlPhase = controlPhase;

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
        if (type != slot.valuesType)
        {
            slot.valuesType = type;
            slot.lastNormalised.fill (std::numeric_limits<float>::quiet_NaN());
        }
        for (size_t k = 0; k < list.size() && k < (size_t) knobsPerSlot; ++k)
        {
            auto normalised = slot.knobs[k]->load();
            if (impl->modulator != nullptr)
                normalised = impl->modulator->applyRaw (slot.knobs[k], normalised); // LFO:er dragna till ratten
            if (normalised != slot.lastNormalised[k]) // (samma tal som förut: omräkningen sparas bara när inget ändrats)
            {
                slot.lastNormalised[k] = normalised;
                slot.values[k] = list[k].fromNormalised (normalised);
            }
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
        context.controlPhase = (context.controlPhase + start) % Modulation::controlInterval;

        for (auto& slot : impl->slots)
            if (slot.isActive())
                slot.process (part, length, context);
    }
}

//==============================================================================
namespace
{
    // Kanalens EQ och kompressor (alltid med, neutrala som standard). Allt skapas i prepare.
    struct ChannelStrip
    {
        std::array<std::array<Biquad, 2>, Effects::eqBands + 2> filters; // räknar som dsp::IIR::Filter, bit för bit
        Effects::EqSettings last;
        bool coefficientsValid = false;
        float env = 0.0f, gainState = 1.0f;
        double sampleRate = 44100.0;

        void prepare (double rate)
        {
            sampleRate = rate;
            for (auto& pair : filters)
                for (auto& filter : pair)
                {
                    filter.setIdentity();
                    filter.reset();
                }
            coefficientsValid = false;
            env = 0.0f;
            gainState = 1.0f;
        }

        void updateEq (const Effects::EqSettings& s)
        {
            const auto limit = (float) sampleRate * 0.45f;
            using Array = dsp::IIR::ArrayCoefficients<float>;
            auto differs = [this, &s] { return std::memcmp (&last, &s, sizeof (s)) != 0; };
            if (coefficientsValid && ! differs())
                return;

            for (auto& f : filters[0]) f.setCoefficients (Array::makeHighPass (sampleRate, s.hp <= 21.0f ? 5.0f : jmin (s.hp, limit), 0.7071f));
            for (auto& f : filters[1]) f.setCoefficients (Array::makeLowShelf (sampleRate, jmin (s.freq[0], limit), 0.7f, Decibels::decibelsToGain (s.gain[0])));
            for (auto& f : filters[2]) f.setCoefficients (Array::makePeakFilter (sampleRate, jmin (s.freq[1], limit), s.q[1], Decibels::decibelsToGain (s.gain[1])));
            for (auto& f : filters[3]) f.setCoefficients (Array::makePeakFilter (sampleRate, jmin (s.freq[2], limit), s.q[2], Decibels::decibelsToGain (s.gain[2])));
            for (auto& f : filters[4]) f.setCoefficients (Array::makeHighShelf (sampleRate, jmin (s.freq[3], limit), 0.7f, Decibels::decibelsToGain (s.gain[3])));
            for (auto& f : filters[5]) f.setCoefficients (Array::makeLowPass (sampleRate, s.lp >= 19500.0f ? limit : jmin (s.lp, limit), 0.7071f));
            last = s;
            coefficientsValid = true;
        }

        void processEq (AudioBuffer<float>& buffer, int numSamples)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    auto x = data[i];
                    for (auto& pair : filters)
                        x = pair[(size_t) ch].process (x);
                    data[i] = x;
                }
            }
        }

        // Kompressor med länkade kanaler. gainOut får förstärkningen (utan make-up) per sample.
        void processComp (AudioBuffer<float>& buffer, int numSamples, float threshold, float ratio, float attackMs, float releaseMs,
                          float makeupDb, SignalTap& gainTap)
        {
            const auto attack = 1.0f - std::exp (-1.0f / (std::max (0.05f, attackMs) * 0.001f * (float) sampleRate));
            const auto release = 1.0f - std::exp (-1.0f / (std::max (1.0f, releaseMs) * 0.001f * (float) sampleRate));
            const auto makeup = Decibels::decibelsToGain (makeupDb);
            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                const auto level = std::max (std::abs (left[i]), std::abs (right[i]));
                env += (level - env) * (level > env ? attack : release);
                const auto db = Decibels::gainToDecibels (env, -100.0f);
                const auto over = db - threshold;
                const auto reduction = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                const auto target = Decibels::decibelsToGain (-reduction);
                gainState += (target - gainState) * 0.3f; // jämnar ut hoppen i förstärkningen
                left[i] *= gainState * makeup;
                right[i] *= gainState * makeup;
                gainTap.push (gainState);
            }
        }
    };
}

struct EffectRack::Impl
{
    explicit Impl (AudioProcessorValueTreeState& apvts)
    {
        for (int ch = 0; ch < numEffectChannels; ++ch)
        {
            const char* keys[] { "CompThr", "CompRatio", "CompAtt", "CompRel", "CompMakeup" };
            for (int k = 0; k < 5; ++k)
                comp[(size_t) ch][(size_t) k] = apvts.getRawParameterValue (Effects::channelParamID (ch, keys[k]));
            chains[(size_t) ch] = std::make_unique<EffectChain> (apvts, ch);
            chains[(size_t) ch]->setSources (&sources);
            level[(size_t) ch] = apvts.getRawParameterValue (Effects::channelParamID (ch, "Level"));
            duck[(size_t) ch] = apvts.getRawParameterValue (Effects::channelParamID (ch, "Duck"));
            eqRefs.emplace_back (apvts, ch);
        }
    }

    SourceSignals sources; // kickens och instrumentens ljud i det här blocket (för SIDECHAIN-effekterna)
    std::array<std::unique_ptr<EffectChain>, numEffectChannels> chains;
    std::array<ChannelStrip, numEffectChannels> strips;
    std::vector<Effects::EqRefs> eqRefs; // en per kanal (skapas i konstruktorn)
    std::array<std::array<std::atomic<float>*, 5>, numEffectChannels> comp {}; // thr, ratio, att, rel, makeup
    std::array<std::atomic<float>*, numEffectChannels> level {}, duck {};
    std::array<AudioBuffer<float>, numEffectChannels> buffers;
    std::array<float, numEffectChannels> gains {}, duckAmounts {};
    std::array<bool, numEffectChannels> active {};
    std::array<bool, numEffectChannels> sent {};    // fick kanalen ljud från någon källa i det här blocket?
    std::array<bool, numEffectChannels> idle {};    // kanalen hade inget att göra i blocket (inget skickat, ingen effekt): hoppades över
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
        impl->strips[(size_t) ch].prepare (sampleRate);
        impl->duckState[(size_t) ch] = 1.0f;
    }
    impl->duckCoefficient = (float) (1.0 - std::exp (-1.0 / (0.001 * sampleRate)));
}

void EffectRack::reset()
{
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        impl->chains[(size_t) ch]->reset();
        auto& strip = impl->strips[(size_t) ch];
        for (auto& pair : strip.filters)
            for (auto& filter : pair)
                filter.reset();
        strip.env = 0.0f;
        strip.gainState = 1.0f;
        impl->duckState[(size_t) ch] = 1.0f;
        impl->buffers[(size_t) ch].clear();
    }
}

void EffectRack::setSources (const SourceSignals& sources)
{
    impl->sources = sources;
}

void EffectRack::setModulator (const Modulator* modulator)
{
    impl->modulator = modulator;
    for (auto& chain : impl->chains)
        chain->setModulator (modulator);
}

void EffectRack::beginBlock (int numSamples, double bpm, double ppq, int controlPhase)
{
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        auto& buffer = impl->buffers[(size_t) ch];
        if (buffer.getNumSamples() < numSamples)
            buffer.setSize (2, numSamples, false, true, true); // bara om DAW:en skickar ett större block än utlovat
        buffer.clear (0, numSamples);

        auto& chain = *impl->chains[(size_t) ch];
        chain.update (bpm, ppq, controlPhase);
        impl->active[(size_t) ch] = true; // en neutral kanal släpper igenom ljudet orört
        impl->sent[(size_t) ch] = false;

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
    impl->sent[(size_t) channel] = true;
    auto& buffer = impl->buffers[(size_t) channel];
    buffer.addFrom (0, 0, left, numSamples, amount);
    buffer.addFrom (1, 0, right != nullptr ? right : left, numSamples, amount);
}

bool EffectRack::isIdle (int channel) const
{
    return jlimit (0, numEffectChannels - 1, channel) == channel && impl->idle[(size_t) channel];
}

const AudioBuffer<float>& EffectRack::output (int channel) const
{
    return impl->buffers[(size_t) jlimit (0, numEffectChannels - 1, channel)];
}

void EffectRack::process (int numSamples, const float* kickEnvelope, AudioBuffer<float>* mix, const bool* duckEnabled)
{
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        auto& buffer = impl->buffers[(size_t) ch];

        // En kanal som ingen skickat något till och som inte har någon effekt (som kan ha en svans) har inget att göra: hoppa över den.
        impl->idle[(size_t) ch] = ! impl->sent[(size_t) ch] && ! impl->chains[(size_t) ch]->hasActive();
        if (impl->idle[(size_t) ch])
            continue;

        impl->chains[(size_t) ch]->process (buffer, numSamples);

        // EQ och kompressor (alltid med, neutrala som standard). Taparna visar vad som når dem.
        auto& strip = impl->strips[(size_t) ch];
        const auto eq = impl->eqRefs[(size_t) ch].read();
        for (int i = 0; i < numSamples; ++i)
            eqInputTap[ch].push (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        if (! eq.isNeutral())
        {
            strip.updateEq (eq);
            strip.processEq (buffer, numSamples);
        }
        for (int i = 0; i < numSamples; ++i)
            compInputTap[ch].push (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        const auto& comp = impl->comp[(size_t) ch];
        const auto ratio = comp[1]->load(), makeup = comp[4]->load();
        if (ratio > 1.01f || makeup > 0.01f)
            strip.processComp (buffer, numSamples, comp[0]->load(), std::max (1.0f, ratio), comp[2]->load(), comp[3]->load(), makeup, compGainTap[ch]);
        else
            for (int i = 0; i < numSamples; ++i)
                compGainTap[ch].push (1.0f);
        for (int i = 0; i < numSamples; ++i)
            compOutputTap[ch].push (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));

        // Kanalens duckning följer kickens envelope (efter effekterna, så att svansar duckas också).
        auto& state = impl->duckState[(size_t) ch];
        const auto amount = duckEnabled == nullptr || duckEnabled[ch] ? impl->duckAmounts[(size_t) ch] : 0.0f;
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

        if (mix != nullptr)
        {
            mix->addFrom (0, 0, buffer, 0, 0, numSamples);
            mix->addFrom (1, 0, buffer, 1, 0, numSamples);
        }
    }
}
