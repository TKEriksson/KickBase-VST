#include "AiChatPage.h"
#include "../Core/AiProvider.h"

//==============================================================================
// Samtalet: ett block per inlägg (användaren till höger, AI:n till vänster, verktygsanrop som små rader), med bilder under.
class AiChatPage::Transcript : public juce::Component
{
public:
    void setEntries (std::vector<AiChat::Entry> newEntries)
    {
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

            // Bilderna (spektrogram) under texten, i full bredd upp till 720 px.
            auto imagesHeight = 0;
            const auto imageWidth = std::min (720, width - 60);
            for (const auto& image : entry.images)
                imagesHeight += (int) ((float) imageWidth * (float) image.getHeight() / (float) std::max (1, image.getWidth())) + 6;
            if (! entry.images.empty())
                blockWidth = std::max (blockWidth, imageWidth + 24), height += imagesHeight + 4;

            const auto x = entry.kind == Kind::user ? width - blockWidth - 12 : entry.kind == Kind::note ? (width - blockWidth) / 2 : 12;
            block.bounds = { x, y, std::min (blockWidth, width - 24), height };
            block.imageWidth = imageWidth;
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
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
            return;
        for (const auto& block : blocks)
            if (block.bounds.contains (event.getPosition()))
            {
                juce::PopupMenu menu;
                menu.addItem ("Copy text", [text = block.entry->text] { juce::SystemClipboard::copyTextToClipboard (text); });
                menu.showMenuAsync (juce::PopupMenu::Options());
                return;
            }
    }

    int contentHeight() const { return getHeight(); }

private:
    struct Block
    {
        const AiChat::Entry* entry = nullptr;
        juce::Rectangle<int> bounds;
        juce::TextLayout layout;
        int imageWidth = 0;
    };

    std::vector<AiChat::Entry> entries;
    std::vector<Block> blocks;
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
        if (gemini())
        {
            Settings::setGeminiModel (model);
            checkModel (false);
        }
        else
            Settings::setChatModel (model);
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
    if (! gemini() || needsKey() || checking)
        return;
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

void AiChatPage::send (const juce::String& text)
{
    if (text.trim().isEmpty() || chat.isBusy())
        return;
    input.clear();
    chat.send (text);
    refresh();
    input.grabKeyboardFocus();
}

void AiChatPage::timerCallback()
{
    refresh();
}

void AiChatPage::refresh()
{
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
        transcript->setEntries (entries);
        if (atBottom || busy)
            viewport.setViewPosition (0, std::max (0, transcript->getHeight() - viewport.getHeight()));

        stopButton.setVisible (busy && ! setupOpen);
        sendButton.setVisible (! busy && ! setupOpen);
        newChatButton.setEnabled (! busy);
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
        g.drawText ("Tell the AI what sound you want, or try one of these:", viewport.getBounds().withHeight (60).withY (viewport.getY() + viewport.getHeight() / 2 - 60),
                    juce::Justification::centred);
    }
}

void AiChatPage::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (28);
    title.setBounds (header.removeFromLeft (130));
    keyButton.setBounds (header.removeFromRight (56).reduced (0, 2));
    header.removeFromRight (4);
    undoButton.setBounds (header.removeFromRight (62).reduced (0, 2));
    header.removeFromRight (4);
    newChatButton.setBounds (header.removeFromRight (86).reduced (0, 2));
    header.removeFromRight (8);
    modelBox.setBounds (header.removeFromLeft (170).reduced (0, 2));
    header.removeFromLeft (4);
    costBox.setBounds (header.removeFromLeft (80).reduced (0, 2));
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

    const auto chipWidth = std::min (330, (area.getWidth() - 40) / 3);
    auto chips = juce::Rectangle<int> (chipWidth * 3 + 16, 30).withCentre ({ area.getCentreX(), area.getCentreY() + 10 });
    for (auto& suggestion : suggestions)
    {
        suggestion.setBounds (chips.removeFromLeft (chipWidth));
        chips.removeFromLeft (8);
    }

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
