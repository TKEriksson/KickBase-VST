#include "EffectsPage.h"
#include "Help.h"

namespace
{
    void setNormalised (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float normalised)
    {
        if (auto* parameter = apvts.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (normalised);
            parameter->endChangeGesture();
        }
    }
}

//==============================================================================
EffectSlotStrip::EffectSlotStrip (PluginBase& p) : processor (p)
{
    setTooltip ("SLOTS - The channel's effects, run in order 1 to 5. Click a slot to edit it, right-click to choose its effect, "
                "drag a slot sideways to change the order.");
    startTimerHz (10); // följer med presets och automation
}

juce::Rectangle<int> EffectSlotStrip::tileBounds (int slot) const
{
    const auto width = getWidth() / slotsPerChannel;
    return { slot * width, 0, width - 4, getHeight() };
}

int EffectSlotStrip::slotAt (int x) const
{
    return juce::jlimit (0, slotsPerChannel - 1, x / std::max (1, getWidth() / slotsPerChannel));
}

void EffectSlotStrip::paint (juce::Graphics& g)
{
    for (int slot = 0; slot < slotsPerChannel; ++slot)
    {
        const auto type = Effects::slotType (processor.apvts, channel, slot);
        const auto tile = tileBounds (slot).toFloat();
        const auto on = type != EffectType::off;
        const auto isSelected = slot == selected;

        g.setColour (isSelected ? Theme::effects.withAlpha (0.2f) : Theme::knobBodyDark.withAlpha (0.7f));
        g.fillRoundedRectangle (tile, 5.0f);
        g.setColour (isSelected ? Theme::effects.withAlpha (0.8f) : Theme::panelBorder.withAlpha (on ? 0.9f : 0.5f));
        g.drawRoundedRectangle (tile.reduced (0.5f), 5.0f, isSelected ? 1.5f : 1.0f);

        g.setColour (Theme::textDim.withAlpha (0.7f));
        g.setFont (Theme::font (9.0f, true));
        g.drawText (juce::String (slot + 1), tile.reduced (6.0f, 2.0f), juce::Justification::topLeft);

        g.setColour (on ? Theme::effects : Theme::textDim.withAlpha (0.45f));
        g.setFont (Theme::font (12.0f, true));
        g.drawText (on ? Effects::name (type) : juce::String ("EMPTY"), tile.reduced (4.0f, 0.0f), juce::Justification::centred);
    }
}

void EffectSlotStrip::showTypeMenu (int slot)
{
    juce::PopupMenu menu;
    const auto names = Effects::typeNames();
    const auto current = (int) Effects::slotType (processor.apvts, channel, slot);
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, i == 0 ? juce::String ("Empty (off)") : names[i], true, i == current);

    const auto channelCopy = channel;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, slot, channelCopy] (int result)
    {
        if (result > 0)
        {
            Effects::setSlotType (processor.apvts, channelCopy, slot, (EffectType) (result - 1));
            selected = slot;
            if (onSelect)
                onSelect (slot);
            repaint();
        }
    });
}

void EffectSlotStrip::mouseDown (const juce::MouseEvent& event)
{
    dragSlot = slotAt (event.x);
    selected = dragSlot;
    if (onSelect)
        onSelect (dragSlot);
    if (event.mods.isPopupMenu())
    {
        showTypeMenu (dragSlot);
        dragSlot = -1;
    }
    repaint();
}

void EffectSlotStrip::mouseDrag (const juce::MouseEvent& event)
{
    if (dragSlot < 0)
        return;

    // Flytta platsen när musen passerar en annan; alla dess inställningar följer med.
    const auto target = slotAt (event.x);
    if (target != dragSlot)
    {
        Effects::moveSlot (processor.apvts, channel, dragSlot, target);
        dragSlot = selected = target;
        if (onSelect)
            onSelect (target);
        repaint();
    }
}

void EffectSlotStrip::mouseUp (const juce::MouseEvent&)
{
    dragSlot = -1;
}

//==============================================================================
// En valruta bland rattarna (t.ex. filtertyp), kopplad till en av platsens parametrar.
class EffectSlotEditor::ChoiceKnob : public juce::Component
{
public:
    ChoiceKnob (PluginBase& p, const juce::String& paramID, const Effects::Knob& knobIn)
        : processor (p), id (paramID), knob (knobIn)
    {
        label.setText (knob.name, juce::dontSendNotification);
        label.setFont (Theme::font (11.0f, true));
        label.setColour (juce::Label::textColourId, Theme::textDim);
        addAndMakeVisible (label);

        for (int i = 0; i < knob.choices.size(); ++i)
            box.addItem (knob.choices[i], i + 1);
        box.setTooltip (knob.name + " - " + Help::forEffectKnob (knob.name));
        box.onChange = [this]
        {
            if (! updating)
                setNormalised (processor.apvts, id, knob.toNormalised ((float) box.getSelectedItemIndex()));
        };
        addAndMakeVisible (box);
        refresh();
    }

