// Tester för ljudmotorerna och mönstret.

#include "../Source/Core/InstrumentSynth.h"
#include "../Source/Core/Ducker.h"
#include "../Source/Core/KickEngine.h"
#include "../Source/Core/Modulation.h"
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

    ClickSample makeSine (double hz, double seconds)
    {
        ClickSample sample;
        sample.audio.setSize (1, (int) (seconds * rate));
        for (int i = 0; i < sample.audio.getNumSamples(); ++i)
            sample.audio.setSample (0, i, 0.8f * (float) std::sin (2.0 * 3.14159265358979 * hz * i / rate));
        return sample;
    }

    void testKickShapesAndSamples()
    {
        std::puts("Kick: nya former, transponering och sampling som hel kick");

        // Alla former och drive-lägen ger ändliga, hyfsat begränsade signaler.
        for (int wave = 0; wave < 4; ++wave)
            for (int mode = 0; mode < 4; ++mode)
            {
                KickEngine kick;
                kick.prepare (rate);
                KickParams p;
                p.wave = wave;
                p.clipMode = mode;
                p.sub = 0.5f;
                p.fm = 0.6f;
                p.noise = 0.5f;
                p.crush = 0.4f;
                p.lowCutHz = 40.0f;
                p.velPitch = 0.5f;
                p.drive = 0.6f;
                kick.setParams (p);
                kick.trigger (0.7f);
                std::vector<float> audio ((size_t) (rate * 0.4));
                for (auto& v : audio)
                    v = kick.nextSample();
                check (allFinite (audio), "alla former och lägen är ändliga", wave * 10 + mode);
                check (peakOf (audio, 0, audio.size()) < 4.0f, "alla former och lägen är begränsade", peakOf (audio, 0, audio.size()));
            }

        // En kicknot med tonhöjd flyttar svansen: +12 halvtoner = dubbla frekvensen.
        {
            KickEngine kick;
            kick.prepare (rate);
            KickParams p;
            p.tailHz = 46.25f;
            p.clickLevel = 0.0f;
            p.drive = 0.0f;
            p.decayMs = 400.0f;
            p.decayCurve = 1.0f;
            kick.setParams (p);
            kick.trigger (1.0f, 12);
            std::vector<float> audio ((size_t) (rate * 0.6));
            for (auto& v : audio)
                v = kick.nextSample();
            const auto tail = estimateFrequency (audio, (size_t) (rate * 0.1), (size_t) (rate * 0.3));
            check (std::abs (tail - 92.5) < 3.0, "kicknotens tonhöjd flyttar svansen", tail);
        }

        // Sampling som hel kick (ersätter): samplingens egen ton, och tyst efter den.
        {
            const auto sample = makeSine (440.0, 0.1);
            KickEngine kick;
            kick.prepare (rate);
            KickParams p;
            p.sampleMode = 2;
            p.sampleLevel = 1.0f;
            p.drive = 0.0f;
            kick.setParams (p);
            kick.setClickSample (&sample);
            kick.trigger (1.0f);
            std::vector<float> audio ((size_t) (rate * 0.3));
            for (auto& v : audio)
                v = kick.nextSample();
            const auto hz = estimateFrequency (audio, (size_t) (rate * 0.01), (size_t) (rate * 0.08));
            check (std::abs (hz - 440.0) < 12.0, "ersätt: samplingens egen ton", hz);
            check (peakOf (audio, (size_t) (rate * 0.12), audio.size()) < 0.001f, "ersätt: tyst efter samplingen", peakOf (audio, (size_t) (rate * 0.12), audio.size()));
            check (peakOf (audio, 0, (size_t) (rate * 0.1)) > 0.3f, "ersätt: samplingen hörs");

            // Lager: samplingen är längre än syntesen och spelas ut.
            p.sampleMode = 1;
            p.decayMs = 20.0f;
            p.holdMs = 5.0f;
            kick.setParams (p);
            kick.reset();
            kick.trigger (1.0f);
            for (auto& v : audio)
                v = kick.nextSample();
            check (peakOf (audio, (size_t) (rate * 0.06), (size_t) (rate * 0.09)) > 0.2f, "lager: samplingen klingar efter syntesen");
        }
    }

    void testInstrumentSample()
    {
        std::puts("Bas: sampling som ljudkälla (egen ton och följer noten)");
        const auto sample = makeSine (100.0, 0.6);
        InstrumentSynth bass;
        bass.prepare (rate);

        InstrumentParams p;
        p.rootHz = 200.0f;
        p.oscA.level = 0.0f;
        p.oscB.level = 0.0f;
        p.sub = 0.0f;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.sustain = 1.0f;
        p.sampleData = &sample;
        p.sampleLevel = 1.0f;
        p.sampleOn = true;
        p.sampleRootHz = 100.0f;

        auto measure = [&] (bool keyTrack)
        {
            p.sampleKeyTrack = keyTrack;
            bass.setParams (p);
            bass.reset();
            bass.noteOn (0, 1.0f, (int) rate);
            std::vector<float> audio ((size_t) (rate * 0.4));
            for (auto& v : audio)
                v = bass.nextSample();
            return audio;
        };

        const auto own = measure (false);
        check (std::abs (estimateFrequency (own, (size_t) (rate * 0.05), (size_t) (rate * 0.3)) - 100.0) < 4.0, "utan KEY spelas samplingen med egen ton",
               estimateFrequency (own, (size_t) (rate * 0.05), (size_t) (rate * 0.3)));
        const auto followed = measure (true);
        check (std::abs (estimateFrequency (followed, (size_t) (rate * 0.05), (size_t) (rate * 0.3)) - 200.0) < 6.0, "med KEY följer samplingen noten",
               estimateFrequency (followed, (size_t) (rate * 0.05), (size_t) (rate * 0.3)));

        p.sampleLevel = 0.0f;
        const auto silent = measure (false);
        check (peakOf (silent, 0, silent.size()) < 1.0e-4f, "nivå 0 ger tyst bas utan oscillatorer");
    }

    void testInstrumentPitchAndRelease()
    {
        std::puts ("Bas: rätt ton och tystnar efter gate + release");
        InstrumentSynth bass;
        bass.prepare (rate);

        InstrumentParams p;
        p.rootHz = 65.41f; // C2
        p.oscA.position = 0.0f; // sinus (Basic-tabellens början)
        p.sub = 0.0f;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.releaseMs = 10.0f;
        bass.setParams (p);

        bass.noteOn (7, 1.0f, (int) (rate * 0.2)); // en kvint upp: G2 = 98 Hz
        std::vector<float> audio ((size_t) (rate * 0.3));
        for (auto& s : audio)
            s = bass.nextSample();

        check (allFinite (audio), "inga NaN");
        const auto frequency = estimateFrequency (audio, (size_t) (rate * 0.02), (size_t) (rate * 0.19));
        check (std::abs (frequency - 98.0) < 1.0, "tonens frekvens", frequency);
        check (! bass.isActive(), "tyst efter release");
    }

    void testInstrumentModulationSources()
    {
        std::puts("Bas: ENV 2, tonhöjdsenvelope och källor");
        InstrumentSynth synth;
        synth.setSeed (1);
        synth.prepare (rate);
        InstrumentParams p;
        p.oscA.level = 0.0f;
        p.oscB.on = false;
        p.sub = 1.0f;
        p.drive = 0.0f;
        p.cutoffHz = 20000.0f;
        p.pitchEnvSemis = 12.0f;
        p.pitchEnvDecayMs = 100.0f;
        p.env2 = { 1.0f, 200.0f, 0.5f, 50.0f };
        synth.setParams (p);

        std::vector<float> audio;
        synth.noteOn (0, 0.8f, (int) (rate * 0.5));
        float peak = 0.0f, velocity = 0.0f, random = 0.0f, afterRelease = 1.0f;
        for (int i = 0; i < (int) (rate * 0.7); ++i)
        {
            const auto out = synth.next();
            audio.push_back (out.left * out.amplitude);
            peak = std::max (peak, synth.getModulationValue (0));
            if (i == 1000)
            {
                velocity = synth.getModulationValue (2);
                random = synth.getModulationValue (3);
            }
            if (i == (int) (rate * 0.65))
                afterRelease = synth.getModulationValue (0);
        }
        check (peak > 0.95f, "ENV 2 når toppen efter attacken", peak);
        check (afterRelease < 0.01f, "ENV 2 har släppt efter gaten", afterRelease);
        check (std::abs (velocity - 0.8f) < 1.0e-5f, "VELOCITY är tonens velocity", velocity);
        check (random > 0.0f && random < 1.0f, "RANDOM är ett slumptal per ton", random);

        const auto early = estimateFrequency (audio, 0, (size_t) (rate * 0.03));
        const auto late = estimateFrequency (audio, (size_t) (rate * 0.35), (size_t) (rate * 0.5));
        check (early > late * 1.15 && std::abs (late - p.rootHz) < p.rootHz * 0.06, "tonhöjdsenvelopen börjar högt och faller till tonen", early);
    }

    void testInstrumentGlide()
    {
        std::puts ("Bas: en glide-ton glider den klingande tonen utan nytt anslag och blir kvar");
        InstrumentSynth bass;
        bass.prepare (rate);

        InstrumentParams p;
        p.rootHz = 55.0f;
        p.oscA.position = 0.0f; // sinus (Basic-tabellens början)
        p.sub = 0.0f;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.sustain = 1.0f;
        bass.setParams (p);

        check (! bass.glideTo (12, 1000), "glide utan klingande ton gör ingenting");

        const auto step = (int) (rate * 0.1);
        bass.noteOn (0, 1.0f, step * 5); // en lång ton
        std::vector<float> audio ((size_t) step * 4);
        for (size_t i = 0; i < audio.size(); ++i)
        {
            if (i == (size_t) step)
                check (bass.glideTo (12, step / 2), "glide-tonen hittar den klingande tonen"); // en oktav upp på en halv step
            audio[i] = bass.nextSample();
        }

        const auto before = estimateFrequency (audio, (size_t) (step * 0.3), (size_t) (step * 0.95));
        const auto after = estimateFrequency (audio, (size_t) (step * 2.0), (size_t) (step * 3.5));
        check (std::abs (before - 55.0) < 1.0, "frekvens före glidet", before);
        check (std::abs (after - 110.0) < 2.0, "tonen blir kvar på målet efter glidet", after);
        check (peakOf (audio, (size_t) step, (size_t) step + 2000) > 0.5f, "ingen dipp: inget nytt anslag");
    }

    void testInstrumentRetriggerWithoutClick()
    {
        std::puts ("Bas: nytt slag medan det förra klingar ger inget hopp");
        InstrumentSynth bass;
        bass.prepare (rate);
        InstrumentParams p;
        p.cutoffHz = 20000.0f;
        p.envAmount = 0.0f;
        p.drive = 0.0f;
        p.sub = 0.0f;
        p.oscA.position = 0.0f; // sinus (Basic-tabellens början)
        bass.setParams (p);

        // Gate längre än steget: andra slaget kommer medan det första fortfarande hörs.
        const auto step = (int) (rate * 0.05);
        auto previous = 0.0f, maxJump = 0.0f;
        for (int i = 0; i < step * 3; ++i)
        {
            if (i % step == 0)
                bass.noteOn (i == step ? 5 : 0, 1.0f, step * 2);
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

        check (pattern.applyTemplate (PatternTemplate::rolling) == InstrumentGrid::sixteenth, "Rolling är i sextondelar");
        const auto quarter = Pattern::ticksPerQuarter;
        auto kickOn = [&pattern] (int tick) { return pattern.note (Pattern::kickLayer, tick).on.load(); };
        check (kickOn (0) && ! kickOn (24) && kickOn (quarter) && kickOn (3 * quarter) && ! kickOn (4 * quarter), "kick på fjärdedelar");
        const auto sixteenth = Pattern::ticksPerStep (InstrumentGrid::sixteenth);
        check (! pattern.note (0, 0).on && pattern.note (0, sixteenth).on
                   && pattern.note (0, 3 * sixteenth).on && ! pattern.note (0, 4 * sixteenth).on,
               "Rolling: K B B B");
        check (pattern.note (0, sixteenth).length == sixteenth, "en ny ton fyller hela steget", pattern.note (0, sixteenth).length.load());

        const auto triplet = Pattern::ticksPerStep (InstrumentGrid::eighthTriplet);
        check (pattern.applyTemplate (PatternTemplate::triplet) == InstrumentGrid::eighthTriplet, "Triplet är i åttondelstrioler");
        check (! pattern.note (0, 3 * triplet).on && pattern.note (0, 4 * triplet).on, "Triplet: K B B");

        // Ändra några steg och kontrollera att allt överlever spara/läsa, också toner utanför rutnätet.
        pattern.addNote (Pattern::kickLayer, 7 * 24, 12, 5, 0.6f); // en kick med tonhöjd, på en sextondel
        pattern.addNote (0, 800, 5, -12, 0.25f, true);
        pattern.addNote (1, 6, 3, 7, 0.5f); // en 64-del, i lager 2

        Pattern loaded;
        loaded.applyTemplate (PatternTemplate::clear);
        loaded.fromValueTree (pattern.toValueTree());

        check (loaded.note (Pattern::kickLayer, 7 * 24).on && loaded.note (Pattern::kickLayer, 7 * 24).pitch == 5 && loaded.note (Pattern::kickLayer, 0).on
                   && ! loaded.note (Pattern::kickLayer, 24).on, "kicknoterna sparas, med tonhöjd");

        // Äldre projekt: kicken som 16 steg per takt blir toner.
        juce::ValueTree legacy (Pattern::treeType);
        legacy.setProperty ("version", 4, nullptr);
        legacy.setProperty ("kick", "1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0", nullptr);
        Pattern migratedKick;
        migratedKick.fromValueTree (legacy);
        check (migratedKick.note (Pattern::kickLayer, 0).on && migratedKick.note (Pattern::kickLayer, 4 * 24).on && migratedKick.note (Pattern::kickLayer, 10 * 24).on
                   && ! migratedKick.note (Pattern::kickLayer, 24).on, "gamla kickstegen blir toner");
        const auto& loadedNote = loaded.note (0, 800);
        check (loadedNote.on && loadedNote.pitch == -12 && loadedNote.slide && loadedNote.length == 5, "tonens på/ton/slide/längd sparas");
        check (std::abs (loadedNote.velocity - 0.25f) < 1.0e-3f, "tonens velocity sparas");
        check (loaded.note (0, 4 * triplet).on, "alla toner i lagret sparas");
        check (loaded.note (1, 6).on && loaded.note (1, 6).pitch == 7 && ! loaded.note (0, 6).on, "lagren sparas var för sig");

        // Tonens längd skalas med lagrets LENGTH (en glide-ton håller inte kvar något: den glider den klingande tonen).
        Pattern held;
        held.applyTemplate (PatternTemplate::clear);
        held.addNote (0, 0, 10, 0, 0.8f, true);
        check (std::abs (held.holdTicks (0, 0, Pattern::ticksPerBar, 0.5f) - 5.0) < 1.0e-6, "längden skalas", held.holdTicks (0, 0, Pattern::ticksPerBar, 0.5f));

        // Version 1 hade rutnäten i ordningen 1/16, 1/8 T, 1/16 T, och ett steg per rutnät (gate som andel).
        juce::ValueTree old (Pattern::treeType);
        juce::ValueTree oldInstrument ("Bass");
        oldInstrument.setProperty ("grid", 0, nullptr);
        oldInstrument.setProperty ("on", "0,1,0,1", nullptr);
        oldInstrument.setProperty ("gate", "0.7,0.5,0.7,0.7", nullptr);
        old.appendChild (oldInstrument, nullptr);

        Pattern migrated;
        migrated.applyTemplate (PatternTemplate::clear);
        migrated.fromValueTree (old);
        check (migrated.note (0, sixteenth).on && migrated.note (0, 3 * sixteenth).on && ! migrated.note (0, 2 * sixteenth).on,
               "gammalt 1/16-mönster hamnar på rätt tid");
        check (migrated.note (0, sixteenth).length == sixteenth / 2, "gammal gate blir längd", migrated.note (0, sixteenth).length.load());
    }
}

