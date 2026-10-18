#include "Components.h"
#include "../Core/PluginBase.h"
#include "Help.h"

namespace
{
    juce::String percent (float amount)
    {
        return (amount > 0.0f ? "+" : "") + juce::String (juce::roundToInt (amount * 100.0f)) + " %";
    }

    void setWithGesture (juce::RangedAudioParameter& parameter, float normalised)
    {
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (normalised);
        parameter.endChangeGesture();
    }
}

void showParameterMenu (juce::Component& source, juce::RangedAudioParameter& parameter, juce::PopupMenu menu)
{
    // DAW:ens egna val (automation, länk till controller ...), om den erbjuder några.
    if (auto* editor = source.findParentComponentOfClass<juce::AudioProcessorEditor>())
    {
        if (auto* context = editor->getHostContext())
        {
            if (auto hostMenu = context->getContextMenuForParameter (&parameter))
            {
                const auto items = hostMenu->getEquivalentPopupMenu();
                if (items.getNumItems() > 0)
                {
                    if (menu.getNumItems() > 0)
                        menu.addSeparator();
                    for (juce::PopupMenu::MenuItemIterator it (items); it.next();)
                        menu.addItem (it.getItem());
                }
            }
        }
    }

    if (menu.getNumItems() > 0)
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&source));
}

PluginBase* findProcessor (const juce::Component& component)
{
    if (auto* editor = component.findParentComponentOfClass<juce::AudioProcessorEditor>())
        return dynamic_cast<PluginBase*> (editor->getAudioProcessor());
    return nullptr;
}

//==============================================================================
KnobComponent::KnobComponent (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& nameIn, juce::Colour accentIn)
    : name (nameIn), accent (accentIn)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setComponentID (paramID);
    slider.setName (nameIn);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (Theme::font (9.5f, true));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

    parameter = apvts.getParameter (paramID);
    if (parameter != nullptr)
    {
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        modulatable = Modulation::isModulatable (*parameter);
        description = Help::forParameter (paramID);
        if (description.isEmpty())
            description = parameter->getName (64);
    }
    updateTooltip (-1);
    startVisualTimer (Settings::Visual::other, 30);
}

KnobComponent::~KnobComponent() = default;

void KnobComponent::setDescription (const juce::String& text)
{
    description = text;
    updateTooltip (-1);
}

void KnobComponent::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromBottom (14));
    slider.setBounds (area);
}

juce::Point<float> KnobComponent::knobCentre() const
{
    return slider.getBounds().toFloat().reduced (Theme::knobRingMargin).getCentre();
}

float KnobComponent::trackRadius() const
{
    // Samma mått som KickbaseLookAndFeel::drawRotarySlider.
    const auto bounds = slider.getBounds().toFloat().reduced (Theme::knobRingMargin);
    return std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f - 3.0f;
}

float KnobComponent::ringRadius (int ring) const
{
    // Alla ringar ligger utanför värdebågen, i marginalen runt ratten.
    return trackRadius() + 4.5f + 3.0f * (float) ring;
}

int KnobComponent::parameterIndex() const
{
    if (cachedIndex < 0)
        if (auto* processor = findProcessor (*this); processor != nullptr && parameter != nullptr)
            cachedIndex = processor->getParameters().indexOf (parameter);
    return cachedIndex;
}

std::vector<KnobComponent::Ring> KnobComponent::rings() const
{
    std::vector<Ring> result;
    auto* processor = findProcessor (*this);
    const auto index = parameterIndex();
    if (processor == nullptr || index < 0)
        return result;

    for (int r = 0; r < Modulation::maxRoutes; ++r)
    {
        const auto& route = processor->modRoutes.routes[(size_t) r];
        const auto source = route.source.load();
        if (source >= 0 && route.target.load() == index)
            result.push_back ({ r, source, route.amount.load() });
    }
    return result;
}

int KnobComponent::ringAt (juce::Point<float> position) const
{
    const auto list = rings();
    if (list.empty())
        return -1;

    // Bara utanför värdebågen styr man en LFO-ring; ratten och bågen styr alltid värdet.
    const auto distance = position.getDistanceFrom (knobCentre());
    if (distance < trackRadius() + 2.0f || distance > ringRadius ((int) list.size() - 1) + 3.0f)
        return -1;

    auto best = 0;
    for (int i = 1; i < (int) list.size(); ++i)
        if (std::abs (distance - ringRadius (i)) < std::abs (distance - ringRadius (best)))
            best = i;
    return best;
}

