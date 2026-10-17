#include "Modules.h"

namespace
{
    constexpr int knobRowHeight = 74;
    constexpr int presetButtonWidth = 150;

    // Modulens presetknapp, uppe till höger i rubriken.
    juce::Rectangle<int> presetBounds (const juce::Component& module)
    {
        return { module.getWidth() - 10 - presetButtonWidth, 5, presetButtonWidth, 20 };
    }

    juce::RangedAudioParameter* param (PluginBase& processor, const juce::String& id)
    {
        auto* parameter = processor.apvts.getParameter (id);
        jassert (parameter != nullptr);
        return parameter;
    }

    void addKnobs (juce::Component& parent, std::vector<std::unique_ptr<KnobComponent>>& knobs, PluginBase& processor,
                   juce::Colour accent, std::initializer_list<std::pair<juce::String, const char*>> specs)
    {
        for (const auto& [id, name] : specs)
        {
            knobs.push_back (std::make_unique<KnobComponent> (processor.apvts, id, name, accent));
            parent.addAndMakeVisible (*knobs.back());
        }
    }

    template <typename T>
    std::vector<juce::Component*> pointers (std::vector<std::unique_ptr<T>>& items, size_t from, size_t to)
    {
        std::vector<juce::Component*> result;
        for (auto i = from; i < to && i < items.size(); ++i)
            result.push_back (items[i].get());
        return result;
    }
}

//==============================================================================
HeaderBar::HeaderBar (PluginBase& p, const juce::String& subtitleIn)
    : processor (p), subtitle (subtitleIn), logo (subtitleIn), presets (p, Presets::Section::full, Theme::pattern)
{
    addAndMakeVisible (presets);
    if (! processor.isEffectVersion())
    {
        exportButton.setTooltip ("Export the kick, the instruments or the full mix as a looping WAV file, "
                                 "at the current tempo and with the last played note.");
        exportButton.onClick = [this] { showExportMenu(); };
        addAndMakeVisible (exportButton);
    }

    if (processor.getKeyboardState() != nullptr) // instrumentet
    {
        listenAll.setTooltip ("LISTEN - Hold to hear the whole synth (the kick, the instruments and the pattern) on the note chosen next to it, looping while you hold. "
                              "Press again to start over. Double-click to let it keep playing until you click again.");
        listenAll.onStart = [this] { processor.requestAudition (PluginBase::previewRequest (PluginBase::previewAllRows)); };
        listenAll.onStop = [this] { processor.requestAudition (PluginBase::previewStop); };
        addAndMakeVisible (listenAll);

        for (int note = 24; note < 96; ++note) // C1 ... B6
            noteBox.addItem (juce::MidiMessage::getMidiNoteName (note, true, true, 5), note + 1);
        noteBox.setSelectedId (processor.previewNote.load() + 1, juce::dontSendNotification);
        noteBox.setTooltip ("PREVIEWS - The note all listening plays: this synth LISTEN button, the rows' speaker buttons, the piano rolls and the oscillators. "
                            "C5 by default.");
        noteBox.onChange = [this] { processor.previewNote = noteBox.getSelectedId() - 1; };
        addAndMakeVisible (noteBox);

        restartButton.setTooltip ("RESTART - Starts the counter and the pattern over at bar 1.1 (when the plugin runs on its own clock; with DAW SYNC the DAW decides).");
        restartButton.onClick = [this] { processor.requestRestart(); };
        addAndMakeVisible (restartButton);
    }

    aiButton.setClickingTogglesState (true);
    aiButton.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    aiButton.onClick = [this]
    {
        processor.setBridgeEnabled (aiButton.getToggleState());
        aiButton.setToggleState (processor.isBridgeEnabled(), juce::dontSendNotification);
        updateAiTooltip();
    };
    addAndMakeVisible (aiButton);
    updateAiTooltip();
    startTimerHz (Settings::animationHz (30));
}

void HeaderBar::updateAiTooltip()
{
    aiButton.setTooltip (processor.isBridgeEnabled()
        ? "AI LINK is ON (instance " + processor.getBridgeId() + "). An AI assistant using the Kickbase MCP bridge can read and change this plugin and hear/measure it offline. Click to switch off."
        : "AI LINK - Off. Switch on to let an AI assistant (through the Kickbase MCP bridge, for example in Claude Desktop) read and change this plugin. Local only; nothing is sent over the internet by Kickbase.");
}

void HeaderBar::paintOverChildren (juce::Graphics& g)
{
    // Returpilen på omstartsknappen (ritad, så att den inte beror på att ett typsnitt har symbolen).
    if (! restartButton.isVisible())
        return;
    const auto centre = restartButton.getBounds().toFloat().getCentre();
    g.setColour (Theme::text);
    juce::Path arrow;
    arrow.startNewSubPath (centre.x + 5.0f, centre.y - 6.0f);
    arrow.lineTo (centre.x + 5.0f, centre.y + 2.0f);
    arrow.lineTo (centre.x - 4.0f, centre.y + 2.0f);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path head;
    head.addTriangle (centre.x - 8.0f, centre.y + 2.0f, centre.x - 3.0f, centre.y - 2.5f, centre.x - 3.0f, centre.y + 6.5f);
    g.fillPath (head);
}

void HeaderBar::resized()
{
    const auto hasPreviews = processor.getKeyboardState() != nullptr;
    const auto height = getHeight();
    const auto controlsHeight = 34, controlsY = (height - controlsHeight) / 2;
    aiButton.setBounds (14 + logo.getWidth() + 22, controlsY, 44, controlsHeight); // AI-knappen först, till vänster om presetväljaren
    presets.setBounds (aiButton.getRight() + 10, controlsY, hasPreviews ? 160 : 200, controlsHeight);
    exportButton.setBounds (presets.getRight() + 10, controlsY, hasPreviews ? 70 : 76, controlsHeight);

    // PREVIEWS: en inringad ruta med högtalaren och tonväljaren, så att man ser att de hör ihop.
    previewGroup = hasPreviews ? juce::Rectangle<int> (exportButton.getRight() + 10, (height - 44) / 2, 112, 44) : juce::Rectangle<int>();
    listenAll.setVisible (hasPreviews);
    noteBox.setVisible (hasPreviews);
    restartButton.setVisible (hasPreviews);
    if (hasPreviews)
    {
        listenAll.setBounds (previewGroup.getX() + 7, previewGroup.getY() + 14, 30, previewGroup.getHeight() - 20);
        noteBox.setBounds (listenAll.getRight() + 5, previewGroup.getY() + 14, 62, previewGroup.getHeight() - 20);
    }


    // Till höger: tonart med lampan under, och en pill med tempo, takt och en knapp som börjar om vid 1.1.
    auto right = getLocalBounds().reduced (14, 0);
    keyBlock = right.removeFromRight (84).withSizeKeepingCentre (84, 40);
    right.removeFromRight (8);
    bpmPill = right.removeFromRight (214).reduced (0, (height - 34) / 2);
    restartButton.setBounds (bpmPill.getRight() - 32, bpmPill.getY() + 4, 26, bpmPill.getHeight() - 8);
}

void HeaderBar::showExportMenu()
{
    juce::PopupMenu menu;
    const char* names[] { "Full mix", "Kick", "Instruments", "Kick + instruments + full mix (3 files)" };
    for (int part = 0; part < 4; ++part)
    {
        juce::PopupMenu lengths;
        for (const auto bars : { 4, 8, 16 })
            lengths.addItem (1 + part * 100 + bars, juce::String (bars) + " bars");
        menu.addSubMenu (names[part], lengths);
    }

    juce::Component::SafePointer<HeaderBar> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exportButton), [safeThis] (int result)
    {
        if (safeThis != nullptr && result > 0)
            safeThis->chooseExportTarget ((result - 1) / 100, (result - 1) % 100);
    });
}

void HeaderBar::chooseExportTarget (int part, int bars)
{
    const juce::String partNames[] { "Full", "Kick", "Instruments" };
    const auto baseName = "Kickbase " + juce::String (bars) + " bars "
                        + juce::String (juce::roundToInt (processor.transportStatus.bpm.load())) + " BPM";
    auto folder = Settings::exportsFolder();
    folder.createDirectory();

    auto report = [] (bool ok, const juce::String& where)
    {
        if (! ok)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", "Could not write " + where);
    };

    juce::Component::SafePointer<HeaderBar> safeThis (this);
    if (part == 3)
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the three WAV files", folder);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis, bars, baseName, partNames, report] (const juce::FileChooser& fc)
        {
            const auto directory = fc.getResult();
            if (safeThis == nullptr || directory == juce::File())
                return;
            for (int i = 0; i < 3; ++i)
            {
                const auto file = directory.getChildFile (baseName + " " + partNames[i] + ".wav");
                report (safeThis->processor.exportWav (file, (PluginBase::ExportPart) i, bars), file.getFullPathName());
            }
        });
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Export " + partNames[part] + " as WAV",
                                                   folder.getChildFile (baseName + " " + partNames[part] + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis, part, bars, report] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (safeThis == nullptr || file == juce::File())
            return;
        const auto wav = file.withFileExtension ("wav");
        report (safeThis->processor.exportWav (wav, (PluginBase::ExportPart) part, bars), wav.getFullPathName());
    });
}

