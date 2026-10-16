#include "SidechainPage.h"

namespace
{
    constexpr float grabDistance = 9.0f;
}

DuckCurveView::DuckCurveView (PluginBase& p) : processor (p)
{
    startTimerHz (15);
}

bool DuckCurveView::curveMode() const
{
    return (int) processor.apvts.getRawParameterValue ("scMode")->load() == (int) Sidechain::Mode::curve;
}

float DuckCurveView::depth (int layer) const
{
    return processor.apvts.getRawParameterValue (ParamID::bass (layer, BassField::duck))->load();
}

int DuckCurveView::numLayers() const
{
    return processor.isEffectVersion() ? 1 : ParamID::numBassLayers;
}

void DuckCurveView::timerCallback()
{
    const auto version = processor.sidechainCurve.version.load();
    const auto mode = curveMode();
    float d = 0.0f;
    for (int layer = 0; layer < numLayers(); ++layer)
        d += depth (layer) * (float) (layer + 1);
    if (version != lastVersion || mode != lastMode || d != lastDepths)
    {
        lastVersion = version;
        lastMode = mode;
        lastDepths = d;
        repaint();
    }
}

juce::Rectangle<float> DuckCurveView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 12.0f);
}

// Formen ritas och redigeras på fullt djup; varje lagers egen volym (djupet) ritas som en linje över den.
juce::Point<float> DuckCurveView::toScreen (float x, float duck) const
{
    const auto area = plotArea();
    return { area.getX() + x * area.getWidth(), area.getY() + duck * area.getHeight() };
}

juce::Point<float> DuckCurveView::fromScreen (juce::Point<float> p) const
{
    const auto area = plotArea();
    return { juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth()),
             juce::jlimit (0.0f, 1.0f, (p.y - area.getY()) / area.getHeight()) };
}

juce::Point<float> DuckCurveView::tensionHandle (size_t segment) const
{
    const auto& points = editing.empty() ? processor.sidechainCurve.getPoints() : editing;
    const auto& a = points[segment];
    const auto& b = points[segment + 1];
    const auto mid = SidechainCurve::shape (0.5f, a.tension);
    return toScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * mid);
}

int DuckCurveView::pointAt (juce::Point<float> p) const
{
    const auto& points = processor.sidechainCurve.getPoints();
    for (int i = 0; i < (int) points.size(); ++i)
        if (toScreen (points[(size_t) i].x, points[(size_t) i].y).getDistanceFrom (p) < grabDistance)
            return i;
    return -1;
}

int DuckCurveView::tensionHandleAt (juce::Point<float> p) const
{
    const auto& points = processor.sidechainCurve.getPoints();
    for (size_t i = 0; i + 1 < points.size(); ++i)
        if (tensionHandle (i).getDistanceFrom (p) < grabDistance)
            return (int) i;
    return -1;
}

