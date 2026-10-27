#pragma once

#include <juce_core/juce_core.h>
#include "Presets.h"

// Kategorier för kick- och instrument-presets. Alla namn står i den här tabellen (versaler): menyn, spara-dialogen och fabrikslistan använder samma.
// Fabrikspresets får sin kategori härifrån (inte ur PresetData.inl, så att presetens index och hashar är orörda); egna presets har ett valfritt
// attribut `category` i filen.
namespace PresetCategories
{
    // Ordningen här är ordningen i menyerna.
    const juce::StringArray& names();

    // Kategorin för fabrikspresets som saknar egen regel och för egna presets utan attribut i filen.
    juce::String defaultName();

    // Har delen kategorier? (Kick och instrument; övriga delar har platta listor.)
    bool usedBy (Presets::Section section);

    // Fabrikspresetets kategori (alltid ett namn ur names()).
    juce::String factoryCategory (Presets::Section section, const juce::String& presetName);

    // Gör om en sparad text till ett giltigt kategorinamn, eller tom text om den inte finns i tabellen.
    juce::String normalise (const juce::String& text);
}