void HeaderBar::timerCallback()
{
    if (processor.getKeyboardState() != nullptr)
    {
        listenAll.setActive (processor.previewingLayer.load() == PluginBase::previewAllRows);
        if (noteBox.getSelectedId() != processor.previewNote.load() + 1)
            noteBox.setSelectedId (processor.previewNote.load() + 1, juce::dontSendNotification);
    }

    // Toppvärdet av de senaste ~20 ms på masterns utgång, med mjukt fall och en håll-markör.
    processor.getMasterSection().outputTap.copyLatest (1024, meterLeft, meterRight);
    auto lo = 0.0f, ro = 0.0f;
    for (size_t i = 0; i < meterLeft.size(); ++i)
    {
        lo = std::max (lo, std::abs (meterLeft[i]));
        ro = std::max (ro, std::abs (meterRight[i]));
    }
    peakL = lo > peakL ? lo : peakL * 0.8f;
    peakR = ro > peakR ? ro : peakR * 0.8f;
    holdL = std::max (peakL, holdL * 0.985f);
    holdR = std::max (peakR, holdR * 0.985f);

    // Loggans glöd följer ljudet: en ny kick startar en våg åt höger, en ny basstöt en våg åt vänster.
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto triggers = processor.transportStatus.kickTriggers.load();
    if (lastKickTriggers >= 0 && triggers != lastKickTriggers)
        lastKickTime = now;
    lastKickTriggers = triggers;

    std::vector<float> bassLeft, bassRight;
    processor.bassAfterDuckTap.copyLatest (256, bassLeft, bassRight);
    auto bassPeak = 0.0f;
    for (const auto sample : bassLeft)
        bassPeak = std::max (bassPeak, std::abs (sample));
    if (bassPeak > 0.03f && bassPeak > lastBassPeak * 1.35f + 0.01f && now - lastBassTime > 0.06)
    {
        lastBassTime = now;
        bassStrength = juce::jlimit (0.35, 1.0, (double) bassPeak * 3.0);
    }
    lastBassPeak = bassPeak > lastBassPeak ? bassPeak : lastBassPeak * 0.85f;

    // Rita bara om det som rör sig: mätaren (30 gånger i sekunden), loggan så länge glöden går, och resten när tempo, takt, tonart eller
    // synk ändras. Att rita hela rubriken 30 gånger i sekunden kostade onödigt mycket.
    const auto reduced = Settings::reduceAnimations();
    if (reduced != reducedLast)
    {
        reducedLast = reduced;
        startTimerHz (Settings::animationHz (30));
    }
    // Neonet lyser med ljudets nivå (utgångens topp): släckt när inget spelas, så att man ser ljuden.
    {
        const auto level = juce::jmax (peakL, peakR);
        const auto target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -80.0f) + 54.0f) / 30.0f);
        glow += (target - glow) * (target > glow ? 0.45f : 0.1f);
        if (glow < 0.01f)
            glow = 0.0f;
    }
    const auto glowMoved = std::abs (glow - glowShown) > 0.02f || (glow == 0.0f && glowShown != 0.0f);
    if (glowMoved)
        glowShown = glow;
    const auto logoActive = glowMoved || (! reduced && (now - lastKickTime < 1.9 || bassStrength * std::exp (-(now - lastBassTime) * 2.2) > 0.01));
    if (logoActive || logoWasActive)
        repaint (0, 0, 14 + logo.getWidth() + 24, getHeight());
    logoWasActive = logoActive;
    repaint (meterBounds().expanded (2));

    const auto& status = processor.transportStatus;
    const auto hash = juce::roundToInt ((float) status.bpm.load() * 10.0f) * 31 + status.patternBar.load() * 7 + status.beat.load() * 3 + status.keyNote.load() * 11
                    + (status.followingHost.load() ? 1 : 0);
    if (hash != lastStatusHash)
    {
        lastStatusHash = hash;
        repaint();
    }
}

// Färgerna glider sakta mellan varandra (en ny färg var sjätte sekund).
static juce::Colour fadeColour (std::initializer_list<juce::uint32> palette, double offset, double now)
{
    const std::vector<juce::uint32> colours (palette);
    const auto position = std::fmod ((now + offset) / 6.0, (double) colours.size());
    const auto index = (int) std::floor (position);
    const auto mix = (float) (position - index);
    const auto smooth = mix * mix * (3.0f - 2.0f * mix);
    return juce::Colour (colours[(size_t) index]).interpolatedWith (juce::Colour (colours[(size_t) ((index + 1) % (int) colours.size())]), smooth);
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& status = processor.transportStatus;
    {
        // Loggan: glöden följer ljudet. En ny kick startar en våg åt höger, en ny stöt från instrumenten en våg åt vänster.
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto kickAge = now - lastKickTime, bassAge = now - lastBassTime;
        // Halv fart mot förut (glöden går över loggan på 0,6 s och tonar ut på ~1,5 s), och starkare. Med minskade animationer står loggan still.
        const auto still = Settings::reduceAnimations();
        LogoPainter::Sweep kick { juce::jlimit (0.0f, 1.0f, (float) (kickAge / 0.60)), still ? 0.0f : (float) std::exp (-kickAge * 2.6), fadeColour ({ 0xffff5fa8, 0xffff9a3c, 0xffb98cff, 0xffffc857 }, 0.0, now) };
        LogoPainter::Sweep bass { 1.0f - juce::jlimit (0.0f, 1.0f, (float) (bassAge / 0.76)), still ? 0.0f : (float) (bassStrength * std::exp (-bassAge * 2.2)),
                                  fadeColour ({ 0xff3fd2ff, 0xff5fffb0, 0xff6b8cff, 0xff5ef2d6 }, 9.0, now) };
        logo.paint (g, { 14.0f, ((float) getHeight() - (float) LogoPainter::height) * 0.5f }, glowShown, kick, bass);
    }

    // Tonart med lampan under (DAW SYNC eller FREE RUN) längst till höger.
    {
        const auto keyNote = status.keyNote.load();
        auto block = keyBlock.toFloat();
        auto keyRow = block.removeFromTop (block.getHeight() * 0.5f);
        g.setFont (Theme::font (10.0f, true));
        g.setColour (Theme::textDim);
        g.drawText ("KEY", keyRow.removeFromLeft (26.0f), juce::Justification::centredLeft);
        g.setFont (Theme::font (16.0f, true));
        g.setColour (Theme::pattern);
        g.drawText (keyNote >= 0 ? Params::keyNames()[keyNote % 12] : juce::String ("-"), keyRow, juce::Justification::centredLeft);

        const auto following = status.followingHost.load();
        const auto lampColour = following ? Theme::mix : Theme::kick;
        const juce::Point<float> centre { block.getX() + 7.0f, block.getCentreY() };
        g.setColour (lampColour.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));
        g.setColour (lampColour.brighter (0.3f));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (centre));
        g.setFont (Theme::font (11.0f, true));
        g.setColour (Theme::textDim);
        g.drawText (following ? "DAW SYNC" : "FREE RUN", block.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    }

    // Tempo och takt i samma kapselformade hårdvarupill; knappen längst till höger börjar om vid 1.1.
    {
        const auto pill = bpmPill.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121420), pill.getX(), pill.getY(),
                                                 juce::Colour (0xff1c1f30), pill.getX(), pill.getBottom(), false));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xff2c3148));
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);

        auto inner = pill.reduced (pill.getHeight() * 0.5f - 4.0f, 0.0f);
        g.setFont (Theme::font (13.0f, true));
        g.setColour (Theme::text);
        g.drawText (juce::String (status.bpm.load(), 1) + " BPM", inner.removeFromLeft (100.0f), juce::Justification::centred);
        g.setColour (juce::Colour (0xff2c3148));
        g.fillRect (juce::Rectangle<float> (inner.getX(), pill.getY() + 7.0f, 1.0f, pill.getHeight() - 14.0f));
        g.setFont (Theme::font (14.0f, true));
        g.setColour (Theme::text);
        g.drawText (juce::String (status.patternBar.load()) + "." + juce::String (status.beat.load()), // takten i mönstret (börjar om när mönstret är klart)
                    inner.removeFromLeft (58.0f), juce::Justification::centred);
    }

    // PREVIEWS: ramen runt högtalaren och tonväljaren.
    if (! previewGroup.isEmpty())
    {
        const auto group = previewGroup.toFloat();
        g.setColour (Theme::pattern.withAlpha (0.07f));
        g.fillRoundedRectangle (group, 6.0f);
        g.setColour (Theme::pattern.withAlpha (0.75f));
        g.drawRoundedRectangle (group.reduced (0.5f), 6.0f, 1.2f);
        g.setFont (Theme::font (8.5f, true));
        g.setColour (Theme::pattern);
        g.drawText ("PREVIEWS", group.withHeight (14.0f).withTrimmedLeft (8.0f), juce::Justification::centredLeft);
    }

    // Utgångsmätare (L och R, -48 till 0 dB): visar att synten faktiskt ger ljud.
    {
        auto meter = meterBounds().toFloat();
        g.setFont (Theme::font (9.0f, true));
        g.setColour (Theme::textDim);
        g.drawText ("OUT", meter.removeFromLeft (26.0f), juce::Justification::centredLeft);
        auto bars = meter.reduced (0.0f, 1.0f);
        const auto barHeight = (bars.getHeight() - 3.0f) * 0.5f;
        auto drawBar = [&] (juce::Rectangle<float> bar, float peak, float hold)
        {
            auto fraction = [] (float amp) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (amp, -60.0f) + 48.0f) / 48.0f); };
            g.setColour (juce::Colour (0xff0b0d18));
            g.fillRoundedRectangle (bar, 2.0f);
            const auto filled = bar.withWidth (bar.getWidth() * fraction (peak));
            g.setGradientFill (juce::ColourGradient (Theme::mix, bar.getX(), 0.0f, Theme::master, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (filled, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillRect (bar.getX() + bar.getWidth() * fraction (hold) - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        };
        drawBar (bars.removeFromTop (barHeight), peakL, holdL);
        bars.removeFromTop (3.0f);
        drawBar (bars, peakR, holdR);
    }
}

//==============================================================================
KickModule::KickModule (PluginBase& p)
    : ModulePanel ("KICK", Theme::kick, &p.apvts, ParamID::kickOn), processor (p),
      keyMode (p.apvts, ParamID::kickKeyFollow, "KEY"),
      presets (p, Presets::Section::kick, Theme::kick),
      display (p)
{
    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    fxSend = std::make_unique<FxSendControl> (p.apvts, ParamID::kickFxChannel, ParamID::kickFxAmount, "FX SEND", Theme::kick);
    addAndMakeVisible (*fxSend);
    setHeaderFxSend (fxSend.get());
    addAndMakeVisible (keyMode);
    addAndMakeVisible (display);

    auto knob = [&] (const char* id, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, id, name, Theme::kick));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };
    auto choice = [&] (const char* id, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, id, name));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };

    // I signalens ordning: stämning och form (envelopena ritas i displayen), kroppens övertoner,
    // klicket, drive och utgång.
    groups.addRow ({ { "TUNING", { knob (ParamID::kickTune, "TUNE") } },
                     { "ENVELOPES", { knob (ParamID::kickPitchAmount, "PITCH AMT"), knob (ParamID::kickLength, "LENGTH") } },
                     { "BODY", { knob (ParamID::kickPhase, "PHASE"), knob (ParamID::kickHarmonics, "HARMONICS"),
                                 knob (ParamID::kickHarmDecay, "H. DECAY"), knob (ParamID::kickHarmTilt, "H. TILT") } },
                     { "CLICK", { knob (ParamID::kickClick, "LEVEL"), knob (ParamID::kickClickLength, "LENGTH"),
                                  knob (ParamID::kickClickTone, "TONE") } },
                     { "DRIVE", { choice (ParamID::kickClipMode, "MODE"), knob (ParamID::kickDrive, "AMOUNT") } },
                     { "OUT", { knob (ParamID::kickHighCut, "HIGH CUT"), knob (ParamID::kickLevel, "LEVEL") } } });

    // Fler lager och former: kroppens form och FM, sub, brusig topp, sampling som hel kick, bitkrossning, lågt skär.
    groups.addRow ({ { "SHAPE", { choice (ParamID::kickWave, "WAVE"), knob (ParamID::kickSub, "SUB"), knob (ParamID::kickFm, "FM"),
                                  knob (ParamID::kickFmRatio, "RATIO"), knob (ParamID::kickFmDecay, "FM DECAY") },
                     "The body's waveform, a sine sub one octave down, and FM (a second sine bends the body's phase at the start: punchier, more metallic)." },
                     { "TOP", { knob (ParamID::kickNoise, "NOISE"), knob (ParamID::kickNoiseDecay, "DECAY"), knob (ParamID::kickNoiseTone, "TONE") },
                     "A noisy top layer on top of the kick: snap and air, with its own decay and high-pass tone." },
                     { "SAMPLE", { choice (ParamID::kickSampleMode, "MODE"), knob (ParamID::kickSampleLevel, "LEVEL"), knob (ParamID::kickSampleTune, "TUNE") },
                     "How a loaded sample is used: Click = only the first moments (the CLICK level), Layer = the whole sample on top of the synth kick, "
                     "Replace = the sample is the kick (the synth is silent). LEVEL and TUNE apply to Layer and Replace." },
                     { "CRUSH", { knob (ParamID::kickCrush, "BITS") } },
                     { "FILTER", { knob (ParamID::kickLowCut, "LOW CUT"), knob (ParamID::kickVelPitch, "VEL>PITCH") } } });

    sampleLabel.setFont (Theme::font (11.0f));
    sampleLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (sampleLabel);

    loadButton.setTooltip ("Load a WAV/AIFF as the click (or drop a file on the kick module)");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a click sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadSample (fc.getResult());
                              });
    };
    addAndMakeVisible (loadButton);

    clearButton.setTooltip ("Use the synthesized click again");
    clearButton.onClick = [this] { processor.clearClickSample(); updateSampleLabel(); };
    addAndMakeVisible (clearButton);

    listenButton.setTooltip ("LISTEN - Plays the kick (you can also click an empty spot in the kick's picture).");
    listenButton.onClick = [this] { processor.requestAudition (0); };
    addAndMakeVisible (listenButton);
    listenSampleButton.setTooltip ("HEAR SAMPLE - Plays just the loaded sample, whatever the MODE is (so you can check what you loaded).");
    listenSampleButton.onClick = [this] { processor.requestAudition (5); };
    addAndMakeVisible (listenSampleButton);

    updateSampleLabel();
    startTimerHz (2); // projektet kan laddas om medan fönstret är öppet
}

