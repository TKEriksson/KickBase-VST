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

    juce::RangedAudioParameter* param (PluginBase& processor, const char* id)
    {
        auto* parameter = processor.apvts.getParameter (id);
        jassert (parameter != nullptr);
        return parameter;
    }

    void addKnobs (juce::Component& parent, std::vector<std::unique_ptr<KnobComponent>>& knobs, PluginBase& processor,
                   juce::Colour accent, std::initializer_list<std::pair<const char*, const char*>> specs)
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
    startTimerHz (15);
}

void HeaderBar::resized()
{
    const auto subtitleWidth = subtitle.isEmpty() ? 0 : 40;
    presets.setBounds (14 + 160 + subtitleWidth, 9, 200, getHeight() - 18);
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
    addAndMakeVisible (presets);
    keyMode.box.setTooltip ("Follow: the kick is tuned to the note's key. Fixed: it stays on C1 + TUNE whatever the note.");
    addAndMakeVisible (keyMode);
    envelope.onBeforePaint = [this] { current = refs.readKick (0); };

    envelope.addCurve ({ [this] (double t) { return KickEngine::pitchEnvelopeAt (current, t) * current.pitchSemis / 72.0f; },
                         Theme::text.withAlpha (0.8f), false, "PITCH" });
    envelope.addCurve ({ [this] (double t) { return KickEngine::amplitudeAt (current, t); }, Theme::kick, true, "AMP" });

    const auto pitchColour = Theme::text;
    envelope.addHandle ({ [this] { return juce::Point<float> (0.0f, current.pitchSemis / 72.0f); },
                          nullptr, param (p, ParamID::kickPitch), false, pitchColour });
    envelope.addHandle ({ [this] { return juce::Point<float> (current.pitchTimeMs * 0.001f, 0.0f); },
                          param (p, ParamID::kickPitchTime), nullptr, false, pitchColour });
    envelope.addHandle ({ [this] {
                              const auto t = current.pitchTimeMs * 0.0005;
                              return juce::Point<float> ((float) t, KickEngine::pitchEnvelopeAt (current, t) * current.pitchSemis / 72.0f); },
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
        { ParamID::kickTune, "TUNE" }, { ParamID::kickPitch, "PITCH" }, { ParamID::kickPitchTime, "P.TIME" },
        { ParamID::kickPitchCurve, "P.CURVE" }, { ParamID::kickClick, "CLICK" }, { ParamID::kickClickLength, "CLK LEN" },
        { ParamID::kickClickTone, "CLK TONE" },
        { ParamID::kickAttack, "ATTACK" }, { ParamID::kickHold, "HOLD" }, { ParamID::kickDecay, "DECAY" },
        { ParamID::kickDecayCurve, "D.CURVE" }, { ParamID::kickDrive, "DRIVE" }, { ParamID::kickLevel, "LEVEL" } });

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
    presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    auto sampleRow = area.removeFromBottom (22);
    clearButton.setBounds (sampleRow.removeFromRight (24));
    sampleRow.removeFromRight (4);
    loadButton.setBounds (sampleRow.removeFromRight (50));
    sampleLabel.setBounds (sampleRow);
    area.removeFromBottom (4);

    auto row2 = area.removeFromBottom (knobRowHeight);
    auto row1 = area.removeFromBottom (knobRowHeight);
    area.removeFromBottom (6);
    envelope.setBounds (area);

    auto first = pointers (knobs, 0, 7);
    auto second = pointers (knobs, 7, 13);
    layoutRow (first, row1);
    const auto cell = row2.getWidth() / 7;
    keyMode.setBounds (row2.removeFromRight (cell).reduced (4, 14));
    layoutRow (second, row2);
}