namespace
{
    void testKeyMapping()
    {
        std::puts ("Tonart: basen följer tonens oktav, kicken håller sig i sitt register");
        check (Params::instrumentRootMidiNote (60) == 36, "FL:s C5 ger C2 i basen", Params::instrumentRootMidiNote (60));
        check (Params::instrumentRootMidiNote (59) < Params::instrumentRootMidiNote (60), "en ton nedåt går nedåt i basen");
        check (Params::instrumentRootMidiNote (72) == 48, "C6 ger C3", Params::instrumentRootMidiNote (72));

        for (int pc = 0; pc < 12; ++pc)
        {
            const auto note = Params::kickTailMidiNote (pc);
            check (note >= 22 && note <= 33 && note % 12 == (pc + 12) % 12, "kickens svans i A#0..A1 med rätt tonnamn", note);
        }
    }
}

namespace
{
    // ENV 2 i läget Sync: tiderna är notvärden som följer tempot.
    void testModEnvSync()
    {
        std::puts("ENV 2/3: Sync följer tempot (1/4 vid 120 BPM = 500 ms), och ett tempobyte mitt i attacken ger en kontinuerlig kurva");

        auto makeParams = [] (bool sync, float attackMs, int attackNote)
        {
            InstrumentParams p;
            p.oscA.level = 0.0f;
            p.oscB.level = 0.0f;
            p.sub = 0.0f;
            p.env2.attackMs = attackMs;
            p.env2.decayMs = 400.0f;
            p.env2.sustain = 1.0f;
            p.env2.sync = sync;
            p.env2.attackNote = attackNote;
            return p;
        };
        // Index 10 = 1/4 i Modulation::syncTimeNames(), index 7 = 1/8.
        check (Modulation::syncTimeNames()[10] == "1/4" && Modulation::syncTimeInQuarters (10) == 1.0 && Modulation::syncTimeInQuarters (7) == 0.5 && Modulation::syncTimeInQuarters (15) == 16.0,
               "notvärdeslistan: 1/4 = en fjärdedel, 4 bars = 16");

        // Sampel tills ENV 2 nått toppen (attacken klar).
        auto attackSamples = [] (const InstrumentParams& p, double bpm)
        {
            InstrumentSynth synth;
            synth.prepare (rate);
            synth.setTempo (bpm);
            synth.setParams (p);
            synth.reset();
            synth.noteOn (0, 1.0f, (int) (rate * 4.0));
            for (int i = 1; i < (int) (rate * 3.0); ++i)
            {
                synth.nextSample();
                if (synth.getModulationValue (0) >= 0.9999f)
                    return i;
            }
            return -1;
        };
        const auto inMs = attackSamples (makeParams (false, 500.0f, 0), 120.0);
        const auto synced = attackSamples (makeParams (true, 5.0f, 10), 120.0);
        check (inMs > 0 && std::abs (synced - inMs) <= 1, "1/4 vid 120 BPM ger samma attack som 500 ms (±1 sampel)", synced - inMs);
        const auto slower = attackSamples (makeParams (true, 5.0f, 10), 60.0);
        const auto inMsSlow = attackSamples (makeParams (false, 1000.0f, 0), 120.0);
        check (std::abs (slower - inMsSlow) <= 1, "1/4 vid 60 BPM ger samma attack som 1000 ms (±1 sampel)", slower - inMsSlow);

        // Ett tempobyte mitt i attacken: kurvan fortsätter utan hopp (steget per sampel ändras, nivån gör det inte).
        {
            InstrumentSynth synth;
            synth.prepare (rate);
            synth.setTempo (120.0);
            synth.setParams (makeParams (true, 5.0f, 10));
            synth.reset();
            synth.noteOn (0, 1.0f, (int) (rate * 4.0));
            auto previous = 0.0f, biggestStep = 0.0f;
            for (int i = 0; i < (int) (rate * 0.4); ++i)
            {
                if (i == (int) (rate * 0.2))
                    synth.setTempo (60.0);
                synth.nextSample();
                const auto level = synth.getModulationValue (0);
                biggestStep = std::max (biggestStep, std::abs (level - previous));
                previous = level;
            }
            check (biggestStep < 1.5f / (float) (rate * 0.5), "tempobyte mitt i attacken: ingen hopp i nivån", biggestStep);
            check (previous > 0.3f && previous < 0.7f, "och attacken fortsätter med nya tempot (långsammare)", previous);
        }
    }
}

