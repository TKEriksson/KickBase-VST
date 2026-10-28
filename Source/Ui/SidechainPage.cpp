#include "SidechainPage.h"

#include "HelpDialog.h"

namespace
{
    constexpr float grabDistance = 9.0f;
}

DuckCurveView::DuckCurveView (PluginBase& p) : processor (p)
{
    startVisualTimer (Settings::Visual::sidechain, 15);
}

bool DuckCurveView::curveMode() const
{
    return (int) processor.apvts.getRawParameterValue ("scMode")->load() == (int) Sidechain::Mode::curve;
}

float DuckCurveView::depth (int layer) const
{
    return processor.apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::duck))->load();
}

int DuckCurveView::numLayers() const
{
    return processor.activeInstruments();
}

void DuckCurveView::timerCallback()
{
    followVisualRate();
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
    g.drawText (numLayers() > 1 ? "VOLUME AFTER EACH KICK  (lines = each instrument)"
                                 : "VOLUME AFTER EACH KICK", getLocalBounds().reduced (12, 2).removeFromTop (12), juce::Justification::left);

    if (! editable)
    {
        g.setColour (Theme::textDim);
        const juce::String text ("Follows the kick envelope. Set MODE to Curve to draw your own.");
        if ((float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) < area.getWidth() - 4.0f)
            g.drawText (text, area.toNearestInt().removeFromBottom (16), juce::Justification::centred);
        else // smal ruta (Kickbase FX): två rader i stället för en avklippt
            g.drawFittedText (text, area.toNearestInt().removeFromBottom (28), juce::Justification::centred, 2);
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
namespace
{
    std::vector<ScopeView::Trace> makeTraces (PluginBase& p)
    {
        std::vector<ScopeView::Trace> traces { { &p.instrumentBeforeDuckTap, 2, juce::Colours::white.withAlpha (0.6f), false, "INSTRUMENTS BEFORE" },
                                               { &p.instrumentAfterDuckTap, 2, Theme::bass, true, "INSTRUMENTS DUCKED" },
                                               { &p.kickTap, 2, Theme::kick, true, "KICK" } };
        for (int layer = 0; layer < (p.isEffectVersion() ? 1 : ParamID::numInstrumentLayers); ++layer)
        {
            ScopeView::Trace trace { &p.duckGainTaps[(size_t) layer], 2, Theme::layerColour (layer), false, "DUCK " + juce::String (layer + 1), true };
            trace.instrument = p.isEffectVersion() ? -1 : layer;
            traces.push_back (trace);
        }
        return traces;
    }
}

DuckRow::DuckRow (PluginBase& p, int layerIn) : processor (p), layer (layerIn)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    slider.setColour (juce::Slider::trackColourId, Theme::layerColour (layer));
    slider.setColour (juce::Slider::thumbColourId, Theme::text);
    slider.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setTooltip ("DUCK - How deeply the kick pushes this instrument's volume down. 0 % = no ducking, 100 % = the volume goes all the way down. "
                       "The bar under the slider shows how much it is ducked right now.");
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamID::bass (layer, InstrumentField::duck), slider);
    if (auto* parameter = p.apvts.getParameter (ParamID::bass (layer, InstrumentField::duck)))
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    startVisualTimer (Settings::Visual::sidechain, 20);
}

void DuckRow::timerCallback()
{
    followVisualRate();
    const auto now = processor.duckReduction[(size_t) layer].load();
    const auto name = processor.getLayerName (layer);
    if (std::abs (now - reduction) > 0.005f || name != shownName)
    {
        reduction = now;
        shownName = name;
        repaint();
    }
}

void DuckRow::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft (74);
    slider.setBounds (area.withTrimmedBottom (3));
}

void DuckRow::paint (juce::Graphics& g)
{
    g.setFont (Theme::font (10.5f, true));
    g.setColour (Theme::layerColour (layer));
    g.drawFittedText (processor.getLayerName (layer), getLocalBounds().withWidth (70), juce::Justification::centredLeft, 1, 0.75f);

    // Hur mycket som ducks just nu, som en smal stapel under reglaget.
    const auto bar = juce::Rectangle<float> ((float) slider.getX() + 4.0f, (float) getHeight() - 3.0f, (float) slider.getWidth() - 52.0f, 2.0f);
    g.setColour (Theme::knobBodyDark);
    g.fillRect (bar);
    g.setColour (Theme::kick);
    g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, reduction)));
}

