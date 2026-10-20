#pragma once

#include "../Core/AiChat.h"
#include "Theme.h"

// Fliken AI: chatta med sin egen AI (Anthropic eller Googles Gemini, med egen API-nyckel) som ändrar ljudet i synten med verktygen. Första
// gången frågar sidan efter leverantör och nyckel; den sparas krypterad på datorn och skickas bara till den valda leverantören.
// För Gemini finns en guide (GET A FREE KEY) som öppnar Google AI Studio, och nyckeln provas innan den sparas.
class AiChatPage : public juce::Component, private juce::Timer
{
public:
    explicit AiChatPage (PluginBase& processor);
    ~AiChatPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Transcript;
    class InputBox : public juce::TextEditor
    {
    public:
        std::function<void()> onSend;
        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::returnKey && ! key.getModifiers().isShiftDown())
            {
                if (onSend)
                    onSend();
                return true;
            }
            return juce::TextEditor::keyPressed (key);
        }
    };

    void timerCallback() override;
    void refresh();
    void send (const juce::String& text);
    void showSetup (bool show);
    void saveKey();
    bool needsKey() const;
    bool gemini() const { return Settings::chatProvider() == "gemini"; }
    void setProvider (bool useGemini);
    void populateModels();
    void checkModel (bool fetchList); // Gemini: hämtar modelllistan (valfritt) och provar att vald modell kan verktyg (bakgrundstråd)
    void updateModelNote();
    void updateSetupTexts();
    void showTestResult (const juce::String& message, bool ok);

    PluginBase& processor;
    AiChat& chat;
    int lastVersion = -1;
    bool setupOpen = false, lastBusy = false, lastHadKey = false;

    bool testing = false; // Gemini-nyckeln provas just nu (bakgrundstråd)
    bool checking = false; // modellen provas just nu (bakgrundstråd)
    juce::String modelNote; // visas i statusraden: vald Gemini-modell saknar verktyg / kan inte nås

    juce::Label title, status, setupTitle, setupText, testResult;
    juce::ComboBox modelBox;
    juce::TextButton keyButton { "KEY" }, newChatButton { "NEW CHAT" }, undoButton { "UNDO" }, sendButton { "SEND" }, stopButton { "STOP" };
    juce::TextButton saveKeyButton { "SAVE KEY" }, cancelKeyButton { "CANCEL" }, removeKeyButton { "REMOVE KEY" };
    juce::TextButton anthropicButton { "ANTHROPIC (paid key)" }, geminiButton { "GEMINI (free option)" }, guideButton { "GET A FREE KEY (GUIDE)" };
    std::array<juce::TextButton, 3> suggestions;
    juce::TextEditor keyEditor;
    InputBox input;
    juce::Viewport viewport;
    std::unique_ptr<Transcript> transcript;
};
