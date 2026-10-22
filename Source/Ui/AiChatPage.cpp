#include "AiChatPage.h"
#include "../Core/AiProvider.h"
#include "../Core/AiMemory.h"
#include "../Core/Settings.h"
#include <map>

//==============================================================================
// Samtalet: ett block per inlägg (användaren till höger, AI:n till vänster, verktygsanrop som små rader), med bilder under.
class AiChatPage::Transcript : public juce::Component
{
public:
    std::function<void (const juce::String&, int, const juce::String&)> onRate;
    std::function<void (const juce::String&)> onFeedbackError;

    void setEntries (std::vector<AiChat::Entry> newEntries)
    {
        for (const auto& block : blocks)
            if (block.comment != nullptr)
                feedbackDrafts[block.feedbackId] = { block.feedbackRating, block.comment->getText() };
        entries = std::move (newEntries);
        layoutBlocks();
    }

    int layoutBlocks()
    {
        blocks.clear();
        const auto width = std::max (200, getWidth());
        auto y = 8;
        for (const auto& entry : entries)
        {
            using Kind = AiChat::Entry::Kind;
            Block block;
            block.entry = &entry;

            const auto maxTextWidth = entry.kind == Kind::note ? width - 80 : entry.kind == Kind::tool ? width - 60 : (int) ((float) width * 0.78f);
            const auto font = entry.kind == Kind::tool ? Theme::font (11.0f) : Theme::font (13.0f);
            const auto colour = entry.kind == Kind::tool || entry.kind == Kind::note ? Theme::textDim : entry.kind == Kind::error ? juce::Colour (0xffff9a9a) : Theme::text;
            juce::AttributedString text;
            text.setWordWrap (juce::AttributedString::byWord);
            text.append ((entry.kind == Kind::tool ? juce::String::fromUTF8 ("\xe2\x96\xb8 ") : juce::String()) + entry.text, font, colour);
            block.layout.createLayout (text, (float) (maxTextWidth - 24));

            const auto textHeight = (int) std::ceil (block.layout.getHeight());
            auto blockWidth = std::max (60, (int) std::ceil (block.layout.getWidth()) + 24);
            auto height = textHeight + (entry.kind == Kind::tool ? 8 : 18);
            if (entry.feedbackAvailable)
                blockWidth = std::max (blockWidth, 330);

            // Bilderna (spektrogram) under texten, i full bredd upp till 720 px.
            auto imagesHeight = 0;
            const auto imageWidth = std::min (720, width - 60);
            for (const auto& image : entry.images)
                imagesHeight += (int) ((float) imageWidth * (float) image.getHeight() / (float) std::max (1, image.getWidth())) + 6;
            if (! entry.images.empty())
                blockWidth = std::max (blockWidth, imageWidth + 24), height += imagesHeight + 4;
            if (entry.feedbackAvailable)
                height += 54;

            const auto x = entry.kind == Kind::user ? width - blockWidth - 12 : entry.kind == Kind::note ? (width - blockWidth) / 2 : 12;
            block.bounds = { x, y, std::min (blockWidth, width - 24), height };
            block.imageWidth = imageWidth;
            block.feedbackId = entry.feedbackId;
            if (entry.feedbackAvailable)
            {
                const auto draft = feedbackDrafts.find (entry.feedbackId);
                block.feedbackRating = draft != feedbackDrafts.end() ? draft->second.first : entry.feedbackRating;
                const auto rowY = block.bounds.getBottom() - 48;
                block.ratingLabel = { x + 12, rowY, 74, 18 };
                auto ratingX = block.ratingLabel.getRight() + 2;
                for (int rating = 1; rating <= 10; ++rating)
                {
                    block.ratingBounds[(size_t) (rating - 1)] = { ratingX, rowY, 20, 18 };
                    ratingX += 21;
                }
                block.saveBounds = { block.bounds.getRight() - 70, rowY + 23, 58, 21 };
                block.comment = std::make_unique<juce::TextEditor>();
                block.comment->setFont (Theme::font (11.0f));
                block.comment->setColour (juce::TextEditor::backgroundColourId, Theme::backgroundBottom);
                block.comment->setColour (juce::TextEditor::textColourId, Theme::text);
                block.comment->setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
                block.comment->setTextToShowWhenEmpty ("What were you not satisfied with? (optional)", Theme::textDim);
                block.comment->setText (draft != feedbackDrafts.end() ? draft->second.second : entry.feedbackComment, juce::dontSendNotification);
                block.comment->onTextChange = [this, id = entry.feedbackId, editor = block.comment.get()]
                {
                    feedbackDrafts[id].second = editor->getText();
                };
                block.comment->setBounds (x + 12, rowY + 22, std::max (40, block.bounds.getWidth() - 92), 22);
                addAndMakeVisible (*block.comment);
                feedbackDrafts[entry.feedbackId] = { block.feedbackRating, block.comment->getText() };
            }
            blocks.push_back (std::move (block));
            y += height + 8;
        }
        setSize (getWidth(), std::max (y + 4, 10));
        repaint();
        return y;
    }

