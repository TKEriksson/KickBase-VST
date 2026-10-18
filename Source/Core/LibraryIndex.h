#pragma once

#include "LibraryManifest.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

// Biblioteksindexet (fas A i docs/content-plan.md): en lista över alla poster i alla giltiga paket under biblioteksmappen, för sök och val i
// gränssnittet och för AI-verktygen. Delas av alla instanser i processen (juce::SharedResourcePointer<LibraryIndex>); bakgrundstråden
// stoppas när sista instansen försvinner.
//
// Regler:
//  - All filläsning sker på bakgrundstråden (aldrig på ljudtråden, aldrig i paint eller en timer).
//  - Ingen skanning i en timer: skanna vid start, när KICK-fliken eller SETTINGS visas (refreshIfChanged) och vid Rescan (rescan).
//  - refreshIfChanged tittar bara på ändringstiderna för biblioteksmappen, kategorimapparna, paketmapparna och manifesten, inte på ljudfilerna.
//  - Paket med ett ogiltigt manifest (okänd licens, trasig JSON ...) visas inte: de syns i problems().
class LibraryIndex : public juce::ChangeBroadcaster, private juce::AsyncUpdater, private juce::Thread
{
public:
    LibraryIndex();
    ~LibraryIndex() override;

    struct Entry
    {
        juce::String category, package, license, source, url;
        juce::String name, author, key;
        juce::StringArray tags;
        int rootNote = -1;
        double bpm = 0.0;
        bool personal = false;       // användarens egna ljud (märks i gränssnittet, hamnar aldrig i en installerare)
        juce::File file;             // absolut sökväg till WAV-filen
        juce::String id() const { return category + "\\" + package + "\\" + file.getFileName(); }
    };
    using Snapshot = std::shared_ptr<const std::vector<Entry>>;

    // Aktuell lista (aldrig null). Kan anropas från vilken tråd som helst.
    Snapshot entries() const;
    juce::StringArray problems() const;   // ett fel per rad: paketet som inte lästes och varför
    int packageCount() const;             // antal paket med giltigt manifest (de som problems() inte nämner)
    juce::File folder() const;            // mappen som senast skannades
    int version() const { return publishedVersion.load(); } // ökar vid varje ny lista (också en oförändrad vid rescan)
    bool isScanning() const { return scanning.load(); }

    // Ber bakgrundstråden att skanna `root` (från vilken tråd som helst, ger sig direkt). Vid en ändring skickas ett ChangeMessage på message thread.
    void rescan (const juce::File& root);
    // Som rescan, men bara om något ändrats sedan senaste skanningen (ändringstiderna, se ovan) eller om mappen är en annan.
    void refreshIfChanged (const juce::File& root);

    // Skannar på anroparens tråd och publicerar direkt (verktygen och testerna). Ger antalet poster.
    int scanNow (const juce::File& root);
    // Väntar tills bakgrundstråden är klar med det den fått beställt (testerna). false om det tog längre än `timeoutMs`.
    bool waitUntilIdle (int timeoutMs = 10000);
    // Stoppar bakgrundstråden: en pågående skanning avbryts mellan kategorier, paket och manifest (utan att publicera). Ger false om tråden
    // inte hann stoppas inom `timeoutMs` (då skulle JUCE ha dödat den med våld). Destruktorn anropar den; den kan också anropas före.
    bool shutDown (int timeoutMs = 5000);

    // Filter för listan: kategori (tom = alla) och sökord (alla ord måste finnas i namn, paket, taggar, upphovsperson eller licens). Ger index i `list`.
    static std::vector<int> filter (const std::vector<Entry>& list, const juce::String& category, const juce::String& search);

private:
    struct Scan
    {
        Snapshot entries;
        juce::StringArray problems;
        juce::String signature;
        int packages = 0;
        bool aborted = false;
    };
    using AbortCheck = std::function<bool()>; // tomt = fortsätt alltid

    static Scan scan (const juce::File& root, const AbortCheck& shouldAbort);
    static juce::String signatureOf (const juce::File& root, const AbortCheck& shouldAbort, bool& aborted); // ändringstiderna, ingen filinnehållsläsning
    void publish (Scan&& result, const juce::File& root);
    void run() override;
    void handleAsyncUpdate() override;

    mutable juce::CriticalSection lock;       // skyddar det som delas mellan trådarna (aldrig ljudtråden)
    Snapshot current;
    juce::StringArray currentProblems;
    int currentPackages = 0;
    juce::String currentSignature;
    juce::File currentRoot;

    juce::File requestedRoot;                 // (under lock)
    bool requestPending = false;              // (under lock)
    bool requestForced = false;               // (under lock)
    std::atomic<int> publishedVersion { 0 };
    std::atomic<bool> scanning { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryIndex)
};
