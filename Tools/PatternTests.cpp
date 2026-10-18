// `tool boundarytest`: tonen som slutar när ett mönster tar slut i DAW:en (FL: Pattern 1 -> Pattern 2) ska inte ge ett sista, hörbart steg på takten där den slutar.
// Bakgrund: med två Kickbase-instanser i varsitt mönster hördes en dubbel kick på första slaget efter bytet, för att den gamla instansen fick "ton av" några
// samplingar efter taktens första steg och hann spela steget innan den tystnade. Dels om "ton av" kommer i samma bit som steget (steg från 3 ms före "ton av" startas
// inte), dels i en senare bit eller ett senare block (kicken och instrumenten som startade av steget tonas ut på 3 ms).
//
// Mäts på ljudet, inte på antalet starter: instrumenten är av, så det som hörs är kicken. Referensen är samma rendering där "ton av" kommer en bit före takten
// (200 samplingar), så att inget steg finns på takten. Skillnaden från 8 ms efter takten och framåt ska vara tyst. Alla 32 lägen mot kontrollrutnätet (en bit är 32
// samplingar) och "ton av" 0-12 samplingar efter takten provas, i flera blockstorlekar.

#include "../Source/Instrument/InstrumentProcessor.h"
#include "Scenarios.h"

namespace
{
    constexpr double rate = 44100.0, bpm = 130.0;
    constexpr int noNoteOff = std::numeric_limits<int>::min();

    struct Head : juce::AudioPlayHead
    {
        juce::int64 samples = 0;
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

    juce::int64 barThreeSample() { return (juce::int64) std::ceil (2.0 * rate * 60.0 / bpm * 4.0); }

    // Kicken (instrumenten av) från 44 samplingar före till 60 ms efter takt 3. `noteOffOffset` = "ton av" så här många samplingar efter takten (noNoteOff = ingen);
    // `restart`: 1 = "ton på" på samma ton och sample direkt efter "ton av", 2 = "ton på" före "ton av". `shift` flyttar värdens samplingsräkning mot kontrollrutnätet.
    std::vector<float> render (int shift, int noteOffOffset, int restart, int blockSize)
    {
        auto processor = std::make_unique<InstrumentProcessor>();
        processor->setPlayConfigDetails (0, 2, rate, blockSize);
        processor->prepareToPlay (rate, blockSize);
        if (auto* parameter = processor->apvts.getParameter (ParamID::noteEnd))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f)); // Cut
        for (int layer = 0; layer < 3; ++layer)
            Scenarios::setInstrument (*processor, layer, InstrumentField::on, 0.0f);
        Head head;
        processor->setPlayHead (&head);

        const auto barThree = barThreeSample();
        const auto noteOffAt = noteOffOffset == noNoteOff ? std::numeric_limits<juce::int64>::max() : barThree + noteOffOffset;
        const auto from = barThree - 44, to = barThree + (juce::int64) (0.060 * rate);
        std::vector<float> result;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (juce::int64 start = 0; start + shift < to + blockSize; start += blockSize)
        {
            const auto hostStart = start + shift;
            head.samples = hostStart;
            midi.clear();
            if (start == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
            if (noteOffAt >= hostStart && noteOffAt < hostStart + blockSize)
            {
                const auto position = (int) (noteOffAt - hostStart);
                if (restart == 2)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), position);
                midi.addEvent (juce::MidiMessage::noteOff (1, 52), position);
                if (restart == 1)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), position);
            }
            buffer.clear();
            processor->processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
                if (const auto host = hostStart + i; host >= from && host < to)
                    result.push_back (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        }
        processor->setPlayHead (nullptr);
        return result;
    }

    // Största skillnaden mellan två renderingar från 8 ms efter takten (efter fade-tiden) och framåt.
    float differenceAfterFade (const std::vector<float>& a, const std::vector<float>& b, size_t* where = nullptr)
    {
        const auto first = (size_t) (44 + 0.008 * rate);
        auto biggest = 0.0f;
        for (size_t i = first; i < std::min (a.size(), b.size()); ++i)
            if (const auto d = std::abs (a[i] - b[i]); d > biggest)
            {
                biggest = d;
                if (where != nullptr)
                    *where = i;
            }
        return biggest;
    }

    float rmsAfter (const std::vector<float>& a)
    {
        const auto first = (size_t) (44 + 0.008 * rate);
        auto sum = 0.0;
        for (size_t i = first; i < a.size(); ++i)
            sum += (double) a[i] * a[i];
        return (float) std::sqrt (sum / (double) std::max<size_t> (1, a.size() - first));
    }
}

int boundaryTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // 1. Tonen slutar: ingen kick på takten där den slutar, vid alla lägen mot kontrollrutnätet och "ton av" 0-12 samplingar efter takten.
    //    Referens: "ton av" 200 samplingar före takten.
    for (const auto blockSize : { 32, 256, 512 })
    {
        auto bad = 0, cases = 0;
        auto worst = 0.0f;
        juce::String detail;
        for (int shift = 0; shift < 32; ++shift)
        {
            const auto reference = render (shift, -200, 0, blockSize);
            for (int offset = 0; offset <= 12; offset += (offset < 4 ? 1 : 4))
            {
                size_t at = 0;
                const auto difference = differenceAfterFade (render (shift, offset, 0, blockSize), reference, &at);
                ++cases;
                worst = std::max (worst, difference);
                if (difference > 0.01f) // -40 dBFS
                {
                    ++bad;
                    if (detail.length() < 80)
                        detail << " " << juce::String (shift) << "/" << juce::String (offset) << "@" << juce::String ((int) at - 44);
                }
            }
        }
        check (bad == 0, "block " + juce::String (blockSize) + ": " + juce::String (cases) + " lägen (rutnät 0-31 x \"ton av\" 0-12 efter takten): inget hörbart steg på takten, största skillnad "
                           + juce::String (juce::Decibels::gainToDecibels (worst, -200.0f), 1) + " dBFS (gräns -40)" + (bad > 0 ? ", hördes (rutnät/offset):" + detail : juce::String()));
    }

    // 2. Nästa mönster i SAMMA instans: "ton av" och "ton på" på samma ton och sample, i båda ordningarna: kicken på takten spelas som vanligt, en gång
    //    (nivån efter tiden är densamma som när tonen aldrig släppts; en dubbel kick skulle ge +6 dB).
    for (const auto restart : { 1, 2 })
        for (const auto blockSize : { 32, 512 })
        {
            auto bad = 0, cases = 0;
            auto worstRatio = 0.0f;
            for (int shift = 0; shift < 32; shift += 3)
            {
                const auto reference = rmsAfter (render (shift, noNoteOff, 0, blockSize));
                for (int offset = 0; offset <= 6; offset += 3)
                {
                    const auto level = rmsAfter (render (shift, offset, restart, blockSize));
                    const auto ratioDb = std::abs (juce::Decibels::gainToDecibels (level / std::max (1.0e-6f, reference)));
                    ++cases;
                    worstRatio = std::max (worstRatio, ratioDb);
                    bad += ratioDb > 1.5f ? 1 : 0;
                }
            }
            check (bad == 0, juce::String (restart == 1 ? "ton av, sedan ton på" : "ton på, sedan ton av") + " på samma sample (block " + juce::String (blockSize) + ", " + juce::String (cases)
                               + " lägen): kicken på takten spelas en gång, nivån skiljer högst " + juce::String (worstRatio, 2) + " dB från en ton som aldrig släpptes (gräns 1,5)");
        }

    std::printf (failed ? "Mönstergränsen: FEL\n" : "Mönstergränsen gick igenom\n");
    return failed ? 1 : 0;
}
