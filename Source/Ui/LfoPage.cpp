#include "LfoPage.h"

LfoDisplay::LfoDisplay (PluginBase& p, int lfoIndex) : processor (p), index (lfoIndex)
{
    startTimerHz (30);
}

void LfoDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = bounds.reduced (6.0f, 8.0f);
    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    const auto shape = (int) processor.apvts.getRawParameterValue (Modulation::paramID (index, "Shape"))->load();
    const auto on = processor.apvts.getRawParameterValue (Modulation::paramID (index, "On"))->load() >= 0.5f;
    const auto colour = on ? Theme::lfoColour (index) : Theme::textDim;

    // En period av formen (random ritas som fyra slumpsteg).
    juce::Path path;
    constexpr int resolution = 120;
    for (int i = 0; i <= resolution; ++i)
    {
        const auto phase = (double) i / resolution;
        const auto cycle = shape == 5 ? (juce::int64) (phase * 4.0) : 0;
        const auto value = Modulation::shapeValue (shape, std::fmod (phase * (shape == 5 ? 4.0 : 1.0), 1.0), cycle);
        const juce::Point<float> point { area.getX() + (float) phase * area.getWidth(), area.getCentreY() - value * area.getHeight() * 0.5f };
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (5.0f));
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.8f));

    // Var LFO:n är just nu.
    const auto phase = processor.modulator.phase[(size_t) index].load();
    const auto value = processor.modulator.value[(size_t) index].load();
    const juce::Point<float> dot { area.getX() + phase * area.getWidth(), area.getCentreY() - value * area.getHeight() * 0.5f };
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
}

//==============================================================================
LfoDragHandle::LfoDragHandle (int source, const juce::String& labelIn) : index (source), label (labelIn)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("DRAG - Drag this onto any knob (kick, instrument or effects) to let " + Modulation::sourceName (index)
                + " move it. Then drag the coloured ring on the knob to set how much.");
}