namespace
{
    void testUserWavetable()
    {
        std::puts ("Wavetable: egen tabell av en WAV, och att den överlever spara/läsa");

        // Fyra cykler om 2048 samples: den första en sågtand, resten fyrkanter. Frames väljs jämnt över filen.
        std::vector<float> mono;
        for (int cycle = 0; cycle < 4; ++cycle)
            for (int i = 0; i < 2048; ++i)
                mono.push_back (cycle == 0 ? 2.0f * (float) i / 2048.0f - 1.0f : (i < 1024 ? 1.0f : -1.0f));

        std::vector<float> frames;
        auto table = Wavetables::fromAudio (mono, "test", frames);
        check (table != nullptr && frames.size() == (size_t) (Wavetable::frameSize * Wavetable::numFrames), "tabellen byggs av 4 cykler");
        if (table == nullptr)
            return;

        // Första framen är sågtanden (0,5 vid tre fjärdedelar av cykeln), sista fyrkanten (-1).
        const auto first = table->sample (0.0f, 0.75, 512.0), last = table->sample (1.0f, 0.75, 512.0);
        check (first > 0.35f && first < 0.65f && last < -0.8f, "frames följer filens cykler", first);

        // Likadant efter att ha sparats som frames och byggts om (projektets sparade läge).
        auto restored = Wavetables::fromFrames ("test", frames);
        check (restored != nullptr && std::abs (restored->sample (0.5f, 0.3, 512.0) - table->sample (0.5f, 0.3, 512.0)) < 1.0e-4f,
               "tabellen blir densamma efter att ha byggts om av frames");

        // En kort fil räknas som en enda cykel: alla frames lika, så POS gör ingenting.
        std::vector<float> single;
        for (int i = 0; i < 600; ++i)
            single.push_back (std::sin (juce::MathConstants<float>::twoPi * (float) i / 600.0f));
        std::vector<float> singleFrames;
        auto singleTable = Wavetables::fromAudio (single, "single", singleFrames);
        check (singleTable != nullptr && std::abs (singleTable->sample (0.0f, 0.25, 512.0) - singleTable->sample (1.0f, 0.25, 512.0)) < 1.0e-4f
                   && singleTable->sample (0.0f, 0.25, 512.0) > 0.9f,
               "en kort fil blir en enda cykel");

        // Morph av: tabellen hoppar mellan sina steg (4 cykler = 4 steg, en enkel cykel = 1, Basic = 4), frame läses direkt utan blandning.
        check (table->getSteps() == 4 && singleTable->getSteps() == 1 && Wavetables::get (0).getSteps() == 4, "antal steg: 4 cykler 4, en enkel cykel 1, Basic 4");
        {
            auto worstOn = 0.0f, worstJump = 0.0f;
            for (int step = 0; step < 4; ++step)
            {
                const auto position = (float) step / 3.0f; // stegets egen plats: Morph på och av ger samma våg (utom avrundning)
                for (const auto phase : { 0.1, 0.37, 0.8 })
                    worstOn = std::max (worstOn, std::abs (table->sample (position, phase, 512.0, false) - table->sample (position, phase, 512.0, true)));
            }
            check (worstOn < 1.0e-6f, "Morph av på ett stegs egen plats = Morph på (under -120 dB)", worstOn);
            // Mellan två steg hoppar den: 0,45 av tredjedelen 0,33 och 0,67 hör till steg 1, och ingen blandning med nästa.
            for (const auto phase : { 0.1, 0.37, 0.8 })
                worstJump = std::max (worstJump, std::abs (table->sample (0.45f, phase, 512.0, false) - table->sample (1.0f / 3.0f, phase, 512.0, false)));
            check (worstJump < 1.0e-6f, "Morph av håller samma vågform genom hela steget", worstJump);
            check (std::abs (table->sample (0.1667f, 0.3, 512.0, true) - table->sample (0.1667f, 0.3, 512.0, false)) > 0.01f, "Morph på blandar mellan stegen (olika än Morph av)");
            check (std::abs (singleTable->sample (0.0f, 0.25, 512.0, false) - singleTable->sample (1.0f, 0.25, 512.0, false)) < 1.0e-6f, "en enkel cykel: Morph av gör ingen skillnad vid olika positioner");
            const auto& basic = Wavetables::get (0);
            auto basicOk = true;
            for (int step = 0; step < 4; ++step)
                basicOk = basicOk && std::abs (basic.sample ((float) step / 3.0f, 0.2, 512.0, false) - basic.sample ((float) step / 3.0f, 0.2, 512.0, true)) < 1.0e-6f;
            check (basicOk && std::abs (basic.sample (0.5f, 0.2, 512.0, false) - basic.sample (2.0f / 3.0f, 0.2, 512.0, false)) < 1.0e-6f, "Basic: Morph av hoppar mellan sinus, triangel, såg och fyrkant");
        }
        auto fourSteps = Wavetables::fromFrames ("test", frames, 4);
        check (fourSteps != nullptr && fourSteps->getSteps() == 4 && restored->getSteps() == Wavetable::numFrames, "fromFrames: antalet steg följer med, saknas det blir det 16");

        check (Wavetables::fromAudio ({ 0.0f, 0.0f, 0.0f }, "tom", singleFrames) == nullptr, "för kort eller tyst ljud ger ingen tabell");
        check (Wavetables::names().size() == Wavetables::listSize() && Wavetables::names()[Wavetables::userIndex()] == "User WAV"
                   && Wavetables::userIndex() == 14,
               "User WAV ligger kvar på plats 14");
        check (Wavetables::names()[Wavetables::classicIndex (2)] == "Classic Saw" && Wavetables::names()[0] == "Basic", "de ursprungliga platserna ligger kvar");
    }

