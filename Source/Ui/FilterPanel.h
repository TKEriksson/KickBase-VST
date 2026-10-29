#pragma once

#include "VisualTimer.h"

#include "../Core/FilterUnit.h"
#include "../Core/PluginBase.h"
#include "Components.h"

// Filtrens frekvenssvar (fas D2/D3): Filter 1, Filter 2 och summan (som synten räknar den, Serial eller Parallel och MIX) som kurvor i en
// logaritmisk frekvensaxel. Kurvorna räknas ur en egen uppsättning filter (aldrig ljudtrådens): en impuls med liten amplitud körs genom FilterUnit
// med inställningarnas värden inklusive LFO/ENV/MACRO-modulering (utan keytrack och filter-envelope) och FFT:as på meddelandetråden, bara när något ändrats.
class FilterResponseView : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    FilterResponseView (PluginBase& processor, int layer, juce::Colour accent);
    void paint (juce::Graphics&) override;

private:
    static constexpr int numPoints = 240;   // punkter längs frekvensaxeln (20 Hz - 20 kHz)
    static constexpr int fftOrder = 14;     // 16384 samplingar vid 48 kHz: tillräckligt för kamfiltrets efterklang och en rimlig upplösning vid 20-100 Hz
    static constexpr double displayRate = 48000.0;

    void timerCallback() override;
    void recompute (const InstrumentParams& params);
    void applyModulation (InstrumentParams& params) const; // lägger LFO/ENV/MACRO-förskjutningen på de modulerbara filterrattarna
    static std::vector<float> keyOf (const InstrumentParams& params);

    struct Modulated { juce::RangedAudioParameter* parameter = nullptr; int index = -1; };
    std::array<Modulated, 9> modulated {}; // cutoff, reso, P1, P2 (Filter 1), cutoff, reso, P1, P2 (Filter 2), MIX

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    ParamRefs refs;

    FilterUnit filter1, filter2, filter2Alone;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> impulse, spectrum;       // arbetsminne (allokeras en gång)
    std::array<float, numPoints> db1 {}, db2 {}, dbSum {};
    std::vector<float> lastKey;
    bool filter2Shown = false;
    float cutoff1 = 0.0f, cutoff2 = 0.0f;
};

// FILTER-fliken i källpanelen: kurvan till vänster, Filter 1 och Filter 2 med modell, typ och ratter till höger, och ROUTING under kurvan.
// P1 och P2 byter namn efter modellen och tonas ned när modellen inte använder dem.
class FilterPage : public juce::Component
{
public:
    FilterPage (PluginBase& processor, int layer, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Block
    {
        juce::String title;
        std::unique_ptr<ChoiceComponent> model, type;
        std::unique_ptr<KnobComponent> cutoff, reso, p1, p2, extra; // extra: KEYTRK (Filter 1) eller MIX (Filter 2)
        std::unique_ptr<juce::ToggleButton> power;                  // bara Filter 2
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
        std::unique_ptr<juce::ParameterAttachment> modelAttachment, onAttachment;
        int modelIndex = 0;
        bool isOn = true;
        juce::Rectangle<int> frame;
    };

    void makeBlock (Block& block, bool second);
    void refresh (Block& block, bool second);
    void layoutBlock (Block& block, juce::Rectangle<int> area);

    PluginBase& processor;
    const int layer;
    juce::Colour accent;
    FilterResponseView view;
    Block first, second;
    std::unique_ptr<ChoiceComponent> routing;
    std::unique_ptr<KnobComponent> allpassFreq, allpassAmount; // allpass efter filtren (AMOUNT 0 = av)
    juce::Label routingNote;
    // fas S: FILTER av/på för hela instrumentet (en knapp) och per källa (en flervalsmeny: låter t.ex. SAMPLE eller NOISE gå förbi båda filtren).
    // Samma parametrar finns som en knapp i varje källas egen flik.
    void showSourcesMenu (int initiallySelected = 0);
    void updateSourcesText();
    static constexpr int numSources = 5; // OSC 1, OSC 2, OSC 3, SAMPLE, NOISE (i ordning med InstrumentField::filterOsc1 ...)
    juce::TextButton sourcesButton, helpButton;
    std::array<std::unique_ptr<juce::ParameterAttachment>, numSources> sourceAttachments;
    std::array<bool, numSources> sourceOn {};
    juce::Rectangle<int> switchCaption; // text framför menyn: vad den gör
};
