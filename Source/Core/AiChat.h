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

    // Kostnadsläget (Settings::chatCostMode: 0 Sparsam, 1 Normal, 2 Rik): hur många steg, hur långa svar och hur mycket gammalt
    // samtal som skickas om vid varje anrop. Hela historiken skickas om vid varje steg, så det är den som kostar.
    struct Limits
    {
        int maxSteps;          // verktygssteg per fråga
        int maxOutputTokens;   // tak per svar (tänkandet räknas in)
        int keepTurns;         // så många av de senaste frågorna skickas oklippta; äldre får kortade resultat och inga bilder
        int oldResultChars;    // längd på ett gammalt verktygsresultat
        int resultChars;       // längd på ett nytt verktygsresultat
        bool confirmHeavy;     // fråga innan render_preview/analyze_audio
        int autoCleanChars;    // över så här stor historik (utan bilder) sparas och exporteras samtalet, och de äldsta frågorna tas bort
    };
    static Limits limits (int costMode);
    static juce::Array<juce::var> trimForRequest (const juce::Array<juce::var>& messages, const Limits& limits); // en kopia; historiken rörs inte
    static bool isHeavyTool (const juce::String& name);
    static juce::String heavyToolWarning (const juce::String& name);

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

    // Varning innan ett dyrt verktyg: tom när inget väntar. answerConfirm: 1 kör, 2 hoppa över, 3 kör och fråga inte mer i det här samtalet.
    juce::String getPendingConfirm() const;
    void answerConfirm (int answer);

    // Sessioner: hela samtalet sparas som en JSON-fil (utan bilder) i Chats-mappen och kan öppnas igen; exportText skriver det läsbara samtalet
    // till en textfil. Nycklarna lämnar aldrig datorn i filerna: redact tar bort dem (och alla "key=..." i adresser) ur all text som skrivs.
    // Rörs inte medan chatten arbetar (returnerar false).
    struct SessionInfo
    {
        juce::File file;
        juce::String title;
        juce::Time saved;
    };
    static juce::File sessionsFolder();                 // Settings "chatsFolder", annars Kickbase/Chats
    static std::vector<SessionInfo> listSessions();     // nyaste först
    static bool deleteSession (const juce::File& file);  // flyttar chatten (och en eventuell .txt med samma namn) till papperskorgen
    static juce::String redact (juce::String text);
    bool hasConversation() const { return ! messages.isEmpty(); }
    bool saveSession (juce::File& savedTo);
    bool loadSession (const juce::File& file);
    bool exportText (juce::File& exportedTo);

private:
    void run() override;
    void autoClean (const Limits& limits);             // före en ny fråga: sparar + exporterar och tar bort de äldsta frågorna när historiken är för stor
    juce::String conversationTitle() const;
    void setStatus (const juce::String& text);
    static juce::String activeKey();         // nyckeln för vald leverantör (Settings::chatProvider); Gemini-delen finns i AiProvider.h
    juce::var buildRequest (const Limits& limits) const;
    bool confirmTool (const juce::String& name);   // bakgrundstråden: true = kör verktyget
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

    juce::String confirmText;                 // under lock
    std::atomic<int> confirmAnswer { 0 };     // 0 väntar, annars svaret (se answerConfirm)
    std::atomic<bool> confirmAll { false };   // "kör och fråga inte mer" gäller tills newChat
};
