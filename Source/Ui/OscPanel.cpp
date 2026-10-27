#include "OscPanel.h"
#include "HarmonicEditor.h"

WavetableView::WavetableView (PluginBase& p, int layerIn, int oscIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), osc (oscIn), accent (accentIn)
{
    setMouseCursor (speakerCursor);
    positionIndex = processor.modulator.indexOf (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aPos)));
    setTooltip ("OSC " + juce::String (osc + 1)
                + " - The wavetable: all its waveforms stacked from back to front. CLICK AND HOLD it to hear just this oscillator for as long as you hold (even if it is off; it loops if the sound ends). "
                  "POS chooses which waveform plays (drag an LFO onto POS to sweep through them); WARP bends the waveform you see in front.");
    startVisualTimer (Settings::Visual::other, 30);
    timerCallback(); // rätt tabell redan i första bilden (annars ritades Basic tills timern gått en gång)
}

float WavetableView::raw (InstrumentField field) const
{
    return processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, field)))->load();
}

void WavetableView::timerCallback()
{
    followVisualRate();
    if (! isShowing())
        return; // dold flik: ingen uträkning
    // Rita om när tabellen, positionen (med LFO) eller warpen ändras.
    const auto position = juce::jlimit (0.0f, 1.0f, raw (InstrumentField::aPos) + processor.modulator.currentOffset (positionIndex));
    const auto table = (int) raw (InstrumentField::aTable);
    const auto warpMode = (int) raw (InstrumentField::aWarp);
    const auto warp = raw (InstrumentField::aWarpAmount);
    const auto ready = table == Wavetables::userIndex() || Wavetables::isReady (table);
    const auto morph = processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscMorphField (osc)))->load() >= 0.5f;
    if (position != lastPosition || table != lastTable || warpMode != lastWarpMode || warp != lastWarp || ready != lastReady || morph != lastMorph)
    {
        lastMorph = morph;
        lastPosition = position;
        lastTable = table;
        lastWarpMode = warpMode;
        lastWarp = warp;
        lastReady = ready;
        repaint();
    }
}

