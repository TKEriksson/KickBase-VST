#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class PluginBase;

// Mixanalysen (SPECTRUM-fliken, knappen ANALYSE, och AI-verktyget analyse_mix): spelar mönstret ett helt varv offline och mäter
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

        juce::var toVar() const;
        juce::String toText() const;
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
    bool start (PluginBase& source, const MixAnalysis::Options& options, std::shared_ptr<const MixAnalysis::Reference> reference);
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
    double bpm = 140.0;
    int keyNote = 60;

    std::atomic<bool> running { false };
    std::atomic<float> progress { 0.0f };
    std::atomic<int> version { 0 };
    mutable juce::CriticalSection lock;
    MixAnalysis::Report report;
};
