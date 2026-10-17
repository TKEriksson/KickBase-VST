#include "BridgeLink.h"
#include "Settings.h"
#include "PluginBase.h"
#include "ToolLayer.h"

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <signal.h>
 #include <unistd.h>
#endif

namespace
{
    int currentProcessId()
    {
       #if JUCE_WINDOWS
        return (int) GetCurrentProcessId();
       #else
        return (int) getpid();
       #endif
    }

    bool processIsAlive (int pid)
    {
        if (pid <= 0)
            return false;
       #if JUCE_WINDOWS
        if (auto handle = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid))
        {
            DWORD code = 0;
            const auto alive = GetExitCodeProcess (handle, &code) && code == STILL_ACTIVE;
            CloseHandle (handle);
            return alive;
        }
        return false;
       #else
        return kill (pid, 0) == 0;
       #endif
    }

    juce::MemoryBlock toBlock (const juce::var& value)
    {
        const auto text = juce::JSON::toString (value, true);
        return { text.toRawUTF8(), text.getNumBytesAsUTF8() };
    }

    juce::var fromBlock (const juce::MemoryBlock& block)
    {
        return juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (block.getData()), (int) block.getSize()));
    }
}

namespace BridgeLink
{
    juce::File registryFolder()
    {
        // Dokument, inte AppData: en Store-installerad AI-app (Claude Desktop) virtualiserar AppData för sina barnprocesser.
        return Settings::rootFolder().getChildFile ("Bridge");
    }

