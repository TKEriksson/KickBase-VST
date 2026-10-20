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
    juce::StringArray models();

    // Verktygsschemat som Gemini accepterar (en delmängd av OpenAPI): tar bort additionalProperties/default/pattern, typlistor blir
    // anyOf, och objekt utan fasta fält (fria nyckel/värde-kartor) blir en sträng med JSON (se restoreArguments).
    juce::var sanitizeSchema (const juce::var& schema);

    // Hela anropet. messages är chattens samtal (Anthropics format).
    juce::var buildRequest (const juce::String& system, const std::vector<ToolLayer::ToolInfo>& tools, const juce::Array<juce::var>& messages);

    // Geminis svar som {content:[text|tool_use], stop_reason, usage:{input_tokens, output_tokens}}. Void vid fel (error fylls i:
    // blockerat svar, tomt svar, ...). Argument som schemat kräver som JSON-sträng packas upp igen.
    juce::var parseResponse (const juce::var& response, const std::vector<ToolLayer::ToolInfo>& tools, juce::String& error);

    // Ett begripligt felmeddelande för ett svar med httpStatus >= 400 (body = hela svarstexten).
    juce::String errorMessage (int httpStatus, const juce::String& body);

    // Provar en nyckel med ett litet anrop (blockerar: anropa från en bakgrundstråd). Tom ok = fungerar; annars felet.
    struct KeyTest { bool ok = false; juce::String message; };
    KeyTest testKey (const juce::String& key);
}