void DuckCurveView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = plotArea();
    const auto editable = curveMode();

    // Rutnät: åttondelar av kurvans längd.
    for (int i = 1; i < 8; ++i)
    {
        g.setColour (Theme::grid);
        g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / 8.0f), area.getY(), area.getBottom());
    }
    g.setColour (Theme::grid);
    g.drawHorizontalLine ((int) area.getY(), area.getX(), area.getRight());

    // Volymkurvan: 1 - djup * duck. I kick-läget används kickens egen envelope.
    const auto& points = editing.empty() ? processor.sidechainCurve.getPoints() : editing;
    SidechainCurve preview;
    if (! editing.empty())
        preview.setPoints (editing);
    const auto& source = editing.empty() ? processor.sidechainCurve : preview;

    // Formen (fullt djup) och varje lagers volym: 1 - djup * form.
    juce::Path volume;
    std::vector<juce::Path> layerPaths ((size_t) numLayers());
    constexpr int resolution = 200;
    for (int i = 0; i <= resolution; ++i)
    {
        const auto x = (float) i / resolution;
        float duck;
        if (editable)
            duck = source.evaluate (x);
        else
        {
            // Kickens envelope över kickens längd (ungefärlig form för visning).
            const auto kp = processor.kickForDisplay (0);
            duck = KickEngine::amplitudeAt (kp, x * KickEngine::lengthSeconds (kp));
        }

        const auto point = toScreen (x, duck);
        if (i == 0)
            volume.startNewSubPath (area.getX(), area.getY());
        volume.lineTo (point);

        for (int layer = 0; layer < numLayers(); ++layer)
        {
            const auto layerPoint = toScreen (x, duck * depth (layer));
            if (i == 0)
                layerPaths[(size_t) layer].startNewSubPath (layerPoint);
            else
                layerPaths[(size_t) layer].lineTo (layerPoint);
        }
    }

    auto fill = volume;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    const auto colour = editable ? Theme::mix : Theme::kick;
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.35f), 0.0f, area.getY(),
                                             colour.withAlpha (0.05f), 0.0f, area.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colour.withAlpha (editable ? 0.8f : 0.45f));
    g.strokePath (volume, juce::PathStrokeType (1.5f));

    // Varje baslagers volym efter kicken (dess DUCK-djup): en linje per lager i lagrets färg.
    if (numLayers() > 1)
    {
        for (int layer = 0; layer < numLayers(); ++layer)
        {
            g.setColour (Theme::layerColour (layer));
            g.strokePath (layerPaths[(size_t) layer], juce::PathStrokeType (2.0f));
        }
    }

    g.setFont (Theme::font (10.0f, true));
    g.setColour (Theme::textDim);
    g.drawText (numLayers() > 1 ? "VOLUME AFTER EACH KICK  (lines = each bass layer)"
                                 : "VOLUME AFTER EACH KICK", getLocalBounds().reduced (12, 2).removeFromTop (12), juce::Justification::left);

    if (! editable)
    {
        g.setColour (Theme::textDim);
        g.drawText ("Follows the kick envelope. Set MODE to Curve to draw your own.",
                    area.toNearestInt().removeFromBottom (16), juce::Justification::centred);
        return;
    }

    // Punkterna.
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto& point = points[(size_t) i];
        const auto screen = toScreen (point.x, point.y);
        const auto active = i == hovered || i == draggedPoint;
        g.setColour (Theme::knobBodyDark);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (screen));
        g.setColour (Theme::mix);
        g.drawEllipse (juce::Rectangle<float> (active ? 12.0f : 10.0f, active ? 12.0f : 10.0f).withCentre (screen), 2.0f);
    }

    // Spänningshandtag: små romber mitt på varje segment.
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto handle = tensionHandle (i);
        juce::Path diamond;
        diamond.addPolygon (handle, 4, 4.0f, juce::MathConstants<float>::pi / 4.0f);
        g.setColour (Theme::mix.withAlpha ((int) i == draggedTension ? 1.0f : 0.5f));
        g.fillPath (diamond);
    }
}

void DuckCurveView::commit (std::vector<SidechainCurve::Point> points)
{
    processor.sidechainCurve.setPoints (std::move (points));
    processor.markStateChanged();
    repaint();
}

