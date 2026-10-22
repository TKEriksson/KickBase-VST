#include "FilterPanel.h"

#include "Help.h"
#include "../Core/Params.h"

namespace
{
    const juce::Colour filter2Colour { 0xffffa040 }; // Filter 2: orange (kurvan och ratterna), Filter 1 har lagrets färg

    constexpr float dbTop = 24.0f, dbBottom = -36.0f; // kurvans höjd i dB

    float frequencyAt (int index, int count) { return 20.0f * std::pow (1000.0f, (float) index / (float) (count - 1)); }

    // Samma utseende som ControlGroups (nedsänkt fack med rubrik).
    void paintGroupFrame (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, juce::Colour accent, int titleIndent)
    {
        const auto frame = bounds.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xdd121420), frame.getX(), frame.getY(),
                                                 juce::Colour (0xdd181b28), frame.getX(), frame.getBottom(), false));
        g.fillRoundedRectangle (frame, 6.0f);
        g.setColour (juce::Colour (0xff24283c));
        g.drawRoundedRectangle (frame.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (frame.getX() + 6.0f, frame.getY() + 0.5f, frame.getRight() - 6.0f, frame.getY() + 0.5f, 1.0f);
        g.setColour (accent.withAlpha (0.92f));
        g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.08f));
        g.drawText (title, bounds.withHeight (ControlGroups::titleHeight + 1).withTrimmedLeft (8 + titleIndent), juce::Justification::centredLeft);
    }
}

//==============================================================================
FilterResponseView::FilterResponseView (PluginBase& p, int layerIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), accent (accentIn), refs (p.apvts)
{
    setTooltip ("FILTER RESPONSE - How the filters change the sound across the frequencies (low on the left, high on the right; the line in the middle is 0 dB, "
                "= no change). The coloured line is FILTER 1, the orange line FILTER 2 and the filled shape what you get from both together. It shows the knob "
                "settings as they are; the filter envelope, KEYTRK and modulation move the real filter around them.");
    const auto n = 1 << fftOrder;
    impulse.assign ((size_t) n, 0.0f);
    spectrum.assign ((size_t) n * 2, 0.0f);
    for (auto* filter : { &filter1, &filter2, &filter2Alone })
        filter->prepare (displayRate);
    startVisualTimer (Settings::Visual::other, 15);
    timerCallback(); // första kurvan direkt (inte först när timern slår)
}

std::vector<float> FilterResponseView::keyOf (const InstrumentParams& p)
{
    return { (float) p.filterModel, p.cutoffHz, p.resonance, p.filterP1, p.filterP2, (float) p.filterType,
             p.filter2On ? 1.0f : 0.0f, (float) p.filter2Model, (float) p.filter2Type, p.filter2CutoffHz, p.filter2Resonance, p.filter2P1, p.filter2P2,
             (float) p.filterRouting, p.filterMix };
}

void FilterResponseView::timerCallback()
{
    followVisualRate();
    const auto params = refs.readInstrument (layer, 36);
    const auto key = keyOf (params);
    if (key == lastKey)
        return;
    lastKey = key;
    recompute (params);
    repaint();
}

