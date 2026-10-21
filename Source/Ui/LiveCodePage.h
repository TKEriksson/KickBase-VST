#pragma once

#include "../Core/LiveCodeRunner.h"
#include "../Core/ToolLayer.h"
#include "Theme.h"
#include <juce_gui_extra/juce_gui_extra.h>

// Fliken LIVE CODE: skriv Strudel-kod (https://strudel.cc). AUTO (av när synthen startar) räknar om koden i bakgrunden ca 400 ms efter varje ändring,
// bara om den är giltig, och skriver mönstret i pianorullen (en ångra-punkt först); STOP slutar lyssna; CREATE skriver koden en gång.
class LiveCodePage : public juce::Component, private juce::Timer, private juce::CodeDocument::Listener
{
public:
    explicit LiveCodePage (PluginBase& processor);
    ~LiveCodePage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Anslutningspunkter: sätts av den som vill veta (t.ex. för att uppdatera pianorullen).
    bool confirmCreate = true; // CREATE visar en bekräftelseruta; testerna stänger av den
    std::function<void()> onPatternChanged;
    std::function<void (const juce::String& error)> onError;

private:
    void timerCallback() override;
    void codeDocumentTextInserted (const juce::String&, int) override;
    void codeDocumentTextDeleted (int, int) override;
    void codeChanged();
    void layoutSliders();                 // slider(v,min,max) i koden får ett reglage vid radens slut
    void sliderReleased (int index);      // skriver tillbaka värdet i texten (ett ångra-steg)
    void setPlaying (bool shouldPlay);
    void runNow();
    void create();
    void finishCreate (const juce::var& result);
    void showError (const juce::String& text);
    void showConsole (const juce::StringArray& console); // G2: console.log från körningen
    void appendLog (const juce::StringArray& newLines);
    bool apply (const juce::var& items, bool overwrite, const juce::var& mods, bool prune = false); // skriver raderna; frågar först om handritade toner skulle skrivas över
    void applyParamWrites (const juce::var& writes);      // F1: .set på parametrar
    void applyEffectWrite (const juce::var& write);       // F3: effektplats (typ eller ratt)
    void warnOverwrite (const juce::var& conflicts, std::function<void (bool overwrite)> retry);
    void syncLocks();
    void syncSavedCode();
    void checkLoaded(); // projekt/preset laddat: stoppa LIVE (en pågående körning kastas) och ta in den nya koden
    void setCodeOnly (bool on); // soloMask = raderna Live Code skrivit; användarens egen SOLO-knapp vinner
    void refreshCodeOnly();     // efter varje körning: följ med nya rader, eller släpp om användaren soloat själv
    void visibilityChanged() override { checkLoaded(); syncSavedCode(); syncLocks(); }

    PluginBase& processor;
    ToolLayer tools;
    bool warningOpen = false; // en varning i taget; LIVE väntar tills den är besvarad
    LiveCodeRunner runner;
    bool playing = false; // AUTO: av när synthen startar
    bool codeOnly = false;
    struct LoadWatch : private juce::Timer // ett projekt kan laddas från vilken tråd som helst: sidan tittar efter på meddelandetråden
    {
        explicit LoadWatch (std::function<void()> f) : callback (std::move (f)) { startTimerHz (10); }
        ~LoadWatch() override { stopTimer(); }
        void timerCallback() override { callback(); }
        std::function<void()> callback;
    };
    int seenLoads = 0;
    LoadWatch loadWatch { [this] { checkLoaded(); layoutSliders(); processor.sweepCodeRoutes(); } };
    juce::uint64 savedSolo = 0, ownSolo = 0; // soloMask före CODE ONLY, och det värde CODE ONLY själv satte

    void showTab (bool errors);

    juce::CodeDocument document;
    juce::CPlusPlusCodeTokeniser tokeniser; // räcker för färg på strängar, siffror, kommentarer
    juce::CodeEditorComponent editor { document, &tokeniser };
    struct SliderSpec { int ordinal = 0, start = 0, end = 0; double value = 0, lo = 0, hi = 1; bool valid = false; }; // start..end = talet v i texten
    std::vector<SliderSpec> sliderSpecs;
    juce::String sliderText;                               // texten specarna är hämtade ur
    std::vector<std::unique_ptr<juce::Slider>> sliderViews; // ett per giltigt slider()-anrop (index = ordningsnummer)
    // G3: noterna som låter just nu lyses upp i koden. Marks hör till en viss text (hash) och slängs när texten ändras.
    struct MarkOverlay : juce::Component
    {
        MarkOverlay() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
        std::function<void (juce::Graphics&)> onPaint;
    };
    struct MarkWatch : private juce::Timer
    {
        explicit MarkWatch (std::function<void()> f) : callback (std::move (f)) { startTimerHz (30); }
        ~MarkWatch() override { stopTimer(); }
        void timerCallback() override { callback(); }
        std::function<void()> callback;
    };
    void updateMarks();
    void paintMarks (juce::Graphics& g);
    MarkOverlay markOverlay;
    std::vector<LiveCode::Mark> marks;
    std::vector<std::pair<int, int>> activeLocs;
    MarkWatch markWatch { [this] { updateMarks(); } };
    double paramWindowStart = 0.0; // tak på .set: högst maxParamWritesPerSecond skrivningar per sekund (en slider som dras kör koden hela tiden)
    int paramWindowCount = 0;
    static constexpr int maxParamWritesPerSecond = 30;
    std::map<juce::String, juce::String> lastParamWrites;  // F1: senast skrivna värdetext per parameter (bara ändringar skrivs)
    std::map<int, double> sliderValues;                    // dragna värden (ordningsnummer -> värde) tills de skrivits in i texten
    bool ownEdit = false;                                  // reglaget skriver själv i texten: ingen ny kodändringsloop
    juce::TextEditor errorLog; // fliken KONSOL: fel och console.log, senaste överst
    int errorCount = 0, logLines = 0;
    static constexpr int maxLogLines = 500;
    juce::TextButton codeTab { "KOD" }, errorTab { "KONSOL" };
    juce::TextButton playButton { "AUTO" }, stopButton { "STOP" }, createButton { "CREATE" }, codeOnlyButton { "CODE ONLY" }; // AUTO är av/på
    juce::Label lockLabel; // LÅS: 1-10 = instrument, K = kicken; en låst rad rör Live Code aldrig
    std::array<juce::TextButton, 11> lockButtons;
    juce::Label errorLabel, licenseLabel;
    juce::HyperlinkButton strudelLink { "strudel.cc", juce::URL ("https://strudel.cc") };
};
