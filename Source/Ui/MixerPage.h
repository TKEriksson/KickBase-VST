#pragma once

#include "../Core/PluginBase.h"
#include "Components.h"

// Färgen för en mixerkälla: kicken orange, baslagren i sina färger, effektkanalerna gula.
juce::Colour mixerSourceColour (int source);

// En kanalremsa i mixern: namn, panorering, fader med dB-mätare, gain i dB och brytarna SC
// (sidechain ducker källan) och MAX (källan går genom maximizern).
class MixerStrip : public juce::Component
{
public:
    MixerStrip (PluginBase& processor, int source);

    // Uppdaterar mätaren med toppen sedan sist (anropas av MixerView:s timer).
    void updateMeter();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginBase& processor;
    const Mixer& mixer;
    int source;
    juce::Colour colour;

    juce::Slider pan, fader;
    juce::TextButton duckButton { "SC" }, maxButton { "MAX" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment, faderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> duckAttachment, maxAttachment;

    std::array<float, 2> meterDb { -100.0f, -100.0f }, holdDb { -100.0f, -100.0f };
    juce::Rectangle<int> meterArea;
};

// Mastern längst till höger: den gemensamma utgångsnivån (OUTPUT) och en mätare för allt som hörs.
class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip (PluginBase& processor);
    void updateMeter();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginBase& processor;
    juce::Slider fader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAttachment;
    float meterDb = -100.0f, holdDb = -100.0f;
    juce::Rectangle<int> meterArea;
};

// Mixern: en remsa per ljudkälla och mastern.
class MixerView : public juce::Component, private juce::Timer
{
public:
    explicit MixerView (PluginBase& processor);
    void resized() override;

private:
    void timerCallback() override;

    PluginBase& processor;
    int lastActive = -1;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    MasterStrip master;
};

// En rad av brytare, en per ljudkälla, för sidechainen (field "Duck") eller maximizern (field "Max").
// De är samma parametrar som SC- och MAX-knapparna i mixern.
class SourceToggles : public juce::Component, private juce::Timer
{
public:
    SourceToggles (PluginBase& processor, const juce::String& field, const juce::String& title);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginBase& processor;
    int lastActive = -1, lastNamesVersion = -1;
    std::vector<int> sourceOfButton;
    juce::String title;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> attachments;
};
