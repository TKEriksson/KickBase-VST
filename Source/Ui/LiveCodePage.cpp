#include "LiveCodePage.h"

namespace
{
    constexpr int cycles = 4;
    constexpr int debounceMs = 400;
}

LiveCodePage::LiveCodePage (PluginBase& processor) : processor (processor), tools (processor)
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
    stopButton.onClick = [this] { playButton.setToggleState (false, juce::dontSendNotification); setPlaying (false); setCodeOnly (false); };
    createButton.onClick = [this] { create(); };
    codeOnlyButton.setClickingTogglesState (true);
    codeOnlyButton.setColour (juce::TextButton::buttonOnColourId, Theme::mix.darker (0.2f));
    codeOnlyButton.setTooltip ("Hear only the rows Live Code has written (solo). Your own SOLO click wins.");
    codeOnlyButton.onClick = [this] { setCodeOnly (codeOnlyButton.getToggleState()); };
    for (auto* b : { &playButton, &stopButton, &createButton, &codeOnlyButton })
        addAndMakeVisible (b);

    lockLabel.setText ("LOCK:", juce::dontSendNotification);
    lockLabel.setColour (juce::Label::textColourId, Theme::textDim);
    lockLabel.setFont (Theme::font (11.0f, true));
    addAndMakeVisible (lockLabel);
    for (size_t i = 0; i < lockButtons.size(); ++i)
    {
        auto& b = lockButtons[i];
        const auto layer = i == 10 ? (int) Pattern::kickLayer : (int) i;
        b.setButtonText (i == 10 ? "K" : juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, Theme::mix.darker (0.2f));
        b.setTooltip ("Lock this row: Live Code never writes it");
        b.onClick = [this, layer, &b]
        {
            auto args = juce::var (new juce::DynamicObject());
            args.getDynamicObject()->setProperty ("layer", layer == Pattern::kickLayer ? juce::var ("kick") : juce::var (layer));
            args.getDynamicObject()->setProperty ("locked", b.getToggleState());
            tools.call ("live_code_lock", args);
        };
        addAndMakeVisible (b);
    }
    syncLocks();

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
    setCodeOnly (false);
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
    top.removeFromLeft (6);
    codeOnlyButton.setBounds (top.removeFromLeft (100));
    area.removeFromTop (6);
    auto locks = area.removeFromTop (22);
    lockLabel.setBounds (locks.removeFromLeft (50));
    for (auto& b : lockButtons)
    {
        b.setBounds (locks.removeFromLeft (26));
        locks.removeFromLeft (2);
    }
    area.removeFromTop (6);
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

void LiveCodePage::syncLocks()
{
    refreshCodeOnly();
    const auto locks = processor.liveCodeLocks.load();
    for (size_t i = 0; i < lockButtons.size(); ++i)
        lockButtons[i].setToggleState ((locks & ToolLayer::liveCodeBit (i == 10 ? (int) Pattern::kickLayer : (int) i)) != 0, juce::dontSendNotification);
}

void LiveCodePage::setCodeOnly (bool on)
{
    if (on && ! codeOnly)
    {
        const auto owned = processor.liveCodeOwned.load();
        if (owned == 0) // soloMask 0 betyder "hör allt": utan egna rader finns inget att solo
        {
            codeOnlyButton.setToggleState (false, juce::dontSendNotification);
            return;
        }
        savedSolo = processor.soloMask.load();
        ownSolo = owned;
        processor.soloMask = ownSolo;
        codeOnly = true;
    }
    else if (! on && codeOnly)
    {
        if (processor.soloMask.load() == ownSolo) // rör inte användarens egna val
            processor.soloMask = savedSolo;
        codeOnly = false;
    }
    codeOnlyButton.setToggleState (codeOnly, juce::dontSendNotification);
}

// Kantfall: soloar användaren exakt de rader Live Code äger (soloMask == ownSolo) går det inte att skilja från CODE ONLY själv.
// Det är acceptabelt: ljudet blir detsamma och knappen förblir tänd.
void LiveCodePage::refreshCodeOnly()
{
    if (codeOnly)
    {
        if (processor.soloMask.load() != ownSolo) // användaren soloade själv: CODE ONLY ger vika
        {
            codeOnly = false;
            codeOnlyButton.setToggleState (false, juce::dontSendNotification);
        }
        else
        {
            ownSolo = processor.liveCodeOwned.load();
            processor.soloMask = ownSolo;
        }
    }
    codeOnlyButton.setEnabled (codeOnly || processor.liveCodeOwned.load() != 0);
}

void LiveCodePage::runNow()
{
    runner.run (document.getAllContent(), cycles, [this] (const juce::var& list, const juce::String& error)
    {
        if (error.isNotEmpty())
            return showError (error);

        showError ({});
        apply (list, false);
    });
}