    void log (const juce::String& side, const juce::String& text)
    {
        const auto file = registryFolder().getChildFile ("bridge.log");
        file.getParentDirectory().createDirectory();
        if (file.getSize() > 200 * 1024)
            file.deleteFile(); // för stor: börja om
        file.appendText (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") + "  [" + side + " " + juce::String (currentProcessId()) + "] " + text + "\n");
    }

    juce::String lastSelectedInstance()
    {
        return registryFolder().getChildFile ("last_instance.txt").loadFileAsString().trim();
    }

    void setLastSelectedInstance (const juce::String& id)
    {
        const auto file = registryFolder().getChildFile ("last_instance.txt");
        if (id.isEmpty())
            file.deleteFile();
        else
        {
            file.getParentDirectory().createDirectory();
            file.replaceWithText (id);
        }
    }

    juce::Array<InstanceInfo> listInstances()
    {
        juce::Array<InstanceInfo> instances;
        for (const auto& file : registryFolder().findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            const auto info = juce::JSON::parse (file);
            InstanceInfo instance { info["id"].toString(), info["name"].toString(), {}, (int) info["pid"] };
            if (auto* pipes = info["pipes"].getArray())
                for (const auto& pipe : *pipes)
                    instance.pipes.add (pipe.toString());
            if (instance.pipes.isEmpty() && info["pipe"].toString().isNotEmpty())
                instance.pipes.add (info["pipe"].toString());
            if (instance.id.isEmpty() || ! processIsAlive (instance.pid))
            {
                file.deleteFile();
                continue;
            }
            instances.add (instance);
        }
        return instances;
    }

    //==========================================================================================
    // En pipe (en anslutning). Tappar bryggan skapas pipen om en kort stund, inte inifrån den egna tråden.
    class Server::Slot : public juce::InterprocessConnection, private juce::Timer
    {
    public:
        // Anslutningens egen tråd tar emot meddelandena (inte huvudtråden): pingen besvaras direkt där, så att länken inte tappas medan huvudtråden
        // är upptagen med ett långt verktygsanrop (en rendering, en inläsning). Verktygen körs fortfarande på huvudtråden.
        Slot (Server& ownerIn, int indexIn, const juce::String& name)
            : InterprocessConnection (false), owner (ownerIn), index (indexIn), pipeName (name), alive (std::make_shared<std::atomic<bool>> (true)) {}
        ~Slot() override { alive->store (false); stopTimer(); disconnect(); }

        bool open() { return createPipe (pipeName, -1, false); }
        void close() { stopTimer(); disconnect(); }
        juce::String getPipeName() const { return pipeName; }
        void setRunning (bool shouldRun) { running = shouldRun; }

    private:
        void connectionMade() override { log ("plugin", "a bridge connected on pipe " + juce::String (index + 1)); }

        void connectionLost() override
        {
            log ("plugin", "the bridge on pipe " + juce::String (index + 1) + " disconnected, listening again");
            juce::MessageManager::callAsync ([this, stillAlive = alive]
            {
                if (stillAlive->load() && running)
                    startTimer (50);
            });
        }

        void messageReceived (const juce::MemoryBlock& message) override
        {
            const auto request = fromBlock (message);
            if (request["tool"].toString() == "ping")
            {
                auto pong = new juce::DynamicObject();
                pong->setProperty ("ok", true);
                auto response = new juce::DynamicObject();
                response->setProperty ("id", request["id"]);
                response->setProperty ("result", juce::var (pong));
                sendMessage (toBlock (juce::var (response)));
                return;
            }
            juce::MessageManager::callAsync ([this, message, stillAlive = alive]
            {
                if (stillAlive->load())
                    owner.handleMessage (*this, message);
            });
        }

        void timerCallback() override
        {
            stopTimer();
            if (! running)
                return;
            disconnect();
            if (! open())
            {
                log ("plugin", "could not reopen " + pipeName + ", trying again");
                startTimer (1000);
            }
        }

        friend class Server;
        Server& owner;
        const int index;
        const juce::String pipeName;
        std::shared_ptr<std::atomic<bool>> alive;
        bool running = false;
    };

    Server::Server (PluginBase& p) : processor (p), tools (std::make_unique<ToolLayer> (p))
    {
        id = juce::String::toHexString (juce::Random::getSystemRandom().nextInt (0x1000000)).paddedLeft ('0', 6);
        for (int i = 0; i < numSlots; ++i)
            slots[(size_t) i] = std::make_unique<Slot> (*this, i, "kickbase-" + juce::String (currentProcessId()) + "-" + id + "-" + juce::String (i + 1));
    }

    Server::~Server() { stop(); }

    int Server::getConnectedCount() const
    {
        auto count = 0;
        for (const auto& slot : slots)
            count += slot->isConnected() ? 1 : 0;
        return count;
    }

    bool Server::start()
    {
        if (running)
            return true;
        for (auto& slot : slots)
        {
            slot->setRunning (true);
            if (! slot->open() && slot == slots.front())
                return false;
        }
        running = true;
        writeRegistry();
        log ("plugin", "AI link on: instance " + id + ", " + juce::String (numSlots) + " pipes");
        return true;
    }

    void Server::stop()
    {
        if (running)
            log ("plugin", "AI link off: instance " + id);
        running = false;
        for (auto& slot : slots)
        {
            slot->setRunning (false);
            slot->close();
        }
        removeRegistry();
    }

    void Server::writeRegistry() const
    {
        auto info = new juce::DynamicObject();
        info->setProperty ("id", id);
        juce::Array<juce::var> pipes;
        for (const auto& slot : slots)
            pipes.add (slot->getPipeName());
        info->setProperty ("pipes", pipes);
        info->setProperty ("pipe", slots.front()->getPipeName()); // (äldre bryggor läser bara en)
        info->setProperty ("pid", currentProcessId());
        info->setProperty ("name", juce::String ("Kickbase in ") + juce::PluginHostType().getHostDescription());
        info->setProperty ("started", juce::Time::getCurrentTime().toISO8601 (true));

        const auto folder = registryFolder();
        folder.createDirectory();
        folder.getChildFile (id + ".json").replaceWithText (juce::JSON::toString (juce::var (info), true));
    }

    void Server::removeRegistry() const
    {
        registryFolder().getChildFile (id + ".json").deleteFile();
    }

    void Server::handleMessage (Slot& from, const juce::MemoryBlock& message)
    {
        // Körs på message thread.
        const auto request = fromBlock (message);
        auto response = new juce::DynamicObject();
        response->setProperty ("id", request["id"]);

        // Liv-kontroll från bryggan (innan den skickar något viktigt efter en paus): svara direkt.
        if (request["tool"].toString() == "ping")
        {
            auto pong = new juce::DynamicObject();
            pong->setProperty ("ok", true);
            response->setProperty ("result", juce::var (pong));
            from.sendMessage (toBlock (juce::var (response)));
            return;
        }

        // Har användaren satt en åtkomstnyckel (SETTINGS > AI LINK) svarar vi bara en brygga som har samma nyckel.
        if (const auto expected = Settings::aiKey(); expected.isNotEmpty() && request["key"].toString() != expected)
        {
            auto denied = new juce::DynamicObject();
            denied->setProperty ("ok", false);
            denied->setProperty ("error", "This Kickbase has an access key (SETTINGS tab, AI LINK). Give the bridge the same key: set KICKBASE_KEY in its environment, or run install-mcp.ps1 -Key <key>.");
            response->setProperty ("result", juce::var (denied));
            from.sendMessage (toBlock (juce::var (response)));
            return;
        }
        response->setProperty ("result", tools->call (request["tool"].toString(), request["args"]));
        from.sendMessage (toBlock (juce::var (response)));
    }

    //==========================================================================================
    Client::Client() : InterprocessConnection (false), accessKey (juce::SystemStats::getEnvironmentVariable ("KICKBASE_KEY", {}).trim()) {}
    Client::~Client() { disconnect(); }

    bool Client::connect (const juce::String& pipeName, int timeoutMs)
    {
        return connect (juce::StringArray (pipeName), timeoutMs);
    }

    // Försöker pipe för pipe: är en upptagen (en annan brygga) tar vi nästa.
    bool Client::connect (const juce::StringArray& pipeNames, int timeoutMs)
    {
        disconnect();
        lastPipes = pipeNames;
        for (const auto& pipeName : pipeNames)
        {
            connected = connectToPipe (pipeName, timeoutMs);
            log ("bridge", juce::String (connected ? "connected to " : "could not connect to ") + pipeName);
            if (connected)
                break;
        }
        if (connected)
            lastActivity = juce::Time::getMillisecondCounter();
        return connected;
    }

    // Efter ett tapp: försök igen några gånger mot samma instans (plugin:et lyssnar på nytt efter en kort stund).
    bool Client::reconnect()
    {
        if (lastPipes.isEmpty())
            return false;
        const auto pipes = lastPipes;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            if (connect (pipes, 1000))
            {
                log ("bridge", "reconnected after attempt " + juce::String (attempt + 1));
                return true;
            }
            juce::Thread::sleep (200 * (attempt + 1));
        }
        return false;
    }

