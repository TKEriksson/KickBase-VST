#include "OscPanel.h"

WavetableView::WavetableView (PluginBase& p, int layerIn, int oscIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), osc (oscIn), accent (accentIn)
{
    positionIndex = processor.modulator.indexOf (ParamID::bass (layer, ParamID::oscField (osc, BassField::aPos)));
    setTooltip ("OSC " + juce::String (osc + 1)
                + " - The wavetable: all its waveforms stacked from back to front. POS chooses which one plays "
                  "(drag an LFO onto POS to sweep through them); WARP bends the waveform you see in front.");
    startTimerHz (30);
}

float WavetableView::raw (BassField field) const
{
    return processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, field)))->load();
}

void WavetableView::timerCallback()
{
    // Rita om när tabellen, positionen (med LFO) eller warpen ändras.
    const auto position = juce::jlimit (0.0f, 1.0f, raw (BassField::aPos) + processor.modulator.currentOffset (positionIndex));
    const auto table = (int) raw (BassField::aTable);
    const auto warpMode = (int) raw (BassField::aWarp);
    const auto warp = raw (BassField::aWarpAmount);
    if (position != lastPosition || table != lastTable || warpMode != lastWarpMode || warp != lastWarp)
    {
        lastPosition = position;
        lastTable = table;
        lastWarpMode = warpMode;
        lastWarp = warp;
        repaint();
    }
}

void WavetableView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const Wavetable* userTable = lastTable >= Wavetables::userIndex() ? processor.userTable (layer, osc) : nullptr;
    const auto& table = userTable != nullptr ? *userTable : Wavetables::get (lastTable < 0 || lastTable >= Wavetables::userIndex() ? 0 : lastTable);
    const auto area = bounds.reduced (8.0f, 10.0f);

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
        const auto warped = BassSynth::warpPhase (mode == WarpMode::fm ? WarpMode::off : mode, warpAmount, phase, 0.0f);
        return table.sample (position, warped, 512.0);
    };
    const auto z = position;
    const auto path = pathFor (current, z, 1.0f);
    g.setColour (accent.withAlpha (0.3f));
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent.brighter (0.3f));
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (Theme::font (9.5f, true));
    g.setColour (Theme::textDim);
    g.drawText (table.getName().toUpperCase(), bounds.reduced (6.0f, 4.0f), juce::Justification::topLeft);
}

//==============================================================================
OscPanel::OscPanel (PluginBase& p, int layerIn, int oscIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), osc (oscIn), accent (accentIn),
      table (p.apvts, ParamID::bass (layer, ParamID::oscField (oscIn, BassField::aTable)), "TABLE"),
      warp (p.apvts, ParamID::bass (layer, ParamID::oscField (oscIn, BassField::aWarp)), "WARP"),
      view (p, layer, oscIn, accentIn)
{
    addAndMakeVisible (table);
    addAndMakeVisible (warp);
    addAndMakeVisible (view);

    // Klassiska vågformer med ett klick (som i FL:s 3x Osc), utan att behöva förstå wavetables.
    const char* shapeNames[] { "SIN", "TRI", "SAW", "SQR" };
    const char* shapeHelp[] { "Pure sine wave.", "Triangle wave: soft, with only odd overtones.",
                              "Saw wave: bright and buzzy, all overtones. The classic bass sound.",
                              "Square wave: hollow, with only odd overtones. Use WARP > PWM to change the pulse width." };
    for (size_t i = 0; i < shapeButtons.size(); ++i)
    {
        auto& button = shapeButtons[i];
        button.setButtonText (shapeNames[i]);
        button.setTooltip (juce::String ("CLASSIC WAVE - ") + shapeHelp[i] + " (Wavetables are in the TABLE list.)");
        button.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.6f));
        button.onClick = [this, i] { chooseTable (Wavetables::count() - 4 + (int) i); };
        addAndMakeVisible (button);
    }

    wavButton.setTooltip ("LOAD WAV - Use a WAV file as this oscillator's wavetable (like Serum). A file of 2048-sample cycles becomes up to 16 "
                          "waveforms that POS sweeps through; a short file is used as one single waveform. Saved in the project.");
    wavButton.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.6f));
    wavButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a WAV file for the wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (! fc.getResult().existsAsFile())
                                      return;
                                  if (processor.loadUserTable (layer, osc, fc.getResult()))
                                      view.repaint();
                                  else
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wavetable",
                                                                              "Could not read that file as a wavetable.");
                              });
    };
    addAndMakeVisible (wavButton);
    startTimerHz (10);

    using F = BassField;
    const std::pair<F, const char*> specs[] { { F::aPos, "POS" }, { F::aLevel, "LEVEL" }, { F::aSemi, "SEMI" }, { F::aFine, "FINE" },
                                              { F::aPhase, "PHASE" }, { F::aUnison, "UNISON" }, { F::aDetune, "DETUNE" },
                                              { F::aBlend, "BLEND" }, { F::aPan, "PAN" }, { F::aStereo, "STEREO" },
                                              { F::aWarpAmount, "WARP" }, { F::aRandom, "RAND" } };
    for (const auto& [field, name] : specs)
    {
        knobs.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, ParamID::oscField (osc, field)), name, accent));
        addAndMakeVisible (*knobs.back());
    }
}

void OscPanel::timerCallback()
{
    const auto current = (int) processor.apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, BassField::aTable)))->load();
    if (current == lastTable)
        return;
    lastTable = current;
    for (size_t i = 0; i < shapeButtons.size(); ++i)
        shapeButtons[i].setToggleState (current == Wavetables::count() - 4 + (int) i, juce::dontSendNotification);
}

void OscPanel::chooseTable (int index)
{
    if (auto* parameter = processor.apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, BassField::aTable))))
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

void OscPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (16);

    // Vänster: tabell, warp och bilden. Höger: rattarna i två rader över hela höjden.
    auto left = area.removeFromLeft (juce::jmin (300, area.getWidth() / 3));
    auto choices = left.removeFromTop (40);
    table.setBounds (choices.removeFromLeft (choices.getWidth() / 2 + 10).withTrimmedRight (3));
    warp.setBounds (choices.withTrimmedLeft (3));
    left.removeFromTop (4);
    auto shapes = left.removeFromTop (20);
    const auto shapeWidth = shapes.getWidth() / (int) shapeButtons.size();
    wavButton.setBounds (shapes.removeFromRight (shapes.getWidth() / 5).reduced (1, 0));
    for (auto& button : shapeButtons)
        button.setBounds (shapes.removeFromLeft (shapes.getWidth() / ((int) shapeButtons.size() - (int) (&button - shapeButtons.data()))).reduced (1, 0));
    juce::ignoreUnused (shapeWidth);
    left.removeFromTop (4);
    view.setBounds (left);
    area.removeFromLeft (4);

    const auto rowHeight = area.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        const auto width = rowArea.getWidth() / 6;
        for (int i = 0; i < 6; ++i)
            knobs[(size_t) (row * 6 + i)]->setBounds (rowArea.removeFromLeft (width));
    }
}