void WavetableView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const Wavetable* userTable = lastTable == Wavetables::userIndex() ? processor.userTable (layer, osc) : nullptr;
    const auto& table = userTable != nullptr ? *userTable : Wavetables::get (lastTable < 0 || lastTable == Wavetables::userIndex() ? 0 : lastTable);
    const auto area = bounds.reduced (8.0f, 10.0f);

    if (display != Display::wave)
    {
        drawCurrent (g, table, bounds.reduced (8.0f, 18.0f));
        g.setFont (Theme::font (9.5f, true));
        g.setColour (Theme::textDim);
        g.drawText (table.getName().toUpperCase(), bounds.reduced (6.0f, 4.0f), juce::Justification::topLeft);
        g.setColour (accent.withAlpha (0.25f));
        g.fillRoundedRectangle (modeRect(), 3.0f);
        g.setColour (Theme::text);
        g.drawText (display == Display::scope ? "SCOPE" : "SPECTRUM", modeRect(), juce::Justification::centred);
        if (hover)
        {
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 1.5f);
        }
        return;
    }

    // Varje frame förskjuts uppåt-höger: de bakre är mindre och mörkare.
    constexpr float depthX = 0.28f, depthY = 0.42f;
    const auto frontWidth = area.getWidth() * (1.0f - depthX);
    const auto frontHeight = area.getHeight() * (1.0f - depthY);
    auto frameOrigin = [&] (float z) // z: 0 = längst fram, 1 = längst bak
    {
        return juce::Point<float> (area.getX() + z * area.getWidth() * depthX, area.getBottom() - frontHeight - z * area.getHeight() * depthY);
    };

    auto pathFor = [&] (const std::function<float (double)>& wave, float z, float scale)
    {
        juce::Path path;
        const auto origin = frameOrigin (z);
        const auto width = frontWidth * scale, height = frontHeight * scale;
        constexpr int points = 96;
        for (int i = 0; i <= points; ++i)
        {
            const auto phase = (double) i / points;
            const juce::Point<float> point { origin.x + (float) phase * width, origin.y + height * 0.5f - wave (phase) * height * 0.45f };
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        return path;
    };

    // Bakifrån och fram, så att de främre ritas över.
    for (int f = Wavetable::numFrames - 1; f >= 0; --f)
    {
        const auto z = (float) f / (Wavetable::numFrames - 1);
        const auto* data = table.displayFrame (f);
        auto wave = [data] (double phase) { return data[juce::jlimit (0, Wavetable::frameSize - 1, (int) (phase * Wavetable::frameSize))]; };
        g.setColour (accent.withAlpha (0.10f + 0.12f * (1.0f - z)));
        g.strokePath (pathFor (wave, z, 1.0f), juce::PathStrokeType (1.0f));
    }

    // Vågformen som spelas: positionen och warpen (samma funktioner som synten använder).
    const auto position = std::max (0.0f, lastPosition);
    const auto mode = (WarpMode) std::max (0, lastWarpMode);
    const auto warpAmount = std::max (0.0f, lastWarp);
    auto current = [&] (double phase)
    {
        const auto warped = InstrumentSynth::warpPhase (mode == WarpMode::fm ? WarpMode::off : mode, warpAmount, phase, 0.0f);
        return table.sample (position, warped, 512.0, lastMorph);
    };
    const auto z = lastMorph ? position : (float) table.stepFrame (position) / (Wavetable::numFrames - 1); // utan Morph ligger kurvan på en av vågformerna
    const auto path = pathFor (current, z, 1.0f);
    g.setColour (accent.withAlpha (0.3f));
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent.brighter (0.3f));
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (Theme::font (9.5f, true));
    g.setColour (Theme::textDim);
    g.drawText (table.getName().toUpperCase(), bounds.reduced (6.0f, 4.0f), juce::Justification::topLeft);

    g.setColour (Theme::textDim.withAlpha (0.7f));
    g.drawText ("WAVE", modeRect(), juce::Justification::centred);

    // Glöd när musen är över bilden (klicka för att lyssna).
    if (hover)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 4.0f);
        g.setColour (accent.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 1.5f);
    }
}

void WavetableView::drawCurrent (juce::Graphics& g, const Wavetable& table, juce::Rectangle<float> area)
{
    const auto position = std::max (0.0f, lastPosition);
    const auto mode = (WarpMode) std::max (0, lastWarpMode);
    const auto warpAmount = std::max (0.0f, lastWarp);
    auto current = [&] (double phase)
    {
        const auto warped = InstrumentSynth::warpPhase (mode == WarpMode::fm ? WarpMode::off : mode, warpAmount, phase, 0.0f);
        return table.sample (position, warped, 512.0, lastMorph);
    };

    juce::Path path;
    if (display == Display::scope)
    {
        constexpr int points = 256;
        for (int i = 0; i <= points; ++i) // två perioder, som en oscilloskopbild
        {
            const auto x = area.getX() + area.getWidth() * (float) i / points;
            const auto y = area.getCentreY() - current (std::fmod (2.0 * i / points, 1.0)) * area.getHeight() * 0.45f;
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        g.setColour (Theme::textDim.withAlpha (0.3f));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    }
    else
    {
        std::array<float, 1024> data {}; // FFT-ens in- och utdata (2 * storleken)
        for (int i = 0; i < 512; ++i)
            data[(size_t) i] = (float) current ((double) i / 512.0);
        fft.performFrequencyOnlyForwardTransform (data.data(), true);
        constexpr int bars = 48; // övertoner 1 ... 48
        const auto barWidth = area.getWidth() / bars;
        for (int h = 1; h <= bars; ++h)
        {
            const auto db = juce::Decibels::gainToDecibels (data[(size_t) h] * 2.0f / 512.0f, -90.0f);
            const auto height = area.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.fillRect (area.getX() + (float) (h - 1) * barWidth + 0.5f, area.getBottom() - height, std::max (1.0f, barWidth - 1.0f), height);
        }
        return;
    }
    g.setColour (accent.withAlpha (0.3f));
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent.brighter (0.3f));
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WavetableView::mouseDown (const juce::MouseEvent& e)
{
    if (modeRect().contains (e.position)) // MODE-etiketten växlar WAVE / SCOPE / SPECTRUM; resten av vyn lyssnar i alla lägen
    {
        display = display == Display::wave ? Display::scope : display == Display::scope ? Display::spectrum : Display::wave;
        repaint();
        return;
    }
    if (e.mods.isPopupMenu()) // PH2: högerklick öppnar övertonsredigeraren
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Edit harmonics...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int result)
        {
            if (result == 1)
                juce::CallOutBox::launchAsynchronously (std::make_unique<HarmonicEditor> (processor, layer, osc, accent), getScreenBounds(), nullptr);
        });
        return;
    }
    hold.start (PluginBase::heldSoloAudition (layer, osc)); // bara den här oscillatorn, så länge musen hålls (loopar om ljudet tar slut)
}

