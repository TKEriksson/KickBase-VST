#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Pattern.h"

// Pianorullar som filer: egna "piano roll"-presets (ett lagers toner, som kan laddas i vilket lager som helst) och
// MIDI-export/import. Pianorullens egna enhet är 96 ticks per fjärdedel, vilket också används som MIDI-upplösning.
namespace RollFiles
{
    // Mappen för sparade pianorullar: Dokument\Kickbase\Presets\Piano Roll.
    juce::File folder();
    juce::Array<juce::File> list();

    // Sparar lagrets toner (och mönsterlängd och rutnät) som en fil.
    bool save (const Pattern& pattern, int layer, int bars, BassGrid grid, const juce::File& file);

    // Ersätter lagrets toner med filens. Ger mönsterlängd och rutnät som filen sparades med.
    bool load (Pattern& pattern, int layer, const juce::File& file, int& bars, BassGrid& grid);

    // Skriver lagrets toner som en MIDI-fil (tempo, 4/4, en spår). `rootMidiNote` är basens grundton (tonhöjden 0);
    // glide-toner skrivs som vanliga toner, eftersom MIDI inte har glide-toner.
    bool exportMidi (const Pattern& pattern, int layer, int bars, double bpm, int rootMidiNote, const juce::File& file);

    // Ersätter lagrets toner med en MIDI-fil: toner som ligger utanför ±24 halvtoner från `rootMidiNote` flyttas
    // oktavvis in. Ger mönsterlängden som behövs (1, 2 eller 4 takter).
    bool importMidi (Pattern& pattern, int layer, const juce::File& file, int rootMidiNote, int& bars);

    int barsToChoice (int bars);
}
