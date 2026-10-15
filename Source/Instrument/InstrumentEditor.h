#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "InstrumentProcessor.h"
#include "../Ui/FruitLookAndFeel.h"
#include "../Ui/EffectsPage.h"
#include "../Ui/LfoPage.h"
#include "../Ui/MasterPage.h"
#include "../Ui/SidechainPage.h"
#include "../Ui/Modules.h"
#include "../Ui/TabbedPanel.h"

// Överst syntarna (kick och de tre baslagren), nedanför verktygen (mönster, effekter, sidechain,
// LFO:er, master), så att mönstret syns och LFO:er kan dras till rattarna ovanför.
// DragAndDropContainer: LFO:er dras till rattarna.
class InstrumentEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
{
public:
    explicit InstrumentEditor (InstrumentProcessor&);
    ~InstrumentEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FruitLookAndFeel lookAndFeel; // först: måste leva längre än komponenterna som ritas med den
    InstrumentProcessor& processor;

    HeaderBar header { processor, {} };
    TabbedPanel synths { &processor.selectedSynthTab };
    PatternModule pattern { processor, true };
    KickModule kick { processor };
    BassModule bass1 { processor, 0 }, bass2 { processor, 1 }, bass3 { processor, 2 };
    TabbedPanel tools { &processor.selectedTab };
    EffectsPage effects { processor };
    SidechainPage sidechain { processor };
    LfoPage lfo { processor };
    MasterPage masterPage { processor };
    InfoBar info;
    juce::MidiKeyboardComponent keyboard { processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::TooltipWindow tooltips { this, 1200 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentEditor)
};
