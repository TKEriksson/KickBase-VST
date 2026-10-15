#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "PresetButton.h"

// Listan över effektplatserna: på/av, före/efter gaten, och dra för att ändra ordning.
class EffectSlotList : public juce::Component, private juce::Timer
{
public:
    explicit EffectSlotList (PluginBase& processor);

    std::function<void (EffectType)> onSelect;
    void setSelected (EffectType type) { selected = type; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }

    int rowAt (int y) const;
    juce::Rectangle<int> rowBounds (int slot) const;
    juce::Rectangle<int> powerArea (int slot) const;
    juce::Rectangle<int> positionArea (int slot) const;
    void toggle (juce::RangedAudioParameter* parameter);

    PluginBase& processor;
    EffectType selected = EffectType::filter;
    int dragSlot = -1;
    bool dragged = false;
};

// En effekts rattar.
class EffectControls : public juce::Component
{
public:
    EffectControls (PluginBase& processor, EffectType type);
    void resized() override;

private:
    std::vector<std::unique_ptr<juce::Component>> controls;
    PresetButton presets;
    std::unique_ptr<juce::TextButton> toggleButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttachment;
};

// Fliken EFFECTS: platslistan till vänster, den valda effektens rattar till höger.
class EffectsPage : public juce::Component
{
public:
    explicit EffectsPage (PluginBase& processor);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void select (EffectType type);

    PluginBase& processor;
    PresetButton chainPresets;
    EffectSlotList slots;
    std::vector<std::unique_ptr<EffectControls>> pages;
    EffectType selected = EffectType::filter;
};
