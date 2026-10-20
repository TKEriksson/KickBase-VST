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
#include <mutex>
#include <condition_variable>
#include <deque>

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

    // Flera trådar skriver svar (läsartråden och verktygstråden): ett svar i taget, annars blandas raderna.
    void send (const var& message)
    {
        static std::mutex outputLock;
        const std::lock_guard<std::mutex> guard (outputLock);
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
                                             "Work in a loop: change the sound, render_preview, read the metrics and the spectrogram, adjust. Save good sounds with save_preset. "
                                             "To check how the kick and instruments share the frequencies (collisions, a kick that is out of tune, rumble, the limiter) call analyse_mix (it starts a background analysis) and then get_mix_report until the status is done; the guide (section 9) says how to fix what it finds: EQ per source, "
                                             "a Sidechain effect so one instrument ducks under another, retuning.");
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
                    BridgeLink::setLastSelectedInstance ({});
                    put (result, "ok", true);
                    put (result, "data", "Now controlling the headless instance.");
                    return toToolResult (result);
                }

                for (const auto& instance : BridgeLink::listInstances())
                    if (instance.id == id)
                    {
                        if (client.connect (instance.pipes))
                        {
                            selected = id;
                            BridgeLink::setLastSelectedInstance (id);
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

            // En omstartad brygga (t.ex. när AI-appen startar om den) hamnar först i "headless": ta upp tråden mot instansen man valde senast.
            if (selected == "headless")
            {
                const auto remembered = BridgeLink::lastSelectedInstance();
                if (remembered.isNotEmpty())
                    for (const auto& instance : BridgeLink::listInstances())
                        if (instance.id == remembered && client.connect (instance.pipes))
                        {
                            selected = remembered;
                            BridgeLink::log ("bridge", "picked the instance " + remembered + " up again after a restart");
                        }
            }

            if (selected == "headless")
                return toToolResult (tools.call (name, arguments));

            auto result = client.call (name, arguments);
            if (! (bool) result["ok"] && ! client.isConnected())
            {
                // Länken tappade. Bryggan försökte redan ansluta igen. Lever instansen fortfarande behåller vi valet (nästa anrop försöker igen);
                // är den borta byter vi till headless och säger det tydligt.
                auto stillThere = false;
                for (const auto& instance : BridgeLink::listInstances())
                    stillThere = stillThere || instance.id == selected;
                if (! stillThere)
                {
                    const auto gone = selected;
                    selected = "headless";
                    BridgeLink::setLastSelectedInstance ({});
                    BridgeLink::log ("bridge", "the instance " + gone + " is gone: back to headless");
                    auto failure = object();
                    put (failure, "ok", false);
                    put (failure, "error", "The Kickbase instance " + gone + " has closed (or its AI button was switched off). You are now controlling the headless instance: nothing you do here is heard in the DAW. "
                                           "Use list_instances and use_instance to pick the instance in the DAW again.");
                    return toToolResult (failure);
                }
            }
            return toToolResult (result);
        }

        var listResources() const
        {
            juce::Array<var> list;
            for (const auto& [uri, title] : { std::pair<const char*, const char*> { "kickbase://guide/agent", "Kickbase agent guide" },
                                              { "kickbase://guide/psytrance", "Psytrance kick and bass reference" },
                                              { "kickbase://guide/school", "Kick and bass school" } })
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
            put (entry, "text", ToolLayer::guide (uri.endsWith ("psytrance") ? "psytrance" : uri.endsWith ("school") ? "school" : "agent"));
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
        bool keyedOk = false, reconnectOk = false, secondOk = false, busyOk = false;
        std::vector<juce::String> lines;
        std::thread worker ([&]
        {
            BridgeLink::Client client;
            auto instances = BridgeLink::listInstances();
            lines.push_back ("instanser: " + juce::String (instances.size()));
            if (instances.isEmpty() || ! client.connect (instances[0].pipes))
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

                // Två bryggor samtidigt (t.ex. två AI-fönster): den andra tar en annan pipe i stället för att nekas.
                {
                    BridgeLink::Client second;
                    secondOk = second.connect (instances[0].pipes) && (bool) second.call ("get_state", object())["ok"] && client.isConnected();
                    lines.push_back (juce::String (secondOk ? "ok" : "FEL") + ": en andra brygga kan vara ansluten samtidigt");
                }

                // Upptagen huvudtråd: medan den sover i fem sekunder (som under en lång rendering) ska länken ändå svara på ping direkt.
                juce::MessageManager::callAsync ([] { juce::Thread::sleep (5000); });
                juce::Thread::sleep (300);
                const auto pingStart = juce::Time::getMillisecondCounterHiRes();
                const auto pong = client.call ("ping", object(), 4000);
                const auto pingMs = juce::Time::getMillisecondCounterHiRes() - pingStart;
                busyOk = (bool) pong["ok"] && pingMs < 1500.0;
                lines.push_back (juce::String (busyOk ? "ok" : "FEL") + ": ping besvaras medan huvudtråden är upptagen (" + juce::String (pingMs, 0) + " ms)");
                juce::Thread::sleep (5000); // huvudtråden blir ledig igen

                // Tappad länk: nästa anrop kopplar upp sig igen av sig självt.
                client.disconnect();
                const auto again = client.call ("get_state", object());
                reconnectOk = (bool) again["ok"];
                lines.push_back (juce::String (reconnectOk ? "ok" : "FEL") + ": bryggan kopplar upp sig igen efter ett tapp");

                // Åtkomstnyckel: utan nyckel nekas bryggan, med rätt nyckel släpps den in.
                client.disconnect();
                const auto settingsFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_selftest_settings.xml");
                Settings::useFile (settingsFile);
                Settings::set ("aiKey", "k1");
                BridgeLink::Client keyed;
                keyed.setAccessKey ({});
                if (keyed.connect (instances[0].pipes))
                {
                    const auto denied = keyed.call ("get_state", object());
                    keyedOk = ! (bool) denied["ok"] && denied["error"].toString().contains ("access key");
                    lines.push_back (juce::String (keyedOk ? "ok" : "FEL") + ": utan nyckel nekas bryggan");
                    keyed.setAccessKey ("k1");
                    const auto allowed = keyed.call ("get_state", object());
                    keyedOk = keyedOk && (bool) allowed["ok"];
                    lines.push_back (juce::String ((bool) allowed["ok"] ? "ok" : "FEL") + ": med rätt nyckel släpps bryggan in");
                }
                Settings::set ("aiKey", {});
                Settings::useFile ({});
                settingsFile.deleteFile();
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
        return done && keyedOk && reconnectOk && secondOk && busyOk && std::abs (tune) < 0.01f ? 0 : 1;
    }
}

int toolMain (int argc, char* argv[]); // Tools/Tool.cpp: utvecklingsverktyget bor i samma program, så att allt kompileras en gång

int main (int argc, char** argv)
{
    // Samma körbara fil är också KickbaseTool (en kopia med det namnet görs efter bygget), eller körs med `tool <kommando>`.
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
        PluginBase::preferCodeInit = true; // testet förutsätter kickTune 0 från Init i koden
    if (argc > 1 && juce::String (argv[1]) == "--selftest-link")
        return selfTestLink();

   #if JUCE_WINDOWS
    _setmode (_fileno (stdin), _O_BINARY);
    _setmode (_fileno (stdout), _O_BINARY);
   #endif

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    Bridge bridge;
    log ("ready");
    BridgeLink::log ("bridge", "MCP server started");

    // Verktygsanrop körs en i taget på en egen tråd, så att läsartråden kan fortsätta svara (ping, listor, avbrott) medan ett långt anrop
    // (t.ex. en rendering i DAW:en) pågår. En AI-app som inte får svar på sin ping eller sina listor kan annars tro att servern dött.
    struct Worker
    {
        explicit Worker (Bridge& bridgeToUse) : bridge (bridgeToUse), thread ([this] { run(); }) {}

        ~Worker()
        {
            {
                const std::lock_guard<std::mutex> guard (mutex);
                finished = true;
            }
            wake.notify_all();
            thread.join();
        }

        void push (const var& request)
        {
            {
                const std::lock_guard<std::mutex> guard (mutex);
                queue.push_back (request);
            }
            wake.notify_one();
        }

    private:
        void run()
        {
            for (;;)
            {
                var request;
                {
                    std::unique_lock<std::mutex> lock (mutex);
                    wake.wait (lock, [this] { return finished || ! queue.empty(); });
                    if (queue.empty())
                        return;
                    request = queue.front();
                    queue.pop_front();
                }

                try
                {
                    const auto response = bridge.handle (request);
                    if (response.isObject())
                        send (response);
                }
                catch (const std::exception& e)
                {
                    BridgeLink::log ("bridge", juce::String ("error in a tool call: ") + e.what());
                    send (errorReply (request["id"], -32603, juce::String ("Internal error: ") + e.what()));
                }
                catch (...)
                {
                    BridgeLink::log ("bridge", "unknown error in a tool call");
                    send (errorReply (request["id"], -32603, "Internal error"));
                }
            }
        }

        Bridge& bridge;
        std::mutex mutex;
        std::condition_variable wake;
        std::deque<var> queue;
        bool finished = false;
        std::thread thread;
    } worker (bridge);

    std::string line;
    while (std::getline (std::cin, line))
    {
        if (line.size() >= 3 && line.compare (0, 3, "\xEF\xBB\xBF") == 0)
            line.erase (0, 3); // en byte order mark (UTF-8) först i strömmen: ignorera
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

        if (request["method"].toString() == "tools/call")
        {
            worker.push (request);
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
            BridgeLink::log ("bridge", juce::String ("error in a request: ") + e.what());
            send (errorReply (request["id"], -32603, juce::String ("Internal error: ") + e.what()));
        }
        catch (...)
        {
            BridgeLink::log ("bridge", "unknown error in a request");
            send (errorReply (request["id"], -32603, "Internal error"));
        }
    }

    // AI-appen stängde strömmen: avsluta direkt (ett pågående verktygsanrop behöver inte vänta ut sina 120 sekunder).
    BridgeLink::log ("bridge", "MCP server ended (the AI app closed the connection)");
    std::fflush (stdout);
    std::_Exit (0);
}
