#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects.h"
#include "KickEngine.h"
#include "Master.h"
#include "Mixer.h"
#include "OfflineRender.h"
#include "Modulation.h"
#include "Params.h"
#include "Pattern.h"
#include "Presets.h"
#include "Sidechain.h"
#include "SignalTap.h"
#include "TransportStatus.h"
#include "SampleSlot.h"
#include "BridgeLink.h"

// Det som instrumentet och FX-versionen har gemensamt: parametrar, mönster, klicksampling,
// sparat läge och klockan.
class PluginBase : public juce::AudioProcessor
{
public:
    PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, bool effectVersion);

    bool isEffectVersion() const { return effectVersion; }

    // Mastersektionen ägs av underklassen (den behöver bara finnas där ljudet bearbetas).
    virtual const MasterSection& getMasterSection() const = 0;

    // Effektkanalerna ägs också av underklassen; gränssnittet läser deras uttag (EQ, kompressor).
    virtual const EffectRack& getEffectRack() const = 0;

    // Mixern (bara instrumentet): gain, panorering, mätare och bypass per ljudkälla.
    virtual const Mixer* getMixer() const { return nullptr; }

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

    // Renderar en kopia av det nuvarande tillståndet offline (den levande ljudtråden rörs inte). Kick eller bas
    // ensam stänger av den andra. Används av MCP-bryggans render_preview.
    juce::AudioBuffer<float> renderOffline (const OfflineRender::Settings& settings, ExportPart part);

    // Egna wavetables från WAV-filer, en per oscillator (lager 0-2, OSC A/B). Anropas från gränssnittet
    // (message thread); ljudtråden läser tabellerna via userTable. Sparas i projektet.
    bool loadUserTable (int layer, int osc, const juce::File& file);
    const Wavetable* userTable (int layer, int osc) const { return userTables[(size_t) (layer * ParamID::oscsPerLayer + osc)].load(); }
    juce::String getUserTableName (int layer, int osc) const { return userNames[(size_t) (layer * ParamID::oscsPerLayer + osc)]; }

    // AI-länken till MCP-bryggan (se BridgeLink). Av som standard; användaren slår på den med AI-knappen. Message thread.
    void setBridgeEnabled (bool enabled);
    bool isBridgeEnabled() const { return bridge != nullptr && bridge->isRunning(); }
    juce::String getBridgeId() const { return bridge != nullptr ? bridge->getId() : juce::String(); }

    // Kickens sampling (klick eller hel kick) och baslagrens samplingar. Anropas från gränssnittet (message thread).
    bool loadClickSample (const juce::File& file);
    void clearClickSample();
    juce::String getClickSampleName() const { return kickSample.name(); }
    const ClickSample* clickForDisplay() const { return getActiveClickSample(); } // message thread

    bool loadBassSample (int layer, const juce::File& file);
    void clearBassSample (int layer);
    juce::String getBassSampleName (int layer) const { return bassSamples[(size_t) juce::jlimit (0, ParamID::numBassLayers - 1, layer)].name(); }
    const ClickSample* getActiveBassSample (int layer) const { return bassSamples[(size_t) juce::jlimit (0, ParamID::numBassLayers - 1, layer)].active(); }

    juce::AudioProcessorValueTreeState apvts;
    Pattern pattern;
    SidechainCurve sidechainCurve;

    // Vad gränssnittet ritar i realtid: kicken, och basen före och efter duckningen under kicken.
    SignalTap kickTap, bassBeforeDuckTap, bassAfterDuckTap;

    // Hur mycket varje baslager duckas just nu: förstärkningen (1 = orörd) sample för sample, och
    // det senaste värdet som minskning 0..1 (för ringen runt DUCK-ratten).
    std::array<SignalTap, ParamID::numBassLayers> duckGainTaps;
    std::array<std::atomic<float>, ParamID::numBassLayers> duckReduction {};

    // Kickens pitch-envelope (halvtoner över svansen, upp till 0,5 s) och volym-envelope
    // (upp till 2 s), redigeras i KICK-fliken.
    NodeEnvelope kickPitchEnv { 0.5f, 72.0f, { { 0.0f, 42.0f, -0.6f }, { 0.045f, 0.0f, 0.0f } } };
    NodeEnvelope kickAmpEnv { 2.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 0.0004f, 1.0f, 0.0f }, { 0.015f, 1.0f, -0.5f }, { 0.155f, 0.0f, 0.0f } } };

    // Kickens inställningar som de är just nu, utan LFO (för gränssnittets bilder).
    KickParams kickForDisplay (int keyPitchClass) const;
    TransportStatus transportStatus;

    // Baslagrens namn: standard BASS 1-3, men man kan döpa dem själv. `layerNamesVersion` ökas vid ändring, så att
    // gränssnittet kan uppdatera sina texter.
    juce::String getLayerName (int layer) const
    {
        const auto index = (size_t) juce::jlimit (0, ParamID::numBassLayers - 1, layer);
        return layerNames[index].isEmpty() ? "BASS " + juce::String (layer + 1) : layerNames[index];
    }
    void setLayerName (int layer, const juce::String& name)
    {
        layerNames[(size_t) juce::jlimit (0, ParamID::numBassLayers - 1, layer)] = name.trim().substring (0, 10);
        ++layerNamesVersion;
    }
    std::atomic<int> layerNamesVersion { 0 };

    std::atomic<bool> followPlayhead { true }; // pianorullen och stegvyn bläddrar med uppspelningen
    std::atomic<int> patternVersion { 0 }; // ökas när mönstret ändras, så att gränssnittet ritar om
    int selectedTab = 0;                   // gränssnittets valda flikar (nedre och övre panelen), så att de
    int selectedSynthTab = 0;              // finns kvar när fönstret öppnas igen
    int selectedPatternMode = 0;           // PATTERN-flikens läge: 0 PATTERN (toner), 1 GATE
    std::array<int, ParamID::numBassLayers> selectedBassSource {}; // baslagrets valda källflik: OSC 1-3 eller SAMPLE
    int selectedPatternLayer = 0;          // PATTERN-flikens valda lager: 0-2 bas, 3 kick
    bool pianoRollOpen = false;            // PATTERN-flikens vy: stegen eller pianorullen
    int selectedFxView = 2;                // EFFECTS-sidans vy: 0 EQ, 1 COMP, 2 FX