void KickModule::timerCallback()
{
    updateSampleLabel();
}

void KickModule::updateSampleLabel()
{
    const auto name = processor.getClickSampleName();
    const auto text = name.isEmpty() ? juce::String ("CLICK: synth  (drop a WAV here)") : "CLICK: " + name;
    if (sampleLabel.getText() != text)
        sampleLabel.setText (text, juce::dontSendNotification);
    clearButton.setEnabled (name.isNotEmpty());
    listenSampleButton.setEnabled (name.isNotEmpty());
}

void KickModule::loadSample (const juce::File& file)
{
    if (! processor.loadClickSample (file))
        sampleLabel.setText ("Could not read " + file.getFileName(), juce::dontSendNotification);
    else
        updateSampleLabel();
}

bool KickModule::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac");
}

void KickModule::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    loadSample (juce::File (files[0]));
    repaint();
}

void KickModule::paintOverChildren (juce::Graphics& g)
{
    if (dragHover)
    {
        g.setColour (Theme::kick.withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (Theme::kick);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, 2.0f);
    }
}

void KickModule::resized()
{
    ModulePanel::resized();
    if (! isEmbedded())
        presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    // Nedersta raden: KEY-väljaren (etikett + ruta) och klicksamplingen bredvid.
    auto bottom = area.removeFromBottom (40);
    keyMode.setBounds (bottom.removeFromLeft (110));
    bottom.removeFromLeft (10);
    auto sampleRow = bottom.removeFromBottom (24);
    clearButton.setBounds (sampleRow.removeFromRight (24));
    sampleRow.removeFromRight (4);
    loadButton.setBounds (sampleRow.removeFromRight (50));
    sampleRow.removeFromRight (10);
    listenSampleButton.setBounds (sampleRow.removeFromRight (96));
    sampleRow.removeFromRight (6);
    listenButton.setBounds (sampleRow.removeFromRight (70));
    sampleRow.removeFromRight (10);
    sampleLabel.setBounds (sampleRow);
    area.removeFromBottom (4);

    // En rad rattar under displayen, som får resten av höjden.
    const auto rowHeight = juce::jlimit (60, 100, area.getHeight() / 3 - ControlGroups::titleHeight - 3);
    const auto groupsHeight = groups.numRows() * (ControlGroups::titleHeight + rowHeight + 3);
    groups.layout (area.removeFromBottom (groupsHeight), rowHeight);
    area.removeFromBottom (4);
    display.setBounds (area);
}

//==============================================================================
BassModule::BassModule (PluginBase& p, int layerIn)
    : ModulePanel ("INSTR " + juce::String (layerIn + 1), Theme::layerColour (layerIn), &p.apvts, ParamID::bass (layerIn, BassField::on)),
      processor (p),
      layer (layerIn),
      refs (p.apvts),
      sources (&p.selectedBassSource[(size_t) layerIn]),
      presets (p, Presets::Section::bass, Theme::layerColour (layerIn), layerIn)
{
    using F = BassField;
    auto id = [this] (F f) { return ParamID::bass (layer, f); };
    const auto accent = Theme::layerColour (layer);

    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    fxSend = std::make_unique<FxSendControl> (p.apvts, id (F::fxChannel), id (F::fxAmount), "FX SEND", accent);
    addAndMakeVisible (*fxSend);
    setHeaderFxSend (fxSend.get());
    envelope.onBeforePaint = [this] { current = refs.readBass (layer, 36); };

    envelope.addCurve ({ [this] (double t) { return BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f; },
                         accent, true, "FILTER" });
    envelope.addCurve ({ [this] (double t) { return ampValueAt (t); }, Theme::text.withAlpha (0.8f), false, "AMP" });

    envelope.addHandle ({ [this] { return juce::Point<float> (std::max (0.0015f, current.envAttackMs * 0.001f), current.envAmount / 8.0f); },
                          param (p, id (F::envAttack)), param (p, id (F::envAmount)), false, accent });
    envelope.addHandle ({ [this] {
                              const auto t = current.envAttackMs * 0.001 + current.envDecayMs * 0.001 / 3.0;
                              return juce::Point<float> ((float) t, BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f); },
                          param (p, id (F::envDecay)), nullptr, false, accent });

    envelope.addHandle ({ [this] { return juce::Point<float> (current.attackMs * 0.001f, 1.0f); },
                          param (p, id (F::attack)), nullptr, false, Theme::text });
    envelope.addHandle ({ [this] {
                              const auto t = (current.attackMs + current.decayMs * 0.5f) * 0.001;
                              return juce::Point<float> ((float) t, ampValueAt (t)); },
                          param (p, id (F::decay)), param (p, id (F::sustain)), false, Theme::text });
    envelope.setNoteOff ([this] { return noteOffNow(); });
    envelope.addHandle ({ [this] { return juce::Point<float> ((float) noteOffNow().seconds + current.releaseMs * 0.001f, 0.0f); },
                          param (p, id (F::release)), nullptr, false, Theme::text });
    addAndMakeVisible (envelope);

    // Källorna i flikar: OSC 1, OSC 2, OSC 3 och SAMPLE, var och en med en på/av-knapp på fliken.
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
        oscs[(size_t) osc] = std::make_unique<OscPanel> (p, layer, osc, accent);
    samplePage = std::make_unique<SamplePage> (p, layer, accent);
    addAndMakeVisible (sources);
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
    {
        sources.addTab ("OSC " + juce::String (osc + 1), accent, *oscs[(size_t) osc]);
        sources.addPower (osc, p.apvts, ParamID::bass (layer, ParamID::oscField (osc, F::aOn)));
    }
    listenButton.setTooltip ("LISTEN - Plays this layer's sound (one second, the key's root note) so you can hear it without a DAW note. "
                             "Click an oscillator's picture to hear just that oscillator.");
    listenButton.onClick = [this] { processor.requestAudition (PluginBase::layerAudition (layer)); };
    addAndMakeVisible (listenButton);
    sources.addTab ("SAMPLE", accent, *samplePage);
    sources.addPower (ParamID::oscsPerLayer, p.apvts, ParamID::bass (layer, F::sampleOn));

    auto knob = [&] (const juce::String& paramID, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, paramID, name, accent));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };
    auto choice = [&] (const juce::String& paramID, const char* name) -> juce::Component*
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, paramID, name));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };

    // Under oscillatorerna, i signalens ordning: tonhöjd, sub/brus, filter -> volym och utgång.
    groups.addRow ({ { "PITCH", { knob (id (F::octave), "OCTAVE"), knob (id (F::tune), "TUNE"),
                                  knob (id (F::pitchEnvAmount), "ENV AMT"), knob (id (F::pitchEnvDecay), "ENV DEC") } },
                     { "SUB", { knob (id (F::sub), "LEVEL"), choice (id (F::subWave), "WAVE"), choice (id (F::subOctave), "OCTAVE") } },
                     { "NOISE", { knob (id (F::noise), "LEVEL"), knob (id (F::noiseTone), "TONE") } },
                     { "FILTER", { choice (id (F::filterType), "TYPE"), knob (id (F::cutoff), "CUTOFF"),
                                   knob (id (F::resonance), "RESO"), knob (id (F::keyTrack), "KEYTRK") } },
                     { "FILTER ENVELOPE", { knob (id (F::envAmount), "AMOUNT"), knob (id (F::envAttack), "ATTACK"), knob (id (F::envDecay), "DECAY"),
                                            knob (id (F::velToEnv), "VEL") } } });
    groups.addRow ({ { "VOLUME ENVELOPE", { knob (id (F::attack), "ATTACK"), knob (id (F::decay), "DECAY"),
                                            knob (id (F::sustain), "SUSTAIN"), knob (id (F::release), "RELEASE") } },
                     { "WARP ENV", { knob (id (F::warpEnv), "AMOUNT") } },
                     { "DRIVE", { knob (id (F::drive), "AMOUNT"), choice (id (F::driveMode), "MODE") } },
                     { "LAYER OUT", { knob (id (F::level), "LEVEL"), knob (id (F::duck), "DUCK"), knob (id (F::gate), "GATE") } } });

    // MODULATION: ENV 2 och ENV 3 (egna envelopes att modulera med), och velocity och slump som källor. Varje källa har ett handtag
    // som dras till en ratt (eller sätts i MATRIX-fliken).
    auto handle = [&] (int kind, const char* label) -> juce::Component*
    {
        controls.push_back (std::make_unique<LfoDragHandle> (Modulation::instrumentSource (layer, kind), label));
        addAndMakeVisible (*controls.back());
        return controls.back().get();
    };
    const auto modStart = controls.size();
    const auto env2 = (int) F::env2Attack, env3 = (int) F::env3Attack;
    modGroups.addRow ({ { "ENV 2", { knob (id ((F) env2), "ATTACK"), knob (id ((F) (env2 + 1)), "DECAY"), knob (id ((F) (env2 + 2)), "SUSTAIN"),
                                     knob (id ((F) (env2 + 3)), "RELEASE"), handle (0, "DRAG") } },
                        { "ENV 3", { knob (id ((F) env3), "ATTACK"), knob (id ((F) (env3 + 1)), "DECAY"), knob (id ((F) (env3 + 2)), "SUSTAIN"),
                                     knob (id ((F) (env3 + 3)), "RELEASE"), handle (1, "DRAG") } },
                        { "MORE SOURCES", { handle (2, "VELOCITY"), handle (3, "RANDOM") } } });
    for (auto i = modStart; i < controls.size(); ++i)
        modControls.push_back (controls[i].get());

    modOpen = p.modEnvelopesOpen;
    modToggle.onClick = [this]
    {
        modOpen = ! modOpen;
        processor.modEnvelopesOpen = modOpen;
        updateModToggle();
        resized();
        repaint();
    };
    modToggle.setTooltip ("MODULATION - Open or close ENV 2 and ENV 3: two extra envelopes (attack, decay, sustain, release) that do not play the sound but move other "
                          "knobs, plus the note's VELOCITY and a RANDOM value per note as sources. Drag a source's handle onto any knob, or use the MATRIX tab.");
    addAndMakeVisible (modToggle);
    updateModToggle();
}