    void testWavetableLibrary()
    {
        std::puts ("Wavetable: alla inbyggda tabeller byggs, är rena och olika");

        const auto start = juce::Time::getMillisecondCounterHiRes();
        Wavetables::ensureAll();
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        std::printf ("  %d tabeller byggda på %.2f s\n", Wavetables::builtInCount(), seconds);
        check (Wavetables::builtInCount() >= 100, "minst 100 inbyggda tabeller", (double) Wavetables::builtInCount());
        check (seconds < 20.0, "alla tabeller byggs på rimlig tid", seconds);

        juce::StringArray seen;
        auto bad = 0;
        auto silent = 0;
        auto identical = 0;
        for (int choice = 0; choice < Wavetables::listSize(); ++choice)
        {
            if (choice == Wavetables::userIndex())
                continue;
            const auto& table = Wavetables::get (choice);
            if (seen.contains (table.getName()))
                ++bad; // samma namn två gånger
            seen.add (table.getName());

            auto peak = 0.0f;
            auto firstEnergy = 0.0, lastEnergy = 0.0, difference = 0.0;
            for (int i = 0; i < Wavetable::frameSize; ++i)
            {
                const auto a = table.displayFrame (0)[i], b = table.displayFrame (Wavetable::numFrames - 1)[i];
                if (! std::isfinite (a) || ! std::isfinite (b))
                    ++bad;
                peak = std::max ({ peak, std::abs (a), std::abs (b) });
                firstEnergy += a * a; lastEnergy += b * b; difference += (a - b) * (a - b);
            }
            if (peak < 0.5f || peak > 1.1f || firstEnergy < 1.0e-3 || lastEnergy < 1.0e-3)
            {
                std::printf ("  tyst eller fel nivå: %s (topp %.3f, energi %.4f / %.4f)\n", table.getName().toRawUTF8(), (double) peak, firstEnergy, lastEnergy);
                ++silent;
            }
            if (difference < 1.0e-6)
                ++identical;
        }
        check (bad == 0, "inga NaN och inga dubbla namn", (double) bad);
        check (silent == 0, "alla tabeller har ett ljud med toppen nära 1", (double) silent);
        std::printf ("  %d tabeller är lika i första och sista framen (enkla klassiska former är ok)\n", identical);

        auto categories = juce::StringArray();
        for (int choice = 0; choice < Wavetables::listSize(); ++choice)
            categories.addIfNotAlreadyThere (Wavetables::categoryOf (choice));
        check (categories.size() >= 8, "tabellerna är sorterade i kategorier", (double) categories.size());
    }
}

