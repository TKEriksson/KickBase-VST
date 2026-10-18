#pragma once

#include "VisualTimer.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "PresetButton.h"

// Visar en LFO:s form och var den är just nu. Är formen Custom redigerar man den här: dra punkterna, dra rombernas mitt för att böja,
// dubbelklicka för en ny punkt, högerklicka för att ta bort en punkt (eller välja en mall). Dubbelklick på en vanlig form gör den till en
// egen kurva att redigera.
class LfoDisplay : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    LfoDisplay (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    bool isCustom() const;
    NodeEnvelope& curve() const { return processor.lfoCurves[(size_t) index]; }
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> point) const;
    juce::Point<float> tensionHandle (const std::vector<NodeEnvelope::Point>& points, size_t segment) const;
    int pointAt (juce::Point<float> point) const;
    int tensionHandleAt (juce::Point<float> point) const;
    void commit (std::vector<NodeEnvelope::Point> points);
    void makeCustom (int templateIndex);
    void showTemplates();
    static std::vector<NodeEnvelope::Point> templatePoints (int which);

    PluginBase& processor;
    const int index;
    std::vector<NodeEnvelope::Point> editing; // kopia under drag
    int draggedPoint = -1, draggedTension = -1, hovered = -1, lastVersion = -1;
    float tensionStart = 0.0f, dragStartY = 0.0f;
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

// En LFO: på/av, form, hastighet (tempo-synkad eller i Hz), handtaget och vad den styr.
class LfoPanel : public juce::Component, private juce::Timer
{
public:
    LfoPanel (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginBase& processor;
    const int index;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    ChoiceComponent shape, mode, rate;
    juce::Label hzLabel;
    juce::Slider hzSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hzAttachment;
    int lastMode = -1;
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
