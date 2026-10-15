#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects.h"
#include "KickEngine.h"
#include "Master.h"
#include "Modulation.h"
#include "Params.h"
#include "Pattern.h"
#include "Presets.h"
#include "Sidechain.h"
#include "TransportStatus.h"

// Det som instrumentet och FX-versionen har gemensamt: parametrar, mönster, klicksampling,
// sparat läge och klockan.
class PluginBase : public juce::AudioProcessor
{
public:
    PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, bool effectVersion);

    bool isEffectVersion() const { return effectVersion; }

    // Mastersektionen ägs av underklassen (den behöver bara finnas där ljudet bearbetas).
    virtual const MasterSection& getMasterSection() const = 0;

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

    // Som patternChanged, för annat som sparas utan att vara en parameter (t.ex. effektordningen).
    void markStateChanged();

    // Presets per del (se Presets.h). Bara i plugin:ets egen meny, inte som VST3-"programs":
    // en värd kan sätta program-parametern när ett projekt laddas och skulle då skriva över
    // det sparade. Anropas från gränssnittet (message thread).
    // `layer` gäller Bass (vilket baslager presetet laddas i eller sparas från).
    void loadFactoryPreset (Presets::Section section, int index, int layer = 0);
    bool saveUserPreset (Presets::Section section, const juce::String& name, int layer = 0);
    bool loadUserPreset (Presets::Section section, const juce::File& file, int layer = 0);
    juce::Array<juce::File> getUserPresets (Presets::Section section) const;
    juce::String getPresetName (Presets::Section section, int layer = 0) const;

    // Renderar ljudet offline till en WAV-fil som loopar: bars takter i DAW:ens senaste tempo och
    // med den senaste tonen. Kick eller bas ensam stänger av den andra. Anropas från gränssnittet.
    enum class ExportPart { full, kick, bass };
    bool exportWav (const juce::File& file, ExportPart part, int bars);

    // Klicksampling. Anropas från gränssnittet (message thread).
    bool loadClickSample (const juce::File& file);
    void clearClickSample();
    juce::String getClickSampleName() const;

    juce::AudioProcessorValueTreeState apvts;
    Pattern pattern;
    EffectOrder effectOrder;
    SidechainCurve sidechainCurve;
    TransportStatus transportStatus;
    std::atomic<int> patternVersion { 0 }; // ökas när mönstret ändras, så att gränssnittet ritar om
    int selectedTab = 0;                   // gränssnittets valda flikar (nedre och övre panelen), så att de
    int selectedSynthTab = 0;              // finns kvar när fönstret öppnas igen

protected:
    // En ny processor av samma slag, som exporten spelar med en kopia av inställningarna.
    virtual std::unique_ptr<PluginBase> createOfflineCopy() const = 0;

    // Anropas i prepareToPlay i underklasserna.
    void prepareBase (double sampleRate);

    // Läser DAW:ens position. Får bara anropas från processBlock.
    juce::Optional<juce::AudioPlayHead::PositionInfo> getHostPosition() const;

    // Klicksamplingen som ljudtråden ska använda just nu (kan vara nullptr).
    const ClickSample* getActiveClickSample() const { return activeClick.load(); }

    ParamRefs params;
    TransportClock clock;

public:
    // LFO-kopplingarna (gränssnittet lägger till och tar bort) och modulatorn som räknar ut dem.
    ModRoutes modRoutes;
    Modulator modulator { apvts, modRoutes };

protected:
    // Läser kickens och basens parametrar, med LFO-modulering.
    KickParams readModulatedKick (int keyPitchClass) const;
    BassParams readModulatedBass (int layer, int rootMidiNote) const;

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

    void resetSectionToDefaults (Presets::Section section, int layer);
    juce::String& presetNameSlot (Presets::Section section, int layer);

    const bool effectVersion;
    std::array<juce::String, Presets::numSections> presetNames; // bara message thread
    std::array<juce::String, ParamID::numBassLayers> bassPresetNames;

    double currentSampleRate = 44100.0;
    std::atomic<ClickSample*> activeClick { nullptr };
    std::unique_ptr<ClickSample> ownedClick;
    std::unique_ptr<ClickSample> retiredClick; // föregående, ljudtråden kan läsa den ett block till
};