void DuckCurveView::mouseMove (const juce::MouseEvent& event)
{
    const auto index = curveMode() ? pointAt (event.position) : -1;
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void DuckCurveView::mouseDown (const juce::MouseEvent& event)
{
    if (! curveMode())
        return;

    auto points = processor.sidechainCurve.getPoints();
    const auto point = pointAt (event.position);

    // Högerklick tar bort en punkt (utom första och sista).
    if (event.mods.isPopupMenu())
    {
        if (point > 0 && point < (int) points.size() - 1)
        {
            points.erase (points.begin() + point);
            commit (std::move (points));
        }
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

void DuckCurveView::mouseDrag (const juce::MouseEvent& event)
{
    if (editing.empty())
        return;

    if (draggedPoint >= 0)
    {
        auto value = fromScreen (event.position);
        auto& point = editing[(size_t) draggedPoint];

        // Punkten får inte passera grannarna; första och sista ligger kvar på kanterna.
        const auto isEdge = draggedPoint == 0 || draggedPoint == (int) editing.size() - 1;
        if (! isEdge)
            point.x = juce::jlimit (editing[(size_t) draggedPoint - 1].x, editing[(size_t) draggedPoint + 1].x, value.x);
        point.y = value.y;
        repaint();
    }
    else if (draggedTension >= 0)
    {
        // Uppåt = mindre böjning nedåt; segmentets riktning avgör tecknet.
        const auto& a = editing[(size_t) draggedTension];
        const auto& b = editing[(size_t) draggedTension + 1];
        const auto direction = b.y >= a.y ? 1.0f : -1.0f;
        editing[(size_t) draggedTension].tension =
            juce::jlimit (-1.0f, 1.0f, tensionStart + direction * (event.position.y - dragStartY) / 80.0f);
        repaint();
    }
}

void DuckCurveView::mouseUp (const juce::MouseEvent&)
{
    if (! editing.empty() && (draggedPoint >= 0 || draggedTension >= 0))
        commit (editing);

    editing.clear();
    draggedPoint = draggedTension = -1;
}

void DuckCurveView::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! curveMode() || pointAt (event.position) >= 0)
        return;

    // Dubbelklick på en tom plats lägger till en punkt där.
    auto points = processor.sidechainCurve.getPoints();
    if ((int) points.size() >= SidechainCurve::maxPoints)
        return;

    const auto value = fromScreen (event.position);
    points.push_back ({ value.x, value.y, 0.0f });
    commit (std::move (points));
}

//==============================================================================
SidechainPage::SidechainPage (PluginBase& p)
    : presets (p, Presets::Section::sidechain, Theme::mix),
      mode (p.apvts, "scMode", "MODE"),
      length (p.apvts, "scLength", "LENGTH"),
      curve (p),
      scope (p, { { &p.bassBeforeDuckTap, 2, juce::Colours::white.withAlpha (0.6f), false, "BASS BEFORE" },
                  { &p.bassAfterDuckTap, 2, Theme::bass, true, "BASS DUCKED" },
                  { &p.kickTap, 2, Theme::kick, true, "KICK" },
                  { &p.duckGainTaps[0], 2, Theme::layerColour (0), false, "DUCK 1", true },
                  { &p.duckGainTaps[1], 2, Theme::layerColour (1), false, "DUCK 2", true },
                  { &p.duckGainTaps[2], 2, Theme::layerColour (2), false, "DUCK 3", true } })
{
    scope.setTooltip ("SIDECHAIN SCOPE - Live: the kick (orange), the bass before the sidechain (white outline) and after it (blue). "
                      "The blue is the bass the kick lets through: watch it being cut away under each kick. "
                      "The coloured curves in from the top and bottom edges show how much each bass layer is ducked (DUCK 1, 2, 3).");
    for (auto* c : std::initializer_list<juce::Component*> { &presets, &mode, &length, &curve, &scope })
        addAndMakeVisible (c);

    const auto numLayers = p.isEffectVersion() ? 1 : ParamID::numBassLayers;
    for (int layer = 0; layer < numLayers; ++layer)
    {
        const auto name = p.isEffectVersion() ? juce::String ("DEPTH") : "BASS " + juce::String (layer + 1);
        depths.push_back (std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, BassField::duck), name,
                                                           p.isEffectVersion() ? Theme::mix : Theme::layerColour (layer)));
        addAndMakeVisible (*depths.back());
        depths.back()->setLiveMeter ([&p, layer] { return p.duckReduction[(size_t) layer].load(); }, Theme::kick);
        depths.back()->setDescription ("DUCK sets how deeply the kick pushes this layer's volume down. 0 % = no ducking, 100 % = the volume goes all the way down. "
                                       "The orange arc inside the knob shows how much it is ducked right now.");
    }

    if (p.getMixer() != nullptr)
    {
        sources = std::make_unique<SourceToggles> (p, "Duck", "SIDECHAIN AFFECTS");
        addAndMakeVisible (*sources);
    }
}

void SidechainPage::resized()
{
    auto area = getLocalBounds();
    if (sources != nullptr)
    {
        sources->setBounds (area.removeFromBottom (24));
        area.removeFromBottom (2);
    }
    auto column = area.removeFromLeft (130);
    area.removeFromLeft (10);
    auto knobColumn = area.removeFromLeft (depths.size() > 1 ? 160 : 90);
    area.removeFromLeft (10);

    presets.setBounds (column.removeFromTop (24));
    column.removeFromTop (6);
    mode.setBounds (column.removeFromTop (40));
    column.removeFromTop (2);
    length.setBounds (column.removeFromTop (40));

    // Djupet per lager, två och två.
    const auto cell = depths.size() > 1 ? knobColumn.getWidth() / 2 : knobColumn.getWidth();
    for (size_t i = 0; i < depths.size(); ++i)
    {
        const auto row = (int) i / 2, col = (int) i % 2;
        depths[i]->setBounds (knobColumn.getX() + col * cell, knobColumn.getY() + row * 86, cell, 82);
    }
    curve.setBounds (area.removeFromLeft (area.getWidth() * 4 / 10));
    area.removeFromLeft (8);
    scope.setBounds (area);
}
