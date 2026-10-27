#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Settings.h"

// Projektfil (.kbproj): hela plugin-läget som XML-text i en fil (spara, öppna, skicka vidare, lägg i git). Innehållet är samma tillstånd som
// DAW-projektet bär (`getStateInformation`), med två extra läsbara attribut på rotelementet. Ingen parameter, ingen ljudväg.
namespace ProjectFile
{
    constexpr int formatVersion = 1;
    constexpr juce::int64 maxFileBytes = 256ll * 1024 * 1024; // större filer läses inte (de är inte projekt, eller trasiga)

    // Skriver tillståndet till `file` via en temporär fil, så att en avbruten skrivning inte förstör den gamla. `pluginVersion` skrivs som attribut.
    bool save (juce::AudioProcessor& processor, const juce::File& file, const juce::String& pluginVersion, juce::String& error);

    // Läser `file` och sätter tillståndet. `rootType` är parameterträdets rotnamn (apvts.state.getType()); filer med annan rot avvisas. Vid fel (saknas, för stor, inte XML, fel rot, okänd eller nyare version) ändras ingenting och `error` förklarar.
    bool load (juce::AudioProcessor& processor, const juce::Identifier& rootType, const juce::File& file, juce::String& error);

    // Autospara: en timer på meddelandetråden skriver tillståndet till `folder` när det har ändrats sedan förra gången (jämförs byte för byte med
    // förra tillståndsblobben). Tre roterande filer per instans (autosave-<id>-0..2.kbproj), och de äldsta över `keepFiles` i mappen tas bort.
    // Första tick sparar bara utgångsläget (ingen fil för en orörd instans). Av/på styrs av inställningen "autosave" (standard på).
    class AutoSaver : private juce::Timer
    {
    public:
        AutoSaver (juce::AudioProcessor& p, juce::File saveFolder) : processor (p), folder (std::move (saveFolder)) {}
        ~AutoSaver() override { stopTimer(); }

        void start (int intervalMs) { startTimer (intervalMs); }
        bool tick();                                       // skriver om något ändrats; true om en fil skrevs (public för tester)
        juce::File lastFile() const { return written; }
        static constexpr int slots = 3;
        static constexpr int keepFiles = 12;
    private:
        void timerCallback() override { if (juce::String (Settings::get ("autosave")) != "0") tick(); }
        juce::AudioProcessor& processor;
        juce::File folder, written;
        juce::String id { juce::String::toHexString (juce::Random::getSystemRandom().nextInt()).paddedLeft ('0', 8) };
        juce::MemoryBlock lastState;
        bool haveLast = false;
        int slot = 0;
    };
}
