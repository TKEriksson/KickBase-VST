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
    bool isCustom() const;
    void editPoints(); // en vanlig form blir en egen kurva (samma form) att redigera med punkter

private:
    void timerCallback() override;
    NodeEnvelope& curve() const { return processor.lfoCurves[(size_t) index]; }
    juce::Rectangle<float> plotArea() const;
    bool isMini() const { return getHeight() < 40; } // minikurvan på ett fällt kort
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
    explicit LfoDragHandle (int source, const juce::String& label = "DRAG");
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

// En LFO som ett kort på fliken LFO: på/av, form, hastighet (tempo-synkad eller i Hz), handtaget och vad den styr. Fällt är kortet en rad
// (namn, minikurva, hastighet, handtaget). Pilen fäller, × eller högerklick tar bort LFO:n (frågar först om den styr någon ratt).
class LfoPanel : public juce::Component, private juce::Timer
{
public:
    LfoPanel (PluginBase& processor, int lfoIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    static constexpr int expandedHeight = 186, foldedHeight = 32;
    int getIndex() const { return index; }
    bool isFolded() const { return processor.lfoFolded >= 0 && ((processor.lfoFolded >> index) & 1) != 0; }
    std::function<void()> onLayoutChange; // fällt, utfällt eller borttaget: sidan lägger ut korten igen
    std::function<void()> onSelect;

private:
    void timerCallback() override;
    void updateVisibility();
    void toggleFold();
    void askToRemove(); // frågar om LFO:n styr någon ratt, annars tas den bort direkt
    void remove();
    juce::String rateText() const;

    PluginBase& processor;
    const int index;
    const juce::String name;
    juce::Rectangle<int> foldArea, removeArea;
    juce::String lastRateText;
    juce::Label confirmText;
    juce::TextButton confirmRemove { "REMOVE" }, confirmCancel { "CANCEL" };
    juce::TextButton editButton { "EDIT POINTS" };
    bool lastCustom = false;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    ChoiceComponent shape, mode, rate;
    juce::Label hzLabel;
    juce::Slider hzSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hzAttachment;
    int lastMode = -1, lastSnapNote = -2;
    bool lastSnapLocked = false;
    // SNAP i Hz-läget: knapp, zon (± %) och en text med närmaste notvärde och om hastigheten är låst mot det.
    juce::TextButton snapButton { "SNAP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapAttachment;
    juce::Slider snapRangeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> snapRangeAttachment;
    juce::Label snapInfo;
    LfoDisplay display;
    LfoDragHandle handle;
    LfoTargets targets;
};

// Fliken LFO: ett kort per aktiv LFO under varandra (rullbart), "+ ADD LFO" under det sista (fas M).
class LfoPage : public juce::Component, private juce::Timer
{
public:
    explicit LfoPage (PluginBase& processor);
    void resized() override;

private:
    void timerCallback() override;
    void layoutCards();
    void addLfo();
    void select (int index);

    PluginBase& processor;
    PresetButton presets;
    juce::TextButton onlySelected { "SHOW ONLY SELECTED" };
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<LfoPanel>> panels; // en per möjlig LFO (dolda när de inte är aktiva)
    juce::TextButton addButton { "+ ADD LFO" };
    int lastVersion = -1;
};
