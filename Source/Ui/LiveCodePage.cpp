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
    if (const auto saved = processor.getLiveCodeText(); saved.isNotEmpty()) // kod sparad i projektet (steg D)
        document.replaceAllContent (saved);
    seenLoads = processor.liveCodeLoads.load();
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
    processor.setLiveCodeText (document.getAllContent()); // sparas med projektet; LIVE sparas aldrig
    if (ownEdit)
        return;
    sliderValues.clear(); // ordningsnumren kan ha flyttat sig
    if (playing)
        startTimer (debounceMs); // ny ändring startar om väntan
}

namespace
{
    // Ordningsnumret = hur många slider( som står före i texten (samma ordning som glue.js räknar). Bara tal som argument får reglage.
    bool readNumber (const juce::String& t, int& pos, int& start, int& end, double& value)
    {
        while (pos < t.length() && juce::CharacterFunctions::isWhitespace (t[pos])) ++pos;
        start = pos;
        while (pos < t.length() && (juce::CharacterFunctions::isDigit (t[pos]) || t[pos] == '.' || t[pos] == '-' || t[pos] == '+' || t[pos] == 'e' || t[pos] == 'E')) ++pos;
        end = pos;
        if (end == start) return false;
        value = t.substring (start, end).getDoubleValue();
        while (pos < t.length() && juce::CharacterFunctions::isWhitespace (t[pos])) ++pos;
        return true;
    }

    juce::String sliderNumberText (double v, double lo, double hi)
    {
        const auto decimals = hi - lo <= 10.0 ? 3 : hi - lo <= 1000.0 ? 1 : 0;
        auto s = juce::String (v, decimals);
        if (s.contains ("."))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s == "-0" ? juce::String ("0") : s;
    }
}

void LiveCodePage::layoutSliders()
{
    if (! isShowing())
        return;
    const auto text = document.getAllContent();
    if (text != sliderText)
    {
        for (auto& s : sliderViews)
            if (s != nullptr && s->isMouseButtonDown())
                return; // aldrig riva ett reglage under drag
        sliderText = text;
        sliderSpecs.clear();
        sliderViews.clear();
        for (int from = 0;;)
        {
            const auto at = text.indexOf (from, "slider(");
            if (at < 0) break;
            from = at + 7;
            SliderSpec spec;
            spec.ordinal = (int) sliderSpecs.size();
            if (at > 0 && (juce::CharacterFunctions::isLetterOrDigit (text[at - 1]) || text[at - 1] == '_' || text[at - 1] == '$'))
            {
                // t.ex. mySlider( är inget slider-anrop
                continue;
            }
            int pos = from, a = 0, b = 0;
            double lo = 0, hi = 0;
            const auto ok = readNumber (text, pos, spec.start, spec.end, spec.value) && pos < text.length() && text[pos++] == ','
                         && readNumber (text, pos, a, b, lo) && pos < text.length() && text[pos++] == ','
                         && readNumber (text, pos, a, b, hi) && pos < text.length() && text[pos] == ')';
            spec.lo = lo; spec.hi = hi;
            spec.valid = ok && hi > lo;
            sliderSpecs.push_back (spec);
        }
        for (auto& spec : sliderSpecs)
        {
            if (! spec.valid) { sliderViews.push_back (nullptr); continue; }
            auto s = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            s->setRange (spec.lo, spec.hi);
            s->setValue (juce::jlimit (spec.lo, spec.hi, spec.value), juce::dontSendNotification);
            s->setColour (juce::Slider::thumbColourId, Theme::mix);
            s->setColour (juce::Slider::trackColourId, Theme::mix.darker (0.4f));
            s->setColour (juce::Slider::backgroundColourId, Theme::panelBorder);
            const auto index = (int) sliderViews.size();
            s->onValueChange = [this, index, raw = s.get()]
            {
                sliderValues[sliderSpecs[(size_t) index].ordinal] = raw->getValue();
                if (playing)
                    startTimer (150);
            };
            s->onDragEnd = [this, index] { sliderReleased (index); };
            editor.addAndMakeVisible (*s);
            sliderViews.push_back (std::move (s));
        }
    }
    for (size_t i = 0; i < sliderViews.size(); ++i)
    {
        auto* s = sliderViews[i].get();
        if (s == nullptr) continue;
        const auto line = juce::CodeDocument::Position (document, sliderSpecs[i].start).getLineNumber();
        const auto end = juce::CodeDocument::Position (document, line, document.getLine (line).trimEnd().length());
        const auto r = editor.getCharacterBounds (end);
        const auto width = 110;
        const auto x = juce::jmin (r.getX() + 10, editor.getWidth() - width - 20);
        const auto visible = r.getY() >= 0 && r.getBottom() <= editor.getHeight();
        s->setVisible (visible);
        if (visible)
            s->setBounds (x, r.getY() + 1, width, juce::jmax (10, r.getHeight() - 2));
    }
}