void BassModule::updateModToggle()
{
    modToggle.setButtonText (juce::String (modOpen ? juce::String::fromUTF8 ("\xe2\x96\xbe") : juce::String::fromUTF8 ("\xe2\x96\xb8"))
                             + "  MODULATION   ENV 2, ENV 3, VELOCITY, RANDOM");
    for (auto* control : modControls)
        control->setVisible (modOpen);
}

bool BassModule::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;aif;aiff;flac");
}

void BassModule::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    samplePage->loadSample (juce::File (files[0]));
    sources.setCurrentTab (ParamID::oscsPerLayer); // visa SAMPLE-fliken
    repaint();
}

void BassModule::paintOverChildren (juce::Graphics& g)
{
    if (dragHover)
    {
        g.setColour (getAccent().withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (getAccent());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, 2.0f);
    }
}

//==============================================================================
SampleView::SampleView (PluginBase& p, int layerIn, juce::Colour accentIn) : processor (p), layer (layerIn), accent (accentIn)
{
    setMouseCursor (speakerCursor);
    setTooltip ("SAMPLE - The waveform of this layer's sample. Click it to hear just the sample. The white line marks where each note starts (START); "
                "the moving marker shows where the sample is playing.");
    startTimerHz (30);
}

void SampleView::timerCallback()
{
    const auto* sample = processor.getActiveBassSample (layer);
    const auto start = processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::sampleStart))->load();
    const auto position = processor.transportStatus.bassSamplePosition[(size_t) layer].load();
    if (position >= 0.0f && (marker < 0.0f || position < marker))
        flashWallTime = juce::Time::getMillisecondCounterHiRes(); // en ny ton (positionen började om)
    const auto flashing = juce::Time::getMillisecondCounterHiRes() - flashWallTime < 500.0;
    if (sample != shown || start != lastStart || position != marker || flashing)
    {
        shown = sample;
        lastStart = start;
        marker = position;
        repaint();
    }
}

void SampleView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto* sample = processor.getActiveBassSample (layer);
    if (sample == nullptr || sample->audio.getNumSamples() < 2)
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawText ("Drop a WAV here, or press LOAD", bounds, juce::Justification::centred);
        return;
    }

    const auto area = bounds.reduced (6.0f, 8.0f);
    const auto* data = sample->audio.getReadPointer (0);
    const auto length = sample->audio.getNumSamples();
    const auto width = std::max (1, (int) area.getWidth());

    juce::Path path;
    for (int x = 0; x < width; ++x)
    {
        const auto from = (int) ((juce::int64) x * length / width);
        const auto to = std::max (from + 1, (int) ((juce::int64) (x + 1) * length / width));
        auto lowest = 0.0f, highest = 0.0f;
        for (int i = from; i < to && i < length; ++i)
        {
            lowest = std::min (lowest, data[i]);
            highest = std::max (highest, data[i]);
        }
        const auto px = area.getX() + (float) x;
        path.addRectangle (px, area.getCentreY() - highest * area.getHeight() * 0.5f, 1.0f, std::max (1.0f, (highest - lowest) * area.getHeight() * 0.5f));
    }
    g.setColour (accent.withAlpha (0.85f));
    g.fillPath (path);

    const auto startX = area.getX() + juce::jlimit (0.0f, 1.0f, lastStart) * area.getWidth();
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawVerticalLine ((int) startX, area.getY(), area.getBottom());

    // Spelmarkör: det som spelats lyses upp, ett lysande streck där samplingen är nu, och en blixt när en ny ton börjar.
    if (marker >= 0.0f)
    {
        const auto markerX = area.getX() + marker * area.getWidth();
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (area.withRight (markerX).toNearestInt());
            g.setColour (accent.brighter (0.5f).withAlpha (0.9f));
            g.fillPath (path);
        }
        g.setColour (accent.withAlpha (0.3f));
        g.fillRect (juce::Rectangle<float> (markerX - 3.0f, area.getY(), 6.0f, area.getHeight()));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (markerX - 0.75f, area.getY(), 1.5f, area.getHeight()));
    }
    const auto sinceFlash = juce::Time::getMillisecondCounterHiRes() - flashWallTime;
    if (sinceFlash < 500.0)
    {
        g.setColour (accent.withAlpha (0.16f * (float) std::exp (-sinceFlash / 140.0)));
        g.fillRoundedRectangle (bounds, 6.0f);
    }

    // Glöd när musen är över rutan.
    if (hover)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 4.0f);
        g.setColour (accent.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 1.5f);
    }
}

void SampleView::mouseDown (const juce::MouseEvent&)
{
    if (processor.getActiveBassSample (layer) != nullptr)
        processor.requestAudition (PluginBase::soloAudition (layer, 3)); // bara samplingen
}

SamplePage::SamplePage (PluginBase& p, int layerIn, juce::Colour accent)
    : processor (p), layer (layerIn), view (p, layerIn, accent)
{
    using F = BassField;
    auto id = [layerIn] (F f) { return ParamID::bass (layerIn, f); };
    auto knob = [&] (F f, const char* name)
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, id (f), name, accent));
        addAndMakeVisible (*controls.back());
    };
    auto choice = [&] (F f, const char* name)
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, id (f), name));
        addAndMakeVisible (*controls.back());
    };
    knob (F::sampleLevel, "LEVEL");
    knob (F::sampleStart, "START");
    knob (F::sampleRoot, "ROOT");
    choice (F::sampleLoop, "LOOP");
    choice (F::sampleKey, "KEY");

    addAndMakeVisible (view);
    label.setFont (Theme::font (11.0f));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);

    loadButton.setTooltip ("Load a WAV/AIFF/FLAC as this layer's sample (or drop a file on the page). The sample is turned on for you. "
                           "Turn the oscillators off (the power buttons on the OSC tabs) to hear only the sample.");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a sample for this layer", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      loadSample (fc.getResult());
                              });
    };
    addAndMakeVisible (loadButton);
    clearButton.setTooltip ("Remove the sample from this layer");
    clearButton.onClick = [this] { processor.clearBassSample (layer); updateLabel(); };
    addAndMakeVisible (clearButton);
    hearButton.setTooltip ("HEAR - Plays just this layer's sample (you can also click the waveform).");
    hearButton.onClick = [this] { processor.requestAudition (PluginBase::soloAudition (layer, 3)); };
    addAndMakeVisible (hearButton);

    updateLabel();
    startTimerHz (2); // projektet kan laddas om medan fönstret är öppet
}

void SamplePage::timerCallback()
{
    updateLabel();
}

void SamplePage::updateLabel()
{
    const auto name = processor.getBassSampleName (layer);
    const auto text = name.isEmpty() ? juce::String ("No sample loaded") : name;
    if (label.getText() != text)
        label.setText (text, juce::dontSendNotification);
    clearButton.setEnabled (name.isNotEmpty());
    hearButton.setEnabled (name.isNotEmpty());
}

