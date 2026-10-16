#pragma once

#include "../Core/PluginBase.h"
#include "ChannelViews.h"
#include "Components.h"
#include "PresetButton.h"

// En kanals fem platser: klicka för att välja, dra i sidled för att byta ordning, högerklicka
// för att välja effekt direkt.
class EffectSlotStrip : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit EffectSlotStrip (PluginBase& processor);

    std::function<void (int)> onSelect;
    void setChannel (int newChannel) { channel = newChannel; repaint(); }
    void setSelected (int slot) { selected = slot; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    int slotAt (int x) const;
    juce::Rectangle<int> tileBounds (int slot) const;
    void showTypeMenu (int slot);

    PluginBase& processor;
    int channel = 0, selected = 0;
    int dragSlot = -1;
};

// Den valda platsens effekttyp och rattar. Rattarna byts ut när typen ändras.
class EffectSlotEditor : public juce::Component, private juce::Timer
{
public:
    explicit EffectSlotEditor (PluginBase& processor);

    void setSlot (int channel, int slot);
    void resized() override;

private:
    class ChoiceKnob;

    void timerCallback() override;
    void rebuild();

    PluginBase& processor;
    int channel = 0, slot = 0;
    EffectType shownType = EffectType::count; // tvingar fram en första uppbyggnad
    bool updatingType = false;

    juce::Label typeLabel;
    juce::ComboBox typeBox;
    std::vector<std::unique_ptr<juce::Component>> controls;
};

// Fliken EFFECTS: kanalvalet, kanalens nivå och duckning, platsraden och den valda platsens rattar.
class EffectsPage : public juce::Component
{
public:
    explicit EffectsPage (PluginBase& processor);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class View { eq, comp, fx };

    void selectChannel (int channel);
    void selectSlot (int slot);
    void selectView (View view);

    PluginBase& processor;
    int channel = 0, slot = 0;
    View view = View::fx;
    PresetButton presets;
    std::array<juce::TextButton, numEffectChannels> channelButtons;
    std::array<juce::TextButton, 3> viewButtons; // EQ, COMP, FX
    std::vector<std::unique_ptr<KnobComponent>> levels, ducks; // en per kanal, bara vald syns
    EffectSlotStrip strip;
    EffectSlotEditor editor;
    EqView eq;
    std::vector<std::unique_ptr<CompView>> comps; // en per kanal, bara vald syns
};