void KnobComponent::timerCallback()
{
    followVisualRate();
    auto* processor = findProcessor (*this);
    if (processor == nullptr)
        return;

    // Rita om när kopplingarna ändras, och medan ratten moduleras (pricken rör sig).
    const auto version = processor->modRoutes.version.load();
    const auto offset = processor->modulator.currentOffset (parameterIndex());
    if (liveMeter)
    {
        const auto live = liveMeter();
        if (std::abs (live - lastLive) > 0.004f)
        {
            lastLive = live;
            repaint();
        }
    }
    if (version != lastRoutesVersion || offset != lastOffset)
    {
        if (version != lastRoutesVersion)
            updateTooltip (-1);
        lastRoutesVersion = version;
        lastOffset = offset;
        repaint();
    }
}

void KnobComponent::paintOverChildren (juce::Graphics& g)
{
    if (parameter == nullptr)
        return;

    const auto centre = knobCentre();
    const auto rotary = slider.getRotaryParameters();
    auto angleAt = [&] (float normalised)
    {
        return rotary.startAngleRadians + juce::jlimit (0.0f, 1.0f, normalised) * (rotary.endAngleRadians - rotary.startAngleRadians);
    };
    const auto value = parameter->getValue();

    if (dropHover)
    {
        g.setColour (Theme::lfo.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (trackRadius() * 2.0f + 8.0f, trackRadius() * 2.0f + 8.0f).withCentre (centre));
    }

    if (liveMeter && lastLive > 0.0f)
    {
        // Liveindikatorn: en båge inne i ratten, från start och så långt som värdet visar.
        const auto radius = trackRadius() - 6.0f;
        juce::Path live;
        live.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, angleAt (0.0f), angleAt (lastLive), true);
        g.setColour (liveColour.withAlpha (0.25f));
        g.strokePath (live, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (liveColour);
        g.strokePath (live, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto list = rings();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& ring = list[(size_t) i];
        const auto radius = ringRadius (i);
        const auto from = angleAt (value), to = angleAt (value + ring.amount);
        juce::Path path;
        path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, std::min (from, to), std::max (from, to), true);
        const auto colour = Theme::sourceColour (ring.source);
        g.setColour (colour.withAlpha (0.25f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (i == dragRing ? 4.0f : 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (! list.empty())
    {
        // Var ratten är just nu, med LFO:erna.
        const auto offset = findProcessor (*this) != nullptr ? findProcessor (*this)->modulator.currentOffset (parameterIndex()) : 0.0f;
        const auto dot = centre.getPointOnCircumference (trackRadius(), angleAt (value + offset));
        g.setColour (Theme::sourceColour (list.front().source));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (dot));
    }
}

void KnobComponent::updateTooltip (int hoveredRing)
{
    juce::String text;
    const auto list = rings();
    if (juce::isPositiveAndBelow (hoveredRing, (int) list.size()))
    {
        const auto& ring = list[(size_t) hoveredRing];
        text = name + " - " + Modulation::sourceName (ring.source) + " moves it " + percent (ring.amount)
             + ". Drag up/down to change how much, right-click to remove.";
    }
    else
    {
        text = name + " - " + description;
        if (! list.empty())
        {
            juce::StringArray sources;
            for (const auto& ring : list)
                sources.add (Modulation::sourceName (ring.source) + " " + percent (ring.amount));
            text << "  [Modulated by " << sources.joinIntoString (", ") << ": drag the ring to change]";
        }
        else if (modulatable)
            text << "  " << Help::modulationHint();
    }

    if (slider.getTooltip() != text)
        slider.setTooltip (text);
}

void KnobComponent::showMenu()
{
    auto* processor = findProcessor (*this);
    if (processor == nullptr || parameter == nullptr)
        return;

    juce::PopupMenu menu;
    for (const auto& ring : rings())
    {
        const auto route = ring.route;
        menu.addItem ("Remove " + Modulation::sourceName (ring.source) + " (" + percent (ring.amount) + ")", [processor, route]
        {
            processor->modRoutes.remove (route);
            processor->markStateChanged();
        });
    }
    auto* p = parameter;
    menu.addItem ("Reset to default", [p] { setWithGesture (*p, p->getDefaultValue()); });
    showParameterMenu (*this, *parameter, menu);
}

bool KnobComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return modulatable && details.description.toString().startsWith ("mod:");
}

void KnobComponent::itemDropped (const SourceDetails& details)
{
    dropHover = false;
    auto* processor = findProcessor (*this);
    const auto index = parameterIndex();
    if (processor != nullptr && index >= 0)
    {
        const auto source = details.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
        processor->modRoutes.add (juce::jlimit (0, Modulation::numSources - 1, source), index, 0.25f);
        processor->markStateChanged();
    }
    repaint();
}

//==============================================================================
void KnobComponent::ModSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        owner.showMenu();
        return;
    }

    const auto position = e.getEventRelativeTo (&owner).position;
    const auto ring = owner.ringAt (position);
    if (ring >= 0)
    {
        const auto list = owner.rings();
        owner.dragRing = ring;
        owner.dragRoute = list[(size_t) ring].route;
        owner.dragStartAmount = list[(size_t) ring].amount;
        owner.dragStartY = position.y;
        owner.repaint();
        return;
    }
    juce::Slider::mouseDown (e);
}

