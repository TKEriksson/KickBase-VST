#include "EffectsPage.h"

namespace
{
    constexpr int rowHeight = 20;
    constexpr int listTop = 30;   // under kedjans presetknapp
    constexpr int listWidth = 250;

    juce::String title (EffectType type) { return Effects::name (type); }

    juce::RangedAudioParameter* effectParam (PluginBase& processor, EffectType type, const char* suffix)
    {
        return processor.apvts.getParameter (Effects::parameterPrefix (type) + suffix);
    }
}

//==============================================================================
EffectSlotList::EffectSlotList (PluginBase& p) : processor (p)
{
    startTimerHz (10); // följer med presets och automation
}

juce::Rectangle<int> EffectSlotList::rowBounds (int slot) const
{
    return { 0, slot * rowHeight, getWidth(), rowHeight - 2 };
}

juce::Rectangle<int> EffectSlotList::powerArea (int slot) const    { return rowBounds (slot).withX (18).withWidth (22); }
juce::Rectangle<int> EffectSlotList::positionArea (int slot) const { return rowBounds (slot).removeFromRight (48).reduced (2, 2); }

int EffectSlotList::rowAt (int y) const
{
    return juce::jlimit (0, numEffects - 1, y / rowHeight);
}

void EffectSlotList::paint (juce::Graphics& g)
{
    for (int slot = 0; slot < numEffects; ++slot)
    {
        const auto type = processor.effectOrder.at (slot);
        const auto row = rowBounds (slot).toFloat();
        const auto on = effectParam (processor, type, "On")->getValue() >= 0.5f;
        const auto post = effectParam (processor, type, "Post")->getValue() >= 0.5f;
        const auto isSelected = type == selected;

        g.setColour (isSelected ? Theme::effects.withAlpha (0.18f) : Theme::knobBodyDark.withAlpha (0.7f));
        g.fillRoundedRectangle (row, 4.0f);
        if (isSelected)
        {
            g.setColour (Theme::effects.withAlpha (0.7f));
            g.drawRoundedRectangle (row.reduced (0.5f), 4.0f, 1.0f);
        }

        // Greppytan (dra för att flytta) och platsnummer.
        g.setColour (Theme::textDim.withAlpha (0.6f));
        g.setFont (Theme::font (9.0f, true));
        g.drawText (juce::String (slot + 1), row.withWidth (16.0f), juce::Justification::centred);

        // På/av-lampa.
        const auto lamp = powerArea (slot).toFloat().withSizeKeepingCentre (10.0f, 10.0f);
        g.setColour (on ? Theme::effects : Theme::textDim.withAlpha (0.35f));
        if (on)
            g.fillEllipse (lamp);
        else
            g.drawEllipse (lamp, 1.5f);

        g.setColour (on ? Theme::text : Theme::textDim);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (title (type), row.withTrimmedLeft (44.0f).withTrimmedRight (52.0f), juce::Justification::centredLeft);

        // PRE / POST.
        const auto pill = positionArea (slot).toFloat();
        const auto pillColour = post ? Theme::bass : Theme::kick;
        g.setColour (pillColour.withAlpha (on ? 0.25f : 0.1f));
        g.fillRoundedRectangle (pill, 3.0f);
        g.setColour (pillColour.withAlpha (on ? 1.0f : 0.5f));
        g.setFont (Theme::font (10.0f, true));
        g.drawText (post ? "POST" : "PRE", pill, juce::Justification::centred);
    }
}

void EffectSlotList::toggle (juce::RangedAudioParameter* parameter)
{
    Presets::setParameter (*parameter, parameter->getValue() >= 0.5f ? 0.0f : 1.0f);
    repaint();
}

void EffectSlotList::mouseDown (const juce::MouseEvent& event)
{
    dragged = false;
    dragSlot = -1;

    if (event.y >= numEffects * rowHeight)
        return;

    const auto slot = rowAt (event.y);
    const auto type = processor.effectOrder.at (slot);

    if (powerArea (slot).expanded (2).contains (event.getPosition()))
        toggle (effectParam (processor, type, "On"));
    else if (positionArea (slot).contains (event.getPosition()))
        toggle (effectParam (processor, type, "Post"));
    else
        dragSlot = slot;

    selected = type;
    if (onSelect)
        onSelect (type);
    repaint();
}

void EffectSlotList::mouseDrag (const juce::MouseEvent& event)
{
    if (dragSlot < 0)
        return;

    // Flytta effekten när musen passerar en annan rad.
    const auto target = rowAt (event.y);
    if (target != dragSlot)
    {
        processor.effectOrder.move (dragSlot, target);
        dragSlot = target;
        dragged = true;
        repaint();
    }
}

void EffectSlotList::mouseUp (const juce::MouseEvent&)
{
    if (dragged)
        processor.markStateChanged();

    dragSlot = -1;
}