//==============================================================================
BassModule::BassModule (PluginBase& p)
    : ModulePanel ("BASS", Theme::bass, &p.apvts, ParamID::bassOn),
      refs (p.apvts),
      wave (p.apvts, ParamID::bassWave, "WAVE"),
      presets (p, Presets::Section::bass, Theme::bass)
{
    addAndMakeVisible (presets);
    envelope.onBeforePaint = [this] { current = refs.readBass (36); };

    envelope.addCurve ({ [this] (double t) { return BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f; },
                         Theme::bass, true, "FILTER" });
    envelope.addCurve ({ [this] (double t) { return ampValueAt (t); }, Theme::text.withAlpha (0.8f), false, "AMP" });

    envelope.addHandle ({ [this] { return juce::Point<float> (0.0015f, current.envAmount / 8.0f); },
                          nullptr, param (p, ParamID::bassEnvAmount), false, Theme::bass });
    envelope.addHandle ({ [this] {
                              const auto t = current.envDecayMs * 0.001 / 3.0;
                              return juce::Point<float> ((float) t, BassSynth::filterEnvelopeAt (current, t) * current.envAmount / 8.0f); },
                          param (p, ParamID::bassEnvDecay), nullptr, false, Theme::bass });

    envelope.addHandle ({ [this] { return juce::Point<float> (current.attackMs * 0.001f, 1.0f); },
                          param (p, ParamID::bassAttack), nullptr, false, Theme::text });
    envelope.addHandle ({ [this] {
                              const auto t = (current.attackMs + current.decayMs * 0.5f) * 0.001;
                              return juce::Point<float> ((float) t, ampValueAt (t)); },
                          param (p, ParamID::bassDecay), param (p, ParamID::bassSustain), false, Theme::text });
    envelope.addHandle ({ [this] { return juce::Point<float> (0.18f + current.releaseMs * 0.001f, 0.0f); },
                          param (p, ParamID::bassRelease), nullptr, false, Theme::text });
    addAndMakeVisible (envelope);

    addAndMakeVisible (wave);
    addKnobs (*this, knobs, p, Theme::bass, {
        { ParamID::bassOctave, "OCTAVE" }, { ParamID::bassTune, "TUNE" }, { ParamID::bassSub, "SUB" },
        { ParamID::bassCutoff, "CUTOFF" }, { ParamID::bassResonance, "RESO" }, { ParamID::bassEnvAmount, "ENV AMT" },
        { ParamID::bassEnvDecay, "ENV DEC" },
        { ParamID::bassVelToEnv, "VEL>ENV" }, { ParamID::bassAttack, "ATTACK" }, { ParamID::bassDecay, "DECAY" },
        { ParamID::bassSustain, "SUSTAIN" }, { ParamID::bassRelease, "RELEASE" }, { ParamID::bassGlide, "GLIDE" },
        { ParamID::bassDrive, "DRIVE" }, { ParamID::bassLevel, "LEVEL" } });
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
    presets.setBounds (presetBounds (*this));
    auto area = getContentBounds();

    auto row2 = area.removeFromBottom (knobRowHeight);
    auto row1 = area.removeFromBottom (knobRowHeight);
    area.removeFromBottom (6);
    envelope.setBounds (area.withTrimmedBottom (22));

    // Första raden: vågform + sju rattar. Andra raden: åtta rattar.
    const auto cell = row1.getWidth() / 8;
    wave.setBounds (row1.removeFromLeft (cell).reduced (2, 14));
    auto first = pointers (knobs, 0, 7);
    auto second = pointers (knobs, 7, 15);
    layoutRow (first, row1);
    layoutRow (second, row2);
}

