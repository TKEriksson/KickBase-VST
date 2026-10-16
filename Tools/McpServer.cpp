// KickbaseMcp: MCP-bryggan. En MCP-server över stdio (för Claude Desktop och andra AI-appar) som ger AI:n
// verktygen i Source/Core/ToolLayer. Den kan köra en egen Kickbase-instans utan DAW ("headless") eller koppla
// upp sig mot en Kickbase i DAW:en (om AI-knappen är påslagen där) via en named pipe.
//
// Stdout är bara för protokollet (en JSON-RPC-rad per meddelande); all loggning går till stderr.

#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/Core/BridgeLink.h"
#include "../Source/Core/ToolLayer.h"
#include "../Source/Instrument/InstrumentProcessor.h"
#include <cstdio>
#include <iostream>
#include <thread>

#if JUCE_WINDOWS
 #include <fcntl.h>
 #include <io.h>
#endif

using juce::var;

namespace
{
    constexpr const char* serverVersion = "0.1.0";

    var object() { return var (new juce::DynamicObject()); }
    void put (var& target, const juce::String& key, const var& value) { target.getDynamicObject()->setProperty (key, value); }

    void send (const var& message)
    {
        const auto text = juce::JSON::toString (message, true);
        std::fwrite (text.toRawUTF8(), 1, text.getNumBytesAsUTF8(), stdout);
        std::fputc ('\n', stdout);
        std::fflush (stdout);
    }

    void log (const juce::String& text)
    {
        std::fprintf (stderr, "[kickbase-mcp] %s\n", text.toRawUTF8());
        std::fflush (stderr);
    }

    var reply (const var& id, const var& result)
    {
        auto message = object();
        put (message, "jsonrpc", "2.0");
        put (message, "id", id);
        put (message, "result", result);
        return message;
    }

    var errorReply (const var& id, int code, const juce::String& text)
    {
        auto error = object();
        put (error, "code", code);
        put (error, "message", text);
        auto message = object();
        put (message, "jsonrpc", "2.0");
        put (message, "id", id);
        put (message, "error", error);
        return message;
    }

    var textContent (const juce::String& text)
    {
        auto content = object();
        put (content, "type", "text");
        put (content, "text", text);
        return content;
    }

    // Verktygsresultatet som MCP-innehåll: text (JSON) och bilder.
    var toToolResult (const var& result)
    {
        auto out = object();
        juce::Array<var> content;
        const auto ok = (bool) result["ok"];

        if (! ok)
        {
            content.add (textContent (result["error"].toString()));
            put (out, "isError", true);
        }
        else
        {
            const auto& data = result["data"];
            content.add (textContent (data.isString() ? data.toString() : juce::JSON::toString (data, false, 4)));
            if (auto* images = result["images"].getArray())
                for (const auto& image : *images)
                {
                    auto entry = object();
                    put (entry, "type", "image");
                    put (entry, "data", image["data"]);
                    put (entry, "mimeType", "image/png");
                    content.add (entry);
                }
        }
        put (out, "content", content);
        return out;
    }

    var toolEntry (const juce::String& name, const juce::String& description, const juce::String& schema)
    {
        auto tool = object();
        put (tool, "name", name);
        put (tool, "description", description);
        put (tool, "inputSchema", juce::JSON::parse (schema));
        return tool;
    }

    //==========================================================================================
    class Bridge
    {
    public:
        Bridge() : headless (std::make_unique<InstrumentProcessor>()), tools (*headless)
        {
            headless->setPlayConfigDetails (0, 2, 44100.0, 512);
            headless->prepareToPlay (44100.0, 512);
        }