//==============================================================================
EffectControls::EffectControls (PluginBase& p, EffectType type)
    : presets (p, Presets::effectSection (type), Theme::effects)
{
    addAndMakeVisible (presets);

    const auto prefix = Effects::parameterPrefix (type);
    auto knob = [&] (const char* suffix, const char* name)
    {
        controls.push_back (std::make_unique<KnobComponent> (p.apvts, (prefix + suffix).toRawUTF8(), name, Theme::effects));
        addAndMakeVisible (*controls.back());
    };
    auto choice = [&] (const char* suffix, const char* name)
    {
        controls.push_back (std::make_unique<ChoiceComponent> (p.apvts, (prefix + suffix).toRawUTF8(), name));
        addAndMakeVisible (*controls.back());
    };

    switch (type)
    {
        case EffectType::filter:
            choice ("Type", "TYPE"); choice ("Slope", "SLOPE");
            knob ("Cutoff", "CUTOFF"); knob ("Reso", "RESO"); knob ("Mix", "MIX");
            break;
        case EffectType::distortion:
            choice ("Mode", "MODE");
            knob ("Drive", "DRIVE"); knob ("Tone", "TONE"); knob ("Mix", "MIX");
            break;
        case EffectType::eq:
            knob ("Low", "LOW"); knob ("MidFreq", "MID FREQ"); knob ("Mid", "MID"); knob ("High", "HIGH");
            break;
        case EffectType::chorus:
            knob ("Rate", "RATE"); knob ("Depth", "DEPTH"); knob ("Delay", "DELAY"); knob ("Feedback", "FEEDBACK"); knob ("Mix", "MIX");
            break;
        case EffectType::phaser:
            knob ("Rate", "RATE"); knob ("Depth", "DEPTH"); knob ("Freq", "FREQ"); knob ("Feedback", "FEEDBACK"); knob ("Mix", "MIX");
            break;
        case EffectType::echo:
            choice ("Time", "TIME");
            knob ("Feedback", "FEEDBACK"); knob ("LowCut", "LOW CUT"); knob ("HighCut", "HIGH CUT"); knob ("Mix", "MIX");
            toggleButton = std::make_unique<juce::TextButton> ("PING PONG");
            toggleButton->setClickingTogglesState (true);
            toggleButton->setColour (juce::TextButton::buttonOnColourId, Theme::effects);
            addAndMakeVisible (*toggleButton);
            toggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, (prefix + "PingPong").toRawUTF8(), *toggleButton);
            break;
        case EffectType::reverb:
            knob ("Size", "SIZE"); knob ("Damping", "DAMPING"); knob ("Width", "WIDTH"); knob ("LowCut", "LOW CUT"); knob ("Mix", "MIX");
            break;
    }
}

void EffectControls::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (24);
    presets.setBounds (header.removeFromRight (160));

    // Valrutor och knappar får en smal kolumn till vänster, rattarna delar resten.
    auto column = area.removeFromLeft (110);
    std::vector<juce::Component*> knobs;
    for (auto& control : controls)
    {
        if (dynamic_cast<ChoiceComponent*> (control.get()) != nullptr)
        {
            control->setBounds (column.removeFromTop (42));
            column.removeFromTop (4);
        }
        else
            knobs.push_back (control.get());
    }
    if (toggleButton != nullptr)
        toggleButton->setBounds (column.removeFromTop (24).withWidth (100));

    const auto knobWidth = std::min (90, area.getWidth() / std::max (1, (int) knobs.size()));
    auto row = area.withHeight (std::min (area.getHeight(), 92));
    for (auto* k : knobs)
        k->setBounds (row.removeFromLeft (knobWidth));
}

//==============================================================================
EffectsPage::EffectsPage (PluginBase& p)
    : processor (p), chainPresets (p, Presets::Section::effects, Theme::effects), slots (p)
{
    addAndMakeVisible (chainPresets);
    addAndMakeVisible (slots);

    for (int i = 0; i < numEffects; ++i)
    {
        pages.push_back (std::make_unique<EffectControls> (p, (EffectType) i));
        addChildComponent (*pages.back());
    }

    slots.onSelect = [this] (EffectType type) { select (type); };
    select (EffectType::filter);
}

void EffectsPage::select (EffectType type)
{
    selected = type;
    slots.setSelected (type);
    for (int i = 0; i < numEffects; ++i)
        pages[(size_t) i]->setVisible (i == (int) type);
    repaint();
}

void EffectsPage::paint (juce::Graphics& g)
{
    // Rubrik för den valda effekten.
    auto right = getLocalBounds().withTrimmedLeft (listWidth + 16);
    g.setColour (Theme::effects);
    g.setFont (Theme::font (13.0f, true));
    g.drawText (title (selected), right.withHeight (24), juce::Justification::centredLeft);

    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    g.drawText ("CHAIN", getLocalBounds().withHeight (24), juce::Justification::centredLeft);

    // Var effekterna hamnar i förhållande till gaten.
    g.setFont (Theme::font (10.0f));
    g.drawText ("PRE = before the gate (gets chopped by it), POST = after (tails ring out). Drag a row to reorder.",
                right.removeFromBottom (14), juce::Justification::centredLeft);
}

void EffectsPage::resized()
{
    auto area = getLocalBounds();
    auto list = area.removeFromLeft (listWidth);
    area.removeFromLeft (16);

    chainPresets.setBounds (list.removeFromTop (24).withTrimmedLeft (50));
    list.removeFromTop (listTop - 24);
    slots.setBounds (list);

    for (auto& page : pages)
        page->setBounds (area);
}
