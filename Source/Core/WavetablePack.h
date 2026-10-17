#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>

// Installerar vågformspaketet AKWF (Adventure Kid Waveforms, över 4000 enkelcykel-filer, CC0 / public domain) i wavetable-mappen, från
// inställningssidan. Hämtningen (cirka 20 MB från GitHub) görs i en bakgrundstråd när man trycker på knappen, och bara då: Kickbase
// skickar inget över nätet i övrigt.
class WavetablePack : private juce::Thread
{
public:
    WavetablePack();
    ~WavetablePack() override;

    static constexpr const char* sourceUrl = "https://github.com/schollz/AKWF-FREE/archive/refs/heads/master.zip";

    // Antal WAV-filer som redan ligger i paketets mapp (Wavetables\AKWF).
    static int installedCount();
    static juce::File packFolder();

    // Packar upp vågformerna (och licensen) ur arkivet till biblioteket. `progress(0..1, antal)` kan returnera false för att avbryta.
    // Ger antalet WAV-filer som skrevs, eller -1 om det avbröts.
    static int extract (const juce::File& archive, const std::function<bool (float, int)>& progress = {});

    void install();                          // börjar hämta och packa upp (inget händer om det redan pågår)
    void cancel();
    bool isBusy() const { return busy; }
    float getProgress() const { return progress; }     // 0..1
    juce::String getStatus() const;                    // en rad text: vad som pågår, eller att det gick bra / fel

    std::function<void()> onFinished;                  // anropas på message thread när det är klart (eller gick fel)

private:
    void run() override;
    void setStatus (const juce::String& text);
    void finish (const juce::String& text);

    std::atomic<bool> busy { false };
    std::atomic<float> progress { 0.0f };
    mutable juce::CriticalSection statusLock;
    juce::String status;
};