void SamplePage::loadSample (const juce::File& file)
{
    if (! processor.loadBassSample (layer, file))
    {
        label.setText ("Could not read " + file.getFileName(), juce::dontSendNotification);
        return;
    }

    // En sampling som inte hörs är förvirrande: slå på den och höj nivån om den står på noll.
    if (auto* on = processor.apvts.getParameter (ParamID::bass (layer, BassField::sampleOn)))
        Presets::setParameter (*on, 1.0f);
    if (auto* level = processor.apvts.getParameter (ParamID::bass (layer, BassField::sampleLevel)); level != nullptr && level->getValue() <= 0.0f)
        Presets::setParameter (*level, 1.0f);
    updateLabel();
}

void SamplePage::resized()
{
    auto area = getLocalBounds().reduced (4);
    auto top = area.removeFromTop (24);
    clearButton.setBounds (top.removeFromRight (24));
    top.removeFromRight (4);
    loadButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (4);
    hearButton.setBounds (top.removeFromRight (56));
    label.setBounds (top);
    area.removeFromTop (6);

    auto right = area.removeFromRight (juce::jlimit (240, 420, area.getWidth() * 2 / 5));
    area.removeFromRight (8);
    view.setBounds (area);

    auto knobsRow = right.removeFromTop (right.getHeight() * 3 / 5);
    const auto cell = knobsRow.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        controls[(size_t) i]->setBounds (knobsRow.removeFromLeft (cell));
    right.removeFromTop (6);
    auto choices = right.removeFromTop (44);
    controls[3]->setBounds (choices.removeFromLeft (choices.getWidth() / 2).reduced (6, 0));
    controls[4]->setBounds (choices.reduced (6, 0));
}

//==============================================================================
EnvelopeView::NoteOff BassModule::noteOffNow() const
{
    const auto real = processor.typicalNoteSeconds (layer);
    return { real > 0.0 ? real : 0.18, real > 0.0 };
}

float BassModule::ampValueAt (double seconds) const
{
    // Envelopen för en typisk ton i instrumentets mönster (finns ingen, ett exempel på 180 ms).
    const auto gateSeconds = noteOffNow().seconds;
    const auto attack = std::max (0.00005, current.attackMs * 0.001);
    const auto decay = std::max (0.0001, current.decayMs * 0.001);
    const auto release = std::max (0.0001, current.releaseMs * 0.001);
    const auto ln = std::log (0.001);

    auto held = [&] (double t)
    {
        if (t < attack)
            return t / attack;
        return current.sustain + (1.0 - current.sustain) * std::exp (ln * (t - attack) / decay);
    };

    if (seconds < gateSeconds)
        return (float) held (seconds);

    return (float) (held (gateSeconds) * std::exp (ln * (seconds - gateSeconds) / release));
}

void BassModule::resized()
{
    ModulePanel::resized();
    if (! isEmbedded())
        presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    // Överst källorna i flikar (OSC 1-3 och SAMPLE), under dem kurvorna (filter + volym) och rattarna.
    sources.setBounds (area.removeFromTop (juce::jlimit (200, 250, area.getHeight() * 11 / 24)));
    area.removeFromTop (6);

    auto leftColumn = area.removeFromLeft (220);
    listenButton.setBounds (leftColumn.removeFromBottom (24));
    leftColumn.removeFromBottom (4);
    envelope.setBounds (leftColumn.reduced (0, 2));
    area.removeFromLeft (6);

    // Raderna, sedan dragspelet MODULATION (en rad till när det är öppet).
    constexpr int toggleHeight = 20;
    const auto rows = groups.numRows() + (modOpen ? 1 : 0);
    const auto rowHeight = juce::jlimit (40, 84, (area.getHeight() - toggleHeight - 4) / rows - ControlGroups::titleHeight - 3);
    groups.layout (area, rowHeight);
    const auto used = groups.numRows() * (ControlGroups::titleHeight + rowHeight + 3);
    auto rest = area.withTrimmedTop (used);
    modToggle.setBounds (rest.removeFromTop (toggleHeight));
    rest.removeFromTop (4);
    if (modOpen)
        modGroups.layout (rest, rowHeight);
}

//==============================================================================
void InstrumentHost::Header::resized()
{
    auto area = getLocalBounds();
    selector.setBounds (area.removeFromLeft (150));
    area.removeFromLeft (4);
    addButton.setBounds (area.removeFromLeft (24));
    area.removeFromLeft (2);
    removeButton.setBounds (area.removeFromLeft (24));
    area.removeFromLeft (10);
    if (extras != nullptr)
        extras->setBounds (area);
}

InstrumentHost::InstrumentHost (PluginBase& p) : processor (p)
{
    header.selector.setTooltip ("INSTRUMENT - Choose which instrument you are editing (up to 10). Right-click or use the menu to rename it. "
                                "Every instrument has its own pattern, oscillators, filter, envelopes, effect send and sidechain depth.");
    header.selector.onClick = [this] { showSelector(); };
    header.addButton.setTooltip ("ADD INSTRUMENT - Adds another instrument (max 10). It starts with default settings and an empty pattern.");
    header.addButton.onClick = [this] { addInstrument(); };
    header.removeButton.setTooltip ("REMOVE INSTRUMENT - Removes the LAST instrument and deletes everything in it from memory (pattern, sound, sample, "
                                    "wavetables, modulation). You are asked first if it has anything in it. To silence another one, turn it off with its power button.");
    header.removeButton.onClick = [this] { removeInstrument(); };
    for (auto* component : std::initializer_list<juce::Component*> { &header.selector, &header.addButton, &header.removeButton })
        header.addAndMakeVisible (component);

    current = juce::jlimit (0, p.activeInstruments() - 1, p.selectedInstrument);
    select (current);
    refresh();
    startTimerHz (4);
}

InstrumentHost::~InstrumentHost()
{
    for (auto* e : extras)
        if (e != nullptr)
            header.removeChildComponent (e);
}

BassModule& InstrumentHost::moduleFor (int layer)
{
    auto& slot = modules[(size_t) layer];
    if (slot == nullptr)
    {
        slot = std::make_unique<BassModule> (processor, layer);
        addChildComponent (*slot);
        extras[(size_t) layer] = slot->embedInTab();
        header.addChildComponent (*extras[(size_t) layer]);
        slot->setBounds (getLocalBounds());
    }
    return *slot;
}

void InstrumentHost::resized()
{
    for (auto& module : modules)
        if (module != nullptr)
            module->setBounds (getLocalBounds());
}

void InstrumentHost::select (int layer)
{
    layer = juce::jlimit (0, processor.activeInstruments() - 1, layer);
    current = layer;
    processor.selectedInstrument = layer;
    moduleFor (layer);

    for (int i = 0; i < ParamID::numBassLayers; ++i)
    {
        if (modules[(size_t) i] != nullptr)
            modules[(size_t) i]->setVisible (i == layer);
        if (extras[(size_t) i] != nullptr)
            extras[(size_t) i]->setVisible (i == layer);
    }
    header.extras = extras[(size_t) layer];
    header.resized();
    lastNamesVersion = -1;
    refresh();
}

void InstrumentHost::refresh()
{
    const auto count = processor.activeInstruments();
    if (current >= count)
    {
        select (count - 1);
        return;
    }
    lastCount = count;
    lastNamesVersion = processor.layerNamesVersion.load();
    header.selector.setButtonText (processor.getLayerName (current) + juce::String::fromUTF8 ("  \xe2\x96\xbe"));
    header.selector.setColour (juce::TextButton::textColourOffId, Theme::layerColour (current));
    header.addButton.setEnabled (count < ParamID::numBassLayers);
    header.removeButton.setEnabled (count > 1);
}

void InstrumentHost::timerCallback()
{
    // ENV-fliken (och andra) kan byta instrument: följ med.
    if (processor.selectedInstrument != current && juce::isPositiveAndBelow (processor.selectedInstrument, processor.activeInstruments()))
        select (processor.selectedInstrument);

    if (processor.activeInstruments() != lastCount || processor.layerNamesVersion.load() != lastNamesVersion)
        refresh();
}

void InstrumentHost::showSelector()
{
    juce::PopupMenu menu;
    const auto count = processor.activeInstruments();
    for (int i = 0; i < count; ++i)
    {
        const auto on = processor.apvts.getRawParameterValue (ParamID::bass (i, BassField::on))->load() >= 0.5f;
        menu.addItem (i + 1, juce::String (i + 1) + "   " + processor.getLayerName (i) + (on ? "" : "   (off)"), true, i == current);
    }
    menu.addSeparator();
    menu.addItem (100, "Rename " + processor.getLayerName (current) + " ...");
    menu.addItem (101, "Add instrument", count < ParamID::numBassLayers);
    menu.addItem (102, "Remove last instrument", count > 1);

    juce::Component::SafePointer<InstrumentHost> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header.selector).withMinimumWidth (header.selector.getWidth()),
                        [safeThis] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;
                            if (result == 100)
                                safeThis->rename (safeThis->current);
                            else if (result == 101)
                                safeThis->addInstrument();
                            else if (result == 102)
                                safeThis->removeInstrument();
                            else
                                safeThis->select (result - 1);
                        });
}

void InstrumentHost::rename (int layer)
{
    auto* window = new juce::AlertWindow ("Rename", "Name for this instrument (it is shown in the pattern, the mixer and the piano roll):", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    renameDialog = window;
    window->addTextEditor ("name", processor.getLayerName (layer));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Default name", 2);
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<InstrumentHost> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window, layer] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;
        safeThis->processor.setLayerName (layer, result == 2 ? juce::String() : window->getTextEditorContents ("name"));
        safeThis->processor.markStateChanged();
    }), true);
}

void InstrumentHost::addInstrument()
{
    const auto count = processor.activeInstruments();
    if (count >= ParamID::numBassLayers)
        return;
    processor.setInstrumentCount (count + 1);
    select (count);
}