        var handle (const var& request)
        {
            const auto method = request["method"].toString();
            const auto id = request["id"];
            const auto isNotification = ! request.hasProperty ("id");

            if (method == "initialize")
            {
                auto result = object();
                const auto requested = request["params"]["protocolVersion"].toString();
                put (result, "protocolVersion", requested.isNotEmpty() ? requested : juce::String ("2024-11-05"));
                auto capabilities = object();
                put (capabilities, "tools", object());
                put (capabilities, "resources", object());
                put (result, "capabilities", capabilities);
                auto info = object();
                put (info, "name", "kickbase");
                put (info, "version", serverVersion);
                put (result, "serverInfo", info);
                put (result, "instructions", "Kickbase is a psytrance kick and bass synth. Call get_guide first: it explains the tools, units, the pattern language and the verification workflow. "
                                             "Work in a loop: change the sound, render_preview, read the metrics and the spectrogram, adjust. Save good sounds with save_preset.");
                return reply (id, result);
            }
            if (method == "ping")
                return reply (id, object());
            if (method == "tools/list")
                return reply (id, listTools());
            if (method == "tools/call")
                return reply (id, callTool (request["params"]["name"].toString(), request["params"]["arguments"]));
            if (method == "resources/list")
                return reply (id, listResources());
            if (method == "resources/read")
                return reply (id, readResource (request["params"]["uri"].toString()));
            if (method == "prompts/list")
            {
                auto result = object();
                put (result, "prompts", juce::Array<var>());
                return reply (id, result);
            }

            if (isNotification)
                return {};
            return errorReply (id, -32601, "Method not found: " + method);
        }

    private:
        var listTools() const
        {
            juce::Array<var> list;
            list.add (toolEntry ("list_instances", "Lists the Kickbase instances you can control: 'headless' (a private instance with no DAW, always available) and any Kickbase running in a DAW with its AI button switched on.",
                                 R"({"type":"object","properties":{}})"));
            list.add (toolEntry ("use_instance", "Chooses which instance the other tools act on: 'headless' or an id from list_instances (a plugin in the DAW; its AI button must be on). Changes in a DAW instance are heard live.",
                                 R"({"type":"object","properties":{"id":{"type":"string"}},"required":["id"]})"));
            for (const auto& info : ToolLayer::tools())
                list.add (toolEntry (info.name, info.description, info.schema));

            auto result = object();
            put (result, "tools", list);
            return result;
        }

        var callTool (const juce::String& name, const var& arguments)
        {
            if (name == "list_instances")
            {
                juce::Array<var> instances;
                auto local = object();
                put (local, "id", "headless");
                put (local, "description", "Private Kickbase without a DAW (renders offline)");
                put (local, "selected", selected == "headless");
                instances.add (local);
                for (const auto& instance : BridgeLink::listInstances())
                {
                    auto entry = object();
                    put (entry, "id", instance.id);
                    put (entry, "description", instance.name);
                    put (entry, "selected", selected == instance.id);
                    instances.add (entry);
                }
                auto data = object();
                put (data, "instances", instances);
                auto result = object();
                put (result, "ok", true);
                put (result, "data", data);
                return toToolResult (result);
            }

            if (name == "use_instance")
            {
                const auto id = arguments["id"].toString().trim();
                auto result = object();
                if (id.equalsIgnoreCase ("headless"))
                {
                    client.disconnect();
                    selected = "headless";
                    put (result, "ok", true);
                    put (result, "data", "Now controlling the headless instance.");
                    return toToolResult (result);
                }

                for (const auto& instance : BridgeLink::listInstances())
                    if (instance.id == id)
                    {
                        if (client.connect (instance.pipe))
                        {
                            selected = id;
                            put (result, "ok", true);
                            put (result, "data", "Now controlling " + instance.name + ". Changes are heard live.");
                        }
                        else
                        {
                            put (result, "ok", false);
                            put (result, "error", "Could not connect. Is the AI button switched on in that Kickbase, and is another bridge already connected?");
                        }
                        return toToolResult (result);
                    }

                put (result, "ok", false);
                put (result, "error", "No such instance. Use list_instances.");
                return toToolResult (result);
            }

            if (selected == "headless")
                return toToolResult (tools.call (name, arguments));

            auto result = client.call (name, arguments);
            if (! client.isConnected())
                selected = "headless";
            return toToolResult (result);
        }

        var listResources() const
        {
            juce::Array<var> list;
            for (const auto& [uri, title] : { std::pair<const char*, const char*> { "kickbase://guide/agent", "Kickbase agent guide" },
                                              { "kickbase://guide/psytrance", "Psytrance kick and bass reference" } })
            {
                auto entry = object();
                put (entry, "uri", uri);
                put (entry, "name", title);
                put (entry, "mimeType", "text/markdown");
                list.add (entry);
            }
            auto result = object();
            put (result, "resources", list);
            return result;
        }

        var readResource (const juce::String& uri) const
        {
            auto entry = object();
            put (entry, "uri", uri);
            put (entry, "mimeType", "text/markdown");
            put (entry, "text", ToolLayer::guide (uri.endsWith ("psytrance") ? "psytrance" : "agent"));
            auto result = object();
            put (result, "contents", juce::Array<var> { entry });
            return result;
        }