//==============================================================================
OscPanel::OscPanel (PluginBase& p, int layerIn, int oscIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), osc (oscIn), accent (accentIn),
      warp (p.apvts, ParamID::bass (layer, ParamID::oscField (oscIn, InstrumentField::aWarp)), "WARP"),
      view (p, layer, oscIn, accentIn)
{
    tableLabel.setText ("TABLE", juce::dontSendNotification);
    tableLabel.setFont (Theme::font (11.0f, true));
    tableLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (tableLabel);

    prevPresetButton.setTooltip ("Previous oscillator preset in the list (Presets/Oscillators). Only this oscillator changes.");
    nextPresetButton.setTooltip ("Next oscillator preset in the list (Presets/Oscillators). Only this oscillator changes.");
    presetButton.setTooltip ("OSC PRESET - Load a saved oscillator: wavetable and position, warp, pitch, unison, phase and the other knobs of THIS oscillator. "
                             "It never touches the other oscillators, the sample, the filters or the effects.");
    savePresetButton.setTooltip ("Save this oscillator as a preset (choose a name and a category folder).");
    resetOscButton.setTooltip ("RESET - Put this oscillator back to its starting values (its on/off stays as it is).");
    prevPresetButton.onClick = [this] { stepPreset (-1); };
    nextPresetButton.onClick = [this] { stepPreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    savePresetButton.onClick = [this] { askForNameAndSave(); };
    resetOscButton.onClick = [this]
    {
        OscPresets::reset (processor.apvts, layer, osc);
        currentPreset = juce::File();
        presetButton.setButtonText ("PRESET");
    };
    for (auto* b : { &prevPresetButton, &nextPresetButton, &presetButton, &savePresetButton, &resetOscButton })
        addAndMakeVisible (*b);
    tableButton.setTooltip ("TABLE - The wavetable this oscillator plays: every waveform in it is one step along POS. Click to choose from categories "
                            "(Analog, Filter, FM, Digital, Vocal, Psy ...), your own library (Documents/Kickbase/Wavetables, e.g. the 4000+ AKWF waveforms) "
                            "or a WAV file/folder.");
    tableButton.onClick = [this] { showTableMenu(); };
    addAndMakeVisible (tableButton);
    addAndMakeVisible (warp);
    addAndMakeVisible (view);

    filterSendButton.setTooltip ("SEND THROUGH FILTER - On: this oscillator goes through Filter 1 and Filter 2 (set on the FILTER tab). "
                                 "Off: it bypasses both filters and goes straight to the amp.");
    filterSendButton.setButtonText ("FILTER");
    filterSendButton.getProperties().set ("labelled", true);
    filterSendButton.setColour (juce::ToggleButton::tickColourId, accent);
    filterSendButton.setColour (juce::ToggleButton::textColourId, Theme::textDim);
    addAndMakeVisible (filterSendButton);
    filterSendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamID::bass (layer, (InstrumentField) ((int) InstrumentField::filterOsc1 + osc)), filterSendButton);

    // Klassiska vågformer med ett klick (som i FL:s 3x Osc), utan att behöva förstå wavetables.
    const char* shapeNames[] { "SIN", "TRI", "SAW", "SQR" };
    const char* shapeHelp[] { "Pure sine wave.", "Triangle wave: soft, with only odd overtones.",
                              "Saw wave: bright and buzzy, all overtones. The classic bass-synth sound.",
                              "Square wave: hollow, with only odd overtones. Use WARP > PWM to change the pulse width." };
    for (size_t i = 0; i < shapeButtons.size(); ++i)
    {
        auto& button = shapeButtons[i];
        button.setButtonText (shapeNames[i]);
        button.setTooltip (juce::String ("CLASSIC WAVE - ") + shapeHelp[i] + " (Wavetables are in the TABLE list.)");
        button.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.6f));
        button.onClick = [this, i] { chooseTable (Wavetables::classicIndex ((int) i)); };
        addAndMakeVisible (button);
    }

    wavButton.setTooltip ("LOAD WAV - Use a WAV file as this oscillator's wavetable (like Serum). A file of 2048-sample cycles becomes up to 16 "
                          "waveforms that POS sweeps through; a short file is used as one single waveform. Saved in the project.");
    wavButton.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.6f));
    wavButton.onClick = [this]
    {
        chooseWavFile();
    };
    addAndMakeVisible (wavButton);

    morphButton.setTooltip ("MORPH - On: POS fades smoothly from one waveform of the table to the next. Off: POS jumps between the table's waveforms "
                            "(Basic has four: sine, triangle, saw, square), like stepping through them. Steps jump; fast modulation of POS can click.");
    morphButton.setClickingTogglesState (true);
    morphButton.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.6f));
    morphAttachment = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter (ParamID::bass (layer, ParamID::oscMorphField (osc))), morphButton);
    addAndMakeVisible (morphButton);

    if (osc == 0)
    {
        ringBox.addItemList ({ "RING: OSC 1 x OSC 2", "RING: OSC 1 x OSC 3", "RING: OSC 1 x OSC 2 x OSC 3" }, 1);
        ringBox.setTooltip ("RING MODULATION - OSC 1 is multiplied by the oscillator(s) you choose here: OSC 2, OSC 3 or both (OSC 1 x OSC 2 x OSC 3). "
                            "AMOUNT blends from plain OSC 1 (0) to pure ring modulation (100 %). The chosen oscillator must be ON; "
                            "it does not have to be loud (its LEVEL only sets what you hear of it directly, the ring uses the full wave). Sums and differences of the pitches give the metallic, bell-like sound.");
        ringAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            p.apvts, ParamID::bass (layer, InstrumentField::ringSource), ringBox);
        addChildComponent (ringBox);
    }
    startTimerHz (10);
    timerCallback(); // tabellens namn syns direkt

    using F = InstrumentField;
    const std::pair<F, const char*> specs[] { { F::aPos, "POS" }, { F::aLevel, "LEVEL" }, { F::aSemi, "SEMI" }, { F::aFine, "FINE" },
                                              { F::aPhase, "PHASE" }, { F::aUnison, "UNISON" }, { F::aDetune, "DETUNE" },
                                              { F::aBlend, "BLEND" }, { F::aPan, "PAN" }, { F::aStereo, "STEREO" },
                                              { F::aRandom, "RAND" } };
    for (const auto& [field, name] : specs)
    {
        knobs.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, ParamID::oscField (osc, field)), name, accent));
        addAndMakeVisible (*knobs.back());
    }
    warpKnob = std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, ParamID::oscField (osc, F::aWarpAmount)), "AMOUNT", accent);
    warpKnob->setLabelAbove (true); // som WARP-menyns rubrik: namnet ovanför
    // Fas S: på OSC 1 visar en lila ring hur långt WARP ENV driver mängden (grundmängden + WARP ENV AMOUNT); syns bara när WARP ENV är över noll.
    if (osc == 0)
        warpKnob->setLiveMeter ([&p, layer = layerIn]
        {
            auto value = [&p, layer] (F field) { return p.apvts.getRawParameterValue (ParamID::bass (layer, field))->load(); };
            const auto env = value (F::warpEnv);
            return env > 0.0f ? juce::jlimit (0.0f, 1.0f, value (F::aWarpAmount) + env) : 0.0f;
        }, Theme::gate);
    addAndMakeVisible (*warpKnob);
}

