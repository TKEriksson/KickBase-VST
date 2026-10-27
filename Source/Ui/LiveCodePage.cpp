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
    // Mörkt tema i stil med Atom One Dark; namnen är CPlusPlusCodeTokeniser:s typer.
    {
        auto scheme = editor.getColourScheme();
        const std::pair<const char*, juce::uint32> colours[] = {
            { "Error", 0xffff6b6b }, { "Comment", 0xff7f848e }, { "Keyword", 0xffc678dd }, { "Operator", 0xff56b6c2 },
            { "Identifier", 0xffe6e6e6 }, { "Integer", 0xffd19a66 }, { "Float", 0xffd19a66 }, { "String", 0xff98c379 },
            { "Bracket", 0xffe5c07b }, { "Punctuation", 0xff9aa5b1 }, { "Preprocessor Text", 0xff61afef } };
        for (const auto& [name, argb] : colours)
            scheme.set (name, juce::Colour (argb));
        editor.setColourScheme (scheme);
    }
    if (const auto saved = processor.getLiveCodeText(); saved.isNotEmpty()) // kod sparad i projektet (steg D)
        document.replaceAllContent (saved);
    seenLoads = processor.liveCodeLoads.load();
    document.clearUndoHistory();
    document.setSavePoint();
    addAndMakeVisible (editor);
    markOverlay.onPaint = [this] (juce::Graphics& g) { paintMarks (g); };
    editor.addAndMakeVisible (markOverlay);
    document.addListener (this);
    completionKeys.handler = [this] (const juce::KeyPress& k, juce::Component* c) { return completionKey (k, c); };
    editor.addKeyListener (&completionKeys);

    errorLog.setMultiLine (true, true);
    errorLog.setReadOnly (true);
    errorLog.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    errorLog.setColour (juce::TextEditor::backgroundColourId, Theme::panel);
    errorLog.setColour (juce::TextEditor::textColourId, Theme::text);
    errorLog.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    errorLog.setText ("Konsolen är tom: fel och console.log från koden visas här.", false);
    addChildComponent (errorLog);

    codeTab.setClickingTogglesState (false);
    codeTab.onClick = [this] { showTab (false); };
    errorTab.onClick = [this] { showTab (true); };
    addAndMakeVisible (codeTab);
    addAndMakeVisible (errorTab);
    showTab (false);

    playButton.setClickingTogglesState (true);
    playButton.setToggleState (false, juce::dontSendNotification); // AUTO är alltid av när synthen startar; användaren slår på det
    playButton.setTooltip ("AUTO: valid code runs by itself after each edit. Nothing runs when a project loads.");

    playButton.onClick = [this] { setPlaying (playButton.getToggleState()); }; // AUTO av/på
    stopButton.onClick = [this] { playButton.setToggleState (false, juce::dontSendNotification); setPlaying (false); setCodeOnly (false); };
    createButton.onClick = [this] { create(); };
    codeOnlyButton.setClickingTogglesState (true);
    codeOnlyButton.setColour (juce::TextButton::buttonOnColourId, Theme::mix.darker (0.2f));
    codeOnlyButton.setTooltip ("Hear only the rows Live Code has written (solo). Your own SOLO click wins.");
    codeOnlyButton.onClick = [this] { setCodeOnly (codeOnlyButton.getToggleState()); };
    for (auto* b : { &playButton, &stopButton, &createButton, &codeOnlyButton, &floatButton, &formatButton })
        addAndMakeVisible (b);
    formatButton.setTooltip ("FORMAT: one call per line, s(\"...\") first. Blocks with // comments are left as they are. Undo (Ctrl+Z) works.");
    formatButton.onClick = [this]
    {
        const auto before = document.getAllContent();
        const auto after = LiveCodeFormat::format (before);
        if (after == before)
            return;
        document.newTransaction();
        document.replaceAllContent (after);
        editor.moveCaretToTop (false);
    };
    floatButton.setTooltip ("FLOAT: opens the code in its own window (same text) so you can see it while you work in other tabs. Click again to close it.");
    floatButton.onClick = [this] { toggleFloat(); };

    lockLabel.setText ("LOCK ROW:", juce::dontSendNotification);
    lockLabel.setTooltip ("LOCK ROW - Click a number to lock that instrument row (K = the kick). A locked row is lit and Live Code never writes it, so your own notes stay. Click again to unlock.");
    lockSummaryLabel.setColour (juce::Label::textColourId, Theme::textDim);
    lockSummaryLabel.setFont (Theme::font (11.0f));
    addAndMakeVisible (lockSummaryLabel);
    lockLabel.setColour (juce::Label::textColourId, Theme::textDim);
    lockLabel.setFont (Theme::font (11.0f, true));
    addAndMakeVisible (lockLabel);
    for (size_t i = 0; i < lockButtons.size(); ++i)
    {
        auto& b = lockButtons[i];
        const auto layer = i == 10 ? (int) Pattern::kickLayer : (int) i;
        b.setButtonText (i == 10 ? "K" : juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0a030));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.setTooltip (i == 10 ? "LOCK KICK - Lit = locked: Live Code never writes the kick row. Click again to unlock."
                              : "LOCK ROW " + juce::String (i + 1) + " - Lit = locked: Live Code never writes instrument " + juce::String (i + 1) + ", so your own notes stay. Click again to unlock.");
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
    floatWindow.reset();
    document.removeListener (this);
    stopTimer();
    runner.cancel();
}