    void paint (juce::Graphics& g) override
    {
        using Kind = AiChat::Entry::Kind;
        for (const auto& block : blocks)
        {
            const auto bounds = block.bounds.toFloat();
            const auto kind = block.entry->kind;
            if (kind == Kind::user)
            {
                g.setColour (Theme::bass.withAlpha (0.22f));
                g.fillRoundedRectangle (bounds, 9.0f);
                g.setColour (Theme::bass.withAlpha (0.7f));
                g.drawRoundedRectangle (bounds.reduced (0.5f), 9.0f, 1.0f);
            }
            else if (kind == Kind::assistant)
            {
                g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
                g.fillRoundedRectangle (bounds, 9.0f);
                g.setColour (Theme::panelBorder);
                g.drawRoundedRectangle (bounds.reduced (0.5f), 9.0f, 1.0f);
            }
            else if (kind == Kind::error)
            {
                g.setColour (juce::Colour (0x30ff5a5a));
                g.fillRoundedRectangle (bounds, 9.0f);
            }
            else if (kind == Kind::tool)
            {
                g.setColour (juce::Colour (0x14ffffff));
                g.fillRoundedRectangle (bounds, 6.0f);
            }

            block.layout.draw (g, bounds.reduced (12.0f, kind == Kind::tool ? 4.0f : 8.0f).withHeight (block.layout.getHeight()));

            auto y = bounds.getY() + (kind == Kind::tool ? 4.0f : 8.0f) + block.layout.getHeight() + 8.0f;
            for (const auto& image : block.entry->images)
            {
                const auto height = (float) block.imageWidth * (float) image.getHeight() / (float) std::max (1, image.getWidth());
                g.drawImage (image, juce::Rectangle<float> (bounds.getX() + 12.0f, y, (float) block.imageWidth, height));
                y += height + 6.0f;
            }

            if (block.entry->feedbackAvailable)
            {
                g.setFont (Theme::font (10.0f));
                g.setColour (Theme::textDim);
                g.drawText (block.entry->feedbackRating > 0 ? "RATED" : "RATE", block.ratingLabel, juce::Justification::centredLeft);
                for (int i = 0; i < 10; ++i)
                {
                    const auto r = block.ratingBounds[(size_t) i].toFloat();
                    g.setColour (block.feedbackRating == i + 1 ? Theme::mix : Theme::panel);
                    g.fillRoundedRectangle (r, 3.0f);
                    g.setColour (block.feedbackRating == i + 1 ? Theme::backgroundBottom : Theme::text);
                    g.drawText (juce::String (i + 1), r, juce::Justification::centred);
                }
                const auto save = block.saveBounds.toFloat();
                g.setColour (Theme::bass.withAlpha (0.5f));
                g.fillRoundedRectangle (save, 4.0f);
                g.setColour (Theme::text);
                g.drawText ("SAVE", save, juce::Justification::centred);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (auto& block : blocks)
        {
            if (event.mods.isPopupMenu() && block.bounds.contains (event.getPosition()))
            {
                juce::PopupMenu menu;
                menu.addItem ("Copy text", [text = block.entry->text] { juce::SystemClipboard::copyTextToClipboard (text); });
                menu.showMenuAsync (juce::PopupMenu::Options());
                return;
            }
            if (! block.entry->feedbackAvailable)
                continue;
            for (int rating = 1; rating <= 10; ++rating)
                if (block.ratingBounds[(size_t) (rating - 1)].contains (event.getPosition()))
                {
                    block.feedbackRating = rating;
                    feedbackDrafts[block.feedbackId].first = rating;
                    repaint();
                    return;
                }
            if (block.saveBounds.contains (event.getPosition()))
            {
                if (block.feedbackRating <= 0)
                {
                    if (onFeedbackError)
                        onFeedbackError ("Choose a rating from 1 to 10 first.");
                }
                else if (onRate)
                    onRate (block.feedbackId, block.feedbackRating, block.comment->getText());
                return;
            }
        }
    }

    int contentHeight() const { return getHeight(); }

private:
    struct Block
    {
        const AiChat::Entry* entry = nullptr;
        juce::Rectangle<int> bounds;
        juce::Rectangle<int> ratingLabel, saveBounds;
        std::array<juce::Rectangle<int>, 10> ratingBounds;
        juce::TextLayout layout;
        std::unique_ptr<juce::TextEditor> comment;
        juce::String feedbackId;
        int feedbackRating = 0;
        int imageWidth = 0;
    };

    std::vector<AiChat::Entry> entries;
    std::vector<Block> blocks;
    std::map<juce::String, std::pair<int, juce::String>> feedbackDrafts;
};

//==============================================================================
AiChatPage::AiChatPage (PluginBase& p) : processor (p), chat (p.getAiChat())
{
    title.setText ("AI ASSISTANT", juce::dontSendNotification);
    title.setFont (Theme::font (13.0f, true));
    title.setColour (juce::Label::textColourId, Theme::bass);
    addAndMakeVisible (title);

    status.setFont (Theme::font (11.0f));
    status.setColour (juce::Label::textColourId, Theme::textDim);
    status.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (status);

    modelBox.setEditableText (true); // egen modell kan skrivas in (listan hämtas från Google och kan sakna nya namn)
    modelBox.onChange = [this]
    {
        const auto model = modelBox.getText().trim();
        if (model.isEmpty())
            return;
        const auto previous = gemini() ? Settings::geminiModel() : Settings::chatModel();
        if (gemini())
        {
            Settings::setGeminiModel (model);
            checkModel (false);
        }
        else
            Settings::setChatModel (model);

        // Byte mitt i ett samtal syns i loggen (inte i en tom chatt, och inte när samma namn väljs igen).
        if (model != previous && ! chat.getEntries().empty())
        {
            AiChat::Entry entry;
            entry.kind = AiChat::Entry::Kind::note;
            entry.text = "Model changed to " + model + ".";
            chat.addEntry (std::move (entry));
        }
    };
    addAndMakeVisible (modelBox);
    populateModels();
    checkModel (true);

    costBox.addItem ("Frugal", 1);
    costBox.addItem ("Normal", 2);
    costBox.addItem ("Rich", 3);
    costBox.setSelectedId (Settings::chatCostMode() + 1, juce::dontSendNotification);
    costBox.setTooltip ("COST - How many tokens the chat may use (a free key runs out fast). Frugal: short answers, few steps, old turns trimmed hard, asks before images. "
                        "Normal: the everyday balance, asks before images. Rich: long answers, many steps, most of the history kept, never asks.");
    costBox.onChange = [this] { Settings::setChatCostMode (costBox.getSelectedId() - 1); };
    addAndMakeVisible (costBox);

    confirmLabel.setFont (Theme::font (12.0f));
    confirmLabel.setColour (juce::Label::textColourId, Theme::text);
    confirmLabel.setMinimumHorizontalScale (0.8f);
    addChildComponent (confirmLabel);
    runButton.onClick = [this] { chat.answerConfirm (1); refresh(); };
    skipButton.onClick = [this] { chat.answerConfirm (2); refresh(); };
    runAlwaysButton.onClick = [this] { chat.answerConfirm (3); refresh(); };
    runButton.setColour (juce::TextButton::buttonColourId, Theme::bass.withAlpha (0.5f));
    for (auto* button : { &runButton, &skipButton, &runAlwaysButton })
        addChildComponent (button);

    keyButton.onClick = [this] { showSetup (! setupOpen); };
    addAndMakeVisible (keyButton);
    newChatButton.setTooltip ("NEW CHAT - Starts a new conversation (the sound stays as it is).");
    newChatButton.onClick = [this] { chat.newChat(); refresh(); };
    addAndMakeVisible (newChatButton);
    chatsButton.setTooltip ("CHATS - Save this chat as a session, open a saved one, or export it to a text file. Long chats are saved and exported automatically before the oldest questions are dropped. API keys are never written to these files.");
    chatsButton.onClick = [this] { showChatsMenu(); };
    addAndMakeVisible (chatsButton);
    memoryButton.setTooltip ("MEMORY - Configure local feedback and work lessons, what the AI may load, and storage limits.");
    memoryButton.onClick = [this] { showMemorySettings(); };
    addAndMakeVisible (memoryButton);
    undoButton.setTooltip ("UNDO - Undoes the AI's last change to the sound.");
    undoButton.onClick = [this] { chat.undo(); refresh(); };
    addAndMakeVisible (undoButton);

    input.setMultiLine (true, true);
    input.setReturnKeyStartsNewLine (true);
    input.setTextToShowWhenEmpty ("Ask for a sound or a change ... (Enter sends, Shift+Enter makes a new line)", Theme::textDim);
    input.setFont (Theme::font (13.0f));
    input.setColour (juce::TextEditor::backgroundColourId, Theme::knobBodyDark);
    input.setColour (juce::TextEditor::textColourId, Theme::text);
    input.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    input.setColour (juce::TextEditor::focusedOutlineColourId, Theme::bass);
    input.onSend = [this] { send (input.getText()); };
    input.onTextChange = [this] { updateSendButton(); };
    addAndMakeVisible (input);
    sendButton.setColour (juce::TextButton::buttonColourId, Theme::bass.withAlpha (0.5f));
    sendButton.onClick = [this] { send (input.getText()); };
    addAndMakeVisible (sendButton);
    stopButton.setTooltip ("STOP - Stops after the step that is running.");
    stopButton.onClick = [this] { chat.stop(); };
    addChildComponent (stopButton);

    const char* ideas[] { "Make the kick punchier", "Build a rolling acid bass on instrument 3", "Check my mix: is the low end clean under the kick?" };
    for (size_t i = 0; i < suggestions.size(); ++i)
    {
        suggestions[i].setButtonText (ideas[i]);
        suggestions[i].onClick = [this, text = juce::String (ideas[i])] { send (text); };
        addChildComponent (suggestions[i]);
    }

    transcript = std::make_unique<Transcript>();
    transcript->onRate = [this] (const juce::String& id, int rating, const juce::String& comment)
    {
        juce::String error;
        if (chat.rateEntry (id, rating, comment, error))
        {
            if (chat.reviewFeedback (id, error))
                status.setText ("Feedback saved. The AI is reviewing it and will propose a plan without changing the sound.", juce::dontSendNotification);
            else
                status.setText ("Feedback saved locally. " + error, juce::dontSendNotification);
        }
        else
            status.setText (error, juce::dontSendNotification);
    };
    transcript->onFeedbackError = [this] (const juce::String& error) { status.setText (error, juce::dontSendNotification); };
    viewport.setViewedComponent (transcript.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    // Nyckeln (första gången, eller med KEY-knappen).
    setupTitle.setText ("CONNECT YOUR AI", juce::dontSendNotification);
    setupTitle.setFont (Theme::font (15.0f, true));
    setupTitle.setColour (juce::Label::textColourId, Theme::bass);
    setupTitle.setJustificationType (juce::Justification::centred);
    addChildComponent (setupTitle);
    setupText.setFont (Theme::font (12.0f));
    setupText.setColour (juce::Label::textColourId, Theme::text);
    setupText.setJustificationType (juce::Justification::centred);
    addChildComponent (setupText);

    // Leverantören: två knappar, den valda lyser.
    anthropicButton.setClickingTogglesState (false);
    anthropicButton.onClick = [this] { setProvider (false); };
    addChildComponent (anthropicButton);
    geminiButton.onClick = [this] { setProvider (true); };
    addChildComponent (geminiButton);
    guideButton.setTooltip ("Opens Google AI Studio in your browser, where you create a Gemini API key. Paste it here afterwards.");
    guideButton.onClick = [this]
    {
        juce::URL ("https://aistudio.google.com/apikey").launchInDefaultBrowser();
        showTestResult ("Google AI Studio opened in your browser. Follow steps 1-3, then paste the key above.", true);
        keyEditor.grabKeyboardFocus();
    };
    addChildComponent (guideButton);
    testResult.setFont (Theme::font (12.0f, true));
    testResult.setJustificationType (juce::Justification::centred);
    addChildComponent (testResult);

    keyEditor.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    keyEditor.setColour (juce::TextEditor::backgroundColourId, Theme::knobBodyDark);
    keyEditor.setColour (juce::TextEditor::textColourId, Theme::text);
    keyEditor.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    keyEditor.onReturnKey = [this] { saveKey(); };
    addChildComponent (keyEditor);
    saveKeyButton.onClick = [this] { saveKey(); };
    addChildComponent (saveKeyButton);
    cancelKeyButton.onClick = [this] { showSetup (false); };
    addChildComponent (cancelKeyButton);
    removeKeyButton.setTooltip ("Removes the saved key from this computer.");
    removeKeyButton.onClick = [this]
    {
        if (gemini())
            Settings::setGeminiApiKey ({});
        else
            Settings::setChatApiKey ({});
        keyEditor.setText ({}, false);
        lastHadKey = false;
        showSetup (true);
    };
    addChildComponent (removeKeyButton);

    showSetup (needsKey());
    refresh();
    startTimerHz (8);
}

AiChatPage::~AiChatPage() = default;

bool AiChatPage::needsKey() const
{
    return (gemini() ? Settings::geminiApiKey() : Settings::chatApiKey()).isEmpty();
}

void AiChatPage::populateModels()
{
    modelBox.clear (juce::dontSendNotification);
    const auto models = AiChat::models();
    for (const auto& model : models)
        modelBox.addItem (model, modelBox.getNumItems() + 1);
    const auto current = gemini() ? Settings::geminiModel() : Settings::chatModel();
    if (models.contains (current))
        modelBox.setSelectedId (models.indexOf (current) + 1, juce::dontSendNotification);
    else
        modelBox.setText (current, juce::dontSendNotification);

    if (gemini())
    {
        updateModelNote();
        modelBox.setTooltip ("MODEL - Which Gemini model answers. Flash is the everyday choice and works on the free tier, Flash-Lite is the fastest, Pro the most capable "
                             "(its free limits are smaller). The limits for your key are shown in Google AI Studio.");
        keyButton.setTooltip ("KEY - Your own Gemini API key (free from Google AI Studio). It is stored encrypted on this computer and only sent to generativelanguage.googleapis.com.");
    }
    else
    {
        modelBox.setTooltip ("MODEL - Which Claude model answers. Sonnet is a good everyday choice, Opus the most capable (and the most expensive), Haiku the fastest and cheapest. "
                             "Usage is billed to your own Anthropic account.");
        keyButton.setTooltip ("KEY - Your own Anthropic API key (from console.anthropic.com). It is stored encrypted on this computer and only sent to api.anthropic.com.");
    }
}

// Texter och knappar på nyckelsidan för vald leverantör.
void AiChatPage::updateSetupTexts()
{
    const auto useGemini = gemini();
    anthropicButton.setColour (juce::TextButton::buttonColourId, useGemini ? Theme::knobBodyDark : Theme::bass.withAlpha (0.55f));
    geminiButton.setColour (juce::TextButton::buttonColourId, useGemini ? Theme::bass.withAlpha (0.55f) : Theme::knobBodyDark);
    keyEditor.setTextToShowWhenEmpty (useGemini ? "AIza..." : "sk-ant-...", Theme::textDim);
    saveKeyButton.setButtonText (useGemini ? "TEST & SAVE" : "SAVE KEY");
    guideButton.setVisible (setupOpen && useGemini);

    if (useGemini)
        setupText.setText ("Chat with an AI that designs the sound for you, right here in the synth. Google's Gemini has a free tier, so you do not need a paid account.\n\n"
                           "1. Click GET A FREE KEY: Google AI Studio opens in your browser. Sign in with a Google account.\n"
                           "2. Click \"Create API key\" and copy the key (it starts with AIza).\n"
                           "3. Paste it below and click TEST & SAVE.\n\n"
                           "Free within the limits shown in Google AI Studio; when they are used up the chat tells you and you can try again later. "
                           "On the free tier Google may use what you send to improve its products and people may read it, so keep private things out. You must be 18 or older. "
                           "In the EEA, Switzerland and the UK Google's terms can require a paid plan: see ai.google.dev/gemini-api/terms. "
                           "The key is stored encrypted on this computer and only sent to generativelanguage.googleapis.com.",
                           juce::dontSendNotification);
    else
        setupText.setText ("Chat with an AI that designs the sound for you, right here in the synth. It needs your own Anthropic API key (create one at console.anthropic.com).\n\n"
                           "The key is stored encrypted on this computer and is only ever sent to api.anthropic.com. Usage is billed to your Anthropic account, "
                           "and a conversation uses some thousand tokens per step (the NEW CHAT button starts over).",
                           juce::dontSendNotification);
}

void AiChatPage::updateModelNote()
{
    modelNote = {};
    if (gemini() && ! needsKey())
        if (Settings::geminiToolsCheck (Settings::geminiModel(), Settings::geminiApiKey()) == 0)
            modelNote = "WARNING: " + Settings::geminiModel() + " did not use tools in a test; pick another model.";
}

void AiChatPage::checkModel (bool fetchList)
{
    updateModelNote();
    if (needsKey() || checking)
        return;
    if (! gemini())
    {
        if (! fetchList)
            return;
        checking = true;
        juce::Thread::launch ([safe = juce::Component::SafePointer<AiChatPage> (this), key = Settings::chatApiKey()]
        {
            const auto list = AiChat::fetchAnthropicModels (key);
            juce::MessageManager::callAsync ([safe, list]
            {
                if (safe == nullptr)
                    return;
                safe->checking = false;
                if (! list.isEmpty() && ! safe->gemini())
                    safe->populateModels();
            });
        });
        return;
    }
    const auto key = Settings::geminiApiKey();
    const auto model = Settings::geminiModel();
    const auto needsProbe = Settings::geminiToolsCheck (model, key) < 0;
    if (! fetchList && ! needsProbe)
        return;

    checking = true;
    juce::Thread::launch ([safe = juce::Component::SafePointer<AiChatPage> (this), key, model, fetchList, needsProbe]
    {
        const auto list = fetchList ? GeminiApi::fetchModels (key) : juce::StringArray();
        const auto probe = needsProbe ? GeminiApi::probeTools (key, model) : -1;
        juce::MessageManager::callAsync ([safe, key, model, list, probe]
        {
            if (safe == nullptr)
                return;
            safe->checking = false;
            if (probe >= 0)
                Settings::setGeminiToolsCheck (model, key, probe == 1);
            if (! list.isEmpty() && safe->gemini())
                safe->populateModels();
            safe->updateModelNote();
        });
    });
}

void AiChatPage::setProvider (bool useGemini)
{
    if (chat.isBusy() || useGemini == gemini())
        return;
    Settings::setChatProvider (useGemini ? "gemini" : "anthropic");
    chat.newChat(); // samtalet hör till leverantören: det börjar om
    populateModels();
    checkModel (true);
    lastHadKey = ! needsKey();
    showSetup (true);
}

void AiChatPage::showTestResult (const juce::String& message, bool ok)
{
    testResult.setText (message, juce::dontSendNotification);
    testResult.setColour (juce::Label::textColourId, ok ? juce::Colour (0xff7be495) : juce::Colour (0xffff9a9a));
    testResult.setVisible (setupOpen && message.isNotEmpty());
}

void AiChatPage::showSetup (bool show)
{
    setupOpen = show;
    keyEditor.setText ({}, false);
    testResult.setText ({}, juce::dontSendNotification);
    for (auto* component : std::initializer_list<juce::Component*> { &setupTitle, &setupText, &keyEditor, &saveKeyButton, &cancelKeyButton, &removeKeyButton, &anthropicButton, &geminiButton })
        component->setVisible (show);
    testResult.setVisible (false);
    cancelKeyButton.setVisible (show && ! needsKey());
    removeKeyButton.setVisible (show && ! needsKey());
    updateSetupTexts();
    viewport.setVisible (! show);
    input.setVisible (! show);
    sendButton.setVisible (! show);
    resized();
    refresh();
}

void AiChatPage::saveKey()
{
    const auto key = keyEditor.getText().trim();
    if (key.isEmpty() || testing)
        return;

    if (! gemini())
    {
        Settings::setChatApiKey (key);
        showSetup (false);
        return;
    }

    // Gemini: nyckeln provas med ett litet anrop på en bakgrundstråd (det kan ta några sekunder), och sparas bara om den fungerar.
    testing = true;
    saveKeyButton.setEnabled (false);
    showTestResult ("Testing the key ...", true);
    juce::Thread::launch ([safe = juce::Component::SafePointer<AiChatPage> (this), key]
    {
        const auto result = GeminiApi::testKey (key);
        juce::MessageManager::callAsync ([safe, key, result]
        {
            if (safe == nullptr)
                return;
            safe->testing = false;
            safe->saveKeyButton.setEnabled (true);
            if (result.ok && safe->gemini())
            {
                Settings::setGeminiApiKey (key);
                safe->showSetup (false);
                safe->checkModel (true); // nytt nyckeltest: modelllistan hämtas och modellen provas om (cachen gäller per nyckel)
            }
            else
                safe->showTestResult (result.message, false);
        });
    });
}

void AiChatPage::send (const juce::String& typed)
{
    // Tomt fält efter stegtaket: Enter och knappen (CONTINUE) skickar "continue".
    const auto text = typed.trim().isEmpty() && chat.canContinue() ? juce::String ("continue") : typed;
    if (text.trim().isEmpty() || chat.isBusy())
        return;
    input.clear();
    chat.send (text);
    refresh();
    input.grabKeyboardFocus();
}

void AiChatPage::updateSendButton()
{
    // CONTINUE (grön) bara när svaret stoppats av ett tak och fältet är tomt; annars SEND.
    const auto continuing = chat.canContinue() && input.getText().trim().isEmpty();
    if (continuing == (sendButton.getButtonText() == "CONTINUE"))
        return;
    sendButton.setButtonText (continuing ? "CONTINUE" : "SEND");
    sendButton.setTooltip (continuing ? "CONTINUE - The AI stopped before it was done. Sends 'continue' (Enter in an empty field does the same)." : juce::String());
    sendButton.setColour (juce::TextButton::buttonColourId, continuing ? juce::Colour (0xff2e9e4f) : Theme::bass.withAlpha (0.5f));
}

void AiChatPage::timerCallback()
{
    refresh();
}

void AiChatPage::showMemorySettings()
{
    const int lookbackValues[] { 10, 25, 100, 250, 500, 1000, 10000, AiMemory::allEntries };
    auto addLookbacks = [&] (juce::PopupMenu& target, int base, int selected, const juce::String& noun)
    {
        for (int i = 0; i < (int) std::size (lookbackValues); ++i)
        {
            const auto count = lookbackValues[i];
            const auto label = count == AiMemory::allEntries ? "ALL available " + noun
                                                             : juce::String (count) + " recent " + noun;
            target.addItem (base + i, label, true, count == selected);
        }
    };

    juce::PopupMenu feedbackLookback, workLookback;
    addLookbacks (feedbackLookback, 100, Settings::feedbackMemoryLookback(), "feedback entries");
    addLookbacks (workLookback, 120, Settings::workMemoryLookback(), "work lessons");

    juce::PopupMenu feedbackRetention, workRetention;
    for (const auto megabytes : { 1, 10, 50, 100 })
    {
        feedbackRetention.addItem (200 + megabytes, juce::String (megabytes) + " MB",
                                   true, megabytes == Settings::feedbackMemoryMaxMb());
        workRetention.addItem (400 + megabytes, juce::String (megabytes) + " MB",
                               true, megabytes == Settings::workMemoryMaxMb());
    }

    juce::PopupMenu menu;
    menu.addItem (1, "Auto-include feedback in AI context (may use tokens)", true, Settings::feedbackMemoryAutoInclude());
    menu.addItem (2, "Auto-include work lessons in AI context (may use tokens)", true, Settings::workMemoryAutoInclude());
    menu.addSubMenu ("Feedback entries available to the AI", feedbackLookback);
    menu.addSubMenu ("Work lessons available to the AI", workLookback);
    menu.addSubMenu ("Maximum local feedback storage", feedbackRetention);
    menu.addSubMenu ("Maximum local work-memory storage", workRetention);
    menu.addItem (6, "Estimated selected memory: about " + juce::String (AiMemory::estimatedPromptTokens())
                          + " tokens per request; later turns may count it again.",
                  false);
    menu.addSeparator();
    menu.addItem (4, "Clear current chat (saved feedback stays)", ! chat.isBusy());
    menu.addItem (3, "Clear all saved feedback...");
    menu.addItem (5, "Clear all saved work lessons...");

    const juce::Component::SafePointer<AiChatPage> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&memoryButton),     [safe, lookbackValues = std::array<int, 8> { 10, 25, 100, 250, 500, 1000, 10000, AiMemory::allEntries }] (int result)
    {
        if (safe == nullptr)
            return;

        if (result == 1)
        {
            const auto enable = ! Settings::feedbackMemoryAutoInclude();
            Settings::setFeedbackMemoryAutoInclude (enable);
            safe->status.setText (enable ? "Selected local feedback will be included in AI context; it may be counted again on later requests."
                                         : "Automatic feedback context is off. The AI may still read it with get_memory when relevant.",
                                  juce::dontSendNotification);
        }
        else if (result == 2)
        {
            const auto enable = ! Settings::workMemoryAutoInclude();
            Settings::setWorkMemoryAutoInclude (enable);
            safe->status.setText (enable ? "Selected local work lessons will be included in AI context; they may be counted again on later requests."
                                         : "Automatic work-memory context is off. The AI may still read it with get_work_memory when relevant.",
                                  juce::dontSendNotification);
        }
        else if (result >= 100 && result < 108)
        {
            const auto count = lookbackValues[result - 100];
            Settings::setFeedbackMemoryLookback (count);
            safe->status.setText ("Feedback lookback set to " + (count == AiMemory::allEntries ? juce::String ("ALL") : juce::String (count))
                                  + ". Estimated context: about " + juce::String (AiMemory::estimatedPromptTokens()) + " tokens.",
                                  juce::dontSendNotification);
        }
        else if (result >= 120 && result < 128)
        {
            const auto count = lookbackValues[result - 120];
            Settings::setWorkMemoryLookback (count);
            safe->status.setText ("Work-memory lookback set to " + (count == AiMemory::allEntries ? juce::String ("ALL") : juce::String (count))
                                  + ". Estimated context: about " + juce::String (AiMemory::estimatedPromptTokens()) + " tokens.",
                                  juce::dontSendNotification);
        }
        else if (result >= 201 && result <= 300)
        {
            const auto megabytes = result - 200;
            juce::String error;
            if (! AiMemory::trimToLimit (AiMemory::file(), megabytes, error))
            {
                safe->status.setText (error, juce::dontSendNotification);
                return;
            }
            Settings::setFeedbackMemoryMaxMb (megabytes);
            safe->status.setText ("Local feedback storage capped at " + juce::String (megabytes) + " MB.", juce::dontSendNotification);
        }
        else if (result >= 401 && result <= 500)
        {
            const auto megabytes = result - 400;
            juce::String error;
            if (! AiMemory::trimWorkToLimit (AiMemory::workFile(), megabytes, error))
            {
                safe->status.setText (error, juce::dontSendNotification);
                return;
            }
            Settings::setWorkMemoryMaxMb (megabytes);
            safe->status.setText ("Local work memory capped at " + juce::String (megabytes) + " MB.", juce::dontSendNotification);
        }
        else if (result == 3)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Clear shared feedback memory")
                                              .withMessage ("This deletes saved ratings and comments shared by all AI chats and MCP clients. "
                                                            "It does not delete messages from the current chat. Continue?")
                                              .withButton ("Delete feedback").withButton ("Cancel"),
                                          [safe] (int choice)
                                          {
                                              if (safe == nullptr || choice != 1)
                                                  return;
                                              juce::String error;
                                              safe->status.setText (AiMemory::clear (error)
                                                                        ? "Saved feedback was cleared from the shared local memory."
                                                                        : error,
                                                                    juce::dontSendNotification);
                                          });
        }
        else if (result == 4)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                              .withTitle ("Clear current chat")
                                              .withMessage ("This removes messages from the open conversation only. Saved chats and shared feedback memory are kept. Continue?")
                                              .withButton ("Clear chat").withButton ("Cancel"),
                                          [safe] (int choice)
                                          {
                                              if (safe == nullptr || choice != 1)
                                                  return;
                                              if (safe->chat.isBusy())
                                              {
                                                  safe->status.setText ("Stop the current request before clearing this chat.", juce::dontSendNotification);
                                                  return;
                                              }
                                              safe->chat.newChat();
                                              safe->refresh();
                                              safe->status.setText ("Current conversation cleared; saved feedback is unchanged.", juce::dontSendNotification);
                                          });
        }
        else if (result == 5)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                              .withTitle ("Clear shared work memory")
                                              .withMessage ("This deletes all locally saved error-recovery lessons shared by chat and MCP. "
                                                            "Feedback ratings and current-chat messages are not deleted. Continue?")
                                              .withButton ("Delete work lessons").withButton ("Cancel"),
                                          [safe] (int choice)
                                          {
                                              if (safe == nullptr || choice != 1)
                                                  return;
                                              juce::String error;
                                              safe->status.setText (AiMemory::clearWork (error)
                                                                        ? "Saved work lessons were cleared from the shared local memory."
                                                                        : error,
                                                                    juce::dontSendNotification);
                                          });
        }
    });
}

