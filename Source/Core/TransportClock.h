#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <limits>
#include <optional>

// Kommer ihåg vilken stepgräns som senast spelades för en viss upplösning, så att ingen
// gräns spelas två gånger när den hamnar precis på en blockgräns.
struct StepCursor
{
    StepCursor (double stepsPerQuarterIn = 96.0) : stepsPerQuarter (stepsPerQuarterIn) {}

    double stepsPerQuarter; // 1 = fjärdedelar, 4 = sextondelar, 3 = åttondelstrioler, 6 = sextondelstrioler
    double lastFiredPpq = -std::numeric_limits<double>::infinity();
    double lastEndPpq = -std::numeric_limits<double>::infinity(); // där förra anropet slutade
    int generation = -1;
};

// Var ett steg ligger: vilken takt (räknat från låtens början, eller från första tonen när
// den egna klockan går) och vilket steg i takten.
struct StepPosition
{
    int stepInBar = 0;
    juce::int64 bar = 0;

    // Stegets plats i ett mönster som är `patternBars` takter långt.
    int stepInPattern (int stepsPerBar, int patternBars) const
    {
        const auto barInPattern = (int) (((bar % patternBars) + patternBars) % patternBars);
        return barInPattern * stepsPerBar + stepInBar;
    }
};

// Håller reda på var i takten vi är, sample för sample.
//
// När DAW:en spelar följer klockan DAW:ens tempo och taktstreck. När transporten står still
// går en egen klocka i DAW:ens tempo (eller 140 BPM om tempot är okänt), så att plugin:et
// går att provspela utan att trycka play.
//
// Positioner anges i ppq: antal fjärdedelar sedan låtens början.
class TransportClock
{
public:
    static constexpr double fallbackBpm = 140.0;

    void prepare (double newSampleRate);

    // Anropas i början av varje block med det DAW:en rapporterar (getPlayHead()->getPosition()).
    void beginBlock (const juce::Optional<juce::AudioPlayHead::PositionInfo>& position, int numSamples);

    // Startar om den egna klockan så att takt 0 börjar på sample `sampleOffset` i blocket.
    // Gör ingenting när DAW:en spelar, eftersom DAW:ens taktstreck då bestämmer.
    void restartFreeRunAt (int sampleOffset);

    // Anropar callback (sampleOffset, StepPosition) för varje stepgräns i [startSample, endSample).
    template <typename Callback>
    void forEachStep (StepCursor& cursor, int startSample, int endSample, Callback&& callback) const;

    // Om en stepgräns passerades högst `toleranceSamples` före `sampleOffset` utan att spelas,
    // markeras den som spelad och dess position returneras. Används när en ton börjar en
    // aning efter taktstrecket, så att första slaget ändå kommer med.
    std::optional<StepPosition> takeStepJustPassed (StepCursor& cursor, int sampleOffset, int toleranceSamples) const;

    // Förhandsspelning: den egna klockan används även när DAW:en spelar, så att DAW:ens loop (t.ex. ett mönster på en takt) inte styr den.
    void setForceFreeRun (bool force) { forceFreeRun = force; if (force) followingHost = false; }

    // Bara egen klocka (D3a): eget tempo (0 = som förut: värdens senaste tempo eller 140), hållning medan man scrubbar takten, och hopp till en takt.
    void setFreeRunBpm (double newBpm) { freeRunBpm = newBpm; }
    void setHold (bool shouldHold)     { hold = shouldHold; }
    void seekFreeRunToBar (juce::int64 bar);

    // Global LOOP (D3): den egna klockan går runt mellan startBar och endBar (0-räknade, slutet exklusivt). DAW:ens transport ignoreras då,
    // eftersom en VST3 inte kan stoppa den. Hoppet görs vid blockstart, så överskottet (högst ett block) behålls.
    void setLoop (bool on, juce::int64 startBar, juce::int64 endBar)
    {
        loopOn = on && endBar > startBar;
        loopStartBar = startBar;
        loopEndBar = endBar;
        if (loopOn)
            setForceFreeRun (true);
    }

    bool isFollowingHost() const      { return followingHost; }
    bool isHostPlaying() const        { return hostPlaying; } // värdens råa status, även när klockan går fritt (förhandsspelning)
    double getBpm() const             { return bpm; }
    double getPpqAt (int sampleOffset) const { return blockStartPpq + sampleOffset * ppqPerSample; }
    double getBarStartPpq() const     { return barStartPpq; }
    double getBarLengthPpq() const    { return barLengthPpq; }
    double getSamplesPerQuarter() const { return 1.0 / ppqPerSample; }

    // Taktnumret vid blockets början, och var en given takt börjar i ppq.
    juce::int64 getBarIndex() const   { return barIndex; }
    double getPpqOfBar (juce::int64 bar) const { return barStartPpq + (double) (bar - barIndex) * barLengthPpq; }

private:
    static constexpr double epsilon = 1.0e-9;

    void syncCursor (StepCursor& cursor, double fromPpq) const;
    StepPosition positionOf (juce::int64 stepFromBarStart, double stepsPerQuarter) const;

    double sampleRate = 44100.0;
    double bpm = fallbackBpm;
    double ppqPerSample = fallbackBpm / 60.0 / 44100.0;
    double blockStartPpq = 0.0;
    double barStartPpq = 0.0;
    double barLengthPpq = 4.0;
    juce::int64 barIndex = 0;
    int blockSize = 0;
    bool followingHost = false;
    bool hostPlaying = false;
    bool forceFreeRun = false;
    bool hold = false;
    double freeRunBpm = 0.0;
    bool loopOn = false;
    juce::int64 loopStartBar = 0, loopEndBar = 0;
    int generation = 0; // ökas vid omstart så att alla StepCursor glömmer vad de spelat
};

template <typename Callback>
void TransportClock::forEachStep (StepCursor& cursor, int startSample, int endSample, Callback&& callback) const
{
    if (endSample <= startSample)
        return;

    const auto spq = cursor.stepsPerQuarter;
    const auto fromPpq = getPpqAt (startSample);
    const auto toPpq = getPpqAt (endSample);

    syncCursor (cursor, fromPpq);
    cursor.lastEndPpq = toPpq;

    // Första stepgränsen som ligger på eller efter fromPpq, räknat från taktens början.
    auto step = (juce::int64) std::ceil ((fromPpq - barStartPpq) * spq - epsilon);

    for (;; ++step)
    {
        const auto stepPpq = barStartPpq + (double) step / spq;

        if (stepPpq >= toPpq)
            break;

        if (stepPpq <= cursor.lastFiredPpq + epsilon)
            continue; // redan spelad i förra blocket

        cursor.lastFiredPpq = stepPpq;

        const auto offset = startSample + (int) std::round ((stepPpq - fromPpq) / ppqPerSample);
        callback (juce::jlimit (startSample, endSample - 1, offset), positionOf (step, spq));
    }
}