    void refresh()
    {
        if (auto* raw = processor.apvts.getRawParameterValue (id))
        {
            const auto index = juce::roundToInt (knob.fromNormalised (raw->load()));
            if (box.getSelectedItemIndex() != index)
            {
                updating = true;
                box.setSelectedItemIndex (index, juce::dontSendNotification);
                updating = false;
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (14));
        box.setBounds (area.removeFromTop (24));
    }

private:
    PluginBase& processor;
    juce::String id;
    Effects::Knob knob;
    juce::Label label;
    juce::ComboBox box;
    bool updating = false;
};

EffectSlotEditor::EffectSlotEditor (PluginBase& p) : processor (p)
{
    typeLabel.setText ("EFFECT", juce::dontSendNotification);
    typeLabel.setFont (Theme::font (11.0f, true));
    typeLabel.setColour (juce::Label::textColourId, Theme::textDim);
    addAndMakeVisible (typeLabel);

    const auto names = Effects::typeNames();
    for (int i = 0; i < names.size(); ++i)
        typeBox.addItem (i == 0 ? juce::String ("Empty (off)") : names[i], i + 1);
    typeBox.setTooltip ("EFFECT - Which effect this slot runs. Choosing a new effect loads its default settings.");
    typeBox.onChange = [this]
    {
        if (! updatingType)
            Effects::setSlotType (processor.apvts, channel, slot, (EffectType) typeBox.getSelectedItemIndex());
    };
    addAndMakeVisible (typeBox);

    startTimerHz (10);
}

void EffectSlotEditor::setSlot (int newChannel, int newSlot)
{
    channel = newChannel;
    slot = newSlot;
    shownType = EffectType::count;
    rebuild();
}

void EffectSlotEditor::timerCallback()
{
    if (Effects::slotType (processor.apvts, channel, slot) != shownType)
        rebuild();
    for (auto& control : controls)
        if (auto* choice = dynamic_cast<ChoiceKnob*> (control.get()))
            choice->refresh();
}

void EffectSlotEditor::rebuild()
{
    shownType = Effects::slotType (processor.apvts, channel, slot);
    updatingType = true;
    typeBox.setSelectedItemIndex ((int) shownType, juce::dontSendNotification);
    updatingType = false;

    controls.clear();
    const auto& list = Effects::knobs (shownType);
    for (size_t k = 0; k < list.size() && k < (size_t) knobsPerSlot; ++k)
    {
        const auto& knob = list[k];
        const auto id = Effects::knobParamID (channel, slot, (int) k);

        if (knob.isChoice())
            controls.push_back (std::make_unique<ChoiceKnob> (processor, id, knob));
        else
        {
            auto control = std::make_unique<KnobComponent> (processor.apvts, id, knob.name, Theme::effects);
            control->slider.textFromValueFunction = [knob] (double v) { return knob.text (knob.fromNormalised ((float) v)); };
            control->setDescription (Help::forEffectKnob (knob.name));
            controls.push_back (std::move (control));
        }
        addAndMakeVisible (*controls.back());
    }
    resized();
    repaint();
}

void EffectSlotEditor::resized()
{
    auto area = getLocalBounds();
    auto first = area.removeFromLeft (130);
    typeLabel.setBounds (first.removeFromTop (14));
    typeBox.setBounds (first.removeFromTop (24));
    area.removeFromLeft (8);

    // Valrutor får en smal kolumn, rattarna delar resten.
    std::vector<juce::Component*> knobs;
    for (auto& control : controls)
    {
        if (dynamic_cast<ChoiceKnob*> (control.get()) != nullptr)
        {
            control->setBounds (first.removeFromTop (42).withTrimmedTop (4));
            first.removeFromTop (2);
        }
        else
            knobs.push_back (control.get());
    }

    const auto width = knobs.empty() ? 0 : std::min (96, area.getWidth() / (int) knobs.size());
    for (auto* knob : knobs)
        knob->setBounds (area.removeFromLeft (width));
}

//==============================================================================
EffectsPage::EffectsPage (PluginBase& p)
    : processor (p), presets (p, Presets::Section::effects, Theme::effects), strip (p), editor (p), eq (p)
{
    addAndMakeVisible (presets);

    const char* viewNames[] { "EQ", "COMP", "FX" };
    const char* viewHelp[] { "CHANNEL EQ - A parametric equalizer on this channel, with a live view of the sound reaching it. Flat (neutral) by default.",
                             "CHANNEL COMPRESSOR - Evens out the level of this channel and shows what it does to the waveform. Off (1:1) by default.",
                             "EFFECTS - The five effect slots of this channel." };
    for (size_t i = 0; i < viewButtons.size(); ++i)
    {
        auto& button = viewButtons[i];
        button.setButtonText (viewNames[i]);
        button.setTooltip (viewHelp[i]);
        button.setClickingTogglesState (false);
        button.setColour (juce::TextButton::buttonOnColourId, Theme::effects.withAlpha (0.55f));
        button.onClick = [this, i] { selectView ((View) i); };
        addAndMakeVisible (button);
    }
    addChildComponent (eq);

    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        auto& button = channelButtons[(size_t) ch];
        button.setButtonText (Effects::channelLabel (ch));
        button.setColour (juce::TextButton::buttonOnColourId, Theme::effects.withAlpha (0.55f));
        button.setTooltip ("FX CHANNEL " + Effects::channelLabel (ch) + " - A mixer channel for effects. Each instrument picks a channel "
                           "and how much of its sound goes through it (FX SEND on the instrument page).");
        button.onClick = [this, ch] { selectChannel (ch); };
        addAndMakeVisible (button);

        levels.push_back (std::make_unique<KnobComponent> (p.apvts, Effects::channelParamID (ch, "Level"), "LEVEL", Theme::effects));
        ducks.push_back (std::make_unique<KnobComponent> (p.apvts, Effects::channelParamID (ch, "Duck"), "DUCK", Theme::effects));
        levels.back()->setDescription ("Volume of this channel's effect output.");
        ducks.back()->setDescription ("DUCK sets how deeply the kick pushes this channel's volume down (tails too, so reverb and echo don't blur the kick). 0 % = no ducking, 100 % = the volume goes all the way down.");
        addChildComponent (*levels.back());
        addChildComponent (*ducks.back());

        comps.push_back (std::make_unique<CompView> (p, ch));
        addChildComponent (*comps.back());
    }