namespace
{
    void testRowLengths()
    {
        std::puts("Mönster: egen längd per rad, loop och 64 takter");
        PatternLengths lengths;
        lengths.total = 8;
        lengths.rows = { 0, 2, 3, 0 }; // kick: hela mönstret, bas 1: 2 takter, bas 2: 3 (rymmer inte jämnt), bas 3: hela
        check (lengths.barsOf (0) == 8 && lengths.barsOf (1) == 2 && lengths.barsOf (2) == 3 && lengths.barsOf (3) == 8, "radens längd", lengths.barsOf (2));
        check (lengths.barInRow (1, 0) == 0 && lengths.barInRow (1, 3) == 1 && lengths.barInRow (1, 9) == 1, "en kortare rad loopar");
        check (lengths.barInRow (0, 11) == 3, "hela mönstret räknas modulo mönsterlängden", lengths.barInRow (0, 11));
        check (lengths.barInRow (1, -1) == 1, "negativ takt");
        lengths.loops.fill (false);
        check (lengths.barInRow (1, 1) == 1 && lengths.barInRow (1, 2) == -1 && lengths.barInRow (1, 8) == 0, "utan loop spelar raden en gång och tystnar");
        lengths.rows = { 0, 99, 0, 0 };
        check (lengths.barsOf (1) == 8, "en rad kan inte vara längre än mönstret");

        check (Params::barsFromChoice (6) == 64 && Params::barsFromChoice (0) == 1 && Params::barsFromChoice (99) == 64, "mönsterlängder 1 ... 64");
        check (Params::rowBarsFromChoice (0) == 0 && Params::rowBarsFromChoice (1) == 1 && Params::rowBarsFromChoice (7) == 64, "radlängder");

        Pattern pattern;
        const auto lastTick = Pattern::maxTicks - 1;
        pattern.addNote (0, lastTick, 4, 90, 0.5f);
        pattern.addNote (1, 5000, 24, -90, 0.5f);
        Pattern loaded;
        loaded.fromValueTree (pattern.toValueTree());
        check (loaded.note (0, lastTick).on && loaded.note (0, lastTick).pitch == 90, "en ton i sista takten av 64 sparas", loaded.note (0, lastTick).pitch.load());
        check (loaded.note (1, 5000).on && loaded.note (1, 5000).pitch == -90, "tonhöjd ±90 sparas");
    }

