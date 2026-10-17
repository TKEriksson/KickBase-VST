#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FxProcessor.h"
#include "../Ui/EditorRoot.h"
#include "../Ui/FruitLookAndFeel.h"
#include "../Ui/AiChatPage.h"
#include "../Ui/EffectsPage.h"
#include "../Ui/LfoPage.h"
#include "../Ui/MasterPage.h"
#include "../Ui/MatrixPage.h"
#include "../Ui/SettingsPage.h"
#include "../Ui/SidechainPage.h"
#include "../Ui/SoundstagePage.h"
#include "../Ui/Modules.h"
#include "../Ui/TabbedPanel.h"

// DragAndDropContainer: LFO:er dras till rattarna.
class FxEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
{
public:
    explicit FxEditor (FxProcessor&);
    ~FxEditor() override;

    void resized() override;

private:
    juce::SharedResourcePointer<Theme::TextureCache> textures; // håller texturerna vid liv så länge editorn finns
    FruitLookAndFeel lookAndFeel; // först: måste leva längre än komponenterna som ritas med den
    FxProcessor& processor;
    EditorRoot root; // allt ligger i roten, som skalas efter fönstrets storlek
    bool sized = false; // true när startstorleken är satt (före det sparas ingen skala)

    HeaderBar header { processor, "FX" };
    TabbedPanel synths { &processor.selectedSynthTab };
    PatternModule pattern { processor, false };
    KickModule kick { processor };
    GateModule gate { processor };
    AiChatPage aiPage { processor };
    TabbedPanel tools { &processor.selectedTab };
    EffectsPage effects { processor };
    SidechainPage sidechain { processor };
    LfoPage lfo { processor };
    MatrixPage matrix { processor };
    SettingsPage settings { processor };
    MasterPage masterPage { processor };
    SoundstagePage soundstage { processor };
    InfoBar info;
    juce::TooltipWindow tooltips { this, 1200 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxEditor)
};