void LiveCodePage::paint (juce::Graphics& g)
{
    g.setColour (playing ? Theme::mix : Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (playing ? "AUTO" : "STOPPED", getLocalBounds().removeFromTop (22).removeFromRight (90).reduced (6, 0), juce::Justification::centredRight);
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
    top.removeFromLeft (6);
    floatButton.setBounds (top.removeFromLeft (70));
    top.removeFromLeft (6);
    formatButton.setBounds (top.removeFromLeft (80));
    area.removeFromTop (6);
    auto locks = area.removeFromTop (22);
    lockLabel.setBounds (locks.removeFromLeft (76));
    for (auto& b : lockButtons)
    {
        b.setBounds (locks.removeFromLeft (34));
        locks.removeFromLeft (2);
    }
    locks.removeFromLeft (6);
    lockSummaryLabel.setBounds (locks);
    area.removeFromTop (6);
    auto tabs = area.removeFromTop (24);
    codeTab.setBounds (tabs.removeFromLeft (70));
    tabs.removeFromLeft (4);
    errorTab.setBounds (tabs.removeFromLeft (110));
    area.removeFromTop (4);

    auto bottom = area.removeFromBottom (20);
    licenseLabel.setBounds (bottom.removeFromLeft (230));
    strudelLink.setBounds (bottom.removeFromLeft (80));
    errorLabel.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (4);
    editor.setBounds (area);
    markOverlay.setBounds (editor.getLocalBounds());
    errorLog.setBounds (area);
}

void LiveCodePage::codeDocumentTextInserted (const juce::String& text, int)
{
    marks.clear();
    codeChanged();
    if (ownEdit) return;
    if ((text == "." || text == "\"") && completionEditor == nullptr)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LiveCodePage> (this)]
        {
            if (safe != nullptr && safe->editor.hasKeyboardFocus (true)) safe->openCompletion (safe->editor);
            else if (safe != nullptr)
                if (auto* floating = safe->floatEditor())
                    if (floating->hasKeyboardFocus (true)) safe->openCompletion (*floating);
        });
    else if (completionEditor != nullptr)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LiveCodePage> (this)] { if (safe != nullptr) safe->refreshCompletion(); });
}

void LiveCodePage::codeDocumentTextDeleted (int, int)
{
    marks.clear();
    codeChanged();
    if (completionEditor != nullptr)
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LiveCodePage> (this)] { if (safe != nullptr) safe->refreshCompletion(); });
}

