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
    void showError (const juce::String& text);

    ToolLayer tools;
    LiveCodeRunner runner;
    bool playing = false;

    void showTab (bool errors);

    juce::CodeDocument document;
    juce::CPlusPlusCodeTokeniser tokeniser; // räcker för färg på strängar, siffror, kommentarer
    juce::CodeEditorComponent editor { document, &tokeniser };
    juce::TextEditor errorLog; // fliken FEL: hela meddelandet, senaste överst
    int errorCount = 0;
    juce::TextButton codeTab { "KOD" }, errorTab { "FEL" };
    juce::TextButton playButton { "LIVE" }, stopButton { "STOP" }, createButton { "CREATE" }; // LIVE är av/på
    juce::Label errorLabel, licenseLabel;
    juce::HyperlinkButton strudelLink { "strudel.cc", juce::URL ("https://strudel.cc") };
};
