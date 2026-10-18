#pragma once

#include "LibraryManifest.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

// Import till biblioteket (fas A i docs/content-plan.md): läser ljudfiler, gör om dem till bibliotekets format och skriver ett paket
// med manifest.json. Allt sker på anroparens tråd (verktyget, aldrig ljudtråden).
//
// Formatet: mono, 44,1 kHz, 24-bit WAV. Tystnad trimmas före attacken och efter slutet (inget annat klipps), det blir en kort fade bara i
// slutet, och nivån normaliseras till -1 dBFS sample peak. Kickar blir högst 2 s (kickens SampleSlot rymmer 2 s), övrigt högst 30 s.
namespace Library
{
    constexpr double libraryRate = 44100.0;
    constexpr double normaliseDb = -1.0;     // sample peak efter importen
    constexpr double silenceDb = -60.0;      // under detta (relativt toppen) räknas som tystnad i början och slutet
    constexpr int attackPreRoll = 24;        // samplingar tystnad som sparas före attacken (så att inget klick klipps)
    constexpr double endFadeSeconds = 0.010; // fade-out i slutet
    constexpr double maxSourceSeconds = 600.0; // längre källfiler vägras (hela filen läses in i minnet)

    double maxSecondsFor (const juce::String& category); // Kicks = 2 s, övriga 30 s

    // Omsampling till libraryRate med juce::WindowedSincInterpolator. Latensen (getBaseLatency / ratio) kompenseras och svansen töms,
    // så att en topp hamnar där den ska (inom en sampling). Samma samplingsfrekvens kopieras oförändrat.
    std::vector<float> resampleToLibraryRate (const std::vector<float>& input, double sourceRate);

    // Trimmar, fadar och normaliserar (efter omsamplingen). Ger false om ljudet är tyst eller bara består av ogiltiga värden.
    bool conditionAudio (std::vector<float>& audio, double maxSeconds, juce::StringArray& notes, juce::String& error);

    // Läser en fil (valfritt antal kanaler, mixas till mono), omsamplar och förbereder. `out` är klart att skrivas som WAV.
    bool convertFile (const juce::File& source, double maxSeconds, std::vector<float>& out, juce::StringArray& notes, juce::String& error);

    // 24-bit mono WAV vid libraryRate. Samma ljud ger byte för byte samma fil.
    bool writeWav (const juce::File& file, const std::vector<float>& audio);

    bool isAudioFile (const juce::File& file); // wav, aif, aiff, flac, ogg, mp3 (efter ändelsen)
    juce::Array<juce::File> findAudioFiles (const juce::File& fileOrFolder); // en fil, eller alla ljudfiler i mappen (med undermappar), sorterade

    struct ImportOptions
    {
        juce::String category;  // en av Library::categories()
        juce::String package;   // paketets namn (och mappnamn). Tomt = namnet på källmappen / filen
        juce::String license;   // krävs, en av de godkända
        juce::String source;    // vem/var det kommer ifrån. Tomt = url, annars "local files"
        juce::String url;
        juce::String author;
        juce::StringArray tags;
        juce::String date;      // ÅÅÅÅ-MM-DD, tomt = i dag
        // Fyller i en post (namn, upphovsperson, url, taggar) för en källfil, t.ex. med uppgifterna från Freesound. Tomt = bara filnamnet.
        std::function<void (const juce::File& source, Item& item)> decorateItem;
    };

    struct ImportResult
    {
        juce::File packageFolder;
        int imported = 0;
        juce::StringArray notes;  // t.ex. "kick.wav: shortened to 2 s"
        juce::StringArray errors; // filer som inte gick att importera, eller skäl att inte börja alls
        bool ok() const { return errors.isEmpty(); }
    };

    // Importerar filerna till libraryRoot\<Kategori>\<Paket>\. Vägrar utan godkänd licens, med okänd kategori eller om paketet redan finns med
    // en annan licens. Finns paketet läggs filerna till (samma namn får ett nummer efter). Inget skrivs om något av det första fallerar.
    ImportResult importFiles (const juce::Array<juce::File>& files, const ImportOptions& options, const juce::File& libraryRoot);
}
