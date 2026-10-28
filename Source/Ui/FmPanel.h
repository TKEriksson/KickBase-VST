#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"
#include "../Core/UserChangeWatcher.h"

// FM-fliken i källpanelen (fas S2c): två operatorer (carrier och modulator, båda sinus). RATIO är modulatorns frekvens relativt carrier,
// INDEX hur kraftigt den moduleras, ENV AMT/ENV TIME hur snabbt indexet faller efter anslaget. PRESET fyller ratterna (och tonhöjdsenvelopen,
// ibland Filter 2); rör man en ratt efteråt hoppar menyn tillbaka till Custom.
class FmPanel : public juce::Component
{
public:
    FmPanel (PluginBase& processor, int layer, juce::Colour accent);
    ~FmPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Diagram; // vågform (två perioder) och index över tid, räknade ur ratterna

    void presetChanged (int index);

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    bool applyingPreset = false;

    juce::ToggleButton filterToggle;
    juce::TextButton helpButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterAttachment;
    std::unique_ptr<ChoiceComponent> preset;
    std::unique_ptr<KnobComponent> level, ratio, index, envAmount, envTime, semis;
    std::unique_ptr<Diagram> waveDiagram, envDiagram;
    std::unique_ptr<UserChangeWatcher> presetWatcher;
    std::vector<std::unique_ptr<UserChangeWatcher>> knobWatchers; // en ratt som ändras av användaren: preset -> Custom
};
