#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "KickEngine.h"
#include "Params.h"
#include "Pattern.h"
#include "TransportStatus.h"

// Det som instrumentet och FX-versionen har gemensamt: parametrar, mönster, klicksampling,
// sparat läge och klockan.
class PluginBase : public juce::AudioProcessor
{
public:
    PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout);

    void releaseResources() override {}
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Anropas av gränssnittet när mönstret ändrats, så att DAW:en vet att projektet ändrats.
    void patternChanged();

    // Fyller mönstret från en mall och ställer basens rutnät därefter.
    void applyPatternTemplate (PatternTemplate t);

    // Fabrikspresets. Bara i plugin:ets egen meny, inte som VST3-"programs": en värd kan
    // sätta program-parametern när ett projekt laddas och skulle då skriva över det sparade.
    void loadPreset (int index);
    juce::String getPresetName() const { return presetName; }

    // Klicksampling. Anropas från gränssnittet (message thread).
    bool loadClickSample (const juce::File& file);
    void clearClickSample();
    juce::String getClickSampleName() const;

    juce::AudioProcessorValueTreeState apvts;
    Pattern pattern;
    TransportStatus transportStatus;
    std::atomic<int> patternVersion { 0 }; // ökas när mönstret ändras, så att gränssnittet ritar om

protected:
    // Anropas i prepareToPlay i underklasserna.
    void prepareBase (double sampleRate);

    // Läser DAW:ens position. Får bara anropas från processBlock.
    juce::Optional<juce::AudioPlayHead::PositionInfo> getHostPosition() const;

    // Klicksamplingen som ljudtråden ska använda just nu (kan vara nullptr).
    const ClickSample* getActiveClickSample() const { return activeClick.load(); }

    ParamRefs params;
    TransportClock clock;

private:
    void setClickOriginal (juce::AudioBuffer<float> audio, double rate, const juce::String& name);
    void rebuildClickForSampleRate();
    juce::ValueTree clickToValueTree() const;
    void clickFromValueTree (const juce::ValueTree& tree);

    // Originalet sparas i projektet; en omsamplad kopia spelas. Bara message thread och
    // prepareToPlay rör originalet, därför låset (aldrig på ljudtråden).
    juce::CriticalSection clickLock;
    juce::AudioBuffer<float> clickOriginal;
    double clickOriginalRate = 0.0;
    juce::String clickName;

    juce::String presetName { "Init" }; // bara message thread

    double currentSampleRate = 44100.0;
    std::atomic<ClickSample*> activeClick { nullptr };
    std::unique_ptr<ClickSample> ownedClick;
    std::unique_ptr<ClickSample> retiredClick; // föregående, ljudtråden kan läsa den ett block till
};
