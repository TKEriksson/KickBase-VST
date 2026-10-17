#pragma once

#include "../Core/AiChat.h"
#include "Theme.h"

// Fliken AI: chatta med sin egen AI (Anthropic, med egen API-nyckel) som ändrar ljudet i synten med verktygen. Första gången frågar sidan
// efter nyckeln; den sparas krypterad på datorn och skickas bara till api.anthropic.com.
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

    PluginBase& processor;
    AiChat& chat;
    int lastVersion = -1;
    bool setupOpen = false, lastBusy = false, lastHadKey = false;

    juce::Label title, status, setupTitle, setupText;
    juce::ComboBox modelBox;
    juce::TextButton keyButton { "KEY" }, newChatButton { "NEW CHAT" }, undoButton { "UNDO" }, sendButton { "SEND" }, stopButton { "STOP" };
    juce::TextButton saveKeyButton { "SAVE KEY" }, cancelKeyButton { "CANCEL" }, removeKeyButton { "REMOVE KEY" };
    std::array<juce::TextButton, 3> suggestions;
    juce::TextEditor keyEditor;
    InputBox input;
    juce::Viewport viewport;
    std::unique_ptr<Transcript> transcript;
};