void FilterResponseView::recompute (const InstrumentParams& p)
{
    const auto n = 1 << fftOrder;
    const auto limit = (float) displayRate * 0.45f;
    cutoff1 = juce::jlimit (20.0f, limit, p.cutoffHz);
    cutoff2 = juce::jlimit (20.0f, limit, p.filter2CutoffHz);
    filter2Shown = p.filter2On;

    for (auto* filter : { &filter1, &filter2, &filter2Alone })
        filter->reset();
    filter1.set (p.filterModel, cutoff1, p.resonance, p.filterP1, p.filterP2, (int) p.filterType);
    filter2.set (p.filter2Model, cutoff2, p.filter2Resonance, p.filter2P1, p.filter2P2, p.filter2Type);
    filter2Alone.set (p.filter2Model, cutoff2, p.filter2Resonance, p.filter2P1, p.filter2P2, p.filter2Type);

    constexpr float amplitude = 0.01f; // liten impuls: Ladder och TB-303 är olinjära, så svaret ska mätas i det nästan linjära området
    const auto parallel = p.filterRouting == 1;
    const auto mix = juce::jlimit (0.0f, 1.0f, p.filterMix);

    // Samma beräkning som i InstrumentSynth: Serial = Filter 2 tar Filter 1:s utsignal, Parallel = båda tar insignalen; summan = F1 + MIX * (F2 - F1).
    auto transform = [&] (const std::function<float (int)>& sampleAt, std::array<float, numPoints>& out)
    {
        for (int i = 0; i < n; ++i)
            spectrum[(size_t) i] = sampleAt (i);
        std::fill (spectrum.begin() + n, spectrum.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (spectrum.data());
        for (int i = 0; i < numPoints; ++i)
        {
            const auto bin = frequencyAt (i, numPoints) / (float) displayRate * (float) n;
            const auto lower = juce::jlimit (0, n / 2 - 1, (int) bin);
            const auto fraction = bin - (float) lower;
            const auto magnitude = (spectrum[(size_t) lower] * (1.0f - fraction) + spectrum[(size_t) lower + 1] * fraction) / amplitude;
            out[(size_t) i] = juce::Decibels::gainToDecibels (magnitude, -60.0f);
        }
    };

    // Ett svep per kurva (filtren nollställs mellan dem och körs om): enklast att hålla identiskt med synten.
    auto input = [amplitude] (int i) { return i == 0 ? amplitude : 0.0f; };
    transform ([&] (int i) { return filter1.process (input (i)); }, db1);
    filter1.reset();
    if (p.filter2On)
    {
        transform ([&] (int i) { return filter2Alone.process (input (i)); }, db2);
        transform ([&] (int i)
                   {
                       const auto x = input (i);
                       const auto y1 = filter1.process (x);
                       const auto y2 = filter2.process (parallel ? x : y1);
                       return y1 + mix * (y2 - y1);
                   }, dbSum);
    }
}

void FilterResponseView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff10121c));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff24283c));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto plot = bounds.reduced (8.0f, 6.0f).withTrimmedBottom (10.0f);
    auto xOfFrequency = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };
    auto yOfDb = [&] (float db) { return plot.getY() + plot.getHeight() * (dbTop - juce::jlimit (dbBottom, dbTop, db)) / (dbTop - dbBottom); };

    g.setFont (Theme::font (8.5f));
    for (const auto hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xOfFrequency (hz);
        const auto major = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;
        g.setColour (Theme::grid.withAlpha (major ? 1.0f : 0.55f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        if (major)
        {
            g.setColour (Theme::textDim);
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 10.0f), juce::Justification::centred);
        }
    }
    for (const auto db : { 12.0f, 0.0f, -12.0f, -24.0f })
    {
        g.setColour (db == 0.0f ? Theme::grid.brighter (0.45f) : Theme::grid);
        g.drawHorizontalLine ((int) yOfDb (db), plot.getX(), plot.getRight());
        g.setColour (Theme::textDim);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (plot.getX() + 1.0f, yOfDb (db) - 10.0f, 22.0f, 10.0f), juce::Justification::centredLeft);
    }

    auto pathOf = [&] (const std::array<float, numPoints>& db, bool closed)
    {
        juce::Path path;
        for (int i = 0; i < numPoints; ++i)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (numPoints - 1);
            const auto y = yOfDb (db[(size_t) i]);
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }
        if (closed)
        {
            path.lineTo (plot.getRight(), yOfDb (0.0f));
            path.lineTo (plot.getX(), yOfDb (0.0f));
            path.closeSubPath();
        }
        return path;
    };

    // Markeringar för cutoff (prickad linje).
    auto cutoffMarker = [&] (float hz, juce::Colour colour)
    {
        g.setColour (colour.withAlpha (0.45f));
        const auto x = xOfFrequency (hz);
        for (float y = plot.getY(); y < plot.getBottom(); y += 6.0f)
            g.drawVerticalLine ((int) x, y, juce::jmin (y + 2.0f, plot.getBottom()));
    };

    const auto& main = filter2Shown ? dbSum : db1;
    if (visualFilled())
    {
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.28f), 0.0f, plot.getY(), accent.withAlpha (0.04f), 0.0f, plot.getBottom(), false));
        g.fillPath (pathOf (main, true));
    }
    if (filter2Shown)
    {
        cutoffMarker (cutoff2, filter2Colour);
        g.setColour (filter2Colour.withAlpha (0.9f));
        g.strokePath (pathOf (db2, false), juce::PathStrokeType (1.3f));
        g.setColour (accent.withAlpha (0.9f));
        g.strokePath (pathOf (db1, false), juce::PathStrokeType (1.3f));
        g.setColour (Theme::text);
        g.strokePath (pathOf (dbSum, false), juce::PathStrokeType (2.0f));
    }
    else
    {
        g.setColour (accent);
        g.strokePath (pathOf (db1, false), juce::PathStrokeType (2.0f));
    }
    cutoffMarker (cutoff1, accent);

    // Förklaring uppe till höger.
    g.setFont (Theme::font (9.0f, true));
    auto legendArea = plot;
    auto legend = legendArea.removeFromTop (12.0f).removeFromRight (190.0f);
    if (filter2Shown)
    {
        g.setColour (Theme::text);
        g.drawText ("BOTH", legend.removeFromRight (44.0f), juce::Justification::centredRight);
        g.setColour (filter2Colour);
        g.drawText ("FILTER 2", legend.removeFromRight (64.0f), juce::Justification::centredRight);
    }
    g.setColour (accent);
    g.drawText ("FILTER 1", legend.removeFromRight (64.0f), juce::Justification::centredRight);
}

