#pragma once

#include "../Core/LiveCodeRunner.h"
#include "../Core/ToolLayer.h"
#include "Theme.h"

// Fliken LIVE CODE: skriv Strudel-kod (https://strudel.cc). PLAY räknar om koden i bakgrunden ca 400 ms efter varje ändring och
// skriver mönstret i pianorullen (en ångra-punkt först); STOP slutar lyssna; CREATE skriver koden en gång.
class LiveCodePage : public juce::Component, private juce::Timer
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
    void setPlaying (bool shouldPlay);
    void runNow();
    void create();
    void showError (const juce::String& text);

    ToolLayer tools;
    LiveCodeRunner runner;
    bool playing = false;

    juce::TextEditor editor;
    juce::TextButton playButton { "PLAY" }, stopButton { "STOP" }, createButton { "CREATE" };
    juce::Label errorLabel, licenseLabel;
    juce::HyperlinkButton strudelLink { "strudel.cc", juce::URL ("https://strudel.cc") };
};
