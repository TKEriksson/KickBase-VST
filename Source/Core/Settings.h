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
    juce::File folderOf (const juce::String& key); // en av nycklarna presetsFolder, exportsFolder, wavetablesFolder, libraryFolder
    juce::File libraryFolder();    // Library (kickar, basar, synthljud och loopar, se Core/LibraryManifest.h)
    juce::File defaultFolder (const juce::String& key); // standardmappen för "presetsFolder", "exportsFolder", "wavetablesFolder" eller "libraryFolder"
    void setFolder (const juce::String& key, const juce::File& folder); // ogiltig/tom mapp = tillbaka till standard

    // AI-länkens åtkomstnyckel: är den satt godtar Kickbase bara anrop från en brygga som har samma nyckel (KICKBASE_KEY).
    juce::String aiKey();
    juce::String makeKey();      // en ny slumpmässig nyckel

    // Krypterar en text för den här användaren på den här datorn (Windows DPAPI; annars bara base64) och tillbaka. Används för AI-nyckeln
    // och för Freesound-verktygets token. Aldrig i klartext i inställningsfilen.
    juce::String protect (const juce::String& plain);
    juce::String unprotect (const juce::String& stored);

    // AI-chatten i synten: användarens egen API-nyckel (Anthropic eller Gemini) och vilken modell som används. Nycklarna sparas krypterade
    // (Windows DPAPI, bara den här användaren kan läsa dem på den här datorn). Anthropic-nyckeln skickas bara till api.anthropic.com,
    // Gemini-nyckeln bara till generativelanguage.googleapis.com.
    juce::String chatApiKey();
    void setChatApiKey (const juce::String& key);   // tom = ta bort
    juce::String chatModel();                       // standard: claude-sonnet-5-5
    void setChatModel (const juce::String& model);
    juce::String chatProvider();                    // "anthropic" (standard) eller "gemini"
    void setChatProvider (const juce::String& provider);
    juce::String geminiApiKey();
    void setGeminiApiKey (const juce::String& key); // tom = ta bort
    juce::String geminiModel();                     // standard: gemini-3.6-flash
    void setGeminiModel (const juce::String& model);
    // Provresultat "kan modellen function calling" per modell och nyckel (nyckelns hash, inte nyckeln): 1 ja, 0 nej, -1 inte provad.
    int geminiToolsCheck (const juce::String& model, const juce::String& key);
    void setGeminiToolsCheck (const juce::String& model, const juce::String& key, bool works);

    // Fönstrets skala (1 = fullstor): 0 = ingen vald, då används standardstorleken.
    float uiScale();
    void setUiScale (float scale);

    // Minskade animationer: lägre bildhastighet för de flesta vyer och ingen glöd i loggan. Värdet minns programmet (det frågas ofta),
    // så ett fönster som redan är öppet följer med först när det öppnas igen (rubriken och spektrumet följer med direkt).
    bool reduceAnimations();
    void setReduceAnimations (bool reduce);
    inline int animationHz (int normalHz) { return reduceAnimations() ? (normalHz > 8 ? normalHz / 2 : normalHz) : normalHz; }

    // Hur ofta vyerna ritas (SETTINGS > OPTIMIZATIONS): ett steg 1-5 per grupp = 5, 10, 20, 30 eller 40 bilder per sekund (standard 30,
    // ANALYZER-spektrumet 20), och om vyer med fyllda ytor fylls (det dyraste att rita). REDUCE ANIMATIONS halverar allt.
    // Vyerna följer en ändring direkt: de jämför visualVersion() i sin timer (se Ui/VisualTimer.h).
    enum class Visual { analyzer, sidechain, maximizer, soundstage, effects, mixer, topBar, other };
    constexpr int numVisuals = 8;
    const char* visualKey (Visual visual);
    int visualFpsStep (Visual visual);
    void setVisualFpsStep (Visual visual, int step);
    inline int fpsForStep (int step) { constexpr int fps[] { 5, 10, 20, 30, 40 }; return fps[juce::jlimit (1, 5, step) - 1]; }
    bool visualFill (Visual visual);
    void setVisualFill (Visual visual, bool fill);
    int visualHz (Visual visual, int baseHz); // vyns takt med valt steg och REDUCE ANIMATIONS
    int visualVersion();                      // ökar vid varje ändring av något ovan (och REDUCE ANIMATIONS)

    // Hur ofta live-spektrumet analyseras (SPEED på ANALYZER-fliken, samma värde): steg 1-5 = 4, 8, 15, 30 eller 40 gånger per sekund
    // (standard 3 = 15), som mest en gång per bildruta.
    int spectrumAnalysisStep();
    void setSpectrumAnalysisStep (int step);
    inline int spectrumAnalysesPerSecond (int step) { constexpr int rate[] { 4, 8, 15, 30, 40 }; return rate[juce::jlimit (1, 5, step) - 1]; }

    // Bara för tester: använd en annan fil.
    void useFile (const juce::File& file);
}