void AiChatPage::showChatsMenu()
{
    auto note = [this] (juce::String text, bool error = false)
    {
        AiChat::Entry entry;
        entry.kind = error ? AiChat::Entry::Kind::error : AiChat::Entry::Kind::note;
        entry.text = std::move (text);
        chat.addEntry (std::move (entry));
    };

    const auto sessions = AiChat::listSessions();
    juce::PopupMenu open;
    for (int i = 0; i < (int) sessions.size() && i < 20; ++i)
        open.addItem (100 + i, sessions[(size_t) i].saved.formatted ("%Y-%m-%d %H:%M") + "  " + sessions[(size_t) i].title);
    if (sessions.empty())
        open.addItem (1, "(no saved chats)", false);

    juce::PopupMenu remove;
    for (int i = 0; i < (int) sessions.size() && i < 20; ++i)
        remove.addItem (200 + i, sessions[(size_t) i].saved.formatted ("%Y-%m-%d %H:%M") + "  " + sessions[(size_t) i].title);
    remove.addSeparator();
    remove.addItem (5, "Delete all saved chats");

    juce::PopupMenu menu;
    menu.addItem (2, "Save this chat", chat.hasConversation());
    menu.addItem (3, "Export this chat to a text file", chat.hasConversation());
    menu.addSubMenu ("Open a saved chat", open, ! sessions.empty());
    menu.addSubMenu ("Delete a saved chat (to the recycle bin)", remove, ! sessions.empty());
    menu.addSeparator();
    menu.addItem (4, "Show the chats folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chatsButton), [this, sessions, note] (int result)
    {
        juce::File file;
        if (result == 2)
        {
            if (chat.saveSession (file))
                note ("Saved the chat to " + file.getFullPathName());
            else
                note ("Could not save the chat.", true);
        }
        else if (result == 3)
        {
            if (chat.exportText (file))
                note ("Exported the chat to " + file.getFullPathName());
            else
                note ("Could not export the chat.", true);
        }
        else if (result == 4)
            AiChat::sessionsFolder().revealToUser();
        else if (result >= 100 && result - 100 < (int) sessions.size())
        {
            if (! chat.loadSession (sessions[(size_t) (result - 100)].file))
                note ("Could not open that chat.", true);
        }
        else if ((result >= 200 && result - 200 < (int) sessions.size()) || result == 5)
        {
            // Bekräfta, och flytta till papperskorgen. Den öppna chatten ligger kvar på skärmen.
            const auto all = result == 5;
            const auto first = all ? 0 : result - 200, last = all ? (int) sessions.size() : first + 1;
            const auto text = all ? juce::String ("Move all ") + juce::String ((int) sessions.size()) + " saved chats to the recycle bin?"
                                  : "Move \"" + sessions[(size_t) first].title + "\" to the recycle bin?";
            juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                              .withTitle ("Delete saved chat").withMessage (text)
                                              .withButton ("Delete").withButton ("Cancel"),
                                          [this, sessions, first, last, note] (int choice)
                                          {
                                              if (choice != 1)
                                                  return;
                                              auto failed = 0;
                                              for (int i = first; i < last; ++i)
                                                  failed += AiChat::deleteSession (sessions[(size_t) i].file) ? 0 : 1;
                                              note (failed == 0 ? "Moved " + juce::String (last - first) + " saved chat(s) to the recycle bin."
                                                                : "Could not delete " + juce::String (failed) + " chat(s).", failed != 0);
                                              refresh();
                                          });
            return;
        }
        refresh();
    });
}

