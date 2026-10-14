// Tester för TransportClock: kontrollerar att varje stepgräns hamnar på exakt rätt sample,
// precis en gång, oavsett hur DAW:en delar upp ljudet i block.

#include "../Source/Core/TransportClock.h"
#include <cstdio>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool condition, const char* what, long long detail = 0)
    {
        if (! condition)
        {
            std::printf ("  FEL: %s (%lld)\n", what, detail);
            ++failures;
        }
    }

    struct Step
    {
        long long sample; // absolut sample sedan testets början
        int stepInBar;
    };

    juce::AudioPlayHead::PositionInfo hostPosition (double bpm, double ppq, juce::Optional<double> barStart = {})
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setIsPlaying (true);
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        info.setPpqPositionOfLastBarStart (barStart);
        return info;
    }

    // Spelar `totalSamples` med DAW-transport från `startPpq`, i block om `blockSize`.
    std::vector<Step> runHost (double sampleRate, double bpm, double startPpq, int blockSize, long long totalSamples,
                               double stepsPerQuarter, bool hostGivesBarStart = true)
    {
        TransportClock clock;
        clock.prepare (sampleRate);
        StepCursor cursor { stepsPerQuarter };
        std::vector<Step> steps;

        const auto ppqPerSample = bpm / 60.0 / sampleRate;
        for (long long done = 0; done < totalSamples; done += blockSize)
        {
            const auto ppq = startPpq + done * ppqPerSample;
            const auto barStart = hostGivesBarStart ? juce::Optional<double> (std::floor (ppq / 4.0) * 4.0) : juce::nullopt;
            clock.beginBlock (hostPosition (bpm, ppq, barStart), blockSize);
            clock.forEachStep (cursor, 0, blockSize, [&] (int offset, int inBar) { steps.push_back ({ done + offset, inBar }); });
        }
        return steps;
    }

    void testQuartersAt120Bpm()
    {
        std::puts ("Fjärdedelar i 120 BPM, block om 512");
        // 120 BPM på 48 kHz: en fjärdedel = 24000 samples.
        const auto steps = runHost (48000.0, 120.0, 0.0, 512, 48000 * 4, 1.0);

        check (steps.size() == 8, "antal slag på 4 sekunder", (long long) steps.size());
        for (size_t i = 0; i < steps.size(); ++i)
        {
            check (steps[i].sample == (long long) i * 24000, "slagets sample", steps[i].sample);
            check (steps[i].stepInBar == (int) (i % 4), "slagets plats i takten", steps[i].stepInBar);
        }
    }

    void testStepOnBlockBoundaryFiresOnce()
    {
        std::puts ("Slag precis på blockgräns spelas en gång");
        // Block om 480: varje fjärdedel (24000) hamnar exakt på en blockgräns.
        const auto steps = runHost (48000.0, 120.0, 0.0, 480, 48000 * 2, 1.0);

        check (steps.size() == 4, "antal slag", (long long) steps.size());
        for (size_t i = 1; i < steps.size(); ++i)
            check (steps[i].sample - steps[i - 1].sample == 24000, "avstånd mellan slag", steps[i].sample - steps[i - 1].sample);
    }

    void testStartMidBeat()
    {
        std::puts ("Start mitt i ett slag väntar till nästa");
        const auto steps = runHost (48000.0, 120.0, 0.5, 512, 48000, 1.0);

        check (! steps.empty() && steps[0].sample == 12000, "första slaget efter en halv fjärdedel",
               steps.empty() ? -1 : steps[0].sample);
        check (! steps.empty() && steps[0].stepInBar == 1, "första slaget är tvåan");
    }

    void testSixteenthsWrapAtBar()
    {
        std::puts ("Sextondelar: 16 per takt, börjar om på ettan");
        // 140 BPM på 44.1 kHz, ojämna block. Testar också utan barStart från DAW:en.
        const auto steps = runHost (44100.0, 140.0, 0.0, 333, 44100 * 4, 4.0, false);

        const auto samplesPerSixteenth = 44100.0 * 60.0 / 140.0 / 4.0;
        for (size_t i = 0; i < steps.size(); ++i)
        {
            const auto expected = (long long) std::round ((double) i * samplesPerSixteenth);
            check (std::abs (steps[i].sample - expected) <= 1, "sextondelens sample", steps[i].sample - expected);
            check (steps[i].stepInBar == (int) (i % 16), "sextondelens plats i takten", steps[i].stepInBar);
        }
    }

    void testBarLineInsideBlock()
    {
        std::puts ("Taktstreck mitt i ett block ger ettan");
        TransportClock clock;
        clock.prepare (48000.0);
        StepCursor cursor { 1.0 };
        std::vector<int> inBar;

        // Blocket börjar på ppq 3.9 i takt 1 (barStart 0) och går över taktstrecket på ppq 4.
        clock.beginBlock (hostPosition (120.0, 3.9, 0.0), 4800);
        clock.forEachStep (cursor, 0, 4800, [&] (int, int step) { inBar.push_back (step); });

        check (inBar.size() == 1 && inBar[0] == 0, "steget efter taktstrecket är ettan", inBar.empty() ? -1 : inBar[0]);
    }

    void testLoopJumpBack()
    {
        std::puts ("Loop tillbaka spelar ettan igen");
        TransportClock clock;
        clock.prepare (48000.0);
        StepCursor cursor { 1.0 };
        int count = 0;

        clock.beginBlock (hostPosition (120.0, 0.0, 0.0), 512);
        clock.forEachStep (cursor, 0, 512, [&] (int, int) { ++count; });
        clock.beginBlock (hostPosition (120.0, 0.0, 0.0), 512); // DAW:en loopar till början
        clock.forEachStep (cursor, 0, 512, [&] (int, int) { ++count; });

        check (count == 2, "ettan spelas i båda varven", count);
    }

    void testFreeRunRestart()
    {
        std::puts ("Egen klocka: startar på tonens sample, 140 BPM utan tempo");
        TransportClock clock;
        clock.prepare (48000.0);
        StepCursor cursor { 1.0 };
        std::vector<long long> samples;

        // Transporten står still och DAW:en anger inget tempo.
        clock.beginBlock (juce::nullopt, 512);
        clock.restartFreeRunAt (100);
        clock.forEachStep (cursor, 100, 512, [&] (int offset, int) { samples.push_back (offset); });

        for (long long done = 512; done < 48000; done += 512)
        {
            clock.beginBlock (juce::nullopt, 512);
            clock.forEachStep (cursor, 0, 512, [&] (int offset, int) { samples.push_back (done + offset); });
        }

        const auto samplesPerQuarter = 48000.0 * 60.0 / 140.0;
        check (samples.size() == 3, "antal slag på en sekund", (long long) samples.size());
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const auto expected = 100 + (long long) std::round ((double) i * samplesPerQuarter);
            check (std::abs (samples[i] - expected) <= 1, "slagets sample", samples[i] - expected);
        }

        // En ny ton inom första slaget startar om klockan och spelar ettan igen.
        clock.beginBlock (juce::nullopt, 512);
        clock.restartFreeRunAt (0);
        int count = 0;
        clock.forEachStep (cursor, 0, 512, [&] (int, int) { ++count; });
        clock.beginBlock (juce::nullopt, 512);
        clock.restartFreeRunAt (10);
        clock.forEachStep (cursor, 10, 512, [&] (int, int) { ++count; });
        check (count == 2, "omstart spelar ettan igen", count);
    }

    void testStepJustPassed()
    {
        std::puts ("Ton strax efter slaget räknas som på slaget");
        TransportClock clock;
        clock.prepare (48000.0);
        StepCursor cursor { 1.0 };

        // Blocket börjar 20 samples efter ettan.
        const auto ppqPerSample = 120.0 / 60.0 / 48000.0;
        clock.beginBlock (hostPosition (120.0, 20 * ppqPerSample, 0.0), 512);

        const auto step = clock.takeStepJustPassed (cursor, 0, 144);
        check (step.has_value() && *step == 0, "ettan tas");
        check (! clock.takeStepJustPassed (cursor, 0, 144).has_value(), "ettan tas bara en gång");

        int count = 0;
        clock.forEachStep (cursor, 0, 512, [&] (int, int) { ++count; });
        check (count == 0, "ettan spelas inte igen i blocket", count);

        StepCursor other { 1.0 };
        check (! clock.takeStepJustPassed (other, 0, 10).has_value(), "för sent för toleransen");
    }
}

int main()
{
    testQuartersAt120Bpm();
    testStepOnBlockBoundaryFiresOnce();
    testStartMidBeat();
    testSixteenthsWrapAtBar();
    testBarLineInsideBlock();
    testLoopJumpBack();
    testFreeRunRestart();
    testStepJustPassed();

    if (failures > 0)
    {
        std::printf ("\n%d test(er) misslyckades\n", failures);
        return 1;
    }

    std::puts ("\nAlla tester gick igenom");
    return 0;
}
