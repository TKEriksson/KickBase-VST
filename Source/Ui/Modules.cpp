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
    : processor (p), subtitle (subtitleIn), presets (p, Presets::Section::full, Theme::pattern)
{
    addAndMakeVisible (presets);
    if (! processor.isEffectVersion())
    {
        exportButton.setTooltip ("Export the kick, the bass or the full mix as a looping WAV file, "
                                 "at the current tempo and with the last played note.");
        exportButton.onClick = [this] { showExportMenu(); };
        addAndMakeVisible (exportButton);
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
    startTimerHz (15);
}

void HeaderBar::updateAiTooltip()
{
    aiButton.setTooltip (processor.isBridgeEnabled()
        ? "AI LINK is ON (instance " + processor.getBridgeId() + "). An AI assistant using the Kickbase MCP bridge can read and change this plugin and hear/measure it offline. Click to switch off."
        : "AI LINK - Off. Switch on to let an AI assistant (through the Kickbase MCP bridge, for example in Claude Desktop) read and change this plugin. Local only; nothing is sent over the internet by Kickbase.");
}

void HeaderBar::resized()
{
    presets.setBounds (14 + Theme::logoWidth (subtitle) + 24, 9, 200, getHeight() - 18);
    exportButton.setBounds (presets.getRight() + 10, 9, 76, getHeight() - 18);
    aiButton.setBounds (exportButton.getRight() + 20 + 112 + 8, 9, 34, getHeight() - 18);
}

void HeaderBar::showExportMenu()
{
    juce::PopupMenu menu;
    const char* names[] { "Full mix", "Kick", "Bass", "Kick + bass + full mix (3 files)" };
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
    const juce::String partNames[] { "Full", "Kick", "Bass" };
    const auto baseName = "Kickbase " + juce::String (bars) + " bars "
                        + juce::String (juce::roundToInt (processor.transportStatus.bpm.load())) + " BPM";
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kickbase").getChildFile ("Exports");
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
    repaint();
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& status = processor.transportStatus;
    auto area = getLocalBounds().reduced (14, 0);
    Theme::paintLogo (g, area.removeFromLeft (420), subtitle);

    // Från höger: tonart, takt.slag, tempo och en lampa som visar om DAW:en styr.
    const auto keyNote = status.keyNote.load();
    auto keyArea = area.removeFromRight (70);
    g.setFont (Theme::font (10.0f, true));
    g.setColour (Theme::textDim);
    g.drawText ("KEY", keyArea.removeFromLeft (26), juce::Justification::centredLeft);
    g.setFont (Theme::font (16.0f, true));
    g.setColour (Theme::pattern);
    g.drawText (keyNote >= 0 ? Params::keyNames()[keyNote % 12] : juce::String ("-"), keyArea, juce::Justification::centredLeft);

    g.setFont (Theme::font (14.0f, true));
    g.setColour (Theme::text);
    g.drawText (juce::String (status.bar.load()) + "." + juce::String (status.beat.load()),
                area.removeFromRight (55), juce::Justification::centredLeft);

    // Tempo i en kapselformad hårdvarupill
    auto bpmArea = area.removeFromRight (110).reduced (4, 9).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121420), bpmArea.getX(), bpmArea.getY(),
                                             juce::Colour (0xff1c1f30), bpmArea.getX(), bpmArea.getBottom(), false));
    g.fillRoundedRectangle (bpmArea, bpmArea.getHeight() * 0.5f);
    g.setColour (juce::Colour (0xff2c3148));
    g.drawRoundedRectangle (bpmArea, bpmArea.getHeight() * 0.5f, 1.0f);
    g.setFont (Theme::font (13.0f, true));
    g.setColour (Theme::text);
    g.drawText (juce::String (status.bpm.load(), 1) + " BPM", bpmArea, juce::Justification::centred);

    // Utgångsmätare (L och R, -48 till 0 dB): visar att synten faktiskt ger ljud.
    {
        auto meter = juce::Rectangle<float> ((float) exportButton.getRight() + 20.0f, 12.0f, 112.0f, (float) getHeight() - 24.0f);
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

    const auto following = status.followingHost.load();
    const auto lampColour = following ? Theme::mix : Theme::kick;
    auto lamp = area.removeFromRight (120);
    const juce::Point<float> centre { (float) lamp.getX() + 8.0f, (float) lamp.getCentreY() };
    g.setColour (lampColour.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre));
    g.setColour (lampColour.brighter (0.3f));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (centre));
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::textDim);
    g.drawText (following ? "DAW SYNC" : "FREE RUN", lamp.withTrimmedLeft (20), juce::Justification::centredLeft);
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
    : ModulePanel ("BASS " + juce::String (layerIn + 1), Theme::layerColour (layerIn), &p.apvts, ParamID::bass (layerIn, BassField::on)),
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

    envelope.addHandle ({ [this] { return juce::Point<float> (0.0015f, current.envAmount / 8.0f); },
                          nullptr, param (p, id (F::envAmount)), false, accent });
    envelope.addHandle ({ [this] {
                              const auto t = current.envDecayMs * 0.001 / 3.0;
                              return juce::Point<float> ((float) t, BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f); },
                          param (p, id (F::envDecay)), nullptr, false, accent });

    envelope.addHandle ({ [this] { return juce::Point<float> (current.attackMs * 0.001f, 1.0f); },
                          param (p, id (F::attack)), nullptr, false, Theme::text });
    envelope.addHandle ({ [this] {
                              const auto t = (current.attackMs + current.decayMs * 0.5f) * 0.001;
                              return juce::Point<float> ((float) t, ampValueAt (t)); },
                          param (p, id (F::decay)), param (p, id (F::sustain)), false, Theme::text });
    envelope.addHandle ({ [this] { return juce::Point<float> (0.18f + current.releaseMs * 0.001f, 0.0f); },
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
    groups.addRow ({ { "PITCH", { knob (id (F::octave), "OCTAVE"), knob (id (F::tune), "TUNE") } },
                     { "SUB", { knob (id (F::sub), "LEVEL"), choice (id (F::subOctave), "OCTAVE") } },
                     { "NOISE", { knob (id (F::noise), "LEVEL") } },
                     { "FILTER", { choice (id (F::filterType), "TYPE"), knob (id (F::cutoff), "CUTOFF"),
                                   knob (id (F::resonance), "RESO"), knob (id (F::keyTrack), "KEYTRK") } },
                     { "FILTER ENVELOPE", { knob (id (F::envAmount), "AMOUNT"), knob (id (F::envDecay), "DECAY"),
                                            knob (id (F::velToEnv), "VEL") } } });
    groups.addRow ({ { "VOLUME ENVELOPE", { knob (id (F::attack), "ATTACK"), knob (id (F::decay), "DECAY"),
                                            knob (id (F::sustain), "SUSTAIN"), knob (id (F::release), "RELEASE") } },
                     { "WARP ENV", { knob (id (F::warpEnv), "AMOUNT") } },
                     { "DRIVE", { knob (id (F::drive), "AMOUNT") } },
                     { "LAYER OUT", { knob (id (F::level), "LEVEL"), knob (id (F::duck), "DUCK"), knob (id (F::gate), "GATE") } } });
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
    setTooltip ("SAMPLE - The waveform of this layer's sample. Click it to hear this layer play a note. The white line marks where each note starts (START); "
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
        processor.requestAudition (1 + layer); // provlyssna lagrets ton (med samplingen)
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
float BassModule::ampValueAt (double seconds) const
{
    // Envelopen för en ton som hålls i 180 ms (ungefär ett långt sextondelssteg).
    constexpr double gateSeconds = 0.18;
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

    envelope.setBounds (area.removeFromLeft (220).reduced (0, 2));
    area.removeFromLeft (6);
    const auto rowHeight = juce::jlimit (44, 84, area.getHeight() / groups.numRows() - ControlGroups::titleHeight - 3);
    groups.layout (area, rowHeight);
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

    help.setText ("Put Kickbase FX on the mixer track of your bass synth. The synth is gated by the "
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
PatternModule::PatternModule (PluginBase& p, bool isInstrument)
    : processor (p), instrument (isInstrument),
      length (p.apvts, ParamID::patternBars, "LENGTH"),
      presets (p, Presets::Section::pattern, Theme::pattern),
      sequencer (p,
                 isInstrument ? juce::StringArray { "BASS 1", "BASS 2", "BASS 3" } : juce::StringArray { "GATE" },
                 isInstrument ? std::vector<juce::Colour> { Theme::layerColour (0), Theme::layerColour (1), Theme::layerColour (2) }
                              : std::vector<juce::Colour> { Theme::gate }),
      pianoRoll (p, { Theme::layerColour (0), Theme::layerColour (1), Theme::layerColour (2), Theme::kick })
{
    addAndMakeVisible (length);
    addAndMakeVisible (presets);
    loopButton.setClickingTogglesState (true);
    loopButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern.withAlpha (0.7f));
    loopButton.setTooltip ("LOOP ROWS - Each row (kick, bass 1-3) can have its own length (the box next to its name). On: a row that is shorter than the "
                           "pattern repeats until the longest row has played out. Off: it plays once and is silent until the pattern starts over.");
    loopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::rowLoop, loopButton);
    addAndMakeVisible (loopButton);
    if (instrument)
    {
        noteEnd = std::make_unique<ChoiceComponent> (p.apvts, ParamID::noteEnd, "NOTE END");
        addAndMakeVisible (*noteEnd);
    }

    // Lanerna för tonerna: velocity, längd och tonhöjd (glide ritas i pianorullen).
    std::vector<std::pair<juce::String, StepSequencer::Lane>> lanes { { "VEL", StepSequencer::Lane::velocity },
                                                                      { "LENGTH", StepSequencer::Lane::gate } };
    if (instrument)
        lanes.push_back ({ "PITCH", StepSequencer::Lane::pitch });

    for (const auto& [name, lane] : lanes)
    {
        auto button = std::make_unique<juce::TextButton> (name);
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        button->setTooltip (name + " - Shows the selected layer's note " + name.toLowerCase() + " in the lane under the rows. "
                            "Not available for a layer that is edited in the piano roll (set those there).");
        const auto laneCopy = lane;
        button->onClick = [this, laneCopy] { selectLane (laneCopy); };
        addAndMakeVisible (*button);
        laneButtons.emplace_back (std::move (button), lane);
    }

    const auto numLayers = instrument ? ParamID::numBassLayers : 1;
    for (int layer = 0; layer < numLayers; ++layer)
    {
        const auto colour = instrument ? Theme::layerColour (layer) : Theme::gate;
        grids.push_back (std::make_unique<ChoiceComponent> (p.apvts, ParamID::bass (layer, BassField::grid), "GRID"));
        lengths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::gate), "LENGTH", colour));
        lengths.back()->setDescription ("Scales the length of every note in this layer. Lower = shorter, punchier notes. (Set a single note's length "
                                        "with the LENGTH lane or in the piano roll.)");
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

            auto button = std::make_unique<juce::TextButton> (p.getLayerName (layer));
            button->addMouseListener (this, false);
            button->setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.55f));
            button->setColour (juce::TextButton::textColourOffId, colour);
            button->setTooltip ("BASS " + juce::String (layer + 1) + " - Edit this layer: its note grid, length, FILL, the lane and the gate settings "
                                "apply to the selected layer.");
            button->onClick = [this, layer] { sequencer.selectLayer (layer); showLayer (layer); };
            addAndMakeVisible (*button);
            layerButtons.push_back (std::move (button));
        }
    }

    if (instrument)
    {
        kickLayerButton.setColour (juce::TextButton::buttonOnColourId, Theme::kick.withAlpha (0.55f));
        kickLayerButton.setColour (juce::TextButton::textColourOffId, Theme::kick);
        kickLayerButton.setTooltip ("KICK - Edit the kick's pattern like a bass layer: its note grid, FILL, pitch and velocity in the lane, and the piano roll "
                                    "(a kick note's pitch moves the kick's tail, so you can play kick melodies).");
        kickLayerButton.onClick = [this] { sequencer.selectLayer (Pattern::kickLayer); showLayer (Pattern::kickLayer); };
        addAndMakeVisible (kickLayerButton);
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
    pianoButton.setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
    pianoButton.setTooltip ("PIANO ROLL - The advanced view of the pattern, like FL Studio's piano roll: notes anywhere in time (1/4 down to 1/64 "
                            "and triplets), pitch up to 96 semitones either way, glide notes and velocity. Once you edit there, the layer's row "
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

void PatternModule::timerCallback()
{
    // Lanerna är för stegmönstret: avstängda för ett lager som redigerats i pianorullen.
    // (Kickens toner spelar hela sitt ljud, så deras längd gör inget.)
    const auto custom = processor.pattern.isCustom (shownLayer);
    for (auto& [button, lane] : laneButtons)
        button->setEnabled (! custom && ! (shownLayer == Pattern::kickLayer && lane == StepSequencer::Lane::gate));
    if (shownLayer == Pattern::kickLayer && sequencer.getLane() == StepSequencer::Lane::gate)
        selectLane (StepSequencer::Lane::velocity);

    if (instrument && lastNamesVersion != processor.layerNamesVersion.load())
    {
        lastNamesVersion = processor.layerNamesVersion.load();
        for (size_t i = 0; i < layerButtons.size(); ++i)
            layerButtons[i]->setButtonText (processor.getLayerName ((int) i));
        repaint();
    }
}

void PatternModule::mouseDoubleClick (const juce::MouseEvent& event)
{
    for (size_t i = 0; i < layerButtons.size(); ++i)
        if (event.originalComponent == layerButtons[i].get())
            sequencer.renameLayer ((int) i);
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

void PatternModule::showPianoRoll (bool open)
{
    processor.pianoRollOpen = open;
    pianoButton.setToggleState (open, juce::dontSendNotification);
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
    for (auto& [button, lane] : laneButtons)
        button->setVisible (! gating && ! rollOpen);

    for (size_t i = 0; i < grids.size(); ++i)
    {
        const auto shown = (int) i == shownLayer;
        grids[i]->setVisible (shown && ! gating);
        lengths[i]->setVisible (shown && ! gating);
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
    resized();
    repaint();
}

void PatternModule::selectLane (StepSequencer::Lane lane)
{
    sequencer.setLane (lane);
    for (auto& [button, buttonLane] : laneButtons)
        button->setToggleState (buttonLane == lane, juce::dontSendNotification);
}

void PatternModule::showLayer (int layer)
{
    shownLayer = layer;
    processor.selectedPatternLayer = layer;
    pianoRoll.setLayer (layer);
    for (size_t i = 0; i < layerButtons.size(); ++i)
        layerButtons[i]->setToggleState ((int) i == layer, juce::dontSendNotification);
    kickLayerButton.setToggleState (layer == Pattern::kickLayer, juce::dontSendNotification);
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

    // Två smala kolumner till vänster: mönstrets längd/slut/presets och vilket lager som redigeras, sedan
    // PATTERN | GATE och det valda lagrets inställningar för läget.
    auto first = area.removeFromLeft (130);
    area.removeFromLeft (8);
    auto second = area.removeFromLeft (150);
    area.removeFromLeft (12);

    length.setBounds (first.removeFromTop (40));
    first.removeFromTop (4);
    if (noteEnd != nullptr)
    {
        noteEnd->setBounds (first.removeFromTop (40));
        first.removeFromTop (6);
    }
    loopButton.setBounds (first.removeFromTop (24));
    first.removeFromTop (6);
    presets.setBounds (first.removeFromTop (24));
    first.removeFromTop (12);
    if (instrument)
    {
        kickLayerButton.setBounds (first.removeFromTop (24));
        first.removeFromTop (4);
    }
    for (auto& button : layerButtons)
    {
        button->setBounds (first.removeFromTop (24));
        first.removeFromTop (4);
    }

    headingBounds = second.removeFromTop (16);
    if (instrument)
    {
        auto tabs = second.removeFromTop (26);
        patternTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2 - 2));
        gateTab.setBounds (tabs.withTrimmedLeft (4));
        second.removeFromTop (6);
    }

    // PATTERN: notrutnätet, lanerna, tonlängd, FILL och PIANO ROLL.
    auto notesColumn = second;
    for (auto& grid : grids)
        grid->setBounds (notesColumn.getX(), notesColumn.getY(), notesColumn.getWidth(), 40);
    if (kickGrid != nullptr)
        kickGrid->setBounds (notesColumn.getX(), notesColumn.getY(), notesColumn.getWidth(), 40);
    notesColumn.removeFromTop (44);
    for (size_t i = 0; i < laneButtons.size(); i += 2)
    {
        auto row = notesColumn.removeFromTop (24);
        laneButtons[i].first->setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        if (i + 1 < laneButtons.size())
            laneButtons[i + 1].first->setBounds (row.withTrimmedLeft (4));
        notesColumn.removeFromTop (4);
    }
    notesColumn.removeFromTop (4);
    auto knobRow = notesColumn.removeFromTop (72);
    for (auto& lengthKnob : lengths)
        lengthKnob->setBounds (knobRow.withSizeKeepingCentre (knobRow.getWidth() / 2, knobRow.getHeight()));
    notesColumn.removeFromTop (4);
    fillButton.setBounds (notesColumn.removeFromTop (24));
    notesColumn.removeFromTop (4);
    pianoButton.setBounds (notesColumn.removeFromTop (28));

    // GATE: på/av, gatens rutnät, djup och mjukhet, FILL.
    auto gateColumn = second;
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