juce::String OscPanel::currentTableName() const
{
    const auto current = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable)))->load();
    if (current == Wavetables::userIndex())
        return processor.getUserTableName (layer, osc).isEmpty() ? juce::String ("User WAV") : processor.getUserTableName (layer, osc);
    return Wavetables::names()[juce::jlimit (0, Wavetables::listSize() - 1, current)];
}

void OscPanel::timerCallback()
{
    const auto current = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable)))->load();
    const auto name = currentTableName();
    if (current != lastTable || name != tableButton.getButtonText())
    {
        lastTable = current;
        tableButton.setButtonText (name);
        for (size_t i = 0; i < shapeButtons.size(); ++i)
            shapeButtons[i].setToggleState (current == Wavetables::classicIndex ((int) i), juce::dontSendNotification);
    }
    if (osc == 0)
    {
        const auto ring = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aWarp)))->load() == (int) WarpMode::ring;
        if (ring != ringShown)
        {
            ringShown = ring;
            ringBox.setVisible (ring);
            for (auto& button : shapeButtons)
                button.setVisible (! ring);
        }
    }
}

void OscPanel::chooseWavFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a WAV file for the wavetable", Wavetables::libraryFolder(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult().existsAsFile())
                                  loadFromLibrary (fc.getResult());
                          });
}