void LiveCodePage::CompletionPopup::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel.darker (0.4f));
    g.setColour (Theme::panelBorder);
    g.drawRect (getLocalBounds());
    const auto rows = juce::jmin ((int) items.size(), maxRows);
    const auto first = juce::jlimit (0, juce::jmax (0, (int) items.size() - rows), selected - rows + 1);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    for (int r = 0; r < rows; ++r)
    {
        const auto i = first + r;
        const auto area = juce::Rectangle<int> (1, r * rowHeight, getWidth() - 2, rowHeight);
        if (i == selected) { g.setColour (Theme::mix.withAlpha (0.35f)); g.fillRect (area); }
        g.setColour (Theme::text);
        g.drawText (items[(size_t) i].label, area.reduced (6, 0), juce::Justification::centredLeft, true);
    }
    if (selected >= 0 && selected < (int) items.size())
    {
        g.setColour (Theme::textDim);
        g.setFont (12.0f);
        g.drawFittedText (items[(size_t) selected].detail, 6, rows * rowHeight + 2, getWidth() - 12, detailHeight - 4, juce::Justification::topLeft, 2);
    }
}

void LiveCodePage::openCompletion (juce::CodeEditorComponent& target)
{
    if (completionEditor != &target)
    {
        closeCompletion();
        completionEditor = &target;
        target.addAndMakeVisible (completion);
        completion.onAccept = [this] { acceptCompletion(); };
    }
    refreshCompletion();
}

void LiveCodePage::closeCompletion()
{
    completionEditor = nullptr;
    if (auto* parent = completion.getParentComponent())
        parent->removeChildComponent (&completion);
}

void LiveCodePage::refreshCompletion()
{
    if (completionEditor == nullptr) return;
    LiveCodeComplete::Source src;
    for (auto& kv : tools.liveCodeParameters())
        if (! kv.first.startsWith ("meter."))
            src.paramIds.push_back (kv.first);
    src.effectCatalog = tools.liveCodeEffects();
    const auto caret = completionEditor->getCaretPos().getPosition();
    auto result = LiveCodeComplete::complete (document.getAllContent(), caret, src);
    if (result.items.empty()) return closeCompletion();
    completion.items = std::move (result.items);
    completion.replaceFrom = result.replaceFrom;
    completion.selected = 0;
    const auto rows = juce::jmin ((int) completion.items.size(), CompletionPopup::maxRows);
    const auto w = juce::jmin (460, completionEditor->getWidth() - 10);
    const auto h = rows * CompletionPopup::rowHeight + CompletionPopup::detailHeight;
    const auto r = completionEditor->getCharacterBounds (completionEditor->getCaretPos());
    auto y = r.getBottom() + 2;
    if (y + h > completionEditor->getHeight()) y = juce::jmax (0, r.getY() - h - 2);
    completion.setBounds (juce::jlimit (0, juce::jmax (0, completionEditor->getWidth() - w), r.getX()), y, w, h);
    completion.toFront (false);
    completion.repaint();
}

void LiveCodePage::acceptCompletion()
{
    if (completionEditor == nullptr || completion.items.empty()) return;
    const auto item = completion.items[(size_t) juce::jlimit (0, (int) completion.items.size() - 1, completion.selected)];
    auto* target = completionEditor;
    const auto from = completion.replaceFrom, caret = target->getCaretPos().getPosition();
    closeCompletion();
    document.newTransaction();
    document.replaceSection (from, caret, item.insert);
    auto offset = item.insert.length();
    if (const auto open = item.insert.indexOfChar ('('); open >= 0 && item.insert.endsWithChar (')'))
        offset = open + 1 + (item.insert.substring (open + 1).startsWithChar ('"') ? 1 : 0);
    target->moveCaretTo (juce::CodeDocument::Position (document, from + offset), false);
    target->grabKeyboardFocus();
}

bool LiveCodePage::completionKey (const juce::KeyPress& key, juce::Component* origin)
{
    auto* target = dynamic_cast<juce::CodeEditorComponent*> (origin);
    if (key.getKeyCode() == juce::KeyPress::spaceKey && key.getModifiers().isCtrlDown() && target != nullptr)
    {
        openCompletion (*target);
        return true;
    }
    if (completionEditor == nullptr) return false;
    const auto code = key.getKeyCode();
    const auto count = (int) completion.items.size();
    if (code == juce::KeyPress::escapeKey) { closeCompletion(); return true; }
    if (code == juce::KeyPress::downKey || code == juce::KeyPress::upKey)
    {
        completion.selected = (completion.selected + (code == juce::KeyPress::downKey ? 1 : count - 1)) % juce::jmax (1, count);
        completion.repaint();
        return true;
    }
    if (code == juce::KeyPress::returnKey || code == juce::KeyPress::tabKey) { acceptCompletion(); return true; }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey || code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey)
        closeCompletion();
    return false;
}

