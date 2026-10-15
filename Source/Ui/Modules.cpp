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
    startTimerHz (15);
}

void HeaderBar::resized()
{
    presets.setBounds (14 + Theme::logoWidth (subtitle) + 24, 9, 200, getHeight() - 18);
    exportButton.setBounds (presets.getRight() + 10, 9, 76, getHeight() - 18);
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

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& status = processor.transportStatus;
    auto area = getLocalBounds().reduced (14, 0);
    Theme::paintLogo (g, area.removeFromLeft (400), subtitle);

    // Från höger: tonart, takt.slag, tempo och en lampa som visar om DAW:en styr.
    const auto keyNote = status.keyNote.load();
    auto keyArea = area.removeFromRight (70);
    g.setFont (Theme::font (10.0f, true));
    g.setColour (Theme::textDim);
    g.drawText ("KEY", keyArea.removeFromLeft (26), juce::Justification::centredLeft);
    g.setFont (Theme::font (20.0f, true));
    g.setColour (Theme::pattern);
    g.drawText (keyNote >= 0 ? Params::keyNames()[keyNote % 12] : juce::String ("-"), keyArea, juce::Justification::centredLeft);

    g.setFont (Theme::font (14.0f, true));
    g.setColour (Theme::text);
    g.drawText (juce::String (status.bar.load()) + "." + juce::String (status.beat.load()),
                area.removeFromRight (60), juce::Justification::centredLeft);
    g.drawText (juce::String (status.bpm.load(), 1) + " BPM", area.removeFromRight (100), juce::Justification::centredLeft);

    const auto following = status.followingHost.load();
    const auto lampColour = following ? Theme::mix : Theme::kick;
    auto lamp = area.removeFromRight (120);
    const juce::Point<float> centre { (float) lamp.getX() + 8.0f, (float) lamp.getCentreY() };
    g.setColour (lampColour.withAlpha (status.patternActive.load() ? 0.35f : 0.12f));
    g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (centre));
    g.setColour (lampColour);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre));
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::textDim);
    g.drawText (following ? "DAW SYNC" : "FREE RUN", lamp.withTrimmedLeft (22), juce::Justification::centredLeft);
}

//==============================================================================
KickModule::KickModule (PluginBase& p)
    : ModulePanel ("KICK", Theme::kick, &p.apvts, ParamID::kickOn), processor (p), refs (p.apvts),
      keyMode (p.apvts, ParamID::kickKeyFollow, "KEY"),
      presets (p, Presets::Section::kick, Theme::kick)
{
    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
    keyMode.box.setTooltip ("Follow: the kick is tuned to the note's key. Fixed: it stays on C1 + TUNE whatever the note.");
    addAndMakeVisible (keyMode);
    envelope.onBeforePaint = [this] { current = refs.readKick (0); };

    envelope.addCurve ({ [this] (double t) { return KickEngine::pitchSemitonesAt (current, t) / 72.0f; },
                         Theme::text.withAlpha (0.8f), false, "PITCH" });
    envelope.addCurve ({ [this] (double t) { return KickEngine::amplitudeAt (current, t); }, Theme::kick, true, "AMP" });

    const auto pitchColour = Theme::text;
    envelope.addHandle ({ [this] { return juce::Point<float> (0.0f, KickEngine::pitchSemitonesAt (current, 0.0) / 72.0f); },
                          nullptr, param (p, ParamID::kickPitch), false, pitchColour });
    envelope.addHandle ({ [this] { return juce::Point<float> (current.pitchTimeMs * 0.001f, 0.0f); },
                          param (p, ParamID::kickPitchTime), nullptr, false, pitchColour });
    envelope.addHandle ({ [this] {
                              const auto t = current.pitchTimeMs * 0.0005;
                              return juce::Point<float> ((float) t, KickEngine::pitchSemitonesAt (current, t) / 72.0f); },
                          nullptr, param (p, ParamID::kickPitchCurve), true, pitchColour });

    envelope.addHandle ({ [this] { return juce::Point<float> ((current.attackMs + current.holdMs) * 0.001f, 1.0f); },
                          param (p, ParamID::kickHold), nullptr, false, Theme::kick });
    envelope.addHandle ({ [this] { return juce::Point<float> ((float) KickEngine::lengthSeconds (current), 0.0f); },
                          param (p, ParamID::kickDecay), nullptr, false, Theme::kick });
    envelope.addHandle ({ [this] {
                              const auto t = (current.attackMs + current.holdMs + current.decayMs * 0.5f) * 0.001;
                              return juce::Point<float> ((float) t, KickEngine::amplitudeAt (current, t)); },
                          nullptr, param (p, ParamID::kickDecayCurve), true, Theme::kick });
    addAndMakeVisible (envelope);

    addKnobs (*this, knobs, p, Theme::kick, {
        { ParamID::kickTune, "TUNE" }, { ParamID::kickPitch, "PITCH" }, { ParamID::kickPitchTime, "TIME" },
        { ParamID::kickPitchCurve, "CURVE" }, { ParamID::kickClick, "LEVEL" }, { ParamID::kickClickLength, "LENGTH" },
        { ParamID::kickClickTone, "TONE" },
        { ParamID::kickAttack, "ATTACK" }, { ParamID::kickHold, "HOLD" }, { ParamID::kickDecay, "DECAY" },
        { ParamID::kickDecayCurve, "CURVE" }, { ParamID::kickGlide, "AMOUNT" }, { ParamID::kickGlideTime, "TIME" },
        { ParamID::kickDrive, "AMOUNT" }, { ParamID::kickLevel, "LEVEL" } });

    auto k = [this] (std::initializer_list<int> indices)
    {
        std::vector<juce::Component*> result;
        for (auto i : indices)
            result.push_back (knobs[(size_t) i].get());
        return result;
    };
    groups.addRow ({ { "KEY", k ({ 0 }) }, { "PITCH SWEEP", k ({ 1, 2, 3 }) }, { "SLOW GLIDE", k ({ 11, 12 }) }, { "DRIVE", k ({ 13 }) } });
    groups.addRow ({ { "VOLUME ENVELOPE", k ({ 7, 8, 9, 10 }) }, { "CLICK", k ({ 4, 5, 6 }) }, { "OUT", k ({ 14 }) } });

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

    // Rattarna växer med utrymmet; kurvorna får minst 150 px.
    const auto rowHeight = juce::jlimit (56, 90, (area.getHeight() - 150) / groups.numRows() - ControlGroups::titleHeight - 3);
    const auto groupsHeight = groups.numRows() * (ControlGroups::titleHeight + rowHeight + 3);
    groups.layout (area.removeFromBottom (groupsHeight), rowHeight);
    area.removeFromBottom (4);
    envelope.setBounds (area);
}