        std::unique_ptr<InstrumentProcessor> headless;
        ToolLayer tools;
        BridgeLink::Client client;
        juce::String selected = "headless";
    };
}

namespace
{
    // Självtest av live-länken: en instans med AI-länken på, och en klient (en annan tråd) som kopplar upp sig via
    // named pipe och kör verktyg i den. Instansens verktyg körs på huvudtråden, som här kör meddelandeloopen.
    int selfTestLink()
    {
        juce::ScopedJuceInitialiser_GUI init;
        const auto owner = std::make_unique<InstrumentProcessor>(); // på heapen: processorn är flera MB
        auto& live = *owner;
        live.setPlayConfigDetails (0, 2, 44100.0, 512);
        live.prepareToPlay (44100.0, 512);
        live.setBridgeEnabled (true);
        std::printf ("instans %s, länk %s\n", live.getBridgeId().toRawUTF8(), live.isBridgeEnabled() ? "på" : "av");

        std::atomic<bool> done { false };
        std::vector<juce::String> lines;
        std::thread worker ([&]
        {
            BridgeLink::Client client;
            auto instances = BridgeLink::listInstances();
            lines.push_back ("instanser: " + juce::String (instances.size()));
            if (instances.isEmpty() || ! client.connect (instances[0].pipe))
                lines.push_back ("FEL: kunde inte ansluta");
            else
            {
                auto args = object();
                auto values = object();
                put (values, "kickTune", 3);
                put (args, "values", values);
                const auto set = client.call ("set_parameters", args);
                lines.push_back ("set_parameters: " + juce::String ((bool) set["ok"] ? "ok" : set["error"].toString()));
                auto previewArgs = object();
                put (previewArgs, "part", "kick");
                put (previewArgs, "bars", 1);
                put (previewArgs, "name", "selftest_link");
                const auto preview = client.call ("render_preview", previewArgs);
                lines.push_back ("render_preview: " + juce::String ((bool) preview["ok"] ? "ok, kick tail " + preview["data"]["metrics"]["kick_tail_hz"].toString() + " Hz, bilder "
                                                                            + juce::String (preview["images"].size()) : preview["error"].toString()));
                client.call ("undo", object());
            }
            done = true;
        });
        for (int i = 0; i < 3000 && ! done; ++i)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        worker.join();

        for (const auto& line : lines)
            std::puts (line.toRawUTF8());
        const auto tune = live.apvts.getRawParameterValue (ParamID::kickTune)->load();
        std::printf ("kickTune i instansen efter undo: %.1f (väntat 0)\n", tune);
        live.setBridgeEnabled (false);
        return done && std::abs (tune) < 0.01f ? 0 : 1;
    }
}

int toolMain (int argc, char* argv[]); // Tools/Tool.cpp: utvecklingsverktyget bor i samma program, så att allt kompileras en gång

int main (int argc, char** argv)
{
    // Samma körbara fil är också MyOwnFruitTool (en kopia med det namnet görs efter bygget), eller körs med `tool <kommando>`.
    if (juce::File (juce::String (argv[0])).getFileNameWithoutExtension().containsIgnoreCase ("Tool"))
        return toolMain (argc, argv);
    if (argc > 1 && juce::String (argv[1]) == "tool")
    {
        std::vector<char*> shifted { argv[0] };
        for (int i = 2; i < argc; ++i)
            shifted.push_back (argv[i]);
        return toolMain ((int) shifted.size(), shifted.data());
    }

    if (argc > 1 && juce::String (argv[1]) == "--selftest-link")
        return selfTestLink();

   #if JUCE_WINDOWS
    _setmode (_fileno (stdin), _O_BINARY);
    _setmode (_fileno (stdout), _O_BINARY);
   #endif

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    Bridge bridge;
    log ("ready");

    std::string line;
    while (std::getline (std::cin, line))
    {
        if (! line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        const auto request = juce::JSON::parse (juce::String::fromUTF8 (line.c_str()));
        if (! request.isObject())
        {
            send (errorReply (var(), -32700, "Parse error"));
            continue;
        }

        try
        {
            const auto response = bridge.handle (request);
            if (response.isObject())
                send (response);
        }
        catch (const std::exception& e)
        {
            send (errorReply (request["id"], -32603, juce::String ("Internal error: ") + e.what()));
        }
    }
    return 0;
}
