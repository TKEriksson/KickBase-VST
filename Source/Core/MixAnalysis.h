#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class PluginBase;

// Mixanalysen (ANALYZER-fliken, knappen ANALYSE, och AI-verktyget analyse_mix): spelar mönstret ett helt varv offline och mäter
// hur kicken, varje instrument och mastern delar på frekvenserna, takt för takt, och skriver en rapport med saker man bör titta på.
//
// Principer (så att rapporten inte ger falsklarm):
//  - Mätningen görs på de verkliga signalerna efter duckningen och EQ:n, så en kollision som sidechainen redan löst syns inte.
//  - Frekvensaxeln delas i band som ungefär följer hörselns kritiska band (smalare i basen, där kick och bas bor).
//  - En källa räknas bara som "spelande" i ett band när den ligger över ett golv i förhållande till mixens starkaste band OCH inte långt
//    under sin egen starkaste nivå. Utsvängningar i utkanten av en källas spektrum räknas alltså inte.
//  - Två källor krockar bara när båda spelar, ungefär lika starkt, under en tillräckligt stor del av tiden i takten (inte vid enstaka ögonblick).
//  - Att kick och bas ligger på samma frekvens samtidigt är inte fel i sig (kicken vinner på sina slag, basen tar luckorna), så sådana
//    fynd begränsas till varning och beskriver vad som kan göras.
//  - Känsligheten (låg, normal, hög) skalar marginaler, golv och hur stor del av tiden som krävs (jämför Neutrons Masking Sensitivity).
namespace MixAnalysis
{
    constexpr int fftOrder = 13;
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hopSamples = 2048;
    constexpr int lowBins = 64; // de lägsta stegen sparas finare (grundtoner)
    constexpr int numInstruments = 10;
    constexpr int kickSource = 0, firstInstrument = 1, masterSource = 1 + numInstruments, numSources = masterSource + 1;

    // Bandgränserna i Hz (numBands() + 1 värden).
    const std::vector<float>& bandEdges();
    int numBands();

    struct Options
    {
        int sensitivity = 1; // 0 = låg, 1 = normal, 2 = hög
        int maxBars = 16;    // högst så många takter analyseras
    };

    enum class Severity { info, warning, problem };

    struct Finding
    {
        Severity severity = Severity::info;
        juce::String category;      // "collision", "masking", "kick", "sub", "master", "reference"
        juce::String title, detail;
        juce::StringArray sources;
        float lowHz = 0.0f, highHz = 0.0f;
        std::vector<int> bars;      // 1-baserade takter där det händer
        float share = 0.0f;         // andel av analyserade takter
    };

    struct SourceInfo
    {
        juce::String name;
        bool present = false;
        float peakDb = -120.0f;     // starkaste bandet
        float fundamentalHz = 0.0f; // lägsta starka grundtonen, 0 = ingen
        juce::String note;
    };

    struct Capture;

    // Vad som tas med när analysen sparas eller hämtas som data (allt som standard).
    struct ExportOptions
    {
        bool findings = true;   // fynden (krockar m.m.)
        bool sources = true;    // översikt per källa (toppnivå, grundton, medelspektrum över hela loopen)
        bool bars = true;       // bandnivåer (medel och topp) för varje källa och takt
        bool harmonics = true;  // grundton och övertonsserie för varje källa och takt
        bool spectra = true;    // spektrum i 1/12-oktavsteg för varje källa och takt
        bool frames = true;     // bandnivåer för varje ram genom hela loopen (bara i filen: mycket data)
        bool kicks = true;      // kickens slag (takt, taktslag) och hur mycket varje instrument duckades vid dem
        bool modulation = true; // hur moduleringen rör sig genom loopen: källornas värden (LFO, makron, envelopes) och de modulerade parametrarnas verkliga värden, per ram, och limiterns sänkning
        bool presets = true;    // vilka presets som är laddade (hela, kick, varje instrument, effekter, sidechain ...)
        bool overview = true;   // alla inställningar ordnade: kicken, varje instruments oscillatorer (A/B/C med wavetable), filter, envelopes, EQ, effektkanalerna med platser, mastern, mixern, modulering
        bool dynamics = true;   // nivå (RMS, topp, crest), ljusstyrka (spektral tyngdpunkt) och energi i sex frekvensområden för varje källa och takt (och varje ram)
        bool about = true;      // en förklaring för en AI som inte känner till Kickbase: vad pluginet är, vad datan är och hur man analyserar den
        bool synth = true;      // alla inställningar: oscillatorer, wavetables, filter, effekter, modulering, mönster och toner

        juce::String toString() const;
        static ExportOptions fromString (const juce::String& text);                 // bokstäver: f s b h p r k y
        static ExportOptions fromNames (const juce::StringArray& names, bool allIfEmpty = true);
    };

    // Begränsar vad som tas med: några källor, några takter, ett frekvensområde, och hur tätt ramarna tas (för att hålla datan liten).
    struct ExportFilter
    {
        juce::StringArray sources;   // delar av namn, t.ex. "kick" eller "instr 2" (tom = alla)
        std::vector<int> bars;       // 1-baserade takter (tom = alla)
        float minHz = 0.0f, maxHz = 1.0e9f;
        int frameStep = 1;           // varannan ram = 2 ...