    void Client::disconnect()
    {
        connected = false;
        InterprocessConnection::disconnect();
    }

    void Client::connectionLost()
    {
        connected = false;
        log ("bridge", "connection lost");
        replyArrived.signal(); // släpper en väntande call
    }

    void Client::messageReceived (const juce::MemoryBlock& message)
    {
        const juce::ScopedLock sl (lock);
        auto parsed = fromBlock (message);
        if ((int) parsed["id"] == expectedId)
        {
            reply = parsed["result"];
            replyArrived.signal();
        }
    }

    // Ett anrop och dess svar. `delivered` säger om meddelandet kom iväg (annars kan det skickas om efter en återanslutning).
    juce::var Client::exchange (const juce::String& tool, const juce::var& args, int timeoutMs, bool& delivered)
    {
        auto failure = [] (const juce::String& text)
        {
            auto result = new juce::DynamicObject();
            result->setProperty ("ok", false);
            result->setProperty ("error", text);
            return juce::var (result);
        };

        delivered = false;
        int id = 0;
        {
            const juce::ScopedLock sl (lock);
            id = expectedId = nextId++;
            reply = juce::var();
            replyArrived.reset();
        }

        auto request = new juce::DynamicObject();
        request->setProperty ("id", id);
        request->setProperty ("tool", tool);
        request->setProperty ("args", args);
        if (accessKey.isNotEmpty())
            request->setProperty ("key", accessKey);
        if (! sendMessage (toBlock (juce::var (request))))
        {
            connected = false;
            return failure ("The Kickbase instance did not accept the message.");
        }
        delivered = true;

        if (! replyArrived.wait (timeoutMs) || reply.isVoid())
        {
            log ("bridge", "no answer to " + tool + (connected ? " (timeout)" : " (connection lost)"));
            return failure (connected ? "The Kickbase instance did not answer in time (it may be busy, for example with a long render). Try again."
                                      : "The connection to the Kickbase instance was lost. It will be reconnected on the next call.");
        }

        const juce::ScopedLock sl (lock);
        return reply;
    }

    juce::var Client::call (const juce::String& tool, const juce::var& args, int timeoutMs)
    {
        auto failure = [] (const juce::String& text)
        {
            auto result = new juce::DynamicObject();
            result->setProperty ("ok", false);
            result->setProperty ("error", text);
            return juce::var (result);
        };

        if (lastPipes.isEmpty())
            return failure ("Not connected to a Kickbase instance. Use list_instances and use_instance.");
        if (! connected && ! reconnect())
            return failure ("Could not reach the Kickbase instance (it may have closed, or another AI app holds the link). Use list_instances.");

        bool delivered = false;

        // Efter en paus: kolla att länken lever innan något viktigt skickas (en halvdöd anslutning märks annars först efter en lång väntan).
        if (juce::Time::getMillisecondCounter() - lastActivity > 15000)
        {
            const auto pong = exchange ("ping", juce::var (new juce::DynamicObject()), 4000, delivered);
            if (! (bool) pong["ok"])
            {
                log ("bridge", "the link did not answer a ping: reconnecting");
                disconnect();
                if (! reconnect())
                    return failure ("The link to the Kickbase instance was lost and could not be restored. Use list_instances.");
            }
        }

        auto result = exchange (tool, args, timeoutMs, delivered);
        if (! delivered)
        {
            // Meddelandet kom inte iväg: koppla upp igen och skicka om (inget har hänt i plugin:et).
            if (reconnect())
                result = exchange (tool, args, timeoutMs, delivered);
        }
        lastActivity = juce::Time::getMillisecondCounter();
        return result;
    }
}
