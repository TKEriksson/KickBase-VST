#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "InstrumentProcessor.h"
#include "../Ui/EditorRoot.h"
#include "../Ui/InstrumentMenu.h"
#include "../Ui/KickbaseLookAndFeel.h"
#include "../Ui/AiChatPage.h"
#include "../Ui/EffectsPage.h"
#include "../Ui/LfoPage.h"
#include "../Ui/MasterPage.h"
#include "../Ui/EnvPage.h"
#include "../Ui/MatrixPage.h"
#include "../Ui/SettingsPage.h"
#include "../Ui/SidechainPage.h"
#include "../Ui/SoundstagePage.h"
#include "../Ui/AnalyzerPage.h"
#include "../Ui/Modules.h"
#include "../Ui/TabbedPanel.h"

// Överst syntarna (mönster, kick och instrumenten, ett i taget), nedanför verktygen (mönster, effekter, sidechain,
// LFO:er, master), så att mönstret syns och LFO:er kan dras till rattarna ovanför.
// DragAndDropContainer: LFO:er dras till rattarna.
class InstrumentEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
{
public:
    explicit InstrumentEditor (InstrumentProcessor&);
    ~InstrumentEditor() override;

    void resized() override;

private:
    juce::SharedResourcePointer<Theme::TextureCache> textures; // håller texturerna vid liv så länge editorn finns
    KickbaseLookAndFeel lookAndFeel; // först: måste leva längre än komponenterna som ritas med den
    InstrumentProcessor& processor;
    // Projektets egna instrumentfärger gäller i resten av gränssnittet: måste sättas före sidorna byggs.
    struct ColourSetup
    {
        explicit ColourSetup (PluginBase& p) { InstrumentMenu::applyColours (p); }
    } colourSetup { processor };
    EditorRoot root; // allt ligger i roten, som skalas efter fönstrets storlek
    bool sized = false; // true när startstorleken är satt (före det sparas ingen skala)

    HeaderBar header { processor, {} };
    TabbedPanel synths { &processor.selectedSynthTab };
    PatternModule pattern { processor, true };
    KickModule kick { processor };
    AiChatPage aiPage { processor };
    InstrumentHost instruments { processor };
    TabbedPanel tools { &processor.selectedTab };
    EffectsPage effects { processor };
    SidechainPage sidechain { processor };
    LfoPage lfo { processor };
    EnvPage env { processor };
    MatrixPage matrix { processor };
    SettingsPage settings { processor };
    MasterPage masterPage { processor };
    SoundstagePage soundstage { processor };
    AnalyzerPage analyzer { processor };
    InfoBar info;
    // Tangentbordet: en tonetikett som inte får plats på tangenten utelämnas (C10 klipptes annars till "C1").
    class Keyboard : public juce::MidiKeyboardComponent
    {
    public:
        using juce::MidiKeyboardComponent::MidiKeyboardComponent;
        juce::String getWhiteNoteText (int midiNoteNumber) override
        {
            auto text = juce::MidiKeyboardComponent::getWhiteNoteText (midiNoteNumber);
            if (text.isNotEmpty() && (float) text.length() * 6.0f > getKeyWidth() - 2.0f)
                return {};
            return text;
        }
    };
    Keyboard keyboard { processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    juce::TooltipWindow tooltips { this, 1200 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentEditor)
};