// 30 Hz: vilka noter låter nu? Spelpositionen är i tick i radens loop (som pianorullens markör); marks räknades över `cycles` takter.
void LiveCodePage::updateMarks()
{
    syncFloat();
    std::vector<std::pair<int, int>> now;
    if (playing && isShowing())
    {
        const auto loop = cycles * LiveCode::ticksPerCycle;
        for (auto& m : marks)
        {
            const auto& ts = processor.transportStatus;
            const auto raw = m.layer < 0 ? ts.kickStep.load() : ts.instrumentSteps[(size_t) m.layer].load();
            if (raw < 0) continue;
            const auto t = raw % loop;
            if (t >= m.beginTick && t < std::max (m.endTick, m.beginTick + LiveCode::ticksPerCycle / 16)) // minst en 16-del så korta noter syns
                now.insert (now.end(), m.locs.begin(), m.locs.end());
        }
    }
    if (now != activeLocs)
    {
        activeLocs = std::move (now);
        markOverlay.repaint();
    }
}

void LiveCodePage::paintMarks (juce::Graphics& g)
{
    g.setColour (Theme::mix.withAlpha (0.45f));
    const auto length = document.getNumCharacters();
    for (auto& [start, end] : activeLocs)
    {
        if (start < 0 || end <= start || end > length) continue;
        const auto a = editor.getCharacterBounds (juce::CodeDocument::Position (document, start));
        const auto b = editor.getCharacterBounds (juce::CodeDocument::Position (document, end - 1));
        if (a.getY() != b.getY()) continue; // flera rader (mallsträng): hoppa över
        g.fillRoundedRectangle (a.getUnion (b).expanded (1, 0).toFloat(), 3.0f);
    }
}