    void testPolyphonyStacks()
    {
        std::puts ("Mönster: staplade toner (polyfoni steg 1)");
        Pattern pattern;
        pattern.clearNotes (0); // standardmallen ligger på lager 0
        pattern.addNote (0, 96, 48, 3, 0.7f, false, { 7, 0, 0 });
        check (pattern.stackSize (0, 96) == 1 && pattern.stackSize (0, 100) == 0, "stackSize: bara huvudtonen / ingen ton");
        check (pattern.addExtra (0, 100, {}) == -1, "ingen stapel utan huvudton");
        check (pattern.addExtra (0, 96, { 24, 10, 0.5f, false }) == 1 && pattern.addExtra (0, 96, { 12, -5, 0.4f, true }) == 2
                  && pattern.addExtra (0, 96, { 6, 1, 1.0f, false }) == 3 && pattern.addExtra (0, 96, {}) == -1, "tre staplar, sedan fullt");
        check (pattern.stackSize (0, 96) == 4, "stackSize 4");
        pattern.removeStack (0, 96, 2);
        Pattern::StackNote s;
        check (pattern.stackSize (0, 96) == 3 && ! pattern.getStack (0, 96, 2, s) && pattern.getStack (0, 96, 3, s) && s.pitch == 1, "removeStack tar bara den staplade");
        pattern.setStack (0, 96, 1, { 30, 12, 0.25f, true });
        pattern.setStack (0, 96, 2, { 30, 12, 0.25f, true }); // släckt slot: ignoreras
        check (pattern.getStack (0, 96, 1, s) && s.length == 30 && s.pitch == 12 && s.slide && ! pattern.getStack (0, 96, 2, s), "setStack");

        Pattern loaded;
        loaded.fromValueTree (pattern.toValueTree());
        check (loaded.stackSize (0, 96) == 3 && loaded.note (0, 96).chord[0] == 7 && loaded.note (0, 96).pitch == 3, "spara/ladda: staplar och ackord");
        check (loaded.getStack (0, 96, 1, s) && s.length == 30 && s.pitch == 12 && s.slide && loaded.getStack (0, 96, 2, s) && s.pitch == 1, "spara/ladda: stapelns fält");

        // Trasig eller tom /e får inte krascha (läses som saknade staplar).
        for (const auto* text : { "96/48/3/0/0.700/7/0/0/e", "96/48/3/0/0.700/e", "96/48/3/0/0.700/e/1/2", "96/48/3/0/0.700/e/x/y/z/w", "96/48/3/0/0.700/7/e/1/24/0.5/0/e/2" })
        {
            auto tree = pattern.toValueTree();
            for (auto child : tree)
                if (child.hasType ("Notes") && (int) child["layer"] == 0)
                    child.setProperty ("notes", text, nullptr);
            Pattern corrupt;
            corrupt.fromValueTree (tree);
            check (corrupt.note (0, 96).on.load() && corrupt.stackSize (0, 96) >= 1 && corrupt.stackSize (0, 96) <= Pattern::maxStack, "trasig /e: tonen finns, inget kraschar");
        }

        // Glidande huvudton: extras räknas inte; vanliga räknas med ackord.
        Pattern voices;
        voices.clearNotes (0);
        voices.addNote (0, 0, 96, 0, 0.8f, false, { 4, 7, 0 });
        voices.addExtra (0, 0, { 96, 12, 0.8f, false });
        check (voices.maxSimultaneous (0) == 4, "maxSimultaneous räknar huvudton, ackord och stapel", voices.maxSimultaneous (0));
        voices.addNote (0, 48, 24, 0, 0.8f, true);
        check (voices.maxSimultaneous (0) == 4, "en glide-ton lägger inga röster");

        voices.addNote (0, 0, 96, 0); // en ny ton ärver inga staplar
        check (voices.stackSize (0, 0) == 1, "addNote nollställer staplar");
        voices.addExtra (0, 0, {});
        voices.trimRow (0, 1); // oförändrad
        voices.removeNote (0, 0);
        voices.addNote (0, 0, 24);
        check (voices.stackSize (0, 0) == 1, "removeNote tar med staplarna");
    }