// Skriver raderna via live_code_apply. Handritade toner som skulle skrivas över ger en fråga (en i taget) i stället för att tyst försvinna.
bool LiveCodePage::apply (const juce::var& items, bool overwrite)
{
    if (warningOpen)
        return false;
    auto args = juce::var (new juce::DynamicObject());
    args.getDynamicObject()->setProperty ("items", items);
    args.getDynamicObject()->setProperty ("overwrite", overwrite);
    const auto result = tools.call ("live_code_apply", args);
    if (! (bool) result["ok"])
    {
        if (auto* conflicts = result["conflicts"].getArray(); conflicts != nullptr && ! conflicts->isEmpty())
            warnOverwrite (result["conflicts"], [this, items] (bool ow) { apply (items, ow); });
        else
            showError (result["error"].toString());
        return false;
    }
    syncLocks();
    refreshCodeOnly();
    if (onPatternChanged)
        onPatternChanged();
    return true;
}

void LiveCodePage::warnOverwrite (const juce::var& conflicts, std::function<void (bool overwrite)> retry)
{
    juce::StringArray names;
    if (auto* a = conflicts.getArray())
        for (auto& c : *a)
            names.add (c.toString().equalsIgnoreCase ("kick") ? juce::String ("kick") : "instrument " + juce::String ((int) c + 1));

    warningOpen = true;
    const auto options = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::WarningIcon)
        .withTitle ("Live Code")
        .withMessage ("The code wants to write " + names.joinIntoString (", ") + ", which already has notes you drew.\n\n"
                      "Overwrite replaces them (undo works). Lock keeps them safe and Live Code skips those rows.")
        .withButton ("Overwrite")
        .withButton ("Lock these")
        .withButton ("Cancel");
    juce::Component::SafePointer<LiveCodePage> safe (this);
    juce::AlertWindow::showAsync (options, [safe, conflicts, retry] (int button)
    {
        if (safe == nullptr)
            return;
        safe->warningOpen = false;
        if (button == 1) // första knappen = 1, andra = 2, sista = 0
            retry (true);
        else if (button == 2)
        {
            if (auto* a = conflicts.getArray())
                for (auto& c : *a)
                {
                    auto args = juce::var (new juce::DynamicObject());
                    args.getDynamicObject()->setProperty ("layer", c);
                    safe->tools.call ("live_code_lock", args);
                }
            safe->syncLocks();
            retry (false);
        }
        else if (safe->playing) // Avbryt: sluta lyssna så att frågan inte kommer tillbaka vid nästa ändring
        {
            safe->playButton.setToggleState (false, juce::dontSendNotification);
            safe->setPlaying (false);
        }
    });
}

void LiveCodePage::create()
{
    runner.cancel();
    auto args = juce::var (new juce::DynamicObject());
    args.getDynamicObject()->setProperty ("code", document.getAllContent());
    args.getDynamicObject()->setProperty ("cycles", cycles);
    const auto result = tools.call ("live_code_create", args);
    if (! (bool) result["ok"])
    {
        if (auto* conflicts = result["conflicts"].getArray(); conflicts != nullptr && ! conflicts->isEmpty() && ! warningOpen)
        {
            warnOverwrite (result["conflicts"], [this] (bool ow)
            {
                auto a = juce::var (new juce::DynamicObject());
                a.getDynamicObject()->setProperty ("code", document.getAllContent());
                a.getDynamicObject()->setProperty ("cycles", cycles);
                a.getDynamicObject()->setProperty ("overwrite", true);
                const auto r = tools.call ("live_code_create", a);
                if (! (bool) r["ok"])
                    return showError (r["error"].toString());
                finishCreate (r);
            });
            return;
        }
        return showError (result["error"].toString());
    }
    finishCreate (result);
}

// Efter lyckad CREATE: uppdatera sidan och berätta vad som skrevs, vilka rader som hoppades över (låsta) och vilka som inte kunde köras.
void LiveCodePage::finishCreate (const juce::var& result)
{
    syncLocks();
    refreshCodeOnly();
    showError ({});
    if (onPatternChanged)
        onPatternChanged();
    if (! confirmCreate)
        return;

    auto rowNames = [] (const juce::var& list)
    {
        juce::StringArray names;
        if (auto* a = list.getArray())
            for (auto& c : *a)
                names.add (c.toString().equalsIgnoreCase ("kick") || (c.isInt() && (int) c == (int) Pattern::kickLayer) ? juce::String ("kick") : "instrument " + juce::String ((int) c + 1));
        return names;
    };
    const auto data = result["data"];
    const auto written = rowNames (data["layers"]);
    const auto skipped = rowNames (data["skipped_locked"]);
    auto message = juce::String ("The code ran.\n\n");
    message << (written.isEmpty() ? juce::String ("No rows were written.") : "Written: " + written.joinIntoString (", ") + ".");
    if (! skipped.isEmpty())
        message << "\n\nCould not run (locked, skipped): " + skipped.joinIntoString (", ") + ".";
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (skipped.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Live Code")
                                      .withMessage (message)
                                      .withButton ("OK"),
                                  nullptr);
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
