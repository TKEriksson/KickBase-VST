// `tool boundarytest`: tonen som slutar när ett mönster tar slut i DAW:en (FL: Pattern 1 -> Pattern 2) ska inte spela ett sista steg på takten där den slutar.
// Bakgrund: med två Kickbase-instanser i varsitt mönster hördes en dubbel kick på första slaget efter bytet, för att den gamla instansen fick
// "ton av" några samplingar efter takten och hann spela steget på takten innan den tystnade.

#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"

namespace
{
    struct Head : juce::AudioPlayHead
    {
        juce::int64 samples = 0;
        double rate = 44100.0, bpm = 130.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            const auto ppq = (double) samples / rate * bpm / 60.0;
            info.setBpm (bpm);
            info.setIsPlaying (true);
            info.setTimeInSamples (samples);
            info.setPpqPosition (ppq);
            info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
            info.setTimeSignature (TimeSignature { 4, 4 });
            return info;
        }
    };

    // Spelar två takter med en ton som hålls, med "ton av" `offsetAfterBar` samplingar efter att takt 3 börjar (negativt = före).
    // Ger antalet kickar som startar från 1 ms före till 40 ms efter takt 3, och hur många av dem som kom FÖRE "ton av".
    // restart: 0 = tonen slutar bara, 1 = "ton av" och sedan "ton på" på samma ton och sample (nästa mönster, samma instans), 2 = "ton på" före "ton av".
    int kicksAtBarThree (int offsetAfterBar, int noteEnd, int blockSize, int& beforeNoteOff, int restart = 0)
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->setPlayConfigDetails (0, 2, 44100.0, blockSize);
        processor->prepareToPlay (44100.0, blockSize);
        if (auto* parameter = processor->apvts.getParameter (ParamID::noteEnd))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) noteEnd));
        Head head;
        processor->setPlayHead (&head);

        const auto samplesPerBar = 44100.0 * 60.0 / head.bpm * 4.0;
        const auto barThree = (juce::int64) std::ceil (2.0 * samplesPerBar);
        const auto noteOffAt = barThree + offsetAfterBar;
        const auto from = barThree - 44, to = barThree + (juce::int64) (0.040 * 44100.0);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        auto lastCount = 0, kicks = 0;
        beforeNoteOff = 0;
        for (juce::int64 start = 0; start < to + blockSize; start += blockSize)
        {
            head.samples = start;
            midi.clear();
            if (start == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
            if (noteOffAt >= start && noteOffAt < start + blockSize)
            {
                const auto position = (int) (noteOffAt - start);
                if (restart == 2)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), position);
                midi.addEvent (juce::MidiMessage::noteOff (1, 52), position);
                if (restart == 1)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), position);
            }
            buffer.clear();
            processor->processBlock (buffer, midi);
            const auto count = processor->transportStatus.kickTriggers.load();
            if (count != lastCount && start + blockSize > from && start < to)
            {
                kicks += count - lastCount;
                if (start + blockSize <= noteOffAt)
                    beforeNoteOff += count - lastCount;
            }
            lastCount = count;
        }
        processor->setPlayHead (nullptr);
        return kicks;
    }
}

int boundaryTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // Mönstrets kick på första slaget finns i Init. NoteEnd 0 = Cut. En kick som startar vid takt 3 och hinner komma före "ton av" är för mycket.
    for (const auto blockSize : { 32, 128, 512 })
    {
        auto extra = 0;
        juce::String detail;
        for (int offset = -3; offset <= 12; ++offset)
        {
            int before = 0;
            const auto kicks = kicksAtBarThree (offset, 0, blockSize, before);
            // En ton som slutar vid takten (offset <= 0) ska inte ha någon kick där. Några samplingar senare (DAW:ens avrundning) ska den inte heller ha det.
            if (kicks > 0)
            {
                ++extra;
                detail << " " << juce::String (offset);
            }
        }
        check (extra == 0, "block " + juce::String (blockSize) + ": ingen kick på takten där tonen slutar (\"ton av\" -3..+12 samplingar runt takten)"
                           + (extra > 0 ? " - kick vid offset:" + detail : juce::String()));
    }
    // Nästa mönster i SAMMA instans: "ton av" och "ton på" på samma ton och sample, i båda ordningarna. Steget på takten ska spelas exakt en gång.
    for (const auto restart : { 1, 2 })
        for (const auto blockSize : { 32, 512 })
        {
            auto wrong = 0;
            juce::String detail;
            for (int offset = 0; offset <= 6; ++offset)
            {
                int before = 0;
                const auto kicks = kicksAtBarThree (offset, 0, blockSize, before, restart);
                if (kicks != 1)
                {
                    ++wrong;
                    detail << " " << juce::String (offset) << ":" << juce::String (kicks);
                }
            }
            check (wrong == 0, juce::String (restart == 1 ? "ton av, sedan ton på" : "ton på, sedan ton av") + " på samma sample (block " + juce::String (blockSize)
                               + "): exakt en kick på takten" + (wrong > 0 ? " - fel (offset:antal)" + detail : juce::String()));
        }
    std::printf (failed ? "Mönstergränsen: FEL\n" : "Mönstergränsen gick igenom\n");
    return failed ? 1 : 0;
}
