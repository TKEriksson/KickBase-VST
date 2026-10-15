#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "InstrumentProcessor.h"
#include "../Ui/FruitLookAndFeel.h"
#include "../Ui/EffectsPage.h"
#include "../Ui/Modules.h"
#include "../Ui/TabbedPanel.h"

class InstrumentEditor : public juce::AudioProcessorEditor
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
    KickModule kick { processor };
    BassModule bass { processor };
    TabbedPanel lower { &processor.selectedTab };
    PatternModule pattern { processor, Theme::bass, "BASS", true };
    EffectsPage effects { processor };
    MixModule mix { processor, true };
    juce::MidiKeyboardComponent keyboard { processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentEditor)
};