//==============================================================================
namespace
{
    void fillChoice (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, juce::ComboBox& box)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            box.addItemList (choice->choices, 1);
    }

    juce::String choiceName (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            return choice->getCurrentChoiceName();
        return {};
    }

    void styleSlider (juce::Slider& s, juce::Colour colour)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 16);
        s.setColour (juce::Slider::trackColourId, colour);
        s.setColour (juce::Slider::thumbColourId, Theme::text);
        s.setColour (juce::Slider::textBoxTextColourId, Theme::text);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
}

FundamentalDuckPanel::Row::Row (PluginBase& p, int layerIn) : processor (p), layer (layerIn)
{
    auto& apvts = p.apvts;
    fillChoice (apvts, ParamID::bass (layer, InstrumentField::fundamentalDuckMode), modeBox);
    fillChoice (apvts, ParamID::bass (layer, InstrumentField::fundamentalDuckSource), sourceBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamID::bass (layer, InstrumentField::fundamentalDuckMode), modeBox);
    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamID::bass (layer, InstrumentField::fundamentalDuckSource), sourceBox);
    styleSlider (depthSlider, Theme::layerColour (layer));
    styleSlider (windowSlider, Theme::layerColour (layer));
    depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamID::bass (layer, InstrumentField::fundamentalDuckDepth), depthSlider);
    windowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamID::bass (layer, InstrumentField::fundamentalDuckWindow), windowSlider);

    modeBox.setTooltip ("FUNDAMENTAL DUCK MODE - INHERIT: this engine follows the values in the FUNDAMENTAL DUCK box above (shown dimmed). "
                        "OFF: this engine is never dipped. OWN: this engine has its own depth, window and source.");
    sourceBox.setTooltip ("SOURCE - What makes this engine's fundamental dip: the kick, another instrument, or any of them.");
    depthSlider.setTooltip ("DEPTH - How many dB this engine's fundamental is cut while the source sounds. It follows the last played voice.");
    windowSlider.setTooltip ("WINDOW - How close (in cents) the source's pitch must be to this engine's fundamental for the dip to apply fully. "
                             "It fades out to nothing at twice the window.");
    for (auto* c : std::initializer_list<juce::Component*> { &modeBox, &sourceBox, &depthSlider, &windowSlider })
        addAndMakeVisible (c);
}

void FundamentalDuckPanel::Row::update()
{
    auto& apvts = processor.apvts;
    const auto newDb = processor.fundamentalDuckDb[(size_t) layer].load();
    const auto newMode = (int) apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::fundamentalDuckMode))->load();
    const auto newInherited = juce::String ("from instrument: ") + juce::String (apvts.getRawParameterValue (ParamID::fundamentalDuckDepth)->load(), 1)
                              + " dB, " + juce::String (juce::roundToInt (apvts.getRawParameterValue (ParamID::fundamentalDuckWindow)->load()))
                              + " cents, " + choiceName (apvts, ParamID::fundamentalDuckSource);
    const auto name = processor.getLayerName (layer);
    if (std::abs (newDb - db) > 0.05f || newMode != mode || newInherited != inherited || name != shownName)
    {
        db = newDb;
        inherited = newInherited;
        shownName = name;
        if (newMode != mode)
        {
            mode = newMode;
            resized();
        }
        repaint();
    }
}

void FundamentalDuckPanel::Row::resized()
{
    auto area = getLocalBounds().reduced (0, 2);
    modeBox.setBounds (70, 2, 70, getHeight() - 4);
    area.removeFromLeft (216); // namn, ÄRV/AV/EGEN, lampa och dB-siffra (se paint)
    const auto own = mode == 2;
    for (auto* c : std::initializer_list<juce::Component*> { &depthSlider, &windowSlider, &sourceBox })
        c->setVisible (own);
    if (! own)
        return;
    sourceBox.setBounds (area.removeFromRight (80));
    area.removeFromRight (6);
    const auto half = area.getWidth() / 2;
    depthSlider.setBounds (area.removeFromLeft (half));
    windowSlider.setBounds (area);
}