    strip.onSelect = [this] (int s) { selectSlot (s); };
    addAndMakeVisible (strip);
    addAndMakeVisible (editor);

    selectChannel (0);
    selectView ((View) juce::jlimit (0, 2, p.selectedFxView));
}

void EffectsPage::selectView (View newView)
{
    view = newView;
    processor.selectedFxView = (int) view;
    for (size_t i = 0; i < viewButtons.size(); ++i)
        viewButtons[i].setToggleState ((View) i == view, juce::dontSendNotification);

    strip.setVisible (view == View::fx);
    editor.setVisible (view == View::fx);
    eq.setVisible (view == View::eq);
    for (int ch = 0; ch < numEffectChannels; ++ch)
        comps[(size_t) ch]->setVisible (view == View::comp && ch == channel);
}

void EffectsPage::selectChannel (int newChannel)
{
    channel = newChannel;
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        channelButtons[(size_t) ch].setToggleState (ch == channel, juce::dontSendNotification);
        levels[(size_t) ch]->setVisible (ch == channel);
        ducks[(size_t) ch]->setVisible (ch == channel);
    }
    strip.setChannel (channel);
    eq.setChannel (channel);
    for (int ch = 0; ch < numEffectChannels; ++ch)
        comps[(size_t) ch]->setVisible (view == View::comp && ch == channel);
    selectSlot (slot);
}

void EffectsPage::selectSlot (int newSlot)
{
    slot = newSlot;
    strip.setSelected (slot);
    editor.setSlot (channel, slot);
}

void EffectsPage::paint (juce::Graphics&) {}

void EffectsPage::resized()
{
    auto area = getLocalBounds();

    // Elva kanaler i två kolumner (FX1-6 och FX7-11).
    auto left = area.removeFromLeft (112);
    const auto perColumn = (numEffectChannels + 1) / 2;
    const auto buttonHeight = std::min (34, (left.getHeight() - 4 * perColumn) / perColumn);
    const auto columnWidth = left.getWidth() / 2;
    for (size_t i = 0; i < channelButtons.size(); ++i)
        channelButtons[i].setBounds (left.getX() + ((int) i / perColumn) * columnWidth, left.getY() + ((int) i % perColumn) * (buttonHeight + 4),
                                     columnWidth - 4, buttonHeight);
    area.removeFromLeft (10);

    auto knobs = area.removeFromLeft (170);
    auto tabs = knobs.removeFromTop (28);
    const auto tabWidth = tabs.getWidth() / 3;
    for (auto& button : viewButtons)
        button.setBounds (tabs.removeFromLeft (tabWidth).reduced (2, 0));
    knobs.removeFromTop (4);
    const auto half = knobs.getWidth() / 2;
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        levels[(size_t) ch]->setBounds (knobs.withWidth (half));
        ducks[(size_t) ch]->setBounds (knobs.withTrimmedLeft (half));
    }
    area.removeFromLeft (10);

    auto top = area.removeFromTop (30);
    presets.setBounds (top.removeFromRight (150).withTrimmedBottom (4));
    top.removeFromRight (8);
    strip.setBounds (top);
    area.removeFromTop (6);
    editor.setBounds (area);
    eq.setBounds (area);
    for (auto& comp : comps)
        comp->setBounds (area);
}
