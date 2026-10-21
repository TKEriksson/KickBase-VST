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
    if (const auto saved = processor.getLiveCodeText(); saved.isNotEmpty()) // kod sparad i projektet (steg D)
        document.replaceAllContent (saved);
    seenLoads = processor.liveCodeLoads.load();
    document.clearUndoHistory();
    document.setSavePoint();
    addAndMakeVisible (editor);
    markOverlay.onPaint = [this] (juce::Graphics& g) { paintMarks (g); };
    editor.addAndMakeVisible (markOverlay);
    document.addListener (this);

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

void LiveCodePage::codeDocumentTextInserted (const juce::String&, int) { marks.clear(); codeChanged(); }
void LiveCodePage::codeDocumentTextDeleted (int, int) { marks.clear(); codeChanged(); }

// 30 Hz: vilka noter låter nu? Spelpositionen är i tick i radens loop (som pianorullens markör); marks räknades över `cycles` takter.
void LiveCodePage::updateMarks()
{
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
