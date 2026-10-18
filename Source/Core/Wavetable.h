#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// En wavetable: en serie vågformer (frames) som WT POS sveper igenom, som i Serum.
//
// Varje frame lagras band-begränsad i en version per oktav (mip-nivåer), så att höga toner
// inte viker ner som falska övertoner: en ton läser den nivå vars övertoner ryms under Nyquist.
class Wavetable
{
public:
    static constexpr int frameSize = 1024; // samples per vågform (upp till 512 övertoner)
    static constexpr int numFrames = 16;
    static constexpr int numLevels = 10;   // 512, 256, ... 1 övertoner
    static constexpr int maxHarmonics = 256; // högsta övertonen i en additiv tabell

    // Formen som funktion av position (0..1 genom tabellen) och fas (0..1), värde -1..1.
    using Shape = std::function<double (double position, double phase)>;

    // Formen som övertoner: för varje position fyller funktionen `amp[k - 1]` och `phase[k - 1]` (radianer, sinus + fas) för
    // övertonerna k = 1..count. Går mycket fortare att bygga än en samplad form, och är redan band-begränsad.
    using Additive = std::function<void (double position, double* amp, double* phase, int count)>;

    Wavetable (juce::String name, const Shape& shape, int analysisOrder = 13);
    Wavetable (juce::String name, const Additive& additive, int harmonicCount);

    const juce::String& getName() const { return name; }

    // Ett sampel. `position` 0..1 (frames interpoleras), `phase` 0..1, `maxHarmonics` = hur
    // många övertoner som ryms (Nyquist / tonens frekvens). `morph` av: positionen snäpps till ett av tabellens steg (stepCount) och
    // frame läses direkt, utan interpolation mellan frames: ljudet hoppar mellan vågformerna i stället för att tona över.
    float sample (float position, double phase, double maxHarmonics, bool morph = true) const;

    // Antal vågformer som Morph av hoppar mellan (1..numFrames): Basic 4 (sinus, triangel, såg, fyrkant), övriga inbyggda 16, en tabell av
    // ljud så många cykler filen hade (en enkel cykel 1). Sparas med tabellen, så att ett projekt låter likadant när det laddas om.
    int getSteps() const { return steps; }
    void setSteps (int count) { steps = juce::jlimit (1, numFrames, count); }
    int stepFrame (float position) const; // frame som Morph av spelar vid positionen

    // En frame utan band-begränsning (för gränssnittets bild), frameSize samples.
    const float* displayFrame (int frame) const { return frameData (0, frame); }

private:
    const float* frameData (int level, int frame) const
    {
        return data.data() + ((size_t) level * numFrames + (size_t) frame) * (frameSize + 1);
    }

    // Skriver alla mip-nivåer för en frame ur ett spektrum (re/im-par, bin h på plats 2h och 2h+1).
    void storeFrame (int frame, std::vector<float>& spectrum);

    juce::String name;
    int steps = numFrames;
    std::vector<float> data; // [nivå][frame][frameSize + 1], sista sampelt = första (för interpolation)
};

// De inbyggda tabellerna och deras val i TABLE-listan (parametern): de 14 första är de ursprungliga, plats 14 är "User WAV"
// (oscillatorns egen inladdade tabell) och därefter kommer fler inbyggda. Platserna ändras aldrig, eftersom sparade projekt och
// presets pekar på dem.
//
// Tabellerna byggs när de först behövs (några hundra millisekunder för de flesta), inte alla på en gång: en inbyggd tabell tar
// ett par MB. Ljudtråden bygger aldrig: är tabellen inte klar spelas "Basic" tills TableBuilder (en bakgrundstråd) byggt den.
namespace Wavetables
{
    int builtInCount();             // antal inbyggda tabeller (utan "User WAV")
    int userIndex();                // TABLE-listans val för "User WAV": oscillatorns egen inladdade tabell
    int listSize();                 // antal val i TABLE-listan: de inbyggda och "User WAV"
    juce::StringArray names();      // namnen i TABLE-listans ordning
    juce::String categoryOf (int choice); // "Analog", "FM" ...; "User" för User WAV
    int classicIndex (int shape);   // 0 sinus, 1 triangel, 2 såg, 3 fyrkant: valet för de klassiska vågformerna

    const Wavetable& get (int choice);  // inbyggd tabell; "Basic" om den inte är byggd än (bygger inte på ljudtråden)
    void ensure (int choice);           // bygger tabellen nu om den inte finns (blockerar)
    void ensureAll();                   // bygger alla (för tester och verktyg)
    bool isReady (int choice);

    // Mappen med egna tabeller (WAV-filer, med undermappar som kategorier): Dokument\Kickbase\Wavetables.
    juce::File libraryFolder();

    // En tabell av ljud (som i Serum): en fil med cykler om 2048 samples blir upp till 16 frames
    // som POS sveper igenom; en kort fil räknas som en enda cykel. `framesOut` får de 16 frames
    // (numFrames * frameSize) som tabellen byggdes av, så att den kan sparas i projektet.
    std::unique_ptr<Wavetable> fromAudio (const std::vector<float>& mono, const juce::String& name, std::vector<float>& framesOut);
    std::unique_ptr<Wavetable> fromFrames (const juce::String& name, const std::vector<float>& frames, int steps = Wavetable::numFrames);
}

// Bakgrundstråden som bygger tabeller som ljudtråden bett om men som inte är klara. Hålls av pluginets processor (delad, så att
// den bara finns medan något plugin lever).
class TableBuilder : private juce::Thread
{
public:
    TableBuilder();
    ~TableBuilder() override;

private:
    void run() override;
};