void OscPanel::loadFromLibrary (const juce::File& fileOrFolder)
{
    if (processor.loadUserTable (layer, osc, fileOrFolder))
        view.repaint();
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wavetable", "Could not read that as a wavetable.");
}

// Menyn: kategorierna med de inbyggda tabellerna, och biblioteket (mappar och WAV-filer i Dokument/Kickbase/Wavetables).
void OscPanel::showTableMenu()
{
    juce::PopupMenu menu;
    const auto current = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable)))->load();
    const auto names = Wavetables::names();

    juce::StringArray categories;
    for (int choice = 0; choice < Wavetables::listSize(); ++choice)
        if (choice != Wavetables::userIndex())
            categories.addIfNotAlreadyThere (Wavetables::categoryOf (choice));

    for (const auto& category : categories)
    {
        juce::PopupMenu sub;
        auto containsCurrent = false;
        for (int choice = 0; choice < Wavetables::listSize(); ++choice)
            if (choice != Wavetables::userIndex() && Wavetables::categoryOf (choice) == category)
            {
                sub.addItem (choice + 1, names[choice], true, choice == current);
                containsCurrent = containsCurrent || choice == current;
            }
        menu.addSubMenu (category.toUpperCase(), sub, true, nullptr, containsCurrent);
    }

    menuFiles.clear();
    const auto library = Wavetables::libraryFolder();
    menu.addSeparator();
    std::function<void (juce::PopupMenu&, const juce::File&, int)> addFolder = [&] (juce::PopupMenu& into, const juce::File& folder, int depth)
    {
        auto wavs = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
        std::sort (wavs.begin(), wavs.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        auto folders = folder.findChildFiles (juce::File::findDirectories, false);
        std::sort (folders.begin(), folders.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });

        if (wavs.size() >= 2)
        {
            menuFiles.push_back (folder);
            into.addItem (999 + (int) menuFiles.size(), "[ Whole folder as one wavetable ]");
            into.addSeparator();
        }
        for (const auto& subfolder : folders)
        {
            if (depth >= 4)
                break;
            juce::PopupMenu sub;
            addFolder (sub, subfolder, depth + 1);
            into.addSubMenu (subfolder.getFileName(), sub);
        }
        for (const auto& wav : wavs)
        {
            menuFiles.push_back (wav);
            into.addItem (999 + (int) menuFiles.size(), wav.getFileNameWithoutExtension());
        }
    };

    if (library.isDirectory())
    {
        juce::PopupMenu libraryMenu;
        addFolder (libraryMenu, library, 0);
        menu.addSubMenu ("LIBRARY (your waves)", libraryMenu);
    }
    menu.addItem (900, "Load WAV file ...");
    menu.addItem (901, "Open the library folder");
    if (current == Wavetables::userIndex())
        menu.addItem (Wavetables::userIndex() + 1, currentTableName() + " (loaded)", true, true);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (tableButton).withMinimumWidth (tableButton.getWidth()),
                        [safe = juce::Component::SafePointer<OscPanel> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == 900)
                                safe->chooseWavFile();
                            else if (result == 901)
                            {
                                Wavetables::libraryFolder().createDirectory();
                                Wavetables::libraryFolder().startAsProcess();
                            }
                            else if (result >= 1000)
                            {
                                if ((size_t) (result - 1000) < safe->menuFiles.size())
                                    safe->loadFromLibrary (safe->menuFiles[(size_t) (result - 1000)]);
                            }
                            else if (result != Wavetables::userIndex() + 1)
                                safe->chooseTable (result - 1);
                        });
}