void LiveCodePage::codeChanged()
{
    processor.setLiveCodeText (document.getAllContent()); // sparas med projektet; AUTO-läget sparas aldrig
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

struct LiveCodePage::FloatContent : juce::Component
{
    explicit FloatContent (LiveCodePage& owner) : page (owner), editor (owner.document, &owner.tokeniser)
    {
        addAndMakeVisible (editor);
        errorLog.setMultiLine (true, true);
        errorLog.setReadOnly (true);
        errorLog.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
        errorLog.setColour (juce::TextEditor::backgroundColourId, Theme::panel);
        errorLog.setColour (juce::TextEditor::textColourId, Theme::text);
        errorLog.setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
        addChildComponent (errorLog);

        std::vector<juce::TextButton*> sources { &page.playButton, &page.stopButton, &page.createButton, &page.codeOnlyButton };
        for (auto& b : page.lockButtons)
            sources.push_back (&b);
        for (size_t i = 0; i < sources.size(); ++i)
        {
            auto* source = sources[i];
            auto& own = buttons[i];
            own.setButtonText (source->getButtonText());
            own.setTooltip (source->getTooltip());
            own.setColour (juce::TextButton::buttonOnColourId, source->findColour (juce::TextButton::buttonOnColourId));
            own.onClick = [source] { source->triggerClick(); };
            addAndMakeVisible (own);
            pairs.push_back ({ &own, source });
        }
        codeTab.onClick = [this] { showErrors (false); };
        errorTab.onClick = [this] { showErrors (true); };
        for (auto* b : { &codeTab, &errorTab })
            addAndMakeVisible (*b);
        for (auto* l : { &lockLabel, &lockSummary, &errorLabel })
            addAndMakeVisible (*l);
        lockLabel.setText ("LOCK ROW:", juce::dontSendNotification);
        lockLabel.setTooltip (page.lockLabel.getTooltip());
        lockLabel.setColour (juce::Label::textColourId, Theme::textDim);
        lockLabel.setFont (Theme::font (11.0f, true));
        lockSummary.setColour (juce::Label::textColourId, Theme::textDim);
        lockSummary.setFont (Theme::font (11.0f));
        errorLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff9a9a));
        errorLabel.setFont (Theme::font (12.0f));
        showErrors (false);
    }

    void showErrors (bool errors)
    {
        editor.setVisible (! errors);
        errorLog.setVisible (errors);
        codeTab.setToggleState (! errors, juce::dontSendNotification);
        errorTab.setToggleState (errors, juce::dontSendNotification);
        codeTab.setColour (juce::TextButton::buttonColourId, errors ? Theme::panel : Theme::mix.darker (0.4f));
        errorTab.setColour (juce::TextButton::buttonColourId, errors ? Theme::mix.darker (0.4f) : Theme::panel);
    }

    // Följer sidans knappar och texter (anropas ofta; sätter bara om något ändrats).
    void sync()
    {
        for (auto& pair : pairs)
            if (pair.first->getToggleState() != pair.second->getToggleState())
                pair.first->setToggleState (pair.second->getToggleState(), juce::dontSendNotification);
        const auto summary = page.lockSummary();
        if (lockSummary.getText() != summary)
            lockSummary.setText (summary, juce::dontSendNotification);
        if (errorLabel.getText() != page.errorLabel.getText())
            errorLabel.setText (page.errorLabel.getText(), juce::dontSendNotification);
        if (errorLog.getText() != page.errorLog.getText())
        {
            errorLog.setText (page.errorLog.getText(), false);
            errorLog.moveCaretToTop (false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6);
        auto top = area.removeFromTop (26);
        for (size_t i = 0; i < 4; ++i)
        {
            buttons[i].setBounds (top.removeFromLeft (i == 3 ? 100 : 76));
            top.removeFromLeft (4);
        }
        area.removeFromTop (4);
        auto locks = area.removeFromTop (24);
        lockLabel.setBounds (locks.removeFromLeft (76));
        for (size_t i = 4; i < buttons.size(); ++i)
        {
            buttons[i].setBounds (locks.removeFromLeft (34));
            locks.removeFromLeft (2);
        }
        lockSummary.setBounds (locks);
        area.removeFromTop (4);
        auto tabs = area.removeFromTop (22);
        codeTab.setBounds (tabs.removeFromLeft (70));
        tabs.removeFromLeft (4);
        errorTab.setBounds (tabs.removeFromLeft (110));
        area.removeFromTop (4);
        errorLabel.setBounds (area.removeFromBottom (22));
        editor.setBounds (area);
        errorLog.setBounds (area);
    }

    LiveCodePage& page;
    juce::CodeEditorComponent editor;
    juce::TextEditor errorLog;
    std::array<juce::TextButton, 15> buttons;
    std::vector<std::pair<juce::TextButton*, juce::TextButton*>> pairs;
    juce::TextButton codeTab { "KOD" }, errorTab { "KONSOL" };
    juce::Label lockLabel, lockSummary, errorLabel;
};

juce::String LiveCodePage::lockSummary() const
{
    juce::StringArray locked;
    for (size_t i = 0; i < lockButtons.size(); ++i)
        if (lockButtons[i].getToggleState())
            locked.add (lockButtons[i].getButtonText());
    return locked.isEmpty() ? juce::String ("Nothing locked") : "Locked: " + locked.joinIntoString (", ");
}

juce::CodeEditorComponent* LiveCodePage::floatEditor()
{
    if (floatWindow != nullptr)
        if (auto* content = dynamic_cast<FloatContent*> (floatWindow->getContentComponent()))
            return &content->editor;
    return nullptr;
}

void LiveCodePage::syncFloat()
{
    if (floatWindow != nullptr)
        if (auto* content = dynamic_cast<FloatContent*> (floatWindow->getContentComponent()))
            content->sync();
}