void InstrumentHost::removeInstrument()
{
    const auto count = processor.activeInstruments();
    if (count <= 1 || confirmOpen)
        return;

    const auto layer = count - 1;
    juce::Component::SafePointer<InstrumentHost> safeThis (this);
    auto remove = [safeThis, layer, count]
    {
        if (safeThis == nullptr)
            return;
        auto& host = *safeThis;
        if (host.current == layer)
            host.select (layer - 1);

        // Panelen rivs först, så att inget i den lyssnar på parametrarna som nollställs.
        if (host.extras[(size_t) layer] != nullptr)
        {
            host.header.removeChildComponent (host.extras[(size_t) layer]);
            host.extras[(size_t) layer] = nullptr;
        }
        host.modules[(size_t) layer].reset();
        host.processor.setInstrumentCount (count - 1);
        host.refresh();
    };

    juce::String what;
    if (! processor.instrumentHasContent (layer, &what))
    {
        remove();
        return;
    }

    confirmOpen = true;
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (
                                      juce::MessageBoxIconType::WarningIcon, "Remove instrument",
                                      "Remove " + processor.getLayerName (layer) + "?\n\nIt has " + what + ".\n\nEverything in it is deleted (pattern, sound, sample, "
                                      "wavetables, modulation) and it can't be undone. Presets you have saved are not touched.",
                                      "Remove", "Cancel"),
                                  [safeThis, remove] (int choice)
    {
        if (safeThis == nullptr)
            return;
        safeThis->confirmOpen = false;
        if (choice == 1)
            remove();
    });
}

//==============================================================================
GateModule::GateModule (PluginBase& p)
    : ModulePanel ("GATE", Theme::gate, &p.apvts, ParamID::bass (0, BassField::on)),
      refs (p.apvts),
      key (p.apvts, ParamID::fxKey, "KEY"),
      presets (p, Presets::Section::bass, Theme::gate)
{
    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    envelope.onBeforePaint = [this]
    {
        attackMs = refs.get (ParamID::gateAttack);
        releaseMs = refs.get (ParamID::gateRelease);
    };

    // En öppning på 100 ms med attack och release.
    envelope.addCurve ({ [this] (double t)
                         {
                             constexpr double open = 0.1;
                             const auto attack = std::max (0.0001, attackMs * 0.001);
                             const auto release = std::max (0.0001, releaseMs * 0.001);
                             const auto opened = std::min (1.0, t / attack);
                             if (t < open)
                                 return (float) opened;
                             return (float) std::max (0.0, std::min (1.0, open / attack) - (t - open) / release);
                         },
                         Theme::gate, true, "GATE" });

    envelope.addHandle ({ [this] { return juce::Point<float> (attackMs * 0.001f, 1.0f); },
                          param (p, ParamID::gateAttack), nullptr, false, Theme::gate });
    envelope.addHandle ({ [this] { return juce::Point<float> (0.1f + releaseMs * 0.001f, 0.0f); },
                          param (p, ParamID::gateRelease), nullptr, false, Theme::gate });
    addAndMakeVisible (envelope);

    addAndMakeVisible (key);
    addKnobs (*this, knobs, p, Theme::gate, {
        { ParamID::gateAttack, "ATTACK" }, { ParamID::gateRelease, "RELEASE" } });
    knobs.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (0, BassField::level), "LEVEL", Theme::gate));
    addAndMakeVisible (*knobs.back());

    help.setText ("Put Kickbase FX on the mixer track of your instrument or bass synth. The synth is gated by the "
                  "gate steps and ducked under the kick. When the DAW is stopped, audio passes through untouched.",
                  juce::dontSendNotification);
    help.setFont (Theme::font (11.0f));
    help.setColour (juce::Label::textColourId, Theme::textDim);
    help.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (help);
}

void GateModule::resized()
{
    ModulePanel::resized();
    if (! isEmbedded())
        presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    help.setBounds (area.removeFromBottom (48));
    auto row = area.removeFromBottom (knobRowHeight);
    area.removeFromBottom (6);
    envelope.setBounds (area);

    const auto cell = row.getWidth() / 5;
    key.setBounds (row.removeFromLeft (cell).reduced (4, 14));
    auto items = pointers (knobs, 0, 3);
    layoutRow (items, row.withTrimmedRight (cell));
}

//==============================================================================
namespace
{
    juce::StringArray instrumentNames()
    {
        juce::StringArray names;
        for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
            names.add ("INSTR " + juce::String (layer + 1));
        return names;
    }

    // Instrumentens färger, och (för pianorullen) kickens efter dem.
    std::vector<juce::Colour> instrumentColours (bool withKick = false)
    {
        std::vector<juce::Colour> colours;
        for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
            colours.push_back (Theme::layerColour (layer));
        if (withKick)
            colours.push_back (Theme::kick);
        return colours;
    }
}

PatternModule::PatternModule (PluginBase& p, bool isInstrument)
    : processor (p), instrument (isInstrument),
      length (p.apvts, ParamID::patternBars, "TOTAL LENGTH"),
      presets (p, Presets::Section::pattern, Theme::pattern),
      sequencer (p, isInstrument ? instrumentNames() : juce::StringArray { "GATE" },
                 isInstrument ? instrumentColours() : std::vector<juce::Colour> { Theme::gate }),
      pianoRoll (p, instrumentColours (true))
{
    addAndMakeVisible (length);
    addAndMakeVisible (presets);
    if (instrument)
    {
        noteEnd = std::make_unique<ChoiceComponent> (p.apvts, ParamID::noteEnd, "NOTE END");
        addAndMakeVisible (*noteEnd);
    }

    // Lanerna för tonerna: velocity, tonlängd och tonhöjd (glide ritas i pianorullen), valda i en rullgardinslista.
    laneOrder = { StepSequencer::Lane::velocity, StepSequencer::Lane::gate };
    laneBox.addItem ("Velocity", 1);
    laneBox.addItem ("Note length", 2);
    if (instrument)
    {
        laneOrder.push_back (StepSequencer::Lane::pitch);
        laneBox.addItem ("Pitch", 3);
    }
    laneLabel.setText ("EDIT IN LANE", juce::dontSendNotification);
    laneLabel.setFont (Theme::font (11.0f, true));
    laneLabel.setColour (juce::Label::textColourId, Theme::textDim);
    laneBox.setTooltip ("EDIT IN LANE - What the strip under the selected row shows and edits for each note: its VELOCITY (volume), its NOTE LENGTH or its PITCH. "
                        "Not available for a row that is edited in the piano roll (set those there).");
    laneBox.onChange = [this] { selectLane (laneOrder[(size_t) juce::jlimit (0, (int) laneOrder.size() - 1, laneBox.getSelectedId() - 1)]); };
    addAndMakeVisible (laneLabel);
    addAndMakeVisible (laneBox);

    const auto numLayers = instrument ? ParamID::numBassLayers : 1;
    for (int layer = 0; layer < numLayers; ++layer)
    {
        const auto colour = instrument ? Theme::layerColour (layer) : Theme::gate;
        grids.push_back (std::make_unique<ChoiceComponent> (p.apvts, ParamID::bass (layer, BassField::grid), "GRID"));
        lengths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::gate), "ALL LENGTHS", colour));
        lengths.back()->setDescription ("NOTE LENGTH SCALE - Scales the length of every note in this instrument at once (100 % = as long as written). "
                                        "Lower = shorter, punchier notes. Set a single note's length with EDIT IN LANE > Note length or in the piano roll.");
        addChildComponent (*grids.back());
        addChildComponent (*lengths.back());

        if (instrument)
        {
            gateGrids.push_back (std::make_unique<ChoiceComponent> (p.apvts, ParamID::bass (layer, BassField::gateGrid), "GATE GRID"));
            gateDepths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::gateDepth), "DEPTH", colour));
            gateSmooths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::gateSmooth), "SMOOTH", colour));
            gateDepths.back()->setDescription ("How much the gate cuts the sound in its closed steps: 0 % = nothing, 100 % = fully silent.");
            gateSmooths.back()->setDescription ("How soft the gate's edges are. Short = hard chopped, longer = smoother.");

            auto onButton = std::make_unique<juce::TextButton> ("GATE ON");
            onButton->setClickingTogglesState (true);
            onButton->setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.6f));
            onButton->setTooltip ("GATE ON - Turns this layer's gate on. The gate is a separate on/off rhythm (its own grid) that chops the layer's sound; "
                                  "it does not change the notes. Click the steps to the right to open and close it.");
            gateOnAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                p.apvts, ParamID::bass (layer, BassField::gateOn), *onButton));
            gateOnButtons.push_back (std::move (onButton));
            addChildComponent (*gateGrids.back());
            addChildComponent (*gateDepths.back());
            addChildComponent (*gateSmooths.back());
            addChildComponent (*gateOnButtons.back());

        }
    }

    if (instrument)
    {
        kickGrid = std::make_unique<ChoiceComponent> (p.apvts, ParamID::rowGridKick, "GRID");
        addChildComponent (*kickGrid);
    }

    // PATTERN | GATE: vad rutnätet redigerar. Bara i instrumentet (FX-versionens mönster är redan gaten).
    for (auto* button : { &patternTab, &gateTab })
    {
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        addChildComponent (*button);
    }
    patternTab.setTooltip ("PATTERN - The notes. The grid is a simple piano roll (click steps to place notes). PIANO ROLL is the advanced view of the same notes.");
    gateTab.setTooltip ("GATE - A separate on/off rhythm that chops the layer's sound, with its own grid. It does not change the notes: "
                        "notes can sound outside the gate. Turn it on with GATE ON.");
    patternTab.onClick = [this] { showMode (StepSequencer::Mode::notes); };
    gateTab.onClick = [this] { showMode (StepSequencer::Mode::gate); };
    patternTab.setVisible (instrument);
    gateTab.setVisible (instrument);

    fillButton.setTooltip ("FILL - Fill the selected layer with a rhythm (rolling, galloping, triplets, offbeat) or clear it. "
                           "This replaces the layer's notes (and the piano roll).");
    fillButton.onClick = [this] { showFillMenu(); };
    addAndMakeVisible (fillButton);
    gateFillButton.setTooltip ("FILL - Fill the selected layer's gate steps with a rhythm (all open, alternating, offbeats ...).");
    gateFillButton.onClick = [this] { showGateFillMenu(); };
    addChildComponent (gateFillButton);

    sequencer.onLayerSelected = [this] (int layer) { showLayer (layer); };
    sequencer.onOpenPianoRoll = [this] (int layer) { sequencer.selectLayer (layer); showLayer (layer); showPianoRoll (true); };
    pianoRoll.onDeleted = [this] { showPianoRoll (false); };
    addAndMakeVisible (sequencer);
    addChildComponent (pianoRoll);

    // Pianorullen ändrar tonhöjd, så den finns bara i instrumentet.
    pianoButton.setClickingTogglesState (false);
    pianoButton.setTooltip ("PIANO ROLL - Green with a +: this row has no piano roll yet, click to add one. Blue: the row has a piano roll, click to open it "
                            "(click again to go back to the steps). The piano roll is the advanced view of the pattern, like FL Studio's: notes anywhere in time "
                            "(1/4 down to 1/64 and triplets), pitch up to 96 semitones either way, glide notes and velocity. Once you edit there, the row "
                            "shows a small piano roll instead of step buttons.");
    pianoButton.onClick = [this] { showPianoRoll (! processor.pianoRollOpen); };
    addChildComponent (pianoButton);

    selectLane (StepSequencer::Lane::velocity);
    mode = instrument && processor.selectedPatternMode == 1 ? StepSequencer::Mode::gate : StepSequencer::Mode::notes;
    sequencer.setMode (mode);
    patternTab.setToggleState (mode == StepSequencer::Mode::notes, juce::dontSendNotification);
    gateTab.setToggleState (mode == StepSequencer::Mode::gate, juce::dontSendNotification);
    {
        const auto layer = processor.selectedPatternLayer;
        const auto valid = layer == Pattern::kickLayer || juce::isPositiveAndBelow (layer, instrument ? Pattern::numLayers : 1);
        sequencer.selectLayer (valid ? layer : 0);
        showLayer (valid ? layer : 0);
    }
    updateVisibility();
    startTimerHz (8);
}

