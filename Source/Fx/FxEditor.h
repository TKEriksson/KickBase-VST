#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FxProcessor.h"
#include "../Ui/FruitLookAndFeel.h"
#include "../Ui/Modules.h"

class FxEditor : public juce::AudioProcessorEditor
{
public:
    explicit FxEditor (FxProcessor&);
    ~FxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FruitLookAndFeel lookAndFeel; // först: måste leva längre än komponenterna som ritas med den
    FxProcessor& processor;

    HeaderBar header { processor.transportStatus, "FX" };
    KickModule kick { processor };
    GateModule gate { processor };
    PatternModule pattern { processor, Theme::gate, "GATE", false };
    MixModule mix { processor, false };
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxEditor)
};
