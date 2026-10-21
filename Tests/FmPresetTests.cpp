// Test av FM-presetens laddningsrisk: ett presetval fyller ratterna bara när användaren väljer, aldrig vid laddning eller automation.

#include "../Source/Core/Params.h"
#include "../Source/Core/UserChangeWatcher.h"
#include "../Source/Ui/FmPresets.h"
#include "TestUtils.h"
#include <cmath>

namespace
{
    constexpr int layer = 0;

    struct FmProcessor : juce::AudioProcessor
    {
        FmProcessor() : apvts (*this, nullptr, "Parameters", createLayout()) {}

        static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
        {
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            Params::addInstrumentCommonParams (layout, layer, 0.8f);
            Params::addInstrumentSynthParams (layout, layer);
            return layout;
        }

        static juce::String id (InstrumentField field) { return ParamID::bass (layer, field); }
        float get (InstrumentField field) { return apvts.getRawParameterValue (id (field))->load(); }

        // Som gränssnittet: en gest runt ändringen. Utan gest = laddning eller automation.
        void set (InstrumentField field, float plain, bool gesture)
        {
            auto* p = apvts.getParameter (id (field));
            if (gesture)
                p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            if (gesture)
                p->endChangeGesture();
        }

        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
    };

    // Samma koppling som FmPanel: presetvalet körs bara på användarens ändring.
    struct PresetHook
    {
        explicit PresetHook (FmProcessor& p)
            : processor (p),
              watcher (*p.apvts.getParameter (FmProcessor::id (InstrumentField::fmPreset)), [this] (float value)
              {
                  const auto index = (int) std::lround (value);
                  if (index < 1 || applying)
                      return;
                  applying = true;
                  FmPresets::apply (processor.apvts, layer, index);
                  applying = false;
              }) {}

        FmProcessor& processor;
        bool applying = false;
        UserChangeWatcher watcher;
    };

    bool closeTo (float a, float b) { return std::abs (a - b) < 1.0e-3f; }

    void testPresetOnlyOnUserChoice()
    {
        std::puts ("FM-preset: bara användarens val fyller i, inte laddning eller automation");
        using F = InstrumentField;
        constexpr int metallic = 3, zap = 1;

        FmProcessor a;
        PresetHook hookA (a);

        a.set (F::fmPreset, (float) metallic, true);
        check (closeTo (a.get (F::fmRatio), FmPresets::get (metallic).ratio), "användarens val fyller i ratio");
        check (closeTo (a.get (F::filter2On), 1.0f) && closeTo (a.get (F::filter2Cutoff), 120.0f), "Metallic Lead slår på Filter 2");

        // Användaren justerar efteråt: pitch-env och Filter 2 ska överleva rundturen.
        a.set (F::pitchEnvAmount, 7.0f, true);
        a.set (F::pitchEnvDecay, 200.0f, true);
        a.set (F::filter2Cutoff, 500.0f, true);
        a.set (F::filter2On, 0.0f, true);

        const auto saved = a.apvts.copyState();

        // Laddning i en ny processor med samma koppling (preset-parametern går 0 -> 3 utan gest).
        FmProcessor b;
        PresetHook hookB (b);
        b.apvts.replaceState (saved);
        check (closeTo (b.get (F::fmPreset), (float) metallic), "preset-valet laddas", b.get (F::fmPreset));
        check (closeTo (b.get (F::pitchEnvAmount), 7.0f), "pitch-env amount oförändrad efter laddning", b.get (F::pitchEnvAmount));
        check (closeTo (b.get (F::pitchEnvDecay), 200.0f), "pitch-env decay oförändrad efter laddning", b.get (F::pitchEnvDecay));
        check (closeTo (b.get (F::filter2Cutoff), 500.0f), "Filter 2 cutoff oförändrad efter laddning", b.get (F::filter2Cutoff));
        check (closeTo (b.get (F::filter2On), 0.0f), "Filter 2 av efter laddning", b.get (F::filter2On));

        // Laddning över en processor där presetet redan har ett annat värde (undo/laddning av ett annat projekt).
        a.apvts.replaceState (saved);
        check (closeTo (a.get (F::pitchEnvAmount), 7.0f) && closeTo (a.get (F::filter2On), 0.0f), "samma värden efter laddning i samma processor");

        // Värdeautomation av preset-parametern (ingen gest) rör inte ratterna.
        b.set (F::fmPreset, (float) zap, false);
        check (closeTo (b.get (F::fmRatio), FmPresets::get (metallic).ratio), "automation av preset fyller inte i ratterna", b.get (F::fmRatio));
        check (closeTo (b.get (F::pitchEnvAmount), 7.0f), "automation av preset rör inte pitch-env", b.get (F::pitchEnvAmount));

        // Kontroll: ett riktigt val i den laddade processorn fungerar fortfarande.
        b.set (F::fmPreset, (float) zap, true);
        check (closeTo (b.get (F::fmRatio), FmPresets::get (zap).ratio), "användarens val fungerar efter laddning", b.get (F::fmRatio));
        check (closeTo (b.get (F::pitchEnvAmount), FmPresets::get (zap).pitchEnvSemis), "presetet skriver pitch-env vid användarens val", b.get (F::pitchEnvAmount));
    }
}

void runFmPresetTests()
{
    testPresetOnlyOnUserChoice();
}