//==============================================================================
BassModule::BassModule (PluginBase& p, int layerIn)
    : ModulePanel ("BASS " + juce::String (layerIn + 1), Theme::layerColour (layerIn), &p.apvts, ParamID::bass (layerIn, BassField::on)),
      layer (layerIn),
      refs (p.apvts),
      presets (p, Presets::Section::bass, Theme::layerColour (layerIn), layerIn)
{
    using F = BassField;
    auto id = [this] (F f) { return ParamID::bass (layer, f); };
    const auto accent = Theme::layerColour (layer);

    setHeaderPreset (&presets);
    addAndMakeVisible (presets);
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

    oscA = std::make_unique<OscPanel> (p, layer, 0, accent);
    oscB = std::make_unique<OscPanel> (p, layer, 1, accent);
    addAndMakeVisible (*oscA);
    addAndMakeVisible (*oscB);

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
                     { "SLIDE", { knob (id (F::glide), "TIME") } },
                     { "LAYER OUT", { knob (id (F::level), "LEVEL"), knob (id (F::duck), "DUCK"), knob (id (F::gate), "GATE") } } });
}

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

    // Överst de två oscillatorerna, under dem kurvorna (filter + volym) och rattarna.
    auto oscRow = area.removeFromTop (juce::jlimit (160, 240, area.getHeight() / 2));
    oscA->setBounds (oscRow.removeFromLeft (oscRow.getWidth() / 2 - 3));
    oscRow.removeFromLeft (6);
    oscB->setBounds (oscRow);
    area.removeFromTop (6);

    envelope.setBounds (area.removeFromLeft (220).reduced (0, 2));
    area.removeFromLeft (6);
    const auto rowHeight = juce::jlimit (50, 84, area.getHeight() / groups.numRows() - ControlGroups::titleHeight - 3);
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
                              : std::vector<juce::Colour> { Theme::gate })
{
    addAndMakeVisible (length);
    addAndMakeVisible (presets);
    if (instrument)
    {
        noteEnd = std::make_unique<ChoiceComponent> (p.apvts, ParamID::noteEnd, "NOTE END");
        addAndMakeVisible (*noteEnd);
    }

    std::vector<std::pair<juce::String, StepSequencer::Lane>> lanes { { "VEL", StepSequencer::Lane::velocity },
                                                                      { "GATE", StepSequencer::Lane::gate } };
    if (instrument)
        lanes.push_back ({ "PITCH", StepSequencer::Lane::pitch });
    lanes.push_back ({ "SLIDE", StepSequencer::Lane::slide });

    for (const auto& [name, lane] : lanes)
    {
        auto button = std::make_unique<juce::TextButton> (name);
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        button->setTooltip (name + " - Shows the selected layer's " + name.toLowerCase() + " per step in the lane under the rows.");
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
        gates.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::gate), "GATE", colour));
        addChildComponent (*grids.back());
        addChildComponent (*gates.back());

        if (instrument)
        {
            slides.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::glide), "SLIDE", colour));
            slides.back()->setDescription ("How long this layer takes to glide between notes on steps with SLIDE on.");
            addChildComponent (*slides.back());

            auto button = std::make_unique<juce::TextButton> ("BASS " + juce::String (layer + 1));
            button->setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.55f));
            button->setColour (juce::TextButton::textColourOffId, colour);
            button->setTooltip ("BASS " + juce::String (layer + 1) + " - Edit this layer: the lane (velocity, gate, pitch, slide), "
                                "its grid, gate, slide time and FILL apply to the selected layer.");
            button->onClick = [this, layer] { sequencer.selectLayer (layer); showLayer (layer); };
            addAndMakeVisible (*button);
            layerButtons.push_back (std::move (button));
        }
    }

    fillButton.setTooltip ("FILL - Fill the selected layer with a rhythm (rolling, galloping, triplets, offbeat) or clear it.");
    fillButton.onClick = [this] { showFillMenu(); };
    addAndMakeVisible (fillButton);

    sequencer.onLayerSelected = [this] (int layer) { showLayer (layer); };
    addAndMakeVisible (sequencer);
    selectLane (StepSequencer::Lane::velocity);
    showLayer (0);
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
    for (size_t i = 0; i < grids.size(); ++i)
    {
        grids[i]->setVisible ((int) i == layer);
        gates[i]->setVisible ((int) i == layer);
        if (i < slides.size())
            slides[i]->setVisible ((int) i == layer);
        if (i < layerButtons.size())
            layerButtons[i]->setToggleState ((int) i == layer, juce::dontSendNotification);
    }
    repaint();
}

