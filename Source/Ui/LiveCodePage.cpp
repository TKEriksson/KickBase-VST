#include "LiveCodePage.h"

namespace
{
    constexpr int cycles = 4;
    constexpr int debounceMs = 400;
}

LiveCodePage::LiveCodePage (PluginBase& processor) : tools (processor)
{
    editor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    editor.setTabSize (2, true);
    editor.setLineNumbersShown (true);
    editor.setColour (juce::CodeEditorComponent::backgroundColourId, Theme::panel);
    editor.setColour (juce::CodeEditorComponent::defaultTextColourId, Theme::text);
    editor.setColour (juce::CodeEditorComponent::lineNumberBackgroundId, Theme::panel.darker (0.3f));
    editor.setColour (juce::CodeEditorComponent::lineNumberTextId, Theme::textDim);
    editor.setColour (juce::CodeEditorComponent::highlightColourId, Theme::mix.withAlpha (0.25f));
    document.replaceAllContent ("$: note(\"c2 [e2 g2]*2\").s(\"i1\")\n$: s(\"bd*4\")");
    document.clearUndoHistory();
    document.setSavePoint();
    addAndMakeVisible (editor);
    document.addListener (this);

    errorLog.setMultiLine (true, true);
    errorLog.setReadOnly (true);
    errorLog.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    errorLog.setColour (juce::TextEditor::backgroundColourId, Theme::panel);
    errorLog.setColour (juce::TextEditor::textColourId, juce::Colour (0xffff9a9a));
    errorLog.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    errorLog.setText ("Inga fel än.", false);
    addChildComponent (errorLog);

    codeTab.setClickingTogglesState (false);
    codeTab.onClick = [this] { showTab (false); };
    errorTab.onClick = [this] { showTab (true); };
    addAndMakeVisible (codeTab);
    addAndMakeVisible (errorTab);
    showTab (false);

    playButton.setClickingTogglesState (true);

    playButton.onClick = [this] { setPlaying (playButton.getToggleState()); }; // LIVE av/på
    stopButton.onClick = [this] { playButton.setToggleState (false, juce::dontSendNotification); setPlaying (false); };
    createButton.onClick = [this] { create(); };
    for (auto* b : { &playButton, &stopButton, &createButton })
        addAndMakeVisible (b);

    errorLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff9a9a));
    errorLabel.setFont (Theme::font (12.0f));
    addAndMakeVisible (errorLabel);

    licenseLabel.setText ("Strudel (AGPL) - see THIRD-PARTY.md -", juce::dontSendNotification);
    licenseLabel.setColour (juce::Label::textColourId, Theme::textDim);
    licenseLabel.setFont (Theme::font (11.0f));
    addAndMakeVisible (licenseLabel);
    strudelLink.setFont (Theme::font (11.0f), false);
    addAndMakeVisible (strudelLink);
}

LiveCodePage::~LiveCodePage()
{
    document.removeListener (this);
    stopTimer();
    runner.cancel();
}

void LiveCodePage::paint (juce::Graphics& g)
{
    g.setColour (playing ? Theme::mix : Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (playing ? "LIVE" : "STOPPED", getLocalBounds().removeFromTop (22).removeFromRight (90).reduced (6, 0), juce::Justification::centredRight);
}

void LiveCodePage::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto top = area.removeFromTop (28);
    playButton.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (6);
    stopButton.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (6);
    createButton.setBounds (top.removeFromLeft (90));
    area.removeFromTop (8);
    auto tabs = area.removeFromTop (24);
    codeTab.setBounds (tabs.removeFromLeft (70));
    tabs.removeFromLeft (4);
    errorTab.setBounds (tabs.removeFromLeft (90));
    area.removeFromTop (4);

    auto bottom = area.removeFromBottom (20);
    licenseLabel.setBounds (bottom.removeFromLeft (230));
    strudelLink.setBounds (bottom.removeFromLeft (80));
    errorLabel.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (4);
    editor.setBounds (area);
    errorLog.setBounds (area);
}

void LiveCodePage::codeDocumentTextInserted (const juce::String&, int) { codeChanged(); }
void LiveCodePage::codeDocumentTextDeleted (int, int) { codeChanged(); }

void LiveCodePage::codeChanged()
{
    if (playing)
        startTimer (debounceMs); // ny ändring startar om väntan
}

void LiveCodePage::showTab (bool errors)
{
    editor.setVisible (! errors);
    errorLog.setVisible (errors);
    codeTab.setToggleState (! errors, juce::dontSendNotification);
    errorTab.setToggleState (errors, juce::dontSendNotification);
    codeTab.setColour (juce::TextButton::buttonColourId, errors ? Theme::panel : Theme::mix.darker (0.4f));
    errorTab.setColour (juce::TextButton::buttonColourId, errors ? Theme::mix.darker (0.4f) : Theme::panel);
}

void LiveCodePage::setPlaying (bool shouldPlay)
{
    playing = shouldPlay;
    if (playing)
    {
        stopTimer();
        runNow();
    }
    else
    {
        stopTimer();
        runner.cancel();
    }
    repaint();
}

void LiveCodePage::timerCallback()
{
    stopTimer();
    runNow();
}

void LiveCodePage::runNow()
{
    runner.run (document.getAllContent(), cycles, [this] (const juce::var& list, const juce::String& error)
    {
        if (error.isNotEmpty())
            return showError (error);

        showError ({});
        // set_pattern tar själv en ångra-punkt före varje ändring.
        if (auto* items = list.getArray())
            for (const auto& item : *items)
            {
                const auto result = tools.call ("set_pattern", item);
                if (! (bool) result["ok"])
                    return showError (result["error"].toString());
            }
        if (onPatternChanged)
            onPatternChanged();
    });
}

void LiveCodePage::create()
{
    auto args = juce::var (new juce::DynamicObject());
    args.getDynamicObject()->setProperty ("code", document.getAllContent());
    args.getDynamicObject()->setProperty ("cycles", cycles);
    const auto result = tools.call ("live_code_create", args);
    if (! (bool) result["ok"])
        return showError (result["error"].toString());
    showError ({});
    if (onPatternChanged)
        onPatternChanged();
}

void LiveCodePage::showError (const juce::String& text)
{
    errorLabel.setText (text.upToFirstOccurrenceOf ("\n", false, false), juce::dontSendNotification);
    if (text.isNotEmpty())
    {
        ++errorCount;
        errorLog.setText (juce::Time::getCurrentTime().toString (false, true) + "  " + text + "\n\n" + (errorCount > 1 ? errorLog.getText() : juce::String()), false);
        errorLog.moveCaretToTop (false);
        errorTab.setButtonText ("FEL (" + juce::String (errorCount) + ")");
        if (onError)
            onError (text);
    }
}