    void testGateAndPianoRollFlag()
    {
        std::puts ("Mönster: gate-steg och pianorullsmarkering sparas per lager");
        Pattern pattern;
        pattern.applyTemplate (PatternTemplate::rolling);
        check (pattern.isGateStepOn (0, 5) && ! pattern.isCustom (0), "gaten är öppen och lagret är ett stegmönster som standard");

        pattern.setGateStep (1, 7, false);
        pattern.addNote (2, 0, 24, 5);
        pattern.setCustom (2, true);

        Pattern loaded;
        loaded.fromValueTree (pattern.toValueTree());
        check (! loaded.isGateStepOn (1, 7) && loaded.isGateStepOn (1, 6) && loaded.isGateStepOn (0, 7), "gate-stegen sparas per lager");
        check (loaded.isCustom (2) && ! loaded.isCustom (0) && ! loaded.isCustom (1), "pianorullsmarkeringen sparas per lager");

        pattern.clearNotes (2);
        check (! pattern.isCustom (2), "när lagret rensas nollställs pianorullsmarkeringen");
    }
}

void runEngineTests()
{
    testKeyMapping();
    testKickLandsOnKey();
    testKickRetriggerFades();
    testKickShapesAndSamples();
    testInstrumentSample();
    testInstrumentPitchAndRelease();
    testInstrumentGlide();
    testInstrumentModulationSources();
    testInstrumentRetriggerWithoutClick();
    testDuckerIsSmooth();
    testGate();
    testPatternTemplatesAndSaving();
    testModEnvSync();
    testUserWavetable();
    testWavetableLibrary();
    testGateAndPianoRollFlag();
    testRowLengths();
    testPolyphonyStacks();
}
