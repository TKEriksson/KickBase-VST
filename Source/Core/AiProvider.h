#pragma once

#include "ToolLayer.h"

// Googles Gemini-API (generateContent) för AI-chatten. Chatten håller samtalet i Anthropics format (text, tool_use, tool_result);
// här översätts det till Geminis format och Geminis svar tillbaka till Anthropics form, så att verktygsloopen i AiChat::run()
// är densamma för båda leverantörerna. Allt här är rena funktioner (ingen tråd, inget nät) utom testKey(), så att det kan
// provas mot sparade svarsfiler.
// Dokumentation: https://ai.google.dev/api/generate-content och https://ai.google.dev/gemini-api/docs/function-calling
namespace GeminiApi
{
    juce::String endpoint (const juce::String& model);   // https://generativelanguage.googleapis.com/v1beta/models/<model>:generateContent
    juce::String withKey (const juce::String& url, const juce::String& key); // lägger nyckeln som ?key= (bekräftat på ai.google.dev/api); aldrig i loggar/export
    juce::StringArray models();         // listan från models.list när den är hämtad (fetchModels), annars fallbackModels()
    juce::StringArray fallbackModels();
    void setFetchedModels (const juce::StringArray& list);

    // models.list: bara textmodeller (gemini-*) som stöder generateContent. Ren funktion (provas mot sparade svar).
    juce::StringArray parseModelList (const juce::var& response);

    // Provar om modellen kan function calling (blockerar). 1 ja, 0 nej, -1 vet ej (nät, kvot 429, ogiltig nyckel).
    int probeTools (const juce::String& key, const juce::String& model);

    // Hämtar listan med nyckeln (blockerar: bakgrundstråd) och sparar den för models(). Tom vid fel; reservlistan gäller då.
    juce::StringArray fetchModels (const juce::String& key);

    // Verktygsschemat som Gemini accepterar (en delmängd av OpenAPI): tar bort additionalProperties/default/pattern, typlistor blir
    // anyOf, och objekt utan fasta fält (fria nyckel/värde-kartor) blir en sträng med JSON (se restoreArguments).
    juce::var sanitizeSchema (const juce::var& schema);

    // Hela anropet. messages är chattens samtal (Anthropics format).
    // maxOutputTokens: taket per svar (tänkandet räknas in); kostnadsläget sänker det.
    juce::var buildRequest (const juce::String& system, const std::vector<ToolLayer::ToolInfo>& tools, const juce::Array<juce::var>& messages, int maxOutputTokens = 16384);

    // Geminis svar som {content:[text|tool_use], stop_reason, usage:{input_tokens, output_tokens}}. Void vid fel (error fylls i:
    // blockerat svar, tomt svar, ...). Argument som schemat kräver som JSON-sträng packas upp igen.
    juce::var parseResponse (const juce::var& response, const std::vector<ToolLayer::ToolInfo>& tools, juce::String& error);

    // Ett begripligt felmeddelande för ett svar med httpStatus >= 400 (body = hela svarstexten).
    juce::String errorMessage (int httpStatus, const juce::String& body);

    // Provar en nyckel med ett litet anrop (blockerar: anropa från en bakgrundstråd). Tom ok = fungerar; annars felet.
    struct KeyTest { bool ok = false; juce::String message; };
    KeyTest testKey (const juce::String& key);
}