//==============================================================================
FilterPage::FilterPage (PluginBase& p, int layerIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), accent (accentIn), view (p, layerIn, accentIn)
{
    using F = InstrumentField;
    first.title = "FILTER 1";
    second.title = "FILTER 2";
    makeBlock (first, false);
    makeBlock (second, true);
    addAndMakeVisible (view);

    routing = std::make_unique<ChoiceComponent> (p.apvts, ParamID::bass (layer, F::filterRouting), "ROUTING");
    addAndMakeVisible (*routing);
    allpassFreq = std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, F::allpassFreq), "AP FREQ", accent);
    allpassAmount = std::make_unique<KnobComponent> (p.apvts, ParamID::bass (layer, F::allpassAmount), "AP AMT", accent);
    addAndMakeVisible (*allpassFreq);
    addAndMakeVisible (*allpassAmount);    routingNote.setText ("SERIAL: filter 1, then filter 2.   PARALLEL: both filter the same sound, MIX blends them.", juce::dontSendNotification);
    routingNote.setFont (Theme::font (9.0f));
    routingNote.setColour (juce::Label::textColourId, Theme::textDim);
    routingNote.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (routingNote);

    filterOnButton.setTooltip ("FILTER ON / OFF - Off: the whole instrument bypasses Filter 1 and Filter 2 (the amp envelope still works).");
    filterOnButton.setColour (juce::ToggleButton::tickColourId, accent);
    filterOnButton.setColour (juce::ToggleButton::textColourId, Theme::text);
    addAndMakeVisible (filterOnButton);
    filterOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::bass (layer, F::filterOn), filterOnButton);

    sourcesButton.setTooltip ("SOURCES THROUGH FILTER - Click to choose which sources are filtered (ticked = through Filter 1 and 2, unticked = bypasses both). "
                              "The same switch is on each source's own tab (OSC 1-3, SAMPLE; NOISE on the INSTR tab).");
    sourcesButton.onClick = [this] { showSourcesMenu(); };
    addAndMakeVisible (sourcesButton);
    for (int i = 0; i < numSources; ++i)
    {
        auto* parameter = p.apvts.getParameter (ParamID::bass (layer, (F) ((int) F::filterOsc1 + i)));
        sourceAttachments[(size_t) i] = std::make_unique<juce::ParameterAttachment> (*parameter, [this, i] (float value)
        {
            sourceOn[(size_t) i] = value >= 0.5f;
            updateSourcesText();
        });
        sourceAttachments[(size_t) i]->sendInitialUpdate();
    }
}

static const char* const sourceNames[] { "OSC 1", "OSC 2", "OSC 3", "SAMPLE", "NOISE" };

