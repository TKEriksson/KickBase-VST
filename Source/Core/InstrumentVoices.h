#pragma once

#include "InstrumentSynth.h"

// Ett instruments röster (fas P): upp till maxVoices InstrumentSynth som delar parametrar och tempo.
// VOICES = 1 spelar som den monofoniska synten gjorde (en röst, legato-anslag, bit-identiskt).
// Fler röster: varje ton får en ledig röst. När VOICES röster är upptagna stjäls den äldsta i release,
// annars den äldsta som hålls; den tonas ut på 3 ms (stealRampMs) medan den nya tonen börjar på en
// annan ledig röst, eller, om alla åtta klingar, på den stulna rösten när rampen är klar.
// Glide, ENV 2/3, VELOCITY, RANDOM och samplingsmarkören följer den senast spelade rösten.
class InstrumentVoices
{
public:
    static constexpr int maxVoices = 8;
    static constexpr double stealRampMs = 3.0;
    using Output = InstrumentSynth::Output;

    // Förbereder alla röster (allt minne allokeras här, inget i ljudtråden).
    void prepare (double sampleRate);
    // Röst k får seed + 1000·k, så röst 0 har samma slump som den monofoniska synten.
    void setSeed (juce::int64 seed);
    void reset();
    // Parametrarna och tempot för fönstret (32 samples). Klingande röster får dem direkt, en röst som
    // aktiveras får dem när tonen börjar; lediga röster kostar inget.
    void setParams (const InstrumentParams& newParams);
    void setTempo (double newBpm);
    // 1..maxVoices. Röster över gränsen tonas ut på 3 ms (de äldsta först), de klipps inte.
    void setNumVoices (int count);
    int getNumVoices() const { return numVoices; }

    // chordNote: en extra ackordton (fas P). Den blir inte den senast spelade rösten och drar inget nytt slumpvärde, så grundtonen
    // (som spelas sist) styr glide och modulering. Med en röst spelas den inte.
    // slice >= 0: en not i raden "Slice N" (se InstrumentSynth::noteOn).
    void noteOn (int semitones, float velocity, int gateSamples, bool chordNote = false, int slice = -1);
    // Alla klingande röster glider lika många halvtoner som den senast spelade behöver för att nå `semitones`.
    // false om den senast spelade inte klingar (då spelas en ny ton).
    bool glideTo (int semitones, int samples);
    void release();
    void fadeOutQuickly (double milliseconds);

    // Med en röst: rösten precis som den är (källa + amplitud). Med fler: summan av rösterna
    // gånger deras amplitud, och amplitud 1.
    Output next();
    bool isActive() const;
    float getModulationValue (int source) const;
    juce::uint64 getNoteCount() const { return noteCounter; } // antal startade toner (Live Code ENV startar på ändringen)
    // Den senast spelade rösten grundton i Hz (0 när den inte klingar): det FUNDAMENTAL DUCK dämpar.
    float getFundamentalHz() const { return voices[(size_t) lastVoice].isActive() ? voices[(size_t) lastVoice].getFrequencyHz() : 0.0f; }
    float getSampleFraction() const { return voices[(size_t) lastVoice].getSampleFraction(); }
    // TIMING-diagrammen: den senast spelade rösten tid sedan anslaget (-1 när den är tyst) och tiden för släppet (-1 medan tonen hålls).
    double playTimeSeconds() const { return voices[(size_t) lastVoice].playTimeSeconds(); }
    double noteOffTimeSeconds() const { return voices[(size_t) lastVoice].noteOffTimeSeconds(); }
    // Röster som klingar eller väntar på en ton (för test och gränssnitt).
    int countSounding() const;
    // Tonen (eller glidemålet) röst k är på väg mot (för test).
    int getVoiceTarget (int voice) const { return voices[(size_t) voice].getTargetSemitones(); }

private:
    struct Slot
    {
        bool fading = false;         // tonas ut på 3 ms (stulen eller över VOICES)
        float fadeGain = 1.0f;
        bool pending = false;        // en ton väntar på att rampen ska bli klar
        int pendingSemitones = 0, pendingGate = 0, pendingSlice = -1;
        float pendingVelocity = 0.0f;
        juce::uint64 order = 0;      // när tonen började (för "äldsta")
    };

    bool occupied (int voice) const;
    void startNote (int voice, int semitones, float velocity, int gateSamples, int slice);
    void startFade (int voice);
    int chooseVictim() const;

    std::array<InstrumentSynth, maxVoices> voices;
    std::array<Slot, maxVoices> slots;
    InstrumentParams params;
    double bpm = 120.0;
    int numVoices = 1;
    int lastVoice = 0;
    bool busy = false;               // fler än en röst, eller röster som tonas ut: summera i next()
    float fadeStep = 0.0f;
    juce::uint64 noteCounter = 0;
    // RANDOM med flera röster: ett tal per ton ur en gemensam följd (varje röst har annars sin egen).
    juce::Random modRandom { 99 };
    float randomValue = 0.0f;
    bool lastNotePoly = false;
};