void OscPanel::chooseTable (int index)
{
    Wavetables::ensure (index); // bygg den nu (några hundra ms som mest) så att det låter direkt
    if (auto* parameter = processor.apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable))))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
        parameter->endChangeGesture();
    }
}

void OscPanel::paint (juce::Graphics& g)
{
    const auto frame = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.3f));
    g.fillRoundedRectangle (frame, 5.0f);
    g.setColour (Theme::panelBorder.withAlpha (0.7f));
    g.drawRoundedRectangle (frame.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (accent);
    g.setFont (Theme::font (12.0f, true));
    g.drawText ("OSC " + juce::String (osc + 1), getLocalBounds().reduced (8, 4).withHeight (16), juce::Justification::centredLeft);
}

//==============================================================================
// OSC-presets
void OscPanel::loadPreset (const juce::File& file)
{
    if (OscPresets::load (processor.apvts, layer, osc, file))
    {
        currentPreset = file;
        presetButton.setButtonText (file.getFileNameWithoutExtension().toUpperCase());
    }
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Oscillator preset", "Could not read that oscillator preset.");
}

void OscPanel::stepPreset (int direction)
{
    OscPresets::ensureFactory();
    const auto files = OscPresets::list();
    if (files.isEmpty())
        return;
    const auto at = files.indexOf (currentPreset);
    const auto next = at < 0 ? (direction > 0 ? 0 : files.size() - 1) : (at + direction + files.size()) % files.size();
    loadPreset (files[next]);
}

void OscPanel::showPresetMenu()
{
    OscPresets::ensureFactory();
    const auto files = OscPresets::list();
    const auto root = OscPresets::folder();
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> categories;
    for (int i = 0; i < files.size(); ++i)
    {
        const auto parent = files[i].getParentDirectory();
        const auto category = parent == root ? juce::String() : parent.getFileName();
        if (category.isEmpty())
            menu.addItem (100 + i, files[i].getFileNameWithoutExtension(), true, files[i] == currentPreset);
        else
            categories[category].addItem (100 + i, files[i].getFileNameWithoutExtension(), true, files[i] == currentPreset);
    }
    for (auto& [name, sub] : categories)
        menu.addSubMenu (name, sub);
    if (files.isEmpty())
        menu.addItem (-1, "(no oscillator presets yet - use SAVE)", false);
    menu.addSeparator();
    menu.addItem (1, "Open the preset folder");

    juce::Component::SafePointer<OscPanel> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [safeThis, files] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        if (result == 1)
        {
            OscPresets::folder().createDirectory();
            OscPresets::folder().startAsProcess();
        }
        else if (juce::isPositiveAndBelow (result - 100, files.size()))
            safeThis->loadPreset (files[result - 100]);
    });
}

