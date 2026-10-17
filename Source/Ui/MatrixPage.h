#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "LfoPage.h"

// Fliken MATRIX: alla kopplingar mellan modulationskällor och rattar i en lista, som i Serum och Massive. Varje rad har en källa (LFO,
// makro, ett instruments ENV 2/3, velocity eller slump), hur mycket den rör och vilken ratt den styr. Överst till vänster: de fyra makrona,
// med handtag att dra till rattar.
class MatrixPage : public juce::Component, private juce::Timer
{
public:
    explicit MatrixPage (PluginBase& processor);
    ~MatrixPage() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    class Row;

    void timerCallback() override;
    void rebuild();
    void addRoute();
    juce::String sourceLabel (int source) const;

    PluginBase& processor;
    int lastVersion = -1, lastActive = -1, lastNamesVersion = -1;

    std::vector<std::unique_ptr<KnobComponent>> macroKnobs;
    std::vector<std::unique_ptr<LfoDragHandle>> macroHandles;
    juce::TextButton addButton { "+ ADD ROUTE" }, clearButton { "CLEAR ALL" };
    juce::Label countLabel, emptyLabel;
    juce::Rectangle<int> macroTitleArea;

    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<Row>> rows;

    friend class Row;
};