void FundamentalDuckPanel::Row::paint (juce::Graphics& g)
{
    g.setFont (Theme::font (10.5f, true));
    g.setColour (Theme::layerColour (layer));
    g.drawFittedText (shownName, juce::Rectangle<int> (0, 0, 66, getHeight()), juce::Justification::centredLeft, 1, 0.75f);

    // Lampan: tänd (i instrumentets färg) medan motorn dämpar grundtonen, och hur många dB.
    const auto lit = db > 0.05f;
    const auto lamp = juce::Rectangle<float> (148.0f, (float) getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
    g.setColour (lit ? Theme::layerColour (layer) : Theme::knobBodyDark);
    g.fillEllipse (lamp);
    g.setColour (Theme::text.withAlpha (0.5f));
    g.drawEllipse (lamp, 1.0f);
    g.setFont (Theme::font (10.0f, false));
    g.setColour (lit ? Theme::text : Theme::textDim);
    g.drawText (lit ? juce::String (-db, 1) + " dB" : juce::String ("-"), juce::Rectangle<int> (162, 0, 48, getHeight()), juce::Justification::centredLeft);

    if (mode == 0) // ÄRV: instrumentets värden i grått
    {
        g.setColour (Theme::textDim.withAlpha (0.55f));
        g.drawText (inherited, juce::Rectangle<int> (216, 0, getWidth() - 216, getHeight()), juce::Justification::centredLeft);
    }
    else if (mode == 1)
    {
        g.setColour (Theme::textDim.withAlpha (0.55f));
        g.drawText ("off for this engine", juce::Rectangle<int> (216, 0, getWidth() - 216, getHeight()), juce::Justification::centredLeft);
    }
}

FundamentalDuckPanel::FundamentalDuckPanel (PluginBase& p) : processor (p)
{
    const juce::String help = "FUNDAMENTAL DUCK - Dips the fundamental of every engine while the kick sounds, so the kick's low end and the bass fundamental don't fight. "
                              "It follows the last played voice of each engine.";
    onButton.setClickingTogglesState (true);
    onButton.setColour (juce::TextButton::buttonOnColourId, Theme::kick.withAlpha (0.7f));
    onButton.setTooltip (help);
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::fundamentalDuckOn, onButton);
    depthKnob = std::make_unique<KnobComponent> (p.apvts, ParamID::fundamentalDuckDepth, "DEPTH", Theme::kick);
    windowKnob = std::make_unique<KnobComponent> (p.apvts, ParamID::fundamentalDuckWindow, "WINDOW", Theme::kick);
    sourceChoice = std::make_unique<ChoiceComponent> (p.apvts, ParamID::fundamentalDuckSource, "SOURCE");
    depthKnob->setDescription ("DEPTH - How many dB the fundamental of each engine is cut while the source sounds (engines set to INHERIT use this).");
    windowKnob->setDescription ("WINDOW - How close (in cents) the source's pitch must be to the engine's fundamental for the dip to apply fully. Fades out to nothing at twice the window.");
    for (auto* c : std::initializer_list<juce::Component*> { &onButton, depthKnob.get(), windowKnob.get(), sourceChoice.get(), &viewport })
        addAndMakeVisible (c);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
    {
        rows.push_back (std::make_unique<Row> (p, layer));
        content.addChildComponent (*rows.back());
    }
    startVisualTimer (Settings::Visual::sidechain, 10);
}

void FundamentalDuckPanel::timerCallback()
{
    followVisualRate();
    if (processor.activeInstruments() != lastActive)
    {
        lastActive = processor.activeInstruments();
        resized();
    }
    for (int i = 0; i < lastActive && i < (int) rows.size(); ++i)
        rows[(size_t) i]->update();
}

void FundamentalDuckPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::knobBodyDark.withAlpha (0.5f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::kick);
    g.drawText ("FUNDAMENTAL DUCK - ALL ENGINES", 8, 4, 220, 16, juce::Justification::centredLeft);
}

void FundamentalDuckPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto top = area.removeFromTop (82);
    top.removeFromTop (18);
    area.removeFromTop (4);
    onButton.setBounds (top.removeFromLeft (44).withHeight (24));
    top.removeFromLeft (6);
    depthKnob->setBounds (top.removeFromLeft (66));
    windowKnob->setBounds (top.removeFromLeft (66));
    sourceChoice->setBounds (top.removeFromLeft (110));

    viewport.setBounds (area);
    constexpr int rowHeight = 28;
    const auto active = std::max (processor.activeInstruments(), 0);
    const auto scrolling = active * rowHeight > area.getHeight();
    const auto width = area.getWidth() - (scrolling ? viewport.getScrollBarThickness() : 0);
    content.setSize (width, std::max (active * rowHeight, area.getHeight()));
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto shown = (int) i < active;
        rows[i]->setVisible (shown);
        if (shown)
            rows[i]->setBounds (0, (int) i * rowHeight, width, rowHeight);
    }
}