void AiChatPage::refresh()
{
    updateSendButton();
    const auto busy = chat.isBusy();
    const auto entries = chat.getEntries();
    const auto hasKey = ! needsKey();
    if (chat.version.load() != lastVersion || busy != lastBusy || hasKey != lastHadKey)
    {
        lastVersion = chat.version.load();
        lastBusy = busy;
        lastHadKey = hasKey;

        const auto atBottom = viewport.getViewPositionY() + viewport.getViewHeight() >= transcript->getHeight() - 24;
        transcript->setSize (viewport.getMaximumVisibleWidth(), transcript->getHeight());
        if (entries.empty() && hasKey)
        {
            // Hälsningen är bara en visning i en tom chatt: den sparas inte och skickas inte till modellen (kostar inga tokens).
            AiChat::Entry greeting;
            greeting.kind = AiChat::Entry::Kind::assistant;
            greeting.text = "Hi! I'm your co-producer, here to help you make your kick and bass sound the way you hear it in your head.\n\n"
                            "Here is what I can do:\n"
                            "- Design sounds: set parameters and build kick and bass patches, layers, FM, filters and modulation\n"
                            "- Write patterns: notes, rhythms and slices, and live code\n"
                            "- Shape the mix: effects, EQ and sidechain, and compare against a reference\n"
                            "- Analyse your mix: find clashes between kick and bass, read the report and fix what it finds\n"
                            "- Work with presets: browse, load and save sounds\n"
                            "- Undo anything I change, so it's safe to experiment\n\n"
                            "How can I help you today?";
            transcript->setEntries ({ greeting });
        }
        else
            transcript->setEntries (entries);
        layoutSuggestions();
        if (atBottom || busy)
            viewport.setViewPosition (0, std::max (0, transcript->getHeight() - viewport.getHeight()));

        stopButton.setVisible (busy && ! setupOpen);
        sendButton.setVisible (! busy && ! setupOpen);
        newChatButton.setEnabled (! busy);
        chatsButton.setEnabled (! busy);
        undoButton.setEnabled (! busy);
        input.setEnabled (true);
        for (auto& suggestion : suggestions)
            suggestion.setVisible (entries.empty() && hasKey && ! setupOpen);
    }

    const auto pending = chat.getPendingConfirm();
    if (pending.isNotEmpty() != confirmOpen)
    {
        confirmOpen = pending.isNotEmpty();
        for (auto* component : std::initializer_list<juce::Component*> { &confirmLabel, &runButton, &skipButton, &runAlwaysButton })
            component->setVisible (confirmOpen && ! setupOpen);
        resized();
    }
    if (confirmOpen)
        confirmLabel.setText (pending, juce::dontSendNotification);

    const auto statusText = chat.getStatus();
    const auto usage = chat.getUsage();
    status.setText (modelNote.isNotEmpty() ? modelNote
                    : statusText.isNotEmpty() ? statusText + (usage.isNotEmpty() ? "   " + usage : juce::String()) : usage, juce::dontSendNotification);
}