void KnobComponent::ModSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (owner.dragRing < 0)
    {
        juce::Slider::mouseDrag (e);
        return;
    }

    auto* processor = findProcessor (owner);
    if (processor == nullptr)
        return;
    const auto y = e.getEventRelativeTo (&owner).position.y;
    const auto sensitivity = e.mods.isShiftDown() ? 600.0f : 150.0f; // shift = finjustering
    const auto amount = juce::jlimit (-1.0f, 1.0f, owner.dragStartAmount + (owner.dragStartY - y) / sensitivity);
    processor->modRoutes.routes[(size_t) owner.dragRoute].amount = amount;
    owner.updateTooltip (owner.dragRing);
    owner.repaint();
}

void KnobComponent::ModSlider::mouseUp (const juce::MouseEvent& e)
{
    if (owner.dragRing < 0)
    {
        juce::Slider::mouseUp (e);
        return;
    }

    owner.dragRing = -1;
    if (auto* processor = findProcessor (owner))
        processor->markStateChanged();
    owner.repaint();
}

void KnobComponent::ModSlider::mouseMove (const juce::MouseEvent& e)
{
    juce::Slider::mouseMove (e);
    const auto ring = owner.ringAt (e.getEventRelativeTo (&owner).position);
    setMouseCursor (ring >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    owner.updateTooltip (ring);
}

void KnobComponent::ModSlider::mouseExit (const juce::MouseEvent& e)
{
    juce::Slider::mouseExit (e);
    owner.updateTooltip (-1);
}

//==============================================================================
ChoiceComponent::ChoiceComponent (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name)
{
    box.parameter = apvts.getParameter (paramID);
    if (box.parameter != nullptr)
    {
        box.addItemList (box.parameter->getAllValueStrings(), 1);
        const auto help = Help::forParameter (paramID);
        box.setTooltip (name + " - " + (help.isNotEmpty() ? help : box.parameter->getName (64)));
    }
    addAndMakeVisible (box);

    label.setText (name, juce::dontSendNotification);
    label.setFont (Theme::font (11.0f, true));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, box);
}

void ChoiceComponent::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (14));
    box.setBounds (area.removeFromTop (24));
}

void ChoiceComponent::Box::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && parameter != nullptr)
        showParameterMenu (*this, *parameter, {});
    else
        juce::ComboBox::mouseDown (e);
}

//==============================================================================
FxSendControl::FxSendControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& channelID, const juce::String& amountID,
                              const juce::String& name, juce::Colour accent)
{
    label.setText ("FX", juce::dontSendNotification);
    label.setFont (Theme::font (11.0f, true));
    label.setColour (juce::Label::textColourId, Theme::textDim);
    label.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (label);

    channel.parameter = apvts.getParameter (channelID);
    if (channel.parameter != nullptr)
    {
        channel.addItemList (channel.parameter->getAllValueStrings(), 1);
        channel.setTooltip ("FX SEND - " + Help::forParameter (channelID));
    }
    addAndMakeVisible (channel);

    amount.parameter = apvts.getParameter (amountID);
    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setColour (juce::Slider::trackColourId, accent);
    amount.setColour (juce::Slider::thumbColourId, Theme::text);
    amount.setPopupDisplayEnabled (true, true, nullptr);
    amount.setTooltip ("FX SEND AMOUNT - " + Help::forParameter (amountID));
    addAndMakeVisible (amount);

    channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, channelID, channel);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, amountID, amount);
    juce::ignoreUnused (name);
}

void FxSendControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromLeft (22));
    area.removeFromLeft (4);
    channel.setBounds (area.removeFromLeft (62));
    area.removeFromLeft (4);
    amount.setBounds (area);
}

void FxSendControl::AmountSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && parameter != nullptr)
        showParameterMenu (*this, *parameter, {});
    else
        juce::Slider::mouseDown (e);
}

