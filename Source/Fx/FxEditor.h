#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FxProcessor.h"
#include "../Ui/FruitLookAndFeel.h"
#include "../Ui/EffectsPage.h"
#include "../Ui/LfoPage.h"
#include "../Ui/MasterPage.h"
#include "../Ui/SidechainPage.h"
#include "../Ui/Modules.h"
#include "../Ui/TabbedPanel.h"

// DragAndDropContainer: LFO:er dras till rattarna.
class FxEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
{
public:
    explicit FxEditor (FxProcessor&);
    ~FxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FruitLookAndFeel lookAndFeel; // först: måste leva längre än komponenterna som ritas med den
    FxProcessor& processor;

    HeaderBar header { processor, "FX" };
    KickModule kick { processor };
    GateModule gate { processor };
    TabbedPanel lower { &processor.selectedTab };
    PatternModule pattern { processor, Theme::gate, "GATE", false };
    EffectsPage effects { processor };
    SidechainPage sidechain { processor };
    LfoPage lfo { processor };
    MasterPage masterPage { processor };
    MixModule mix { processor, false };
    InfoBar info;
    juce::TooltipWindow tooltips { this, 1200 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxEditor)
};