juce::String PatternModule::rowName (int row) const
{
    const auto layer = row == 0 ? Pattern::kickLayer : row - 1;
    return layer == Pattern::kickLayer ? juce::String ("KICK") : instrument ? processor.getLayerName (layer) : juce::String ("GATE");
}

void PatternModule::extendRow (int row, int beforeBars, int nowBars, bool repeat)
{
    auto& pattern = processor.pattern;
    const auto layer = row == 0 ? Pattern::kickLayer : row - 1;
    const auto oldTicks = beforeBars * Pattern::ticksPerBar, newTicks = nowBars * Pattern::ticksPerBar;
    for (int tick = oldTicks; tick < newTicks && tick < Pattern::maxTicks; ++tick)
    {
        auto& note = pattern.note (layer, tick);
        const auto& source = pattern.note (layer, tick % oldTicks);
        if (repeat && source.on.load())
        {
            note.length = source.length.load();
            note.pitch = source.pitch.load();
            note.velocity = source.velocity.load();
            note.slide = source.slide.load();
            note.on = true;
        }
        else
            note.on = false;
    }
    if (layer != Pattern::kickLayer)
    {
        const auto stepsPerBar = Pattern::bassStepsPerBar ((BassGrid) juce::jlimit (0, Pattern::numGrids - 1,
            (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateGrid))->load()));
        const auto oldSteps = beforeBars * stepsPerBar, newSteps = nowBars * stepsPerBar;
        for (int step = oldSteps; step < newSteps && step < Pattern::maxGateSteps; ++step)
            pattern.gateStep (layer, step) = repeat ? pattern.isGateStepOn (layer, step % oldSteps) : true;
    }
    processor.patternChanged();
}

void PatternModule::trimRow (int row, int bars)
{
    const auto layer = row == 0 ? Pattern::kickLayer : row - 1;
    const auto limit = bars * Pattern::ticksPerBar;
    for (int tick = 0; tick < Pattern::maxTicks; ++tick)
    {
        auto& note = processor.pattern.note (layer, tick);
        if (! note.on.load())
            continue;
        if (tick >= limit)
            note.on = false;
        else if (tick + note.length.load() > limit)
            note.length = limit - tick; // en ton som går över slutet kortas
    }
    processor.patternChanged();
}

void PatternModule::restoreChoices (const std::array<int, numPatternRows + 1>& choices)
{
    for (size_t i = 0; i < choices.size(); ++i)
        if (choices[i] >= 0)
            if (auto* parameter = processor.apvts.getParameter (i == 0 ? ParamID::patternBars : ParamID::rowBars ((int) i - 1)))
                Presets::setParameter (*parameter, parameter->convertTo0to1 ((float) choices[i]));
    const auto restored = Params::readPatternLengths (processor.apvts);
    for (int row = 0; row < numPatternRows; ++row)
        lastRowBars[(size_t) row] = restored.barsOf (row);
    lastPatternVersion = processor.patternVersion.load();
}

// Längdändringar. Förkortas en rad (själv, eller för att totalen blev kortare) tas det som inte får plats bort, med en bekräftelse först
// om något går förlorat (toner som bara är en upprepning av det som blir kvar tas bort utan fråga). Förlängs en rad får man frågan om
// upprepningen ska kopieras in i de nya takterna. Avbryter man går längderna tillbaka. Görs inget när ett helt projekt eller preset laddas.
void PatternModule::checkLengthChanges()
{
    if (confirmOpen)
        return;

    auto& apvts = processor.apvts;
    auto choiceOf = [&apvts] (const char* id) { return (int) std::round (apvts.getRawParameterValue (id)->load()); };
    std::array<int, numPatternRows + 1> choices {};
    choices[0] = choiceOf (ParamID::patternBars);
    for (int row = 0; row < numPatternRows; ++row)
        choices[(size_t) row + 1] = choiceOf (ParamID::rowBars (row));
    const auto lengthsNow = Params::readPatternLengths (apvts);
    auto& pattern = processor.pattern;
    const auto layerOf = [] (int row) { return row == 0 ? Pattern::kickLayer : row - 1; };

    const auto fresh = lastRowBars[0] < 0 || processor.patternVersion.load() != lastPatternVersion;
    std::vector<int> shrunk, grown;
    if (! fresh)
        for (int row = 0; row < numPatternRows; ++row)
        {
            if (lengthsNow.barsOf (row) > lastRowBars[(size_t) row])
                grown.push_back (row);
            else if (lengthsNow.barsOf (row) < lastRowBars[(size_t) row])
                shrunk.push_back (row);
        }

    const auto previousChoices = lastChoices;
    const auto previousBars = lastRowBars;
    std::array<int, numPatternRows> nowBars {};
    for (int row = 0; row < numPatternRows; ++row)
        nowBars[(size_t) row] = lengthsNow.barsOf (row);

    juce::Component::SafePointer<PatternModule> safeThis (this);

    // Förlängs en rad (själv, eller för att totalen blev längre) fortsätter den som förut: en rad som loopar får upprepningen kopierad in i de
    // nya takterna (det är redan det som spelades), annars töms de. Ingen fråga, och ingen längd som kan avbrytas och hoppa tillbaka.
    for (const auto row : grown)
        extendRow (row, previousBars[(size_t) row], nowBars[(size_t) row], lengthsNow.loops[(size_t) row]);

    if (! shrunk.empty())
    {
        // Vad går förlorat? Toner efter det nya slutet som inte bara är en upprepning av det som finns kvar.
        struct Loss { int row; int bars; int notes; };
        std::vector<Loss> losses;
        for (const auto row : shrunk)
        {
            const auto layer = layerOf (row);
            const auto limit = nowBars[(size_t) row] * Pattern::ticksPerBar, oldTicks = previousBars[(size_t) row] * Pattern::ticksPerBar;
            auto count = 0;
            auto differs = false;
            for (int tick = limit; tick < oldTicks && tick < Pattern::maxTicks; ++tick)
            {
                const auto& note = pattern.note (layer, tick);
                const auto& again = pattern.note (layer, tick % limit);
                if (note.on.load())
                    ++count;
                if (note.on.load() != again.on.load()
                    || (note.on.load() && (note.length.load() != again.length.load() || note.pitch.load() != again.pitch.load()
                                           || note.slide.load() != again.slide.load() || std::abs (note.velocity.load() - again.velocity.load()) > 0.001f)))
                    differs = true;
            }
            if (count > 0 && differs)
                losses.push_back ({ row, nowBars[(size_t) row], count });
        }

        if (losses.empty())
        {
            for (const auto row : shrunk)
                trimRow (row, nowBars[(size_t) row]); // bara upprepningar: inget går förlorat
        }
        else if (! confirmOpen)
        {
            juce::String message = "Shortening removes notes that do not fit any more:\n\n";
            for (const auto& loss : losses)
                message << "  " << rowName (loss.row) << ": " << loss.notes << (loss.notes == 1 ? " note" : " notes") << " after bar " << loss.bars << "\n";
            message << "\nThis can't be undone. Shorten anyway?";

            confirmOpen = true;
            juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Shorten the pattern",
                                                                                         message, "Shorten", "Cancel"),
                                          [safeThis, shrunk, nowBars, previousChoices] (int choice)
            {
                if (safeThis == nullptr)
                    return;
                if (choice == 1)
                {
                    for (const auto row : shrunk)
                        safeThis->trimRow (row, nowBars[(size_t) row]);
                    for (int row = 0; row < numPatternRows; ++row)
                        safeThis->lastRowBars[(size_t) row] = nowBars[(size_t) row];
                    safeThis->lastPatternVersion = safeThis->processor.patternVersion.load();
                }
                else
                {
                    safeThis->restoreChoices (previousChoices); // Avbryt: tillbaka till de längder som gällde innan
                }
                safeThis->confirmOpen = false;
            });
        }
    }

    if (! confirmOpen)
    {
        lastRowBars = nowBars;
        lastChoices = choices;
    }
    lastPatternVersion = processor.patternVersion.load();
}

void PatternModule::timerCallback()
{
    checkLengthChanges();

    // Instrument som lagts till eller tagits bort: raderna följer, och ett borttaget instrument kan inte vara valt.
    if (instrument && processor.activeInstruments() != lastActive)
    {
        lastActive = processor.activeInstruments();
        if (shownLayer != Pattern::kickLayer && shownLayer >= lastActive)
        {
            sequencer.selectLayer (0);
            showLayer (0);
        }
        resized();
        repaint();
    }

    // (Kickens toner spelar hela sitt ljud, så deras längd gör inget.)
    const auto custom = processor.pattern.isCustom (shownLayer);
    laneBox.setEnabled (! custom);
    laneBox.setItemEnabled (2, shownLayer != Pattern::kickLayer);
    if (shownLayer == Pattern::kickLayer && sequencer.getLane() == StepSequencer::Lane::gate)
        selectLane (StepSequencer::Lane::velocity);

    if (instrument && lastNamesVersion != processor.layerNamesVersion.load())
    {
        lastNamesVersion = processor.layerNamesVersion.load();
        repaint(); // rubriken visar det valda lagrets namn
    }
    updatePianoButton();
}

