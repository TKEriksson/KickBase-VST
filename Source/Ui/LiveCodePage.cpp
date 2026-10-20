#include "LiveCodePage.h"

namespace
{
    constexpr int cycles = 4;
    constexpr int debounceMs = 400;
}

LiveCodePage::LiveCodePage (PluginBase& processor) : tools (processor)
{
    editor.setMultiLine (true, false);
    editor.setReturnKeyStartsNewLine (true);
    editor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain)));
    editor.setColour (juce::TextEditor::backgroundColourId, Theme::panel);
    editor.setColour (juce::TextEditor::textColourId, Theme::text);
    editor.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    editor.setText ("note(\"c2 [e2 g2]*2\").s(\"i1\")\ns(\"bd*4\")", false);
    editor.onTextChange = [this]
    {
        if (playing)
            startTimer (debounceMs); // ny ändring startar om väntan
    };
    addAndMakeVisible (editor);

    playButton.onClick = [this] { setPlaying (true); };
    stopButton.onClick = [this] { setPlaying (false); };
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

    auto bottom = area.removeFromBottom (20);
    licenseLabel.setBounds (bottom.removeFromLeft (230));
    strudelLink.setBounds (bottom.removeFromLeft (80));
    errorLabel.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (4);
    editor.setBounds (area);
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
    runner.run (editor.getText(), cycles, [this] (const juce::var& list, const juce::String& error)
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
    args.getDynamicObject()->setProperty ("code", editor.getText());
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
    errorLabel.setText (text, juce::dontSendNotification);
    if (text.isNotEmpty() && onError)
        onError (text);
}