void LiveCodePage::toggleFloat()
{
    if (floatWindow != nullptr)
    {
        closeCompletion();
        floatWindow.reset();
        floatButton.setToggleState (false, juce::dontSendNotification);
        return;
    }
    auto content = std::make_unique<FloatContent> (*this);
    content->editor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    content->editor.setTabSize (2, true);
    content->editor.setLineNumbersShown (true);
    content->editor.setColour (juce::CodeEditorComponent::backgroundColourId, Theme::panel);
    content->editor.setColour (juce::CodeEditorComponent::defaultTextColourId, Theme::text);
    content->editor.setColour (juce::CodeEditorComponent::lineNumberBackgroundId, Theme::panel.darker (0.3f));
    content->editor.setColour (juce::CodeEditorComponent::lineNumberTextId, Theme::textDim);
    content->editor.setColour (juce::CodeEditorComponent::highlightColourId, Theme::mix.withAlpha (0.25f));
    content->editor.addKeyListener (&completionKeys);
    content->setSize (620, 460);
    content->sync();

    floatWindow = std::make_unique<FloatWindow>();
    floatWindow->setUsingNativeTitleBar (true);
    floatWindow->setContentOwned (content.release(), true);
    floatWindow->setResizable (true, true);
    floatWindow->setResizeLimits (560, 260, 3000, 2000); // så att alla knappar får plats; höjd och bredd är fria
    floatWindow->setAlwaysOnTop (true);
    floatWindow->onClose = [safe = juce::Component::SafePointer<LiveCodePage> (this)]
    {
        if (safe != nullptr)
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->toggleFloat(); });
    };
    floatWindow->centreWithSize (620, 460);
    floatWindow->setVisible (true);
    floatButton.setToggleState (true, juce::dontSendNotification);
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
        lastParamWrites.clear(); // första körningen efter AUTO på skriver alltid
        stopTimer();
        if (document.getAllContent().trim().isNotEmpty()) // tom editor: inget att köra, rader och modulering lämnas orörda
            runNow();
    }
    else
    {
        stopTimer();
        runner.cancel();
        processor.liveCodeCodeLocks = 0; // F2: lås från koden släpps vid stopp (handlås ligger kvar)
        processor.liveCodeCodeNoteLocks = 0;
        processor.clearCodeMods (false); // LFO/ENV från koden tonar ut och kopplingarna städas av loadWatch
    }
    repaint();
}

void LiveCodePage::timerCallback()
{
    stopTimer();
    runNow();
}

// Ett projekt eller preset kan ha laddats medan sidan fanns: ta in den sparade koden (körs aldrig av sig själv, och aldrig tom kod).
void LiveCodePage::syncSavedCode()
{
    const auto saved = processor.getLiveCodeText();
    if (saved.isNotEmpty() && saved != document.getAllContent())
    {
        const juce::ScopedValueSetter<bool> guard (ownEdit, true); // inläst kod är ingen ändring: AUTO får inte köra den
        document.replaceAllContent (saved);
    }
}

