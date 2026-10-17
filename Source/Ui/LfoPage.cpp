#include "LfoPage.h"

LfoDisplay::LfoDisplay (PluginBase& p, int lfoIndex) : processor (p), index (lfoIndex)
{
    startTimerHz (30);
}

bool LfoDisplay::isCustom() const
{
    return (int) processor.apvts.getRawParameterValue (Modulation::paramID (index, "Shape"))->load() == 6;
}

void LfoDisplay::timerCallback()
{
    repaint();
}

juce::Rectangle<float> LfoDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 10.0f);
}

// y = 0 är överst (värdet +1), y = 1 underst (-1).
juce::Point<float> LfoDisplay::toScreen (float x, float y) const
{
    const auto area = plotArea();
    return { area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight() };
}

juce::Point<float> LfoDisplay::fromScreen (juce::Point<float> point) const
{
    const auto area = plotArea();
    return { juce::jlimit (0.0f, 1.0f, (point.x - area.getX()) / area.getWidth()), juce::jlimit (0.0f, 1.0f, (point.y - area.getY()) / area.getHeight()) };
}

juce::Point<float> LfoDisplay::tensionHandle (const std::vector<NodeEnvelope::Point>& points, size_t segment) const
{
    const auto& a = points[segment];
    const auto& b = points[segment + 1];
    return toScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * NodeEnvelope::shape (0.5f, a.tension));
}

int LfoDisplay::pointAt (juce::Point<float> point) const
{
    const auto& points = curve().getPoints();
    for (int i = 0; i < (int) points.size(); ++i)
        if (toScreen (points[(size_t) i].x, points[(size_t) i].y).getDistanceFrom (point) < 9.0f)
            return i;
    return -1;
}

int LfoDisplay::tensionHandleAt (juce::Point<float> point) const
{
    const auto& points = curve().getPoints();
    for (size_t i = 0; i + 1 < points.size(); ++i)
        if (tensionHandle (points, i).getDistanceFrom (point) < 9.0f)
            return (int) i;
    return -1;
}

void LfoDisplay::commit (std::vector<NodeEnvelope::Point> points)
{
    curve().setPoints (std::move (points));
    processor.markStateChanged();
    repaint();
}

// Mallarna: formerna man kan börja från (fas 0..1 i x, 0 = uppe i y).
std::vector<NodeEnvelope::Point> LfoDisplay::templatePoints (int which)
{
    switch (which)
    {
        case 0:  return PluginBase::sineCurve();
        case 1:  return { { 0.0f, 0.5f, 0.0f }, { 0.25f, 0.0f, 0.0f }, { 0.75f, 1.0f, 0.0f }, { 1.0f, 0.5f, 0.0f } };               // triangel
        case 2:  return { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };                                                             // såg upp
        case 3:  return { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };                                                             // såg ner
        case 4:  return { { 0.0f, 0.0f, 0.0f }, { 0.499f, 0.0f, 0.0f }, { 0.501f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };             // fyrkant
        case 5:  return { { 0.0f, 0.0f, -0.7f }, { 1.0f, 1.0f, 0.0f } };                                                            // fall: snabbt först
        default:
        {
            // Slumpsteg: åtta platåer.
            std::vector<NodeEnvelope::Point> points;
            auto& random = juce::Random::getSystemRandom();
            for (int step = 0; step < 8; ++step)
            {
                const auto y = random.nextFloat();
                points.push_back ({ (float) step / 8.0f + (step > 0 ? 0.002f : 0.0f), y, 0.0f });
                points.push_back ({ (float) (step + 1) / 8.0f - (step < 7 ? 0.002f : 0.0f), y, 0.0f });
            }
            return points;
        }
    }
}

void LfoDisplay::makeCustom (int templateIndex)
{
    curve().setPoints (templatePoints (templateIndex));
    if (auto* parameter = processor.apvts.getParameter (Modulation::paramID (index, "Shape")))
        Presets::setParameter (*parameter, parameter->convertTo0to1 (6.0f));
    processor.markStateChanged();
    repaint();
}

void LfoDisplay::showTemplates()
{
    juce::PopupMenu menu;
    const char* names[] { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Fall (fast, then slow)", "Random steps" };
    menu.addSectionHeader ("Start from a shape");
    for (int i = 0; i < 7; ++i)
        menu.addItem (i + 1, names[i]);
    juce::Component::SafePointer<LfoDisplay> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis] (int result)
    {
        if (safeThis != nullptr && result > 0)
            safeThis->makeCustom (result - 1);
    });
}

void LfoDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = plotArea();
    const auto shape = (int) processor.apvts.getRawParameterValue (Modulation::paramID (index, "Shape"))->load();
    const auto on = processor.apvts.getRawParameterValue (Modulation::paramID (index, "On"))->load() >= 0.5f;
    const auto custom = shape == 6;
    const auto colour = on ? Theme::lfoColour (index) : Theme::textDim;

    // Rutnät: nollinjen och åttondelar av perioden.
    g.setColour (Theme::grid);
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / 8.0f), area.getY(), area.getBottom());
    g.setColour (Theme::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    // En period av formen (random ritas som fyra slumpsteg).
    juce::Path path;
    constexpr int resolution = 160;
    const auto& points = editing.empty() ? curve().getPoints() : editing;
    NodeEnvelope preview (1.0f, 1.0f, points.empty() ? std::vector<NodeEnvelope::Point> { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f } } : points);
    for (int i = 0; i <= resolution; ++i)
    {
        const auto phase = (double) i / resolution;
        float value;
        if (custom)
            value = 1.0f - 2.0f * preview.evaluate ((float) phase);
        else
        {
            const auto cycle = shape == 5 ? (juce::int64) (phase * 4.0) : 0;
            value = Modulation::shapeValue (shape, std::fmod (phase * (shape == 5 ? 4.0 : 1.0), 1.0), cycle);
        }
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

    g.setFont (Theme::font (9.5f));
    g.setColour (Theme::textDim);
    if (! custom)
    {
        g.drawText ("Double-click to draw your own curve", getLocalBounds().reduced (8, 3), juce::Justification::bottomLeft);
        return;
    }

    // Punkterna och rombernas handtag för böjning.
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto screen = toScreen (points[(size_t) i].x, points[(size_t) i].y);
        const auto active = i == hovered || i == draggedPoint;
        g.setColour (Theme::knobBodyDark);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (screen));
        g.setColour (colour);
        g.drawEllipse (juce::Rectangle<float> (active ? 12.0f : 10.0f, active ? 12.0f : 10.0f).withCentre (screen), 2.0f);
    }
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        juce::Path diamond;
        diamond.addPolygon (tensionHandle (points, i), 4, 4.0f, juce::MathConstants<float>::pi / 4.0f);
        g.setColour (colour.withAlpha ((int) i == draggedTension ? 1.0f : 0.5f));
        g.fillPath (diamond);
    }
    g.setColour (Theme::textDim);
    g.drawText ("Double-click: add point   Right-click: remove", getLocalBounds().reduced (8, 3), juce::Justification::bottomLeft);
}

void LfoDisplay::mouseMove (const juce::MouseEvent& event)
{
    const auto point = isCustom() ? pointAt (event.position) : -1;
    if (point != hovered)
    {
        hovered = point;
        repaint();
    }
}

void LfoDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (! isCustom())
        return;

    auto points = curve().getPoints();
    const auto point = pointAt (event.position);

    if (event.mods.isPopupMenu())
    {
        if (point > 0 && point < (int) points.size() - 1)
        {
            points.erase (points.begin() + point);
            commit (std::move (points));
        }
        else if (point < 0)
            showTemplates();
        return;
    }

    draggedPoint = point;
    draggedTension = point < 0 ? tensionHandleAt (event.position) : -1;
    if (draggedTension >= 0)
    {
        tensionStart = points[(size_t) draggedTension].tension;
        dragStartY = event.position.y;
    }
    editing = points;
}

void LfoDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (editing.empty())
        return;

    if (draggedPoint >= 0)
    {
        auto value = fromScreen (event.position);
        auto& point = editing[(size_t) draggedPoint];

        // Punkten får inte passera grannarna; första och sista ligger kvar på kanterna. Skift = hela åttondelar i tid.
        const auto isEdge = draggedPoint == 0 || draggedPoint == (int) editing.size() - 1;
        if (event.mods.isShiftDown())
            value.x = std::round (value.x * 16.0f) / 16.0f;
        if (! isEdge)
            point.x = juce::jlimit (editing[(size_t) draggedPoint - 1].x, editing[(size_t) draggedPoint + 1].x, value.x);
        point.y = event.mods.isShiftDown() ? std::round (value.y * 8.0f) / 8.0f : value.y;
        repaint();
    }
    else if (draggedTension >= 0)
    {
        const auto& a = editing[(size_t) draggedTension];
        const auto& b = editing[(size_t) draggedTension + 1];
        const auto direction = b.y >= a.y ? 1.0f : -1.0f;
        editing[(size_t) draggedTension].tension = juce::jlimit (-1.0f, 1.0f, tensionStart + direction * (event.position.y - dragStartY) / 80.0f);
        repaint();
    }
}

void LfoDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! editing.empty() && (draggedPoint >= 0 || draggedTension >= 0))
        commit (editing);
    editing.clear();
    draggedPoint = draggedTension = -1;
}

void LfoDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! isCustom())
    {
        // En vanlig form blir en egen kurva att redigera (samma form som den hade).
        const auto shape = (int) processor.apvts.getRawParameterValue (Modulation::paramID (index, "Shape"))->load();
        makeCustom (shape == 5 ? 6 : shape);
        return;
    }
    if (pointAt (event.position) >= 0)
        return;

    auto points = curve().getPoints();
    if ((int) points.size() >= NodeEnvelope::maxPoints)
        return;
    const auto value = fromScreen (event.position);
    points.push_back ({ value.x, value.y, 0.0f });
    commit (std::move (points));
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
    : processor (p),
      index (lfoIndex),
      shape (p.apvts, Modulation::paramID (lfoIndex, "Shape").toRawUTF8(), "SHAPE"),
      mode (p.apvts, Modulation::paramID (lfoIndex, "Mode").toRawUTF8(), "RATE MODE"),
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

    hzLabel.setText ("FREE RATE", juce::dontSendNotification);
    hzLabel.setFont (Theme::font (11.0f, true));
    hzLabel.setColour (juce::Label::textColourId, Theme::textDim);
    hzSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    hzSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    hzSlider.setColour (juce::Slider::trackColourId, Theme::lfoColour (lfoIndex));
    hzSlider.setColour (juce::Slider::thumbColourId, Theme::text);
    hzSlider.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    hzSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    hzSlider.setTooltip ("FREE RATE - How fast the LFO runs, in cycles per second (Hz), independent of the song's note values. It still follows the DAW's position, "
                         "so it stays in step with the playhead. Double-click for 1 Hz.");
    hzAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, Modulation::paramID (lfoIndex, "Hz"), hzSlider);
    hzSlider.setDoubleClickReturnValue (true, 1.0);
    mode.box.setTooltip ("RATE MODE - Sync: the LFO runs in note values of the tempo (RATE). Hz: it runs at its own speed in cycles per second.");
    for (auto* c : std::initializer_list<juce::Component*> { &shape, &mode, &rate, &hzLabel, &hzSlider, &display, &handle, &targets })
        addAndMakeVisible (c);
    startTimerHz (6);
    timerCallback();
}

void LfoPanel::timerCallback()
{
    const auto current = (int) processor.apvts.getRawParameterValue (Modulation::paramID (index, "Mode"))->load();
    if (current == lastMode)
        return;
    lastMode = current;
    rate.setVisible (current == 0);
    hzLabel.setVisible (current == 1);
    hzSlider.setVisible (current == 1);
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

    auto left = area.removeFromLeft (96);
    shape.setBounds (left.removeFromTop (38));
    left.removeFromTop (2);
    mode.setBounds (left.removeFromTop (38));
    left.removeFromTop (2);
    const auto rateSlot = left.removeFromTop (38);
    rate.setBounds (rateSlot);
    auto hzArea = rateSlot.withHeight (rateSlot.getHeight() + left.getHeight()); // fri hastighet tar rattens plats (och lite mer)
    hzLabel.setBounds (hzArea.removeFromTop (15));
    hzSlider.setBounds (hzArea.removeFromTop (40));
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
