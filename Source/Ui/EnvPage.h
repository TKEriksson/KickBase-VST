#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "EnvelopeView.h"
#include "LfoPage.h"

// Fliken ENV: alla det valda instrumentets envelopes som diagram man kan redigera (dra handtagen): VOLUME (ENV 1, som formar ljudet),
// FILTER (filtrets envelope) och ENV 2 och ENV 3, som är modulationskällor: dra handtaget på en av dem till en ratt i fönstret ovanför.
class EnvPage : public juce::Component, private juce::Timer
{
public:
    explicit EnvPage (PluginBase& processor);
    ~EnvPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Tile
    {
        juce::String title, hint;
        juce::Colour colour;
        std::unique_ptr<EnvelopeView> view;
        std::unique_ptr<LfoDragHandle> handle;
        std::unique_ptr<juce::TextButton> modeButton;                    // MS | SYNC (ENV 2 och ENV 3)
        std::unique_ptr<juce::ButtonParameterAttachment> modeAttachment;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void rebuild();
    EnvelopeView::NoteOff noteOffOf (int forLayer, double example) const;

    PluginBase& processor;
    ParamRefs refs;
    InstrumentParams current;
    int layer = -1, lastActive = -1, lastNamesVersion = -1;
    juce::ComboBox instrumentBox;
    juce::Label instrumentLabel;
    std::vector<Tile> tiles;
};
