#pragma once

#include <juce_core/juce_core.h>

// Biblioteket (fas A i docs/content-plan.md): Dokument\Kickbase\Library\<Kategori>\<Paket>\manifest.json beskriver ett paket med ljud
// (kickar, basar, synthljud, loopar). Den här filen läser, kontrollerar och skriver manifesten. Den rör aldrig ljudtråden och läser
// inga ljudfiler: bara JSON och filnamn.
//
// Regler (content-plan.md, regel 9): varje post har en källa och en licens. Bara de licenser som står i listan godtas, annars visas
// inte paketet. PERSONAL är användarens egna ljud: de märks i gränssnittet och får aldrig hamna i en installerare (redistributable = false).
namespace Library
{
    // Kategorierna är mappnamn under biblioteket.
    const juce::StringArray& categories(); // Kicks, Bass, Synth, Pads, BassLoops, SynthLoops, Multi

    // Exakt de här licenserna godtas (inga jokertecken): CC0-1.0, MIT, BSD-2-Clause, BSD-3-Clause och PERSONAL.
    bool isAllowedLicense (const juce::String& license);
    inline bool isPersonal (const juce::String& license) { return license == "PERSONAL"; }

    // Ett filnamn i ett manifest ska ligga inuti paketmappen: inga "..", inga absoluta sökvägar, ingen enhetsbokstav, ingen UNC, inte tomt.
    bool isSafeRelativePath (const juce::String& file);

    bool isValidDate (const juce::String& text); // ÅÅÅÅ-MM-DD

    struct Item
    {
        juce::String file;          // relativt paketmappen
        juce::String name;
        juce::StringArray tags;
        int rootNote = -1;          // MIDI-ton (0-127), -1 = okänd
        double bpm = 0.0;           // 0 = okänt
        juce::String key;
        juce::String author;
        juce::String url;           // valfri: sidan för just det här ljudet (t.ex. https://freesound.org/s/<id>/)

        bool operator== (const Item& other) const;
    };

    struct Manifest
    {
        juce::String package;
        juce::String source;
        juce::String url;
        juce::String license;
        bool redistributable = true; // false om och endast om license == PERSONAL
        juce::String date;           // ÅÅÅÅ-MM-DD
        std::vector<Item> items;

        bool operator== (const Manifest& other) const;
    };

    // Resultatet av en tolkning: ett manifest, eller felmeddelanden (varje med paketets namn och fältet). `ok()` = inga fel.
    struct ParseResult
    {
        Manifest manifest;
        juce::StringArray errors;
        bool ok() const { return errors.isEmpty(); }
    };

    // `label` = det som står först i felmeddelandena när paketets namn inte går att läsa (t.ex. mappnamnet).
    // `packageFolder` används för att kontrollera att varje fil ligger inuti paketet, och (om `checkFilesExist`) att den finns.
    ParseResult parse (const juce::String& json, const juce::String& label, const juce::File& packageFolder, bool checkFilesExist = false);
    ParseResult parseFile (const juce::File& manifestFile, bool checkFilesExist = false);

    juce::var toVar (const Manifest& manifest);
    juce::String toJson (const Manifest& manifest);
    bool write (const juce::File& manifestFile, const Manifest& manifest); // skapar mappen vid behov

    constexpr const char* manifestName = "manifest.json";

    // Kontrollerar alla paket under `root` (root\<Kategori>\<Paket>\manifest.json). Ger en rad per paket och en rad per fel.
    struct CheckReport
    {
        int packages = 0;
        int items = 0;
        int personalPackages = 0;
        juce::StringArray lines;  // läsbar rapport
        juce::StringArray errors; // bara felen
    };
    CheckReport checkFolder (const juce::File& root, bool checkFilesExist = true);
}
