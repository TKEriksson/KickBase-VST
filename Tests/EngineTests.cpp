// Tester för ljudmotorerna och mönstret.

#include "../Source/Core/BassSynth.h"
#include "../Source/Core/Ducker.h"
#include "../Source/Core/KickEngine.h"
#include "../Source/Core/Params.h"
#include "../Source/Core/Pattern.h"
#include "../Source/Core/StepGate.h"
#include "TestUtils.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr double rate = 48000.0;

    // Uppskattar frekvensen ur antal nollgenomgångar uppåt i [from, to).
    double estimateFrequency (const std::vector<float>& audio, size_t from, size_t to)
    {
        int crossings = 0;
        size_t first = 0, last = 0;
        for (size_t i = from + 1; i < to; ++i)
        {
            if (audio[i - 1] < 0.0f && audio[i] >= 0.0f)
            {
                if (crossings == 0)
                    first = i;
                last = i;
                ++crossings;
            }
        }
        return crossings > 1 ? (crossings - 1) * rate / (double) (last - first) : 0.0;
    }

    bool allFinite (const std::vector<float>& audio)
    {
        for (auto s : audio)
            if (! std::isfinite (s))
                return false;
        return true;
    }

    float peakOf (const std::vector<float>& audio, size_t from, size_t to)
    {
        auto peak = 0.0f;
        for (size_t i = from; i < to && i < audio.size(); ++i)
            peak = std::max (peak, std::abs (audio[i]));
        return peak;
    }

    void testKickLandsOnKey()
    {
        std::puts ("Kick: svansen landar på tonarten och klingar ut");
        KickEngine kick;
        kick.prepare (rate);

        KickParams p;
        p.tailHz = 46.25f; // F#1
        p.clickLevel = 0.0f;
        p.drive = 0.0f;
        p.decayMs = 400.0f;
        p.decayCurve = 1.0f;
        kick.setParams (p);
        kick.trigger (1.0f);

        std::vector<float> audio ((size_t) (rate * 0.6));
        for (auto& s : audio)
            s = kick.nextSample();

        check (allFinite (audio), "inga NaN");
        check (peakOf (audio, 0, audio.size()) <= 1.0f, "toppvärde högst 1", peakOf (audio, 0, audio.size()));

        // Efter svepet (45 ms) ska frekvensen ligga på tonarten.
        const auto tailFrequency = estimateFrequency (audio, (size_t) (rate * 0.1), (size_t) (rate * 0.35));
        check (std::abs (tailFrequency - 46.25) < 1.5, "svansens frekvens", tailFrequency);

        // I början sveper den högt: över 200 Hz de första millisekunderna.
        const auto startFrequency = estimateFrequency (audio, 0, (size_t) (rate * 0.01));
        check (startFrequency > 200.0, "svepet börjar högt", startFrequency);

        // Längd = attack + hold + decay = 430 ms. Efter det är det tyst.
        check (peakOf (audio, (size_t) (rate * 0.44), audio.size()) == 0.0f, "tyst efter kicken");
        check (kick.getEnvelope() == 0.0f, "envelopen är noll efteråt", kick.getEnvelope());
    }

    void testKickRetriggerFades()
    {
        std::puts ("Kick: ny kick tonar ut den gamla utan hopp");
        KickEngine kick;
        kick.prepare (rate);
        KickParams p;
        p.clickLevel = 0.0f;
        kick.setParams (p);

        kick.trigger (1.0f);
        std::vector<float> audio ((size_t) (rate * 0.2));
        for (size_t i = 0; i < audio.size(); ++i)
        {
            if (i == (size_t) (rate * 0.1))
                kick.trigger (1.0f);
            audio[i] = kick.nextSample();
        }

        // Största hoppet mellan två samples ska vara litet (inget knäpp).
        auto maxJump = 0.0f;
        for (size_t i = 1; i < audio.size(); ++i)
            maxJump = std::max (maxJump, std::abs (audio[i] - audio[i - 1]));
        check (maxJump < 0.2f, "största hopp mellan samples", maxJump);
    }

    void testBassPitchAndRelease()
    {
        std::puts ("Bas: rätt ton och tystnar efter gate + release");
        BassSynth bass;
        bass.prepare (rate);

        BassParams p;
        p.rootHz = 65.41f; // C2
        p.shape = 0.0f; // sinus
        p.sub = 0.0f;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.releaseMs = 10.0f;
        bass.setParams (p);

        bass.noteOn (7, 1.0f, (int) (rate * 0.2), false); // en kvint upp: G2 = 98 Hz
        std::vector<float> audio ((size_t) (rate * 0.3));
        for (auto& s : audio)
            s = bass.nextSample();

        check (allFinite (audio), "inga NaN");
        const auto frequency = estimateFrequency (audio, (size_t) (rate * 0.02), (size_t) (rate * 0.19));
        check (std::abs (frequency - 98.0) < 1.0, "tonens frekvens", frequency);
        check (! bass.isActive(), "tyst efter release");
    }

    void testBassSlide()
    {
        std::puts ("Bas: slide glider till nästa ton utan nytt anslag");
        BassSynth bass;
        bass.prepare (rate);

        BassParams p;
        p.rootHz = 55.0f;
        p.shape = 0.0f; // sinus
        p.sub = 0.0f;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.glideMs = 20.0f;
        p.sustain = 1.0f;
        bass.setParams (p);

        const auto stepSamples = (int) (rate * 0.1);
        bass.noteOn (0, 1.0f, (int) (stepSamples * 1.05), true);

        std::vector<float> audio ((size_t) stepSamples * 2);
        for (size_t i = 0; i < audio.size(); ++i)
        {
            if (i == (size_t) stepSamples)
            {
                check (bass.isActive(), "första tonen klingar när nästa steg kommer");
                bass.noteOn (12, 1.0f, stepSamples / 2, false); // en oktav upp
            }
            audio[i] = bass.nextSample();
        }

        const auto before = estimateFrequency (audio, (size_t) (stepSamples * 0.3), (size_t) (stepSamples * 0.95));
        const auto after = estimateFrequency (audio, (size_t) (stepSamples * 1.3), (size_t) (stepSamples * 1.45));
        check (std::abs (before - 55.0) < 1.0, "frekvens före glid", before);
        check (std::abs (after - 110.0) < 2.0, "frekvens efter glid", after);

        // Legato: ingen ny attack, så nivån får inte dyka vid övergången.
        check (peakOf (audio, (size_t) stepSamples, (size_t) stepSamples + 2000) > 0.5f, "ingen dipp vid glidet");
    }

    void testBassRetriggerWithoutClick()
    {
        std::puts ("Bas: nytt slag medan det förra klingar ger inget hopp");
        BassSynth bass;
        bass.prepare (rate);
        BassParams p;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.sub = 0.0f;
        p.shape = 0.0f; // sinus
        bass.setParams (p);

        // Gate längre än steget: andra slaget kommer medan det första fortfarande hörs.
        const auto step = (int) (rate * 0.05);
        auto previous = 0.0f, maxJump = 0.0f;
        for (int i = 0; i < step * 3; ++i)
        {
            if (i % step == 0)
                bass.noteOn (i == step ? 5 : 0, 1.0f, step * 2, false);
            const auto s = bass.nextSample();
            if (i > 0)
                maxJump = std::max (maxJump, std::abs (s - previous));
            previous = s;
        }
        // En sinus i 65-90 Hz ändras högst ~0.012 per sample; ett fashopp vore mycket större.
        check (maxJump < 0.05f, "största hopp mellan samples", maxJump);
    }

    void testDuckerIsSmooth()
    {
        std::puts ("Ducking: glider ner på ungefär en millisekund");
        Ducker ducker;
        ducker.prepare (rate);

        const auto first = ducker.next (1.0f, 0.8f);
        check (first > 0.95f, "inget hopp på första samplet", first);

        auto gain = first;
        for (int i = 0; i < (int) (rate * 0.01); ++i)
            gain = ducker.next (1.0f, 0.8f);
        check (std::abs (gain - 0.2f) < 0.01f, "når 1 - amount", gain);
    }

    void testGate()
    {
        std::puts ("Gate: öppnar, stänger och släpper igenom när DAW:en står");
        StepGate gate;
        gate.prepare (rate);
        gate.setTimes (1.0f, 5.0f);
        gate.open (0.8f, 4800);

        float level = 0.0f;
        for (int i = 0; i < 2400; ++i)
            level = gate.next (false);
        check (std::abs (level - 0.8f) < 1.0e-4f, "öppen nivå = velocity", level);

        for (int i = 0; i < 4800; ++i)
            level = gate.next (false);
        check (level == 0.0f, "stängd efter gate + release", level);

        for (int i = 0; i < 480; ++i)
            level = gate.next (true);
        check (level == 1.0f, "helt öppen när den släpper igenom", level);
    }

    void testPatternTemplatesAndSaving()
    {
        std::puts ("Mönster: mallar och spara/läsa");
        Pattern pattern;

        check (pattern.applyTemplate (PatternTemplate::rolling) == BassGrid::sixteenth, "Rolling är i sextondelar");
        check (pattern.isKickOn (0) && ! pattern.isKickOn (1) && pattern.isKickOn (4), "kick på fjärdedelar");
        check (! pattern.bassStep (0, BassGrid::sixteenth, 0).on && pattern.bassStep (0, BassGrid::sixteenth, 1).on
                   && pattern.bassStep (0, BassGrid::sixteenth, 3).on && ! pattern.bassStep (0, BassGrid::sixteenth, 4).on,
               "Rolling: K B B B");

        check (pattern.applyTemplate (PatternTemplate::triplet) == BassGrid::eighthTriplet, "Triplet är i åttondelstrioler");
        check (! pattern.bassStep (0, BassGrid::eighthTriplet, 3).on && pattern.bassStep (0, BassGrid::eighthTriplet, 4).on,
               "Triplet: K B B");

        // Ändra några steg och kontrollera att allt överlever spara/läsa.
        pattern.kickStep (7) = true;
        auto& step = pattern.bassStep (0, BassGrid::sixteenthTriplet, 50);
        step.on = true;
        step.pitch = -12;
        step.slide = true;
        step.gate = 0.33f;
        step.velocity = 0.25f;

        Pattern loaded;
        loaded.applyTemplate (PatternTemplate::clear);
        loaded.fromValueTree (pattern.toValueTree());

        check (loaded.isKickOn (7) && loaded.isKickOn (0) && ! loaded.isKickOn (1), "kickstegen sparas");
        const auto& loadedStep = loaded.bassStep (0, BassGrid::sixteenthTriplet, 50);
        check (loadedStep.on && loadedStep.pitch == -12 && loadedStep.slide, "basstegets på/ton/slide sparas");
        check (std::abs (loadedStep.gate - 0.33f) < 1.0e-4f && std::abs (loadedStep.velocity - 0.25f) < 1.0e-4f,
               "basstegets gate/velocity sparas");
        check (loaded.bassStep (0, BassGrid::eighthTriplet, 4).on, "andra rutnät sparas också");

        // Version 1 hade rutnäten i ordningen 1/16, 1/8 T, 1/16 T.
        juce::ValueTree old (Pattern::treeType);
        juce::ValueTree oldBass ("Bass");
        oldBass.setProperty ("grid", 0, nullptr);
        oldBass.setProperty ("on", "0,1,0,1", nullptr);
        old.appendChild (oldBass, nullptr);

        Pattern migrated;
        migrated.applyTemplate (PatternTemplate::clear);
        migrated.fromValueTree (old);
        check (migrated.bassStep (0, BassGrid::sixteenth, 1).on && migrated.bassStep (0, BassGrid::sixteenth, 3).on
                   && ! migrated.bassStep (0, BassGrid::quarter, 1).on,
               "gammalt 1/16-mönster hamnar i 1/16");
    }
}

namespace
{
    void testKeyMapping()
    {
        std::puts ("Tonart: basen följer tonens oktav, kicken håller sig i sitt register");
        check (Params::bassRootMidiNote (60) == 36, "FL:s C5 ger C2 i basen", Params::bassRootMidiNote (60));
        check (Params::bassRootMidiNote (59) < Params::bassRootMidiNote (60), "en ton nedåt går nedåt i basen");
        check (Params::bassRootMidiNote (72) == 48, "C6 ger C3", Params::bassRootMidiNote (72));

        for (int pc = 0; pc < 12; ++pc)
        {
            const auto note = Params::kickTailMidiNote (pc);
            check (note >= 22 && note <= 33 && note % 12 == (pc + 12) % 12, "kickens svans i A#0..A1 med rätt tonnamn", note);
        }
    }
}

void runEngineTests()
{
    testKeyMapping();
    testKickLandsOnKey();
    testKickRetriggerFades();
    testBassPitchAndRelease();
    testBassSlide();
    testBassRetriggerWithoutClick();
    testDuckerIsSmooth();
    testGate();
    testPatternTemplatesAndSaving();
}