//==============================================================================
SidechainPage::SidechainPage (PluginBase& p)
    : processor (p),
      presets (p, Presets::Section::sidechain, Theme::mix),
      mode (p.apvts, "scMode", "MODE"),
      length (p.apvts, "scLength", "LENGTH"),
      curve (p),
      scope (p, makeTraces (p))
{
    scope.setVisual (Settings::Visual::sidechain);
    scope.setTooltip ("SIDECHAIN SCOPE - Live: the kick (orange), the instruments before the sidechain (white outline) and after it (blue). "
                      "The blue is what the kick lets through: watch it being cut away under each kick. "
                      "The coloured curves in from the top and bottom edges show how much each instrument is ducked.");
    for (auto* c : std::initializer_list<juce::Component*> { &presets, &mode, &length, &curve, &scope, &halftimeButton })
        addAndMakeVisible (c);
    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::sidechain, "the SIDECHAIN tab (ducking, curve, fundamental duck)");
    addAndMakeVisible (helpButton);
    if (! p.isEffectVersion())
    {
        fundamentalDuck = std::make_unique<FundamentalDuckPanel> (p);
        addAndMakeVisible (*fundamentalDuck);
    }

    halftimeButton.setClickingTogglesState (true);
    halftimeButton.setTooltip ("HALFTIME - The scope shows 8 beats instead of 4, so a halftime or long pattern (kick every other beat) fits. Only changes the view.");
    halftimeButton.onClick = [this] { scope.setBeats (halftimeButton.getToggleState() ? 8 : 4); };

    if (p.isEffectVersion())
    {
        depthKnob = std::make_unique<KnobComponent> (p.apvts, ParamID::bass (0, InstrumentField::duck), "DEPTH", Theme::mix);
        addAndMakeVisible (*depthKnob);
        depthKnob->setLiveMeter ([&p] { return p.duckReduction[0].load(); }, Theme::kick);
        depthKnob->setDescription ("DUCK sets how deeply the kick pushes the volume down. 0 % = no ducking, 100 % = the volume goes all the way down. "
                                   "The orange arc inside the knob shows how much it is ducked right now.");
    }
    else
    {
        depthViewport.setViewedComponent (&depthContent, false);
        depthViewport.setScrollBarsShown (true, false);
        depthViewport.setScrollBarThickness (10);
        addAndMakeVisible (depthViewport);
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        {
            depthRows.push_back (std::make_unique<DuckRow> (p, layer));
            depthContent.addChildComponent (*depthRows.back());
        }
    }

    if (p.getMixer() != nullptr)
    {
        sources = std::make_unique<SourceToggles> (p, "Duck", "SIDECHAIN AFFECTS");
        addAndMakeVisible (*sources);
    }
    startTimerHz (4);
}

void SidechainPage::timerCallback()
{
    if (processor.activeInstruments() != lastActive)
    {
        lastActive = processor.activeInstruments();
        resized();
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
    auto depthColumn = area.removeFromLeft (depthRows.empty() ? 90 : 230);
    area.removeFromLeft (10);

    auto presetRow = column.removeFromTop (24);
    helpButton.setBounds (presetRow.removeFromRight (44));
    presetRow.removeFromRight (4);
    presets.setBounds (presetRow);
    column.removeFromTop (6);
    mode.setBounds (column.removeFromTop (40));
    column.removeFromTop (2);
    length.setBounds (column.removeFromTop (40));
    column.removeFromTop (6);
    halftimeButton.setBounds (column.removeFromTop (22));

    // Djupet per instrument: en rad var, som rullar lodrätt när de inte får plats (reglagen ska inte bli små).
    if (depthKnob != nullptr)
        depthKnob->setBounds (depthColumn.withHeight (82));
    const auto active = processor.activeInstruments();
    constexpr int rowHeight = 30;
    if (! depthRows.empty())
    {
        depthViewport.setBounds (depthColumn);
        const auto scrolling = active * rowHeight > depthColumn.getHeight();
        const auto width = depthColumn.getWidth() - (scrolling ? depthViewport.getScrollBarThickness() : 0);
        depthContent.setSize (width, std::max (active * rowHeight, depthColumn.getHeight()));
        for (size_t i = 0; i < depthRows.size(); ++i)
        {
            const auto shown = (int) i < active;
            depthRows[i]->setVisible (shown);
            if (shown)
                depthRows[i]->setBounds (0, (int) i * rowHeight, width, rowHeight);
        }
    }
    if (fundamentalDuck != nullptr)
    {
        fundamentalDuck->setBounds (area.removeFromBottom (std::min (area.getHeight() / 2, 190)));
        area.removeFromBottom (6);
    }
    curve.setBounds (area.removeFromLeft (area.getWidth() * 4 / 10));
    area.removeFromLeft (8);
    scope.setBounds (area);
}
