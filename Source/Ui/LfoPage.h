#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "PresetButton.h"

// Visar en LFO:s form och var den är just nu.
class LfoDisplay : public juce::Component, private juce::Timer
{
public:
    LfoDisplay (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    PluginBase& processor;
    const int index;
};

// Handtaget man drar till en ratt för att modulera den (som korset i Massive). Gäller vilken modulationskälla som helst (LFO, makro,
// ENV 2/3 ...): `source` är källans nummer (se Modulation).
class LfoDragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LfoDragHandle (int source, const juce::String& label = "DRAG TO A KNOB");
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    const int index;
    juce::String label;
};

// Listan över rattarna som LFO:n styr. Högerklick tar bort en koppling.
class LfoTargets : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LfoTargets (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::String describe() const;

    PluginBase& processor;
    const int index;
    int lastVersion = -1;
    juce::String text;
};

// En LFO: på/av, form, hastighet, handtaget och vad den styr.
class LfoPanel : public juce::Component
{
public:
    LfoPanel (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const int index;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    ChoiceComponent shape, rate;
    LfoDisplay display;
    LfoDragHandle handle;
    LfoTargets targets;
};

// Fliken LFO.
class LfoPage : public juce::Component
{
public:
    explicit LfoPage (PluginBase& processor);
    void resized() override;

private:
    PresetButton presets;
    std::vector<std::unique_ptr<LfoPanel>> panels;
};