// Släppt reglage: det dragna värdet blir talet i koden (ett ångra-steg). Texten ändras av oss själva, så ingen ny debounce-loop.
void LiveCodePage::sliderReleased (int index)
{
    if (document.getAllContent() != sliderText || index >= (int) sliderSpecs.size() || sliderViews[(size_t) index] == nullptr)
        return;
    const auto& spec = sliderSpecs[(size_t) index];
    const auto number = sliderNumberText (sliderViews[(size_t) index]->getValue(), spec.lo, spec.hi);
    ownEdit = true;
    document.newTransaction();
    document.replaceSection (spec.start, spec.end, number);
    ownEdit = false;
    sliderValues.clear(); // värdet står nu i texten
    if (playing)
        startTimer (debounceMs); // avrundningen kan skilja från det dragna värdet
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
        processor.clearCodeMods (false); // LFO/ENV från koden tonar ut och kopplingarna städas av loadWatch
    }
    repaint();
}

void LiveCodePage::timerCallback()
{
    stopTimer();
    runNow();
}

// Ett projekt eller preset kan ha laddats medan sidan fanns: ta in den sparade koden (aldrig under LIVE, och aldrig tom kod).
void LiveCodePage::syncSavedCode()
{
    const auto saved = processor.getLiveCodeText();
    if (! playing && saved.isNotEmpty() && saved != document.getAllContent())
        document.replaceAllContent (saved);
}

void LiveCodePage::checkLoaded()
{
    const auto loads = processor.liveCodeLoads.load();
    if (loads == seenLoads)
        return;
    seenLoads = loads;
    if (playing) // LIVE skulle annars skriva över det nya projektets mönster med gammal kod
    {
        playButton.setToggleState (false, juce::dontSendNotification);
        setPlaying (false); // runner.cancel() kastar en pågående körning
        setCodeOnly (false);
    }
    syncSavedCode();
    syncLocks();
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
    runner.run (document.getAllContent(), cycles, [this] (const juce::var& list, const juce::String& error, const juce::var& mods)
    {
        if (error.isNotEmpty())
            return showError (error);

        showError ({});
        apply (list, false, mods);
    }, sliderValues);
}

// Skriver raderna via live_code_apply. Handritade toner som skulle skrivas över ger en fråga (en i taget) i stället för att tyst försvinna.
bool LiveCodePage::apply (const juce::var& items, bool overwrite, const juce::var& mods)
{
    if (warningOpen)
        return false;
    auto args = juce::var (new juce::DynamicObject());
    args.getDynamicObject()->setProperty ("items", items);
    args.getDynamicObject()->setProperty ("overwrite", overwrite);
    args.getDynamicObject()->setProperty ("mods", mods);
    const auto result = tools.call ("live_code_apply", args);
    if (! (bool) result["ok"])
    {
        if (auto* conflicts = result["conflicts"].getArray(); conflicts != nullptr && ! conflicts->isEmpty())
            warnOverwrite (result["conflicts"], [this, items, mods] (bool ow) { apply (items, ow, mods); });
        else
            showError (result["error"].toString());
        return false;
    }
    if (auto* problems = result["data"]["problems"].getArray(); problems != nullptr && ! problems->isEmpty())
    {
        juce::StringArray lines;
        for (auto& p : *problems)
            lines.add (p.toString());
        showError (lines.joinIntoString ("\n"));
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