//==============================================================================
InfoBar::InfoBar()
{
    setInterceptsMouseClicks (false, false);
    timerCallback(); // starttexten syns direkt
    startTimerHz (10);
}

void InfoBar::timerCallback()
{
    // Texten för kontrollen under musen (tooltip), om den hör till samma fönster.
    juce::String newText;
    auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    if (under != nullptr && under->getTopLevelComponent() == getTopLevelComponent())
    {
        for (auto* c = under; c != nullptr && newText.isEmpty(); c = c->getParentComponent())
            if (auto* client = dynamic_cast<juce::TooltipClient*> (c))
                newText = client->getTooltip();
    }
    if (newText.isEmpty())
        newText = "Hover over any control to see what it does.  Drag an LFO (LFO tab) onto a knob to modulate it.  "
                  "Right-click a control for DAW automation.";

    if (newText != text)
    {
        text = newText;
        repaint();
    }
}

void InfoBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::knobBodyDark.withAlpha (0.7f));
    g.fillRoundedRectangle (bounds, 4.0f);

    auto area = getLocalBounds().reduced (8, 0);
    const auto dash = text.indexOf (" - ");
    if (dash > 0)
    {
        // "NAMN - förklaring": namnet i fetstil.
        const auto title = text.substring (0, dash);
        g.setColour (Theme::text);
        g.setFont (Theme::font (12.0f, true));
        const auto width = juce::GlyphArrangement::getStringWidthInt (Theme::font (12.0f, true), title) + 10;
        g.drawText (title, area.removeFromLeft (width), juce::Justification::centredLeft);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawFittedText (text.substring (dash + 3), area, juce::Justification::centredLeft, 1, 0.85f);
    }
    else
    {
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        g.drawFittedText (text, area, juce::Justification::centredLeft, 1, 0.85f);
    }
}

//==============================================================================
void ControlGroups::layout (juce::Rectangle<int> area, int rowHeight)
{
    frames.clear();

    // Valrutor (rullgardinslistor) får en och en halv ratts bredd, så att texten i dem syns.
    auto weightOf = [] (const juce::Component* control)
    {
        if (const auto* choice = dynamic_cast<const ChoiceComponent*> (control))
            return choice->weight;
        return 1.0f;
    };

    for (const auto& row : rows)
    {
        // Varje rad fyller hela bredden.
        auto total = 0.0f;
        for (const auto& group : row)
            for (auto* control : group.controls)
                total += weightOf (control);
        const auto cellWidth = (float) area.getWidth() / std::max (1.0f, total);

        auto rowArea = area.removeFromTop (titleHeight + rowHeight);
        area.removeFromTop (3);
        for (const auto& group : row)
        {
            auto groupWeight = 0.0f;
            for (auto* control : group.controls)
                groupWeight += weightOf (control);
            auto groupArea = rowArea.removeFromLeft (juce::roundToInt (cellWidth * groupWeight));
            frames.push_back ({ groupArea.reduced (2, 0), group.title });

            auto controlsArea = groupArea.withTrimmedTop (titleHeight);
            for (auto* control : group.controls)
            {
                auto cell = controlsArea.removeFromLeft (juce::roundToInt (cellWidth * weightOf (control)));
                if (dynamic_cast<ChoiceComponent*> (control) != nullptr)
                    control->setBounds (cell.reduced (4, 0).withSizeKeepingCentre (cell.getWidth() - 8, 40));
                else
                    control->setBounds (cell);
            }
        }
    }
}

void ControlGroups::paint (juce::Graphics& g, juce::Colour accent) const
{
    for (const auto& [bounds, title] : frames)
    {
        const auto frame = bounds.toFloat();

        // Nedsänkt hårdvarufack: mörk sockel med skugga och 1px specular-toppkant
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xdd121420), frame.getX(), frame.getY(),
                                                 juce::Colour (0xdd181b28), frame.getX(), frame.getBottom(), false));
        g.fillRoundedRectangle (frame, 6.0f);

        // Mörk innerkant
        g.setColour (juce::Colour (0xff24283c));
        g.drawRoundedRectangle (frame.reduced (0.5f), 6.0f, 1.0f);

        // Specular highlight i överkant
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (frame.getX() + 6.0f, frame.getY() + 0.5f, frame.getRight() - 6.0f, frame.getY() + 0.5f, 1.0f);

        // Rubrik i ren, distinkt accentfärg med extra spärrning
        g.setColour (accent.withAlpha (0.92f));
        g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.08f));
        g.drawText (title, bounds.withHeight (titleHeight + 1).withTrimmedLeft (8), juce::Justification::centredLeft);
    }
}