void FilterPage::updateSourcesText()
{
    juce::StringArray names;
    for (int i = 0; i < numSources; ++i)
        if (sourceOn[(size_t) i])
            names.add (sourceNames[i]);
    sourcesButton.setButtonText ((names.isEmpty() ? juce::String ("NONE (all bypass the filter)") : names.joinIntoString (", ")) + "  v");
}

// Flerval: menyn öppnas igen efter varje klick så att flera källor kan växlas i rad; Esc eller klick utanför stänger den.
void FilterPage::showSourcesMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Through the filter (click to toggle)");
    for (int i = 0; i < numSources; ++i)
        menu.addItem (i + 1, sourceNames[i], true, sourceOn[(size_t) i]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sourcesButton),
                        [safe = juce::Component::SafePointer<FilterPage> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;
                            const auto index = (size_t) (result - 1);
                            safe->sourceAttachments[index]->setValueAsCompleteGesture (safe->sourceOn[index] ? 0.0f : 1.0f);
                            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showSourcesMenu(); });
                        });
}

void FilterPage::makeBlock (Block& block, bool isSecond)
{
    using F = InstrumentField;
    auto id = [this] (F f) { return ParamID::bass (layer, f); };
    const auto colour = isSecond ? filter2Colour : accent;

    block.model = std::make_unique<ChoiceComponent> (processor.apvts, id (isSecond ? F::filter2Model : F::filterModel), "MODEL");
    block.type = std::make_unique<ChoiceComponent> (processor.apvts, id (isSecond ? F::filter2Type : F::filterType), "TYPE");
    block.cutoff = std::make_unique<KnobComponent> (processor.apvts, id (isSecond ? F::filter2Cutoff : F::cutoff), "CUTOFF", colour);
    block.reso = std::make_unique<KnobComponent> (processor.apvts, id (isSecond ? F::filter2Reso : F::resonance), "RESO", colour);
    block.p1 = std::make_unique<KnobComponent> (processor.apvts, id (isSecond ? F::filter2P1 : F::filterP1), "P1", colour);
    block.p2 = std::make_unique<KnobComponent> (processor.apvts, id (isSecond ? F::filter2P2 : F::filterP2), "P2", colour);
    block.extra = std::make_unique<KnobComponent> (processor.apvts, id (isSecond ? F::filterMix : F::keyTrack), isSecond ? "MIX" : "KEYTRK", colour);
    for (auto* c : std::initializer_list<juce::Component*> { block.model.get(), block.type.get(), block.cutoff.get(), block.reso.get(), block.p1.get(), block.p2.get(), block.extra.get() })
        addAndMakeVisible (c);

    block.modelAttachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter (id (isSecond ? F::filter2Model : F::filterModel)),
                                                                          [this, &block, isSecond] (float value) { block.modelIndex = (int) std::lround (value); refresh (block, isSecond); });
    if (isSecond)
    {
        block.power = std::make_unique<juce::ToggleButton>();
        block.power->setColour (juce::ToggleButton::tickColourId, colour);
        block.power->setTooltip ("FILTER 2 ON / OFF - Turns the second filter on. Off, it costs no CPU and the sound is exactly as with FILTER 1 alone.");
        addAndMakeVisible (*block.power);
        block.powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id (F::filter2On), *block.power);
        block.onAttachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter (id (F::filter2On)),
                                                                           [this, &block] (float value) { block.isOn = value >= 0.5f; refresh (block, true); });
        block.onAttachment->sendInitialUpdate();
    }
    block.modelAttachment->sendInitialUpdate();
}