void OscPanel::askForNameAndSave()
{
    auto* window = new juce::AlertWindow ("Save oscillator preset", "Name and category (a folder):", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    saveDialog = window;
    window->addTextEditor ("name", "My oscillator", "Name:");
    window->addTextEditor ("category", "User", "Category:");
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<OscPanel> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;
        const auto name = juce::File::createLegalFileName (window->getTextEditorContents ("name").trim());
        const auto category = juce::File::createLegalFileName (window->getTextEditorContents ("category").trim());
        auto file = OscPresets::folder();
        if (category.isNotEmpty())
            file = file.getChildFile (category);
        file = file.getChildFile (name + ".oscpreset");
        if (name.isEmpty() || ! OscPresets::save (safeThis->processor.apvts, safeThis->layer, safeThis->osc, file))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save", "Check that the name is valid.");
        else
        {
            safeThis->currentPreset = file;
            safeThis->presetButton.setButtonText (name.toUpperCase());
        }
    }), true);
}

void OscPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    {
        const auto titleRow = area.removeFromTop (16);
        auto strip = titleRow.withTrimmedLeft (area.getWidth() / 3);
        filterSendButton.setBounds (strip.removeFromRight (60));
        strip.removeFromRight (4);
        resetOscButton.setBounds (strip.removeFromRight (46).reduced (1, 0));
        savePresetButton.setBounds (strip.removeFromRight (42).reduced (1, 0));
        nextPresetButton.setBounds (strip.removeFromRight (22).reduced (1, 0));
        prevPresetButton.setBounds (strip.removeFromRight (22).reduced (1, 0));
        presetButton.setBounds (strip.removeFromRight (juce::jmin (160, strip.getWidth())).reduced (1, 0));
    }

    // Vänster: tabell, warp och bilden. Höger: rattarna i två rader över hela höjden.
    // WARP-ratten ligger i en egen kolumn till höger om menyerna och formknapparna (de två översta raderna), så att den inte trycker ner vågbilden.
    constexpr int warpColumn = 64;
    auto left = area.removeFromLeft (juce::jmin (300, area.getWidth() / 3) + warpColumn);
    auto top = left.removeFromTop (40 + 4 + 20);
    warpKnob->setBounds (top.removeFromRight (warpColumn));
    auto choices = top.removeFromTop (40);
    auto tableArea = choices.removeFromLeft (choices.getWidth() / 2 + 10).withTrimmedRight (3);
    tableLabel.setBounds (tableArea.removeFromTop (14));
    tableButton.setBounds (tableArea.removeFromTop (24));
    warp.setBounds (choices.withTrimmedLeft (3));
    top.removeFromTop (4);
    auto shapes = top;
    morphButton.setBounds (shapes.removeFromRight (shapes.getWidth() / 4 + 4).reduced (1, 0)); // längst till höger: MORPH är inte en vågform
    wavButton.setBounds (shapes.removeFromRight (shapes.getWidth() / 5 + 2).reduced (1, 0));
    ringBox.setBounds (shapes.reduced (1, 0));
    for (auto& button : shapeButtons)
        button.setBounds (shapes.removeFromLeft (shapes.getWidth() / ((int) shapeButtons.size() - (int) (&button - shapeButtons.data()))).reduced (1, 0));
    left.removeFromTop (4);
    view.setBounds (left);
    area.removeFromLeft (4);

    const auto rowHeight = area.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        const auto width = rowArea.getWidth() / 6;
        for (int i = 0; i < 6 && (size_t) (row * 6 + i) < knobs.size(); ++i)
            knobs[(size_t) (row * 6 + i)]->setBounds (rowArea.removeFromLeft (width));
    }
}