//==============================================================================
GateModule::GateModule (PluginBase& p)
    : ModulePanel ("GATE", Theme::gate, &p.apvts, ParamID::bassOn),
      refs (p.apvts),
      key (p.apvts, ParamID::fxKey, "KEY"),
      presets (p, Presets::Section::bass, Theme::gate)
{
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
        { ParamID::gateAttack, "ATTACK" }, { ParamID::gateRelease, "RELEASE" }, { ParamID::bassLevel, "LEVEL" } });

    help.setText ("Put MyOwnFruit FX on the mixer track of your bass synth. The synth is gated by the "
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
PatternModule::PatternModule (PluginBase& p, juce::Colour bassAccent, const juce::String& bassName, bool showPitchLane)
    : length (p.apvts, ParamID::patternBars, "LENGTH"),
      grid (p.apvts, ParamID::bassGrid, "BASS GRID"),
      presets (p, Presets::Section::pattern, Theme::pattern),
      sequencer (p, bassAccent, bassName)
{
    addAndMakeVisible (length);
    addAndMakeVisible (grid);
    addAndMakeVisible (presets);

    std::vector<std::pair<juce::String, StepSequencer::Lane>> lanes { { "VEL", StepSequencer::Lane::velocity },
                                                                      { "GATE", StepSequencer::Lane::gate } };
    if (showPitchLane)
        lanes.push_back ({ "PITCH", StepSequencer::Lane::pitch });
    lanes.push_back ({ "SLIDE", StepSequencer::Lane::slide });

    for (const auto& [name, lane] : lanes)
    {
        auto button = std::make_unique<juce::TextButton> (name);
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonOnColourId, Theme::pattern);
        const auto laneCopy = lane;
        button->onClick = [this, laneCopy] { selectLane (laneCopy); };
        addAndMakeVisible (*button);
        laneButtons.emplace_back (std::move (button), lane);
    }

    addAndMakeVisible (sequencer);
    selectLane (StepSequencer::Lane::velocity);
}

void PatternModule::selectLane (StepSequencer::Lane lane)
{
    sequencer.setLane (lane);
    for (auto& [button, buttonLane] : laneButtons)
        button->setToggleState (buttonLane == lane, juce::dontSendNotification);
}

void PatternModule::resized()
{
    auto area = getLocalBounds();

    auto column = area.removeFromLeft (120);
    area.removeFromLeft (10);
    length.setBounds (column.removeFromTop (40));
    column.removeFromTop (4);
    grid.setBounds (column.removeFromTop (40));
    column.removeFromTop (8);
    presets.setBounds (column.removeFromTop (24));
    column.removeFromTop (10);

    // Lane-knapparna två och två.
    for (size_t i = 0; i < laneButtons.size(); i += 2)
    {
        auto row = column.removeFromTop (24);
        laneButtons[i].first->setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        if (i + 1 < laneButtons.size())
            laneButtons[i + 1].first->setBounds (row.withTrimmedLeft (4));
        column.removeFromTop (4);
    }

    sequencer.setBounds (area);
}

//==============================================================================
MixModule::MixModule (PluginBase& p, bool showNoteEnd)
    : ModulePanel ("MIX", Theme::mix)
{
    addKnobs (*this, knobs, p, Theme::mix, { { ParamID::duckAmount, "DUCK" }, { ParamID::bassGate, "GATE" }, { ParamID::gain, "MASTER" } });
    knobs[0]->slider.setTooltip ("How much the bass ducks under the kick");
    knobs[1]->slider.setTooltip ("Scales the gate length of all bass steps");

    if (showNoteEnd)
    {
        noteEnd = std::make_unique<ChoiceComponent> (p.apvts, ParamID::noteEnd, "NOTE END");
        noteEnd->box.setTooltip ("What happens when the DAW note ends mid-pattern");
        addAndMakeVisible (*noteEnd);
    }
}

void MixModule::resized()
{
    ModulePanel::resized();
    auto area = getContentBounds();

    if (noteEnd != nullptr)
    {
        noteEnd->setBounds (area.removeFromBottom (40));
        area.removeFromBottom (6);
    }

    auto top = area.removeFromTop (area.getHeight() / 2);
    knobs[0]->setBounds (top.removeFromLeft (top.getWidth() / 2));
    knobs[1]->setBounds (top);
    knobs[2]->setBounds (area.withSizeKeepingCentre (area.getWidth() / 2, area.getHeight()));
}
