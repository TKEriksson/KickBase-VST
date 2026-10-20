#pragma once

#include "PluginBase.h"
#include "ToolLayer.h"

// AI-chatten i synten: man skriver till en AI med sin egen API-nyckel (Anthropic), och den ändrar ljudet med samma verktyg som MCP-bryggan
// (ToolLayer): sätter parametrar, skriver mönster, renderar och mäter. Samtalet körs i en bakgrundstråd; verktygen körs på message thread.
// Nyckeln och modellen ligger i Settings; nyckeln skickas bara till api.anthropic.com.
class AiChat : private juce::Thread
{
public:
    struct Entry
    {
        enum class Kind { user, assistant, tool, note, error };
        Kind kind = Kind::note;
        juce::String text;
        std::vector<juce::Image> images; // t.ex. spektrogrammet från render_preview
    };

    explicit AiChat (PluginBase& processor);
    ~AiChat() override;

    static juce::StringArray models();

    void send (const juce::String& text);   // message thread
    void stop();                            // avbryter efter pågående anrop
    void newChat();                         // tömmer samtalet (inte medan den arbetar)
    void undo();                            // ångrar AI:ns senaste ändring (verktyget undo)

    bool isBusy() const { return busy; }
    juce::String getStatus() const;         // "Thinking ...", "Running set_parameters ..." eller tom
    std::vector<Entry> getEntries() const;  // en kopia av samtalet som det visas
    juce::String getUsage() const;          // "12.3k in / 1.2k out" (tokens i det här samtalet)
    std::atomic<int> version { 0 };         // ökas när något ändrats, så att gränssnittet ritar om

    void addEntry (Entry entry);            // lägger ett inlägg i samtalet (används internt, och av tester)

private:
    void run() override;
    void setStatus (const juce::String& text);
    static juce::String activeKey();         // nyckeln för vald leverantör (Settings::chatProvider); Gemini-delen finns i AiProvider.h
    juce::var buildRequest() const;
    juce::var post (const juce::String& key, const juce::var& body, int& httpStatus, juce::String& error);
    juce::var callTool (const juce::String& name, const juce::var& input);

    PluginBase& processor;
    ToolLayer tools;
    juce::Array<juce::var> messages; // samtalet i API:ets format (bara bakgrundstråden och send/newChat när den står stilla rör den)

    mutable juce::CriticalSection lock;
    std::vector<Entry> log;
    juce::String status;
    std::atomic<bool> busy { false };

    // Anropet som pågår (annars nullptr), så att stop() och destruktorn kan avbryta en blockerande läsning (WebInputStream::cancel är gjord
    // för att anropas från en annan tråd). Förr väntade destruktorn upp till 15 s på ett svar när fönstret stängdes mitt i ett anrop.
    juce::CriticalSection streamLock;
    juce::WebInputStream* activeStream = nullptr;
    void cancelRequest();
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true); // läses av väntande anrop till message thread
    std::atomic<juce::int64> tokensIn { 0 }, tokensOut { 0 };

    static constexpr int maxSteps = 24;
};