void LiveCodePage::checkLoaded()
{
    const auto loads = processor.liveCodeLoads.load();
    if (loads == seenLoads)
        return;
    seenLoads = loads;
    if (playing) // AUTO ligger kvar på, men en väntande eller pågående körning med gammal kod kastas så att det nya projektets mönster är orört
    {
        stopTimer();
        runner.cancel();
        lastParamWrites.clear();
        processor.liveCodeCodeLocks = 0;
        processor.liveCodeCodeNoteLocks = 0;
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
    lockSummaryLabel.setText (lockSummary(), juce::dontSendNotification);
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
    auto params = tools.liveCodeParameters();
    // F4: mätare, läses här på meddelandetråden ur befintliga atomics (ingen ny tapp i ljudvägen)
    params["meter.outputPeak"] = processor.getMasterSection().outputPeakDb.load();
    params["meter.gainReduction"] = processor.getMasterSection().gainReductionDb.load();
    params["meter.solo"] = processor.soloMask.load();
    for (size_t i = 0; i < processor.duckReduction.size(); ++i)
        params["meter.duck" + juce::String ((int) i + 1)] = processor.duckReduction[i].load();
    const auto text = document.getAllContent();
    runner.run (text, cycles, [this, hash = text.hashCode64()] (const juce::var& list, const juce::String& error, const juce::var& mods, const juce::var& writes, const juce::StringArray& console)
    {
        marks = error.isEmpty() && document.getAllContent().hashCode64() == hash ? runner.getMarks() : std::vector<LiveCode::Mark>{};
        showConsole (console);
        if (error.isNotEmpty())
            return showError (error);

        showError ({});
        PluginBase::EditScope undo (processor, "Live Code run"); // en körning är ett ångrasteg; ändras inget läggs inget
        // F2: en .lock() som tagits bort ur koden släpper direkt; en ny låser först efter att körningen skrivit (första gången skrivs, sedan låses).
        juce::uint64 lockAll = 0, lockNotes = 0;
        if (auto* entries = writes.getArray())
            for (auto& w : *entries)
                if (w.hasProperty ("lock"))
                    if (const auto layer = ToolLayer::liveCodeLayerOf (w["lock"].toString()); layer >= 0)
                        (w["kind"].toString() == "notes" ? lockNotes : lockAll) |= ToolLayer::liveCodeBit (layer);
        processor.liveCodeCodeLocks &= lockAll;
        processor.liveCodeCodeNoteLocks &= lockNotes;
        applyParamWrites (writes);
        apply (list, false, mods, true); // AUTO/LIVE: rader som tagits bort ur koden töms
        processor.liveCodeCodeLocks = lockAll;
        processor.liveCodeCodeNoteLocks = lockNotes;
    }, sliderValues, std::move (params), tools.liveCodeEffects());
}

// F3: {fx, slot, type, knob, value}. Ett typbyte (knob tomt) nollställer rattarna som set_effect gör; en ratt behåller de andra (keep).
// Som .set på parametrar skrivs bara det som ändrats i koden sedan förra körningen.
void LiveCodePage::applyEffectWrite (const juce::var& w)
{
    const auto knob = w["knob"].toString();
    const auto key = w["fx"].toString() + "." + w["slot"].toString() + (knob.isEmpty() ? ".type" : ".knob." + knob);
    const auto text = w["type"].toString() + "|" + w["value"].toString();
    if (const auto it = lastParamWrites.find (key); it != lastParamWrites.end() && it->second == text)
        return;
    lastParamWrites[key] = text;
    auto args = juce::var (new juce::DynamicObject());
    auto* o = args.getDynamicObject();
    o->setProperty ("channel", w["fx"]);
    o->setProperty ("slot", w["slot"]);
    o->setProperty ("type", w["type"]);
    if (knob.isNotEmpty())
    {
        auto knobs = juce::var (new juce::DynamicObject());
        knobs.getDynamicObject()->setProperty (juce::Identifier (knob), w["value"]);
        o->setProperty ("knobs", knobs);
        o->setProperty ("keep", true);
    }
    const auto result = tools.call ("set_effect", args);
    if (! (bool) result["ok"])
        showError (result["error"].toString());
    else if (auto* problems = result["data"]["problems"].getArray(); problems != nullptr && ! problems->isEmpty())
        showError ((*problems)[0].toString());
}

// .set på parametrar: skriver bara det som ändrats i koden sedan förra körningen (så handvridning inte slås ut var gång) och
// hoppar över parametrar på låsta rader. Går via set_parameters på message thread.
void LiveCodePage::applyParamWrites (const juce::var& writes)
{
    auto* list = writes.getArray();
    if (list == nullptr)
        return;
    const auto locks = processor.liveCodeParamLocks();
    auto values = juce::var (new juce::DynamicObject());
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (now - paramWindowStart >= 1000.0)
    {
        paramWindowStart = now;
        paramWindowCount = 0;
    }
    auto capped = false;
    for (auto& w : *list)
    {
        if (w.hasProperty ("lock"))
            continue; // lås-poster hanteras i runNow
        if (w.hasProperty ("fx"))
        {
            applyEffectWrite (w); // F3: typbyte eller ratt på en effektplats, i kodens ordning
            continue;
        }
        const auto id = w["id"].toString();
        const auto text = w["value"].toString();
        if (const auto layer = ToolLayer::liveCodeLayerOf (id); layer >= 0 && (locks & ToolLayer::liveCodeBit (layer)) != 0)
            continue;
        if (const auto it = lastParamWrites.find (id); it != lastParamWrites.end() && it->second == text)
            continue;
        if (paramWindowCount >= maxParamWritesPerSecond)
        {
            capped = true; // lastParamWrites rörs inte, så värdet skrivs vid nästa körning
            continue;
        }
        ++paramWindowCount;
        lastParamWrites[id] = text;
        values.getDynamicObject()->setProperty (juce::Identifier (id), w["value"]);
    }
    if (capped)
        showError ("Too many .set() writes per second (max " + juce::String (maxParamWritesPerSecond) + "); the rest are written on the next run.");
    if (values.getDynamicObject()->getProperties().size() == 0)
        return;
    auto args = juce::var (new juce::DynamicObject());
    args.getDynamicObject()->setProperty ("values", values);
    const auto result = tools.call ("set_parameters", args);
    if (! (bool) result["ok"])
        showError (result["error"].toString());
    else if (auto* problems = result["data"]["problems"].getArray(); problems != nullptr && ! problems->isEmpty())
        showError ((*problems)[0].toString());
}

// Skriver raderna via live_code_apply. Handritade toner som skulle skrivas över ger en fråga (en i taget) i stället för att tyst försvinna.
bool LiveCodePage::apply (const juce::var& items, bool overwrite, const juce::var& mods, bool prune)
{
    if (warningOpen)
        return false;
    auto args = juce::var (new juce::DynamicObject());
    args.getDynamicObject()->setProperty ("items", items);
    args.getDynamicObject()->setProperty ("overwrite", overwrite);
    args.getDynamicObject()->setProperty ("mods", mods);
    args.getDynamicObject()->setProperty ("prune", prune);
    const auto result = tools.call ("live_code_apply", args);
    if (! (bool) result["ok"])
    {
        if (auto* conflicts = result["conflicts"].getArray(); conflicts != nullptr && ! conflicts->isEmpty())
            warnOverwrite (result["conflicts"], [this, items, mods, prune] (bool ow) { apply (items, ow, mods, prune); });
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
                a.getDynamicObject()->setProperty ("overwrite", ow);
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
    juce::StringArray problems;
    if (auto* list = data["problems"].getArray())
        for (const auto& problem : *list)
            problems.add (problem.toString());
    auto message = juce::String ("The code ran.\n\n");
    message << (written.isEmpty() ? juce::String ("No rows were written.") : "Written: " + written.joinIntoString (", ") + ".");
    if (! skipped.isEmpty())
        message << "\n\nCould not run (locked, skipped): " + skipped.joinIntoString (", ") + ".";
    if (! problems.isEmpty())
    {
        message << "\n\nSome actions were skipped:\n" << problems.joinIntoString ("\n");
        showError (problems.joinIntoString ("\n"));
    }
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (skipped.isEmpty() && problems.isEmpty() ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon)
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
        auto lines = juce::StringArray::fromLines (text);
        lines.set (0, "FEL  " + lines[0]);
        appendLog (lines);
        errorTab.setButtonText ("KONSOL (" + juce::String (errorCount) + ")");
        if (onError)
            onError (text);
    }
}

// G2: konsolen visar fel och console.log, senaste överst, högst maxLogLines rader. newLines läses uppifrån och ned.
void LiveCodePage::appendLog (const juce::StringArray& newLines)
{
    juce::StringArray all;
    const auto stamp = juce::Time::getCurrentTime().toString (false, true) + "  ";
    for (int i = 0; i < newLines.size(); ++i)
        all.add (i == 0 ? stamp + newLines[i] : "    " + newLines[i]);
    if (logLines > 0)
        all.addArray (juce::StringArray::fromLines (errorLog.getText()));
    if (all.size() > maxLogLines)
        all.removeRange (maxLogLines, all.size() - maxLogLines);
    logLines = all.size();
    errorLog.setText (all.joinIntoString ("\n"), false);
    errorLog.moveCaretToTop (false);
}

void LiveCodePage::showConsole (const juce::StringArray& console)
{
    if (console.isEmpty())
        return;
    auto lines = console;
    for (int i = 0; i < lines.size(); ++i)
        lines.set (i, lines[i].replaceFirstOccurrenceOf ("log: ", "").replaceFirstOccurrenceOf ("warn: ", "VARNING  ").replaceFirstOccurrenceOf ("error: ", "FEL  "));
    appendLog (lines);
}