protected:
    // En ny processor av samma slag, som exporten spelar med en kopia av inställningarna.
    virtual std::unique_ptr<PluginBase> createOfflineCopy() const = 0;

    // Anropas i prepareToPlay i underklasserna.
    void prepareBase (double sampleRate);

    // Läser DAW:ens position. Får bara anropas från processBlock.
    juce::Optional<juce::AudioPlayHead::PositionInfo> getHostPosition() const;

    // Klicksamplingen som ljudtråden ska använda just nu (kan vara nullptr).
    const ClickSample* getActiveClickSample() const { return kickSample.active(); }

    ParamRefs params;
    ParamRefs displayParams { apvts }; // utan modulering, för gränssnittet
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
    juce::ValueTree bassSamplesToValueTree() const;
    void bassSamplesFromValueTree (const juce::ValueTree& tree);

    // Originalet sparas i projektet; en omsamplad kopia spelas. Bara message thread och
    // prepareToPlay rör originalet, därför låset (aldrig på ljudtråden).
    static constexpr int numUserSlots = ParamID::numBassLayers * ParamID::oscsPerLayer;
    void setUserTable (int slot, std::unique_ptr<Wavetable> table, std::vector<float> frames, const juce::String& name);
    juce::ValueTree userTablesToValueTree() const;
    void userTablesFromValueTree (const juce::ValueTree& tree);

    std::array<std::atomic<const Wavetable*>, numUserSlots> userTables {};
    std::array<std::unique_ptr<Wavetable>, numUserSlots> userOwned;
    std::vector<std::unique_ptr<Wavetable>> retiredTables; // ljudtråden kan läsa en ersatt tabell ett block till
    std::array<std::vector<float>, numUserSlots> userFrames;
    std::array<juce::String, numUserSlots> userNames;

    std::unique_ptr<BridgeLink::Server> bridge;

    SampleSlot kickSample { 2.0 };
    std::array<SampleSlot, ParamID::numBassLayers> bassSamples { SampleSlot (12.0), SampleSlot (12.0), SampleSlot (12.0) };

    void resetSectionToDefaults (Presets::Section section, int layer);
    juce::String& presetNameSlot (Presets::Section section, int layer);

    const bool effectVersion;
    std::array<juce::String, Presets::numSections> presetNames; // bara message thread
    std::array<juce::String, ParamID::numBassLayers> bassPresetNames;
    std::array<juce::String, ParamID::numBassLayers> layerNames;

    double currentSampleRate = 44100.0;
};
