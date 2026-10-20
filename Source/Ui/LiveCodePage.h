#pragma once

#include "../Core/LiveCodeRunner.h"
#include "../Core/ToolLayer.h"
#include "Theme.h"
#include <juce_gui_extra/juce_gui_extra.h>

// Fliken LIVE CODE: skriv Strudel-kod (https://strudel.cc). LIVE (förut PLAY) räknar om koden i bakgrunden ca 400 ms efter varje ändring och
// skriver mönstret i pianorullen (en ångra-punkt först); STOP slutar lyssna; CREATE skriver koden en gång.
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
    void setPlaying (bool shouldPlay);
    void runNow();
    void create();
    void finishCreate (const juce::var& result);
    void showError (const juce::String& text);
    bool apply (const juce::var& items, bool overwrite); // skriver raderna; frågar först om handritade toner skulle skrivas över
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
    bool playing = false;
    bool codeOnly = false;
    struct LoadWatch : private juce::Timer // ett projekt kan laddas från vilken tråd som helst: sidan tittar efter på meddelandetråden
    {
        explicit LoadWatch (std::function<void()> f) : callback (std::move (f)) { startTimerHz (4); }
        ~LoadWatch() override { stopTimer(); }
        void timerCallback() override { callback(); }
        std::function<void()> callback;
    };
    int seenLoads = 0;
    LoadWatch loadWatch { [this] { checkLoaded(); } };
    int savedSolo = 0, ownSolo = 0; // soloMask före CODE ONLY, och det värde CODE ONLY själv satte

    void showTab (bool errors);

    juce::CodeDocument document;
    juce::CPlusPlusCodeTokeniser tokeniser; // räcker för färg på strängar, siffror, kommentarer
    juce::CodeEditorComponent editor { document, &tokeniser };
    juce::TextEditor errorLog; // fliken FEL: hela meddelandet, senaste överst
    int errorCount = 0;
    juce::TextButton codeTab { "KOD" }, errorTab { "FEL" };
    juce::TextButton playButton { "LIVE" }, stopButton { "STOP" }, createButton { "CREATE" }, codeOnlyButton { "CODE ONLY" }; // LIVE är av/på
    juce::Label lockLabel; // LÅS: 1-10 = instrument, K = kicken; en låst rad rör Live Code aldrig
    std::array<juce::TextButton, 11> lockButtons;
    juce::Label errorLabel, licenseLabel;
    juce::HyperlinkButton strudelLink { "strudel.cc", juce::URL ("https://strudel.cc") };
};
