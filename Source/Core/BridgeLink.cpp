#include "BridgeLink.h"
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
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kickbase").getChildFile ("Bridge");
    }

    juce::Array<InstanceInfo> listInstances()
    {
        juce::Array<InstanceInfo> instances;
        for (const auto& file : registryFolder().findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            const auto info = juce::JSON::parse (file);
            InstanceInfo instance { info["id"].toString(), info["pipe"].toString(), info["name"].toString(), (int) info["pid"] };
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
    Server::Server (PluginBase& p) : InterprocessConnection (true), processor (p), tools (std::make_unique<ToolLayer> (p))
    {
        id = juce::String::toHexString (juce::Random::getSystemRandom().nextInt (0x1000000)).paddedLeft ('0', 6);
        pipeName = "kickbase-" + juce::String (currentProcessId()) + "-" + id;
    }

    Server::~Server() { stop(); }

    bool Server::open()
    {
        return createPipe (pipeName, -1, false);
    }

    bool Server::start()
    {
        if (running)
            return true;
        if (! open())
            return false;
        running = true;
        writeRegistry();
        return true;
    }

    void Server::stop()
    {
        stopTimer();
        running = false;
        disconnect();
        removeRegistry();
    }

    void Server::writeRegistry() const
    {
        auto info = new juce::DynamicObject();
        info->setProperty ("id", id);
        info->setProperty ("pipe", pipeName);
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

    void Server::connectionLost()
    {
        // En ny klient kan ansluta: pipen skapas om en kort stund (inte inifrån den egna tråden).
        if (running)
            startTimer (200);
    }

    void Server::timerCallback()
    {
        stopTimer();
        if (! running)
            return;
        disconnect();
        if (! open())
            startTimer (1000);
    }

    void Server::messageReceived (const juce::MemoryBlock& message)
    {
        // Körs på message thread.
        const auto request = fromBlock (message);
        auto response = new juce::DynamicObject();
        response->setProperty ("id", request["id"]);
        response->setProperty ("result", tools->call (request["tool"].toString(), request["args"]));
        sendMessage (toBlock (juce::var (response)));
    }

    //==========================================================================================
    Client::Client() : InterprocessConnection (false) {}
    Client::~Client() { disconnect(); }

    bool Client::connect (const juce::String& pipeName, int timeoutMs)
    {
        disconnect();
        connected = connectToPipe (pipeName, timeoutMs);
        return connected;
    }

    void Client::disconnect()
    {
        connected = false;
        InterprocessConnection::disconnect();
    }

    void Client::connectionLost()
    {
        connected = false;
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

    juce::var Client::call (const juce::String& tool, const juce::var& args, int timeoutMs)
    {
        auto failure = [] (const juce::String& text)
        {
            auto result = new juce::DynamicObject();
            result->setProperty ("ok", false);
            result->setProperty ("error", text);
            return juce::var (result);
        };

        if (! connected)
            return failure ("Not connected to a Kickbase instance. Use list_instances and use_instance.");

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
        if (! sendMessage (toBlock (juce::var (request))))
        {
            connected = false;
            return failure ("The Kickbase instance did not accept the message (closed?). Run list_instances again.");
        }

        if (! replyArrived.wait (timeoutMs) || reply.isVoid())
            return failure (connected ? "The Kickbase instance did not answer in time." : "The connection to the Kickbase instance was lost.");

        const juce::ScopedLock sl (lock);
        return reply;
    }
}