void PatternModule::showMode (StepSequencer::Mode newMode)
{
    mode = newMode;
    processor.selectedPatternMode = mode == StepSequencer::Mode::gate ? 1 : 0;
    patternTab.setToggleState (mode == StepSequencer::Mode::notes, juce::dontSendNotification);
    gateTab.setToggleState (mode == StepSequencer::Mode::gate, juce::dontSendNotification);
    sequencer.setMode (mode);
    updateVisibility();
}

void PatternModule::updatePianoButton()
{
    pianoButton.setState (! processor.pattern.isCustom (shownLayer), processor.pianoRollOpen);
}

void PatternModule::showPianoRoll (bool open)
{
    processor.pianoRollOpen = open;
    updatePianoButton();
    if (open && mode == StepSequencer::Mode::gate)
        showMode (StepSequencer::Mode::notes);
    updateVisibility();
}

void PatternModule::updateVisibility()
{
    const auto gating = instrument && mode == StepSequencer::Mode::gate;
    const auto rollOpen = ! gating && instrument && processor.pianoRollOpen;

    pianoRoll.setVisible (rollOpen);
    sequencer.setVisible (! rollOpen);
    pianoButton.setVisible (instrument && ! gating);
    fillButton.setVisible (! gating);
    gateFillButton.setVisible (gating);
    laneLabel.setVisible (! gating && ! rollOpen);
    laneBox.setVisible (! gating && ! rollOpen);

    for (size_t i = 0; i < grids.size(); ++i)
    {
        const auto shown = (int) i == shownLayer;
        grids[i]->setVisible (shown && ! gating);
        if (i < gateGrids.size())
        {
            gateGrids[i]->setVisible (shown && gating);
            gateDepths[i]->setVisible (shown && gating);
            gateSmooths[i]->setVisible (shown && gating);
            gateOnButtons[i]->setVisible (shown && gating);
        }
    }
    if (kickGrid != nullptr)
        kickGrid->setVisible (shownLayer == Pattern::kickLayer && ! gating);
    updateLengthKnobs();
    updatePianoButton();
    resized();
    repaint();
}

// ALL LENGTHS (instrumentets noters längd, alla på en gång) hör ihop med tonlängden: knappen visas bara när EDIT IN LANE står på Note length.
void PatternModule::updateLengthKnobs()
{
    const auto gating = instrument && mode == StepSequencer::Mode::gate;
    const auto wanted = ! gating && sequencer.getLane() == StepSequencer::Lane::gate && shownLayer != Pattern::kickLayer;
    for (size_t i = 0; i < lengths.size(); ++i)
        lengths[i]->setVisible (wanted && (int) i == shownLayer);
}

void PatternModule::selectLane (StepSequencer::Lane lane)
{
    sequencer.setLane (lane);
    for (size_t i = 0; i < laneOrder.size(); ++i)
        if (laneOrder[i] == lane)
            laneBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
    updateLengthKnobs();
}

void PatternModule::showLayer (int layer)
{
    shownLayer = layer;
    processor.selectedPatternLayer = layer;
    pianoRoll.setLayer (layer);
    updateVisibility();
}

void PatternModule::showKickFillMenu()
{
    // Kickens mallar, på rutnätet 1/16: en takt som upprepas över hela mönstret.
    const juce::StringArray names { "Four on the floor", "Every other beat", "Half-time (beat 3)", "Double (every 8th)", "Syncopated (x..x..x.)", "Clear" };
    const char* bars[] { "x...x...x...x...", "x.......x.......", "........x.......", "x.x.x.x.x.x.x.x.", "x..x..x.x..x..x.", "................" };

    juce::PopupMenu menu;
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    juce::Component::SafePointer<PatternModule> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fillButton), [safeThis, bars] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        auto& p = safeThis->processor;
        juce::String steps;
        for (int bar = 0; bar < Pattern::maxBars; ++bar)
            steps << bars[result - 1];
        p.pattern.setKickSteps (steps, Pattern::ticksPerStep (BassGrid::sixteenth));
        if (auto* gridParam = p.apvts.getParameter (ParamID::rowGridKick))
            Presets::setParameter (*gridParam, gridParam->convertTo0to1 ((float) BassGrid::sixteenth));
        p.patternChanged();
    });
}

void PatternModule::showFillMenu()
{
    if (shownLayer == Pattern::kickLayer)
    {
        showKickFillMenu();
        return;
    }

    juce::PopupMenu menu;
    const auto names = Pattern::templateNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    juce::Component::SafePointer<PatternModule> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fillButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        auto& p = safeThis->processor;
        const auto layer = safeThis->shownLayer;
        const auto grid = p.pattern.applyLayerTemplate (layer, (PatternTemplate) (result - 1));
        if (auto* gridParam = p.apvts.getParameter (ParamID::bass (layer, BassField::grid)))
            Presets::setParameter (*gridParam, gridParam->convertTo0to1 ((float) grid));
        p.patternChanged();
    });
}

void PatternModule::showGateFillMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "All open");
    menu.addItem (2, "Alternate (open, closed, open ...)");
    menu.addItem (3, "Offbeats (closed, open, closed ...)");
    menu.addItem (4, "Chop (every 4th step closed)");
    menu.addItem (5, "All closed");

    juce::Component::SafePointer<PatternModule> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gateFillButton), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        auto& p = safeThis->processor;
        const auto layer = safeThis->shownLayer;
        const auto gridIndex = juce::jlimit (0, Pattern::numGrids - 1, (int) p.apvts.getRawParameterValue (ParamID::bass (layer, BassField::gateGrid))->load());
        const auto stepsPerBar = Pattern::bassStepsPerBar ((BassGrid) gridIndex);

        for (int i = 0; i < Pattern::maxGateSteps; ++i)
        {
            const auto step = i % stepsPerBar;
            const auto open = result == 1 || (result == 2 && step % 2 == 0) || (result == 3 && step % 2 == 1) || (result == 4 && step % 4 != 3);
            p.pattern.gateStep (layer, i) = open;
        }
        if (auto* on = p.apvts.getParameter (ParamID::bass (layer, BassField::gateOn)))
            Presets::setParameter (*on, 1.0f); // en fylld gate är till för att höras
        p.patternChanged();
    });
}

void PatternModule::paint (juce::Graphics& g)
{
    // Rubrik för det valda lagrets inställningar.
    if (! headingBounds.isEmpty())
    {
        g.setFont (Theme::font (11.0f, true));
        g.setColour (! instrument ? Theme::gate : shownLayer == Pattern::kickLayer ? Theme::kick : Theme::layerColour (shownLayer));
        g.drawText (! instrument ? juce::String ("GATE") : shownLayer == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (shownLayer),
                    headingBounds, juce::Justification::centredLeft);
    }
}

void PatternModule::resized()
{
    auto area = getLocalBounds();

    // En smal kolumn till vänster: mönstrets längd/slut/presets, PATTERN | GATE (ovanpå varandra) och det valda lagrets inställningar för
    // läget. Vilket lager som redigeras väljs med knapparna vid namnen i raderna.
    auto column = area.removeFromLeft (134);
    area.removeFromLeft (10);

    length.setBounds (column.removeFromTop (40));
    column.removeFromTop (4);
    if (noteEnd != nullptr)
    {
        noteEnd->setBounds (column.removeFromTop (40));
        column.removeFromTop (6);
    }
    presets.setBounds (column.removeFromTop (24));
    column.removeFromTop (8);

    headingBounds = column.removeFromTop (16);
    if (instrument)
    {
        patternTab.setBounds (column.removeFromTop (24));
        column.removeFromTop (3);
        gateTab.setBounds (column.removeFromTop (24));
        column.removeFromTop (6);
    }

    // PATTERN: notrutnätet, lanerna (ovanpå varandra), tonlängd, FILL och PIANO ROLL.
    auto notesColumn = column;
    for (auto& grid : grids)
        grid->setBounds (notesColumn.getX(), notesColumn.getY(), notesColumn.getWidth(), 40);
    if (kickGrid != nullptr)
        kickGrid->setBounds (notesColumn.getX(), notesColumn.getY(), notesColumn.getWidth(), 40);
    notesColumn.removeFromTop (44);
    laneLabel.setBounds (notesColumn.removeFromTop (14));
    laneBox.setBounds (notesColumn.removeFromTop (24));
    notesColumn.removeFromTop (6);
    auto knobRow = notesColumn.removeFromTop (66);
    for (auto& lengthKnob : lengths)
        lengthKnob->setBounds (knobRow.withSizeKeepingCentre (knobRow.getWidth() * 3 / 4, knobRow.getHeight()));
    notesColumn.removeFromTop (3);
    fillButton.setBounds (notesColumn.removeFromTop (24));
    notesColumn.removeFromTop (4);
    pianoButton.setBounds (notesColumn.removeFromTop (30));

    // GATE: på/av, gatens rutnät, djup och mjukhet, FILL.
    auto gateColumn = column;
    for (auto& button : gateOnButtons)
        button->setBounds (gateColumn.getX(), gateColumn.getY(), gateColumn.getWidth(), 26);
    gateColumn.removeFromTop (30);
    for (auto& grid : gateGrids)
        grid->setBounds (gateColumn.getX(), gateColumn.getY(), gateColumn.getWidth(), 40);
    gateColumn.removeFromTop (44);
    auto gateKnobs = gateColumn.removeFromTop (72);
    const auto half = gateKnobs.getWidth() / 2;
    for (auto& depth : gateDepths)
        depth->setBounds (gateKnobs.withWidth (half));
    for (auto& smooth : gateSmooths)
        smooth->setBounds (gateKnobs.withTrimmedLeft (half));
    gateColumn.removeFromTop (4);
    gateFillButton.setBounds (gateColumn.removeFromTop (24));

    sequencer.setBounds (area);
    pianoRoll.setBounds (area);
}