// Det som följer modellen: TYPE gäller bara Kickbase (0) och Ladder (2); P1 och P2 byter namn och tonas ned när modellen inte använder dem;
// ett avstängt Filter 2 tonas ned helt.
void FilterPage::refresh (Block& block, bool isSecond)
{
    const auto model = juce::jlimit (0, 4, block.modelIndex);
    const char* modelNames[] { "Kickbase", "TB-303", "Ladder", "Formant", "Comb" };
    const char* p1Names[] { "", "", "DRIVE", "MORPH", "DAMPING" };
    const char* p2Names[] { "", "", "", "BLEND", "BLEND" };
    const auto typeUsed = model == 0 || model == 2;
    const auto dim = block.isOn ? 1.0f : 0.45f;
    using F = InstrumentField;
    auto id = [this, isSecond] (F first, F other) { return ParamID::bass (layer, isSecond ? other : first); };

    block.model->setAlpha (dim);
    block.cutoff->setAlpha (dim);
    block.reso->setAlpha (dim);
    block.extra->setAlpha (dim);
    block.type->setAlpha (typeUsed ? dim : 0.4f * dim);
    block.type->box.setTooltip (typeUsed ? Help::forParameter (id (F::filterType, F::filter2Type))
                                         : "TYPE - ignored with the " + juce::String (modelNames[model]) + " model (it has its own shape). Switch MODEL to Kickbase or Ladder to use TYPE.");

    auto setP = [&] (KnobComponent& knob, const char* name, const char* unusedName, F first, F other)
    {
        const auto used = juce::String (name).isNotEmpty();
        knob.setLabelText (used ? name : unusedName);
        knob.setAlpha (used ? dim : 0.4f * dim);
        knob.setDescription (used ? Help::forParameter (id (first, other)) + " Here: " + juce::String (name) + "."
                                  : "Not used by the " + juce::String (modelNames[model]) + " model. Switch MODEL to Ladder, Formant or Comb to use it.");
    };
    setP (*block.p1, p1Names[model], "P1", F::filterP1, F::filter2P1);
    setP (*block.p2, p2Names[model], "P2", F::filterP2, F::filter2P2);
}

void FilterPage::layoutBlock (Block& block, juce::Rectangle<int> area)
{
    block.frame = area;
    auto content = area.withTrimmedTop (ControlGroups::titleHeight + 2).reduced (4, 0);
    if (block.power != nullptr)
        block.power->setBounds (area.getX() + 6, area.getY() - 1, 16, ControlGroups::titleHeight + 3);

    auto choices = content.removeFromLeft (104);
    const auto choiceHeight = juce::jmin (38, choices.getHeight() / 2);
    block.model->setBounds (choices.removeFromTop (choiceHeight).reduced (0, 0));
    block.type->setBounds (choices.removeFromTop (choiceHeight));
    const auto width = content.getWidth() / 5;
    for (auto* knob : { block.cutoff.get(), block.reso.get(), block.p1.get(), block.p2.get() })
        knob->setBounds (content.removeFromLeft (width));
    block.extra->setBounds (content);
}

void FilterPage::paint (juce::Graphics& g)
{
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f));
    g.drawText ("THROUGH FILTER:", switchCaption, juce::Justification::centredLeft);
    paintGroupFrame (g, first.frame, first.title, accent, 0);
    paintGroupFrame (g, second.frame, second.title, filter2Colour, 20);
}

void FilterPage::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    constexpr int blockWidth = 400;
    auto right = area.removeFromRight (blockWidth);
    area.removeFromRight (6);
    auto switchRow = area.removeFromTop (22);
    area.removeFromTop (2);
    filterOnButton.setBounds (switchRow.removeFromLeft (104));
    switchRow.removeFromLeft (10);
    switchCaption = switchRow.removeFromLeft (100);
    sourcesButton.setBounds (switchRow.removeFromLeft (juce::jmin (300, switchRow.getWidth())));

    auto bottom = area.removeFromBottom (58);
    area.removeFromBottom (4);
    view.setBounds (area);
    allpassAmount->setBounds (bottom.removeFromRight (74));
    allpassFreq->setBounds (bottom.removeFromRight (74));
    bottom.removeFromRight (8);
    routing->setBounds (bottom.removeFromLeft (120).withTrimmedBottom (2).withHeight (juce::jmin (40, bottom.getHeight())));
    bottom.removeFromLeft (10);
    routingNote.setBounds (bottom.withTrimmedTop (14).withHeight (26));

    const auto half = (right.getHeight() - 6) / 2;
    layoutBlock (first, right.removeFromTop (half));
    right.removeFromTop (6);
    layoutBlock (second, right);
}
