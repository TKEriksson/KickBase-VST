#pragma once

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>

class PluginBase;
class ToolLayer;

// Länken mellan en Kickbase-instans i DAW:en och MCP-bryggan (KickbaseMcp.exe).
//
// Plugin:et lyssnar på en named pipe (bara lokalt, ingen nätverksport) när användaren slagit på AI-knappen och
// kör verktygslagret på message thread. Varje instans skriver en registerfil, så att bryggan kan lista dem
// (list_instances). Meddelanden är JSON: { "id": n, "tool": "...", "args": {...} } och { "id": n, "result": {...} }.
namespace BridgeLink
{
    juce::File registryFolder();

    struct InstanceInfo
    {
        juce::String id, pipe, name;
        int pid = 0;
    };

    // De instanser som körs just nu (föräldralösa registerfiler, t.ex. efter en krasch, städas bort).
    juce::Array<InstanceInfo> listInstances();

    // Plugin-sidan.
    class Server : private juce::InterprocessConnection, private juce::Timer
    {
    public:
        explicit Server (PluginBase& processor);
        ~Server() override;

        bool start();
        void stop();
        bool isRunning() const { return running; }
        juce::String getId() const { return id; }

    private:
        void connectionMade() override {}
        void connectionLost() override;
        void messageReceived (const juce::MemoryBlock& message) override;
        void timerCallback() override;
        bool open();
        void writeRegistry() const;
        void removeRegistry() const;

        PluginBase& processor;
        std::unique_ptr<ToolLayer> tools;
        juce::String id, pipeName;
        bool running = false;
    };

    // Bryggans sida: ansluter till en instans och kör verktyg i den.
    class Client : private juce::InterprocessConnection
    {
    public:
        Client();
        ~Client() override;

        bool connect (const juce::String& pipeName, int timeoutMs = 3000);
        void disconnect();
        bool isConnected() const { return connected; }
        void setAccessKey (const juce::String& key) { accessKey = key; } // den nyckel plugin:et kräver (SETTINGS > AI LINK), om den är satt

        // Ger verktygets resultat, eller { "ok": false, "error": ... } om anslutningen eller svaret uteblir.
        juce::var call (const juce::String& tool, const juce::var& args, int timeoutMs = 120000);

    private:
        void connectionMade() override {}
        void connectionLost() override;
        void messageReceived (const juce::MemoryBlock& message) override;

        juce::String accessKey;
        juce::CriticalSection lock;
        juce::WaitableEvent replyArrived;
        juce::var reply;
        int expectedId = 0, nextId = 1;
        std::atomic<bool> connected { false };
    };
}