void LfoDragHandle::paint (juce::Graphics& g)
{
    const auto colour = Theme::sourceColour (index);
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colour.withAlpha (isMouseOver() ? 0.35f : 0.18f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (colour);
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

    // Ett kors med pilar, som i Massive. I en smal ruta ligger texten under korset.
    const auto narrow = bounds.getWidth() < 110.0f;
    auto icon = narrow ? bounds.removeFromTop (bounds.getHeight() * 0.55f).withSizeKeepingCentre (22.0f, 22.0f).reduced (3.0f)
                       : bounds.removeFromLeft (bounds.getHeight()).reduced (4.0f);
    const auto c = icon.getCentre();
    const auto r = icon.getWidth() / 2.0f;
    g.drawLine (c.x - r, c.y, c.x + r, c.y, 1.6f);
    g.drawLine (c.x, c.y - r, c.x, c.y + r, 1.6f);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (c));

    g.setFont (Theme::font (narrow ? 9.5f : 10.5f, true));
    g.drawFittedText (label, bounds.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void LfoDragHandle::mouseDrag (const juce::MouseEvent&)
{
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        if (! container->isDragAndDropActive())
        {
            // Bilden som följer musen: en liten bricka i LFO:ns färg.
            juce::Image image (juce::Image::ARGB, 64, 22, true);
            {
                juce::Graphics g (image);
                g.setColour (Theme::sourceColour (index));
                g.fillRoundedRectangle (0.0f, 0.0f, 64.0f, 22.0f, 5.0f);
                g.setColour (Theme::knobBodyDark);
                g.setFont (Theme::font (12.0f, true));
                g.drawFittedText (Modulation::sourceName (index), 0, 0, 64, 22, juce::Justification::centred, 1, 0.6f);
            }
            container->startDragging ("mod:" + juce::String (index), this, juce::ScaledImage (image));
        }
    }
}

//==============================================================================
LfoTargets::LfoTargets (PluginBase& p, int lfoIndex) : processor (p), index (lfoIndex)
{
    startTimerHz (5);
}

juce::String LfoTargets::describe() const
{
    juce::StringArray parts;
    const auto& parameters = processor.getParameters();
    for (const auto& route : processor.modRoutes.routes)
    {
        const auto target = route.target.load();
        if (route.source.load() != index || ! juce::isPositiveAndBelow (target, parameters.size()))
            continue;
        const auto amount = route.amount.load();
        parts.add (parameters[target]->getName (40) + " " + (amount > 0.0f ? "+" : "")
                   + juce::String (juce::roundToInt (amount * 100.0f)) + "%");
    }
    return parts.joinIntoString (", ");
}

void LfoTargets::timerCallback()
{
    const auto newText = describe();
    if (newText != text)
    {
        text = newText;
        setTooltip (text.isEmpty() ? juce::String()
                                   : Modulation::sourceName (index) + " MOVES - " + text + ". Right-click to remove one.");
        repaint();
    }
}

void LfoTargets::paint (juce::Graphics& g)
{
    g.setFont (Theme::font (10.5f));
    g.setColour (text.isEmpty() ? Theme::textDim.withAlpha (0.6f) : Theme::text);
    g.drawFittedText (text.isEmpty() ? juce::String ("Not connected to any knob yet") : juce::String::fromUTF8 ("\xe2\x86\x92 ") + text,
                      getLocalBounds(), juce::Justification::centredLeft, 2, 0.8f);
}

void LfoTargets::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    const auto& parameters = processor.getParameters();
    for (int r = 0; r < Modulation::maxRoutes; ++r)
    {
        const auto& route = processor.modRoutes.routes[(size_t) r];
        const auto target = route.target.load();
        if (route.source.load() != index || ! juce::isPositiveAndBelow (target, parameters.size()))
            continue;
        menu.addItem ("Remove " + parameters[target]->getName (40), [this, r]
        {
            processor.modRoutes.remove (r);
            processor.markStateChanged();
        });
    }
    if (menu.getNumItems() > 0)
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

//==============================================================================
LfoPanel::LfoPanel (PluginBase& p, int lfoIndex)
    : index (lfoIndex),
      shape (p.apvts, Modulation::paramID (lfoIndex, "Shape").toRawUTF8(), "SHAPE"),
      rate (p.apvts, Modulation::paramID (lfoIndex, "Rate").toRawUTF8(), "RATE"),
      display (p, lfoIndex),
      handle (lfoIndex),
      targets (p, lfoIndex)
{
    power.setColour (juce::ToggleButton::tickColourId, Theme::lfoColour (lfoIndex));
    power.setTooltip (Modulation::sourceName (lfoIndex) + " - turns the LFO on or off (its connections stay)");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, Modulation::paramID (lfoIndex, "On"), power);

    for (auto* c : std::initializer_list<juce::Component*> { &shape, &rate, &display, &handle, &targets })
        addAndMakeVisible (c);
}

void LfoPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::knobBodyDark.withAlpha (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (Theme::lfoColour (index));
    g.setFont (Theme::font (13.0f, true));
    g.drawText (Modulation::sourceName (index), getLocalBounds().withHeight (26).withTrimmedLeft (32), juce::Justification::centredLeft);
}

void LfoPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto header = area.removeFromTop (20);
    power.setBounds (header.removeFromLeft (24));
    handle.setBounds (header.removeFromRight (124));
    area.removeFromTop (4);

    targets.setBounds (area.removeFromBottom (28));
    area.removeFromBottom (2);

    auto left = area.removeFromLeft (84);
    shape.setBounds (left.removeFromTop (40));
    left.removeFromTop (4);
    rate.setBounds (left.removeFromTop (40));
    area.removeFromLeft (6);
    display.setBounds (area);
}

//==============================================================================
LfoPage::LfoPage (PluginBase& p) : presets (p, Presets::Section::lfo, Theme::lfo)
{
    addAndMakeVisible (presets);
    for (int i = 0; i < Modulation::numLfos; ++i)
    {
        panels.push_back (std::make_unique<LfoPanel> (p, i));
        addAndMakeVisible (*panels.back());
    }
}

void LfoPage::resized()
{
    auto area = getLocalBounds();
    presets.setBounds (area.removeFromTop (22).removeFromRight (160));
    area.removeFromTop (4);

    const auto gap = 8;
    const auto width = (area.getWidth() - gap * (Modulation::numLfos - 1)) / Modulation::numLfos;
    for (auto& panel : panels)
    {
        panel->setBounds (area.removeFromLeft (width));
        area.removeFromLeft (gap);
    }
}
