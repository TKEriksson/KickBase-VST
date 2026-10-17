#pragma once

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <array>

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

    // En liten loggfil (Dokument/Kickbase/Bridge/bridge.log) med anslutningshändelser från båda sidor, för felsökning när länken tappar.
    void log (const juce::String& side, const juce::String& text);

    // Instansen bryggan senast valde (så att en omstartad brygga kan koppla upp sig igen), eller tom.
    juce::String lastSelectedInstance();
    void setLastSelectedInstance (const juce::String& id);

    struct InstanceInfo
    {
        juce::String id, name;
        juce::StringArray pipes; // flera pipes per instans, så att fler än en brygga (flera AI-fönster) kan vara anslutna samtidigt
        int pid = 0;
    };

    // De instanser som körs just nu (föräldralösa registerfiler, t.ex. efter en krasch, städas bort).
    juce::Array<InstanceInfo> listInstances();

    // Plugin-sidan. Fyra pipes lyssnar samtidigt: en brygga som hänger kvar (t.ex. ett glömt AI-fönster) blockerar då inte nästa.
    class Server
    {
    public:
        explicit Server (PluginBase& processor);
        ~Server();

        bool start();
        void stop();
        bool isRunning() const { return running; }
        juce::String getId() const { return id; }
        int getConnectedCount() const; // så många AI-appar som är anslutna just nu

    private:
        class Slot;
        static constexpr int numSlots = 4;

        void writeRegistry() const;
        void removeRegistry() const;
        void handleMessage (Slot& from, const juce::MemoryBlock& message);

        PluginBase& processor;
        std::unique_ptr<ToolLayer> tools;
        std::array<std::unique_ptr<Slot>, numSlots> slots;
        juce::String id;
        bool running = false;
    };

    // Bryggans sida: ansluter till en instans och kör verktyg i den.
    class Client : private juce::InterprocessConnection
    {
    public:
        Client();
        ~Client() override;

        bool connect (const juce::String& pipeName, int timeoutMs = 3000);
        bool connect (const juce::StringArray& pipeNames, int timeoutMs = 3000); // den första som svarar
        bool reconnect();                       // kopplar upp sig mot samma instans igen (några försök)
        bool hasTarget() const { return ! lastPipes.isEmpty(); }
        void disconnect();
        bool isConnected() const { return connected; }
        void setAccessKey (const juce::String& key) { accessKey = key; } // den nyckel plugin:et kräver (SETTINGS > AI LINK), om den är satt

        // Ger verktygets resultat, eller { "ok": false, "error": ... } om anslutningen eller svaret uteblir.
        juce::var call (const juce::String& tool, const juce::var& args, int timeoutMs = 120000);

    private:
        void connectionMade() override {}
        void connectionLost() override;
        void messageReceived (const juce::MemoryBlock& message) override;
        juce::var exchange (const juce::String& tool, const juce::var& args, int timeoutMs, bool& delivered);

        juce::StringArray lastPipes;
        juce::uint32 lastActivity = 0;
        juce::String accessKey;
        juce::CriticalSection lock;
        juce::WaitableEvent replyArrived;
        juce::var reply;
        int expectedId = 0, nextId = 1;
        std::atomic<bool> connected { false };
    };
}
