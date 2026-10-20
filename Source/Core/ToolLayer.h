#pragma once

#include "MixAnalysis.h"
#include "PluginBase.h"

// Verktygslagret: det en AI-agent (via MCP-bryggan) kan göra med Kickbase. Samma lager används av
// det fristående bryggprogrammet (en egen processor utan DAW) och av plugin:et i DAW:en (när
// länken är påslagen), så att verktygen bara skrivs en gång.
//
// Allt körs på message thread (plugin:et) eller huvudtråden (bryggprogrammet), aldrig på ljudtråden:
// parametrar sätts som en användare skulle (med gest), och rendering sker offline på en kopia av tillståndet.
//
// Varje verktyg tar ett JSON-objekt och ger ett: { "ok": true, "data": {...}, "images": [{ "name", "data" (base64 png) }] }
// eller { "ok": false, "error": "..." }.
class ToolLayer
{
public:
    explicit ToolLayer (PluginBase& processor);

    struct ToolInfo
    {
        juce::String name, description, schema; // schema: JSON Schema för argumenten
    };

    static std::vector<ToolInfo> tools();

    // Texten i en guide ("agent", "psytrance" eller "school").
    static juce::String guide (const juce::String& topic);

    juce::var call (const juce::String& tool, const juce::var& args);

private:
    juce::var listParameters (const juce::var& args) const;
    juce::var listEffects() const;
    juce::var getState (const juce::var& args) const;
    juce::var setParameters (const juce::var& args);
    juce::var setEffect (const juce::var& args);
    juce::var setKickEnvelopes (const juce::var& args);
    juce::var setPattern (const juce::var& args);
    juce::var setGate (const juce::var& args);
    juce::var setSidechainCurve (const juce::var& args);
    juce::var setModulation (const juce::var& args);
    juce::var listPresets (const juce::var& args) const;
    juce::var loadPreset (const juce::var& args);
    juce::var savePreset (const juce::var& args);
    juce::var renderPreview (const juce::var& args);
    juce::var analyseMix (const juce::var& args);
    juce::var getMixReport (const juce::var& args);
    juce::var saveMixReport (const juce::var& args);
    juce::var addReportNote (const juce::var& args);
    juce::var analyzeAudio (const juce::var& args);
    juce::var pianoRollFiles (const juce::String& tool, const juce::var& args);
    juce::var loadKickSample (const juce::var& args);
    juce::var listLibrary (const juce::var& args);
    juce::var loadLibraryKick (const juce::var& args);
    juce::var loadInstrumentSample (const juce::var& args);
    juce::var undo (bool redo);

    void snapshot();
    int parameterIndex (const juce::String& id) const;

    PluginBase& processor;
    std::vector<juce::MemoryBlock> undoStack, redoStack;
};
