#pragma once

#include <map>
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

    // Live Code: bit för en rad i liveCodeLocks / liveCodeOwned (radid: 0-9 instrument, 10 kicken, 11-63 underrader).
    static juce::uint64 liveCodeBit (int layer) { return 1ull << layer; }

    // Radid ur ett verktygsargument: 0-9 (heltal eller siffertext), "kick" eller en nyckel som "i3.sample" (0-baserad, som sparfilens `row`).
    // create:true skapar en underrad som saknas. Svar: id, -1 = giltig nyckel men raden finns inte (eller poolen full), -2 = ogiltigt argument.
    int rowOfArg (const juce::var& arg, bool create) const;
    // Live Code F1: parametrar som koden kan läsa (id -> värde i egna enheter, utan effektplatsernas) och skriva.
    std::map<juce::String, double> liveCodeParameters() const;
    // Live Code F3: effektkatalogen koden ser: {types:[namn], knobs:{typ:[rattnamn]}, slots:[[typ x5] x11]} (ögonblicksbild från före körningen).
    juce::var liveCodeEffects() const;
    // Rad (0-9 instrument, 10 kick via liveCodeBit) som parametern hör till, -1 om ingen (master, lfo, fx ...).
    static int liveCodeLayerOf (const juce::String& parameterId);

private:
    juce::var applyLiveCode (const juce::var& items, bool overwrite, const juce::var& mods, bool prune = false); // prune: rader koden äger men inte längre nämner töms (AUTO)
    juce::StringArray applyCodeMods (const juce::var& mods);
    juce::var liveCodeLock (const juce::var& args);
    juce::var listParameters (const juce::var& args) const;
    juce::var listEffects() const;
    juce::var getState (const juce::var& args) const;
    juce::var setParameters (const juce::var& args);
    juce::var setEffect (const juce::var& args);
    juce::var setKickEnvelopes (const juce::var& args);
    juce::var setPattern (const juce::var& args);
    juce::var setGate (const juce::var& args);
    juce::var setFm (const juce::var& args);
    juce::var editWavetableHarmonics (const juce::var& args);
    juce::var analyseWavetable (const juce::var& args);
    juce::var liveCodeCreate (const juce::var& args);
    juce::var liveCodeRun (const juce::var& args);
    juce::var liveCodeStop();
    juce::var setSidechainCurve (const juce::var& args);
    juce::var setModulation (const juce::var& args);
    juce::var listPresets (const juce::var& args) const;
    juce::var loadPreset (const juce::var& args);
    juce::var savePreset (const juce::var& args);
    juce::var renderPreview (const juce::var& args);
    juce::var compareRenders (const juce::var& args) const;
    juce::var sweepParameter (const juce::var& args);
    juce::var scoreVsReference (const juce::var& args);
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
    std::map<juce::String, juce::var> renderedPreviews; // render_preview-namn -> {metrics, checks}, för compare_renders
};