void AiChatPage::paint (juce::Graphics& g)
{
    g.setColour (Theme::knobBodyDark.withAlpha (0.45f));
    g.fillRoundedRectangle (viewport.getBounds().toFloat().expanded (2.0f), 6.0f);
    if (suggestions[0].isVisible())
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (13.0f));
        g.drawText ("Tell the AI what sound you want, or try one of these:", hintBounds, juce::Justification::centred);
    }
}

void AiChatPage::layoutSuggestions()
{
    // Under hälsningen (transcriptens höjd); får de inte plats ligger de längst ner i vyn.
    const auto area = viewport.getBounds();
    const auto chipWidth = std::min (330, (area.getWidth() - 40) / 3);
    const auto top = std::min (area.getY() + transcript->getHeight() + 10, area.getBottom() - 30 - 26 - 6);
    hintBounds = { area.getX(), top, area.getWidth(), 22 };
    auto chips = juce::Rectangle<int> (chipWidth * 3 + 16, 30).withCentre ({ area.getCentreX(), top + 22 + 4 + 15 });
    for (auto& suggestion : suggestions)
    {
        suggestion.setBounds (chips.removeFromLeft (chipWidth));
        chips.removeFromLeft (8);
    }
    repaint();
}

void AiChatPage::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (28);
    title.setBounds (header.removeFromLeft (112));
    keyButton.setBounds (header.removeFromRight (48).reduced (0, 2));
    header.removeFromRight (4);
    undoButton.setBounds (header.removeFromRight (54).reduced (0, 2));
    header.removeFromRight (4);
    newChatButton.setBounds (header.removeFromRight (76).reduced (0, 2));
    header.removeFromRight (4);
    chatsButton.setBounds (header.removeFromRight (52).reduced (0, 2));
    header.removeFromRight (4);
    memoryButton.setBounds (header.removeFromRight (44).reduced (0, 2));
    header.removeFromRight (8);
    modelBox.setBounds (header.removeFromLeft (145).reduced (0, 2));
    header.removeFromLeft (4);
    costBox.setBounds (header.removeFromLeft (72).reduced (0, 2));
    header.removeFromLeft (10);
    status.setBounds (header);
    area.removeFromTop (6);

    auto bottom = area.removeFromBottom (70);
    area.removeFromBottom (6);
    sendButton.setBounds (bottom.removeFromRight (90).reduced (0, 6));
    stopButton.setBounds (sendButton.getBounds());
    bottom.removeFromRight (6);
    input.setBounds (bottom);

    if (confirmOpen)
    {
        auto strip = area.removeFromBottom (44);
        area.removeFromBottom (4);
        runAlwaysButton.setBounds (strip.removeFromRight (170).reduced (0, 6));
        strip.removeFromRight (4);
        skipButton.setBounds (strip.removeFromRight (70).reduced (0, 6));
        strip.removeFromRight (4);
        runButton.setBounds (strip.removeFromRight (70).reduced (0, 6));
        confirmLabel.setBounds (strip);
    }

    viewport.setBounds (area);
    transcript->setSize (viewport.getMaximumVisibleWidth(), transcript->getHeight());
    transcript->layoutBlocks();

    layoutSuggestions();

    // Nyckelsidan, mitt i ytan.
    auto setup = getLocalBounds().withTrimmedTop (34).withSizeKeepingCentre (640, gemini() ? 400 : 270);
    setupTitle.setBounds (setup.removeFromTop (28));
    auto providers = setup.removeFromTop (28);
    anthropicButton.setBounds (providers.removeFromLeft (200));
    providers.removeFromLeft (6);
    geminiButton.setBounds (providers.removeFromLeft (200));
    setup.removeFromTop (6);
    setupText.setBounds (setup.removeFromTop (gemini() ? 220 : 110));
    setup.removeFromTop (8);
    keyEditor.setBounds (setup.removeFromTop (28));
    setup.removeFromTop (8);
    auto buttons = setup.removeFromTop (28);
    saveKeyButton.setBounds (buttons.removeFromLeft (110));
    buttons.removeFromLeft (6);
    guideButton.setBounds (buttons.removeFromLeft (190));
    buttons.removeFromLeft (6);
    cancelKeyButton.setBounds (buttons.removeFromLeft (90));
    removeKeyButton.setBounds (buttons.removeFromRight (120));
    setup.removeFromTop (6);
    testResult.setBounds (setup.removeFromTop (36));
}