void PatternModule::showFillMenu()
{
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

void PatternModule::paint (juce::Graphics& g)
{
    // Rubrik för det valda lagrets inställningar.
    if (! grids.empty())
    {
        const auto area = grids[(size_t) shownLayer]->getBounds().withHeight (14).translated (0, -18);
        g.setFont (Theme::font (11.0f, true));
        g.setColour (instrument ? Theme::layerColour (shownLayer) : Theme::gate);
        g.drawText (instrument ? "BASS " + juce::String (shownLayer + 1) : juce::String ("GATE"), area, juce::Justification::centredLeft);
    }
}

void PatternModule::resized()
{
    auto area = getLocalBounds();

    // Två smala kolumner till vänster: mönstrets längd/slut/presets och vilket lager som
    // redigeras, sedan lanet och det valda lagrets rutnät, gate, slide och mallar.
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
    presets.setBounds (first.removeFromTop (24));
    first.removeFromTop (12);
    for (auto& button : layerButtons)
    {
        button->setBounds (first.removeFromTop (24));
        first.removeFromTop (4);
    }

    second.removeFromTop (18); // rubriken med lagrets namn
    for (auto& grid : grids)
        grid->setBounds (second.getX(), second.getY(), second.getWidth(), 40);
    second.removeFromTop (44);

    for (size_t i = 0; i < laneButtons.size(); i += 2)
    {
        auto row = second.removeFromTop (24);
        laneButtons[i].first->setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        if (i + 1 < laneButtons.size())
            laneButtons[i + 1].first->setBounds (row.withTrimmedLeft (4));
        second.removeFromTop (4);
    }
    second.removeFromTop (4);

    auto knobRow = second.removeFromTop (72);
    const auto half = knobRow.getWidth() / 2;
    for (auto& gate : gates)
        gate->setBounds (slides.empty() ? knobRow.withSizeKeepingCentre (half, knobRow.getHeight()) : knobRow.withWidth (half));
    for (auto& slide : slides)
        slide->setBounds (knobRow.withTrimmedLeft (half));
    second.removeFromTop (4);
    fillButton.setBounds (second.removeFromTop (24));

    sequencer.setBounds (area);
}
