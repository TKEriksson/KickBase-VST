#pragma once

#include <juce_core/juce_core.h>

// Inställningar som gäller alla Kickbase-fönster och projekt (inte ett enskilt projekt): mappar för presets, exporter och wavetables,
// AI-länkens åtkomstnyckel och fönstrets storlek. De ligger i en liten XML-fil, Dokument\Kickbase\settings.xml, som läses och skrivs
// vid behov (inget sparas i minnet, så flera plugins och DAW:ar delar samma fil).
namespace Settings
{
    juce::File rootFolder();     // Dokument\Kickbase: här ligger inställningsfilen och AI-länkens register
    juce::File file();           // settings.xml

    juce::String get (const juce::String& key, const juce::String& fallback = {});
    void set (const juce::String& key, const juce::String& value); // tomt värde tar bort inställningen (= standard)

    // Mapparna: egen mapp om den är vald (och finns), annars standard under rootFolder.
    juce::File presetsFolder();    // Presets
    juce::File exportsFolder();    // Exports
    juce::File wavetablesFolder(); // Wavetables
    juce::File defaultFolder (const juce::String& key); // standardmappen för "presetsFolder", "exportsFolder" eller "wavetablesFolder"
    void setFolder (const juce::String& key, const juce::File& folder); // ogiltig/tom mapp = tillbaka till standard

    // AI-länkens åtkomstnyckel: är den satt godtar Kickbase bara anrop från en brygga som har samma nyckel (KICKBASE_KEY).
    juce::String aiKey();
    juce::String makeKey();      // en ny slumpmässig nyckel

    // AI-chatten i synten: användarens egen API-nyckel (Anthropic) och vilken modell som används. Nyckeln sparas krypterad (Windows DPAPI,
    // bara den här användaren kan läsa den på den här datorn) och skickas bara till api.anthropic.com.
    juce::String chatApiKey();
    void setChatApiKey (const juce::String& key);   // tom = ta bort
    juce::String chatModel();                       // standard: claude-sonnet-5-5
    void setChatModel (const juce::String& model);

    // Fönstrets skala (1 = fullstor): 0 = ingen vald, då används standardstorleken.
    float uiScale();
    void setUiScale (float scale);

    // Bara för tester: använd en annan fil.
    void useFile (const juce::File& file);
}