        static std::vector<int> parseBars (const juce::var& value); // en lista med tal, eller en text som "1-4,7"
        bool wantsBar (int bar1Based) const;
    };

    struct Report
    {
        bool valid = false;
        juce::String error;
        int bars = 0, sensitivity = 1;
        double bpm = 0.0, seconds = 0.0;
        std::vector<SourceInfo> sources;
        std::vector<Finding> findings;
        float kickTailHz = 0.0f, limiterMaxDb = 0.0f;
        juce::String referenceName;
        std::shared_ptr<const Capture> capture; // rådata från körningen (för sparad analys)
        juce::var synthState;                   // alla inställningar när analysen startade (om anroparen gav dem)
        juce::var aiNotes;                      // en AI:s anteckningar i rapporten (förslag och det som fixats), om anroparen gav dem

        juce::var toVar() const;
        juce::String toText() const;

        // Allt man kan få ut: se ExportOptions. `sourceFilter` begränsar källorna (delar av namn, t.ex. "kick" eller "instr 2").
        juce::var toExportVar (const ExportOptions& options, const ExportFilter& filter = {}) const;
        juce::String toExportJson (const ExportOptions& options, const ExportFilter& filter = {}) const; // som toExportVar, med nivåerna för varje ram om options.frames
        bool saveExport (const juce::File& file, const ExportOptions& options, const ExportFilter& filter = {}) const;
    };

    // En referenslåt: långtidsmedelvärdet av spektrumet (effekt per steg).
    struct Reference
    {
        juce::String name;
        double rate = 44100.0;
        std::vector<float> power; // fftSize / 2 steg
    };

    std::shared_ptr<const Reference> loadReference (const juce::File& file, juce::String& error);

    // Rådata från en körning. Öppen så att utvärderingen kan testas på påhittade data.
    struct Capture
    {
        int frames = 0, bars = 0;
        double rate = 48000.0, bpm = 140.0;
        std::array<bool, numSources> present {};
        std::array<juce::String, numSources> names;
        std::vector<float> bandDb;     // [ram][källa][band], dB
        std::vector<float> lowPower;   // [ram][källa][lowBins], effekt
        std::vector<int> frameBar;     // takt (0-baserad) per ram
        std::vector<std::array<float, numInstruments>> duckDepthDb; // per kickslag: hur mycket varje instrument duckades (dB)
        float limiterMaxReductionDb = 0.0f;
        std::vector<double> frameSeconds;       // ramens mitt, sekunder från loopens början
        std::vector<double> kickOnsetSeconds;   // kickens slag
        std::vector<float> barPower;            // [takt][källa][fftSize/2]: medeleffekt per steg över takten
        std::vector<int> barFrameCount;         // antal ramar per takt
        std::vector<float> framePeakDb, frameRmsDb; // [ram][källa]: toppnivå och RMS över ramens 171 ms (dB, 0 = full skala)
        std::vector<float> frameDuckDb;         // [ram][instrument]: hur mycket instrumentet duckades under ramen (dB)
        std::vector<float> frameLimiterDb;      // [ram]: limiterns största sänkning under ramen (dB)
        std::vector<juce::String> modSourceNames, modTargetIds; // de modulationskällor och parametrar som har kopplingar
        std::vector<float> frameModSources;     // [ram][källa]: källans värde (LFO -1..1, övriga 0..1)
        std::vector<float> frameModTargets;     // [ram][parameter]: parameterns verkliga värde med modulering, i dess egen enhet

        float& band (int source, int frame, int b);
        float band (int source, int frame, int b) const;
        void resize (int frameCount);
    };

    Report evaluate (const Capture& capture, const Options& options, const Reference* reference);

    // Förbereder en kopia av pluginet (meddelandetråden) och kör analysen på den (valfri tråd). `progress` får 0..1.
    std::unique_ptr<PluginBase> prepareCopy (PluginBase& source);
    Report run (PluginBase& preparedCopy, double bpm, int keyNote, const Options& options, const Reference* reference,
                const std::function<void (float)>& progress, const std::function<bool()>& cancelled);
}

// Kör analysen på en egen tråd, med förlopp, så att gränssnittet inte hänger sig.
class MixAnalyser : private juce::Thread
{
public:
    MixAnalyser();
    ~MixAnalyser() override;

    // Meddelandetråden.
    bool start (PluginBase& source, const MixAnalysis::Options& options, std::shared_ptr<const MixAnalysis::Reference> reference, juce::var synthState = {});
    void cancel();

    bool isRunning() const { return running.load(); }
    float getProgress() const { return progress.load(); }
    MixAnalysis::Report getReport() const;
    int getVersion() const { return version.load(); } // ökar när en ny rapport är klar
    bool hasReport() const { return version.load() > 0; }

private:
    void run() override;

    std::unique_ptr<PluginBase> copy;
    MixAnalysis::Options options;
    std::shared_ptr<const MixAnalysis::Reference> reference;
    juce::var synthState;
    double bpm = 140.0;
    int keyNote = 60;

    std::atomic<bool> running { false };
    std::atomic<float> progress { 0.0f };
    std::atomic<int> version { 0 };
    mutable juce::CriticalSection lock;
    MixAnalysis::Report report;
};
