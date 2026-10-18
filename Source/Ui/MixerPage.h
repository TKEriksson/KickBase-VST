#pragma once

#include "VisualTimer.h"

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

// Mixern: en remsa per ljudkälla som används (med rullning i sidled när de inte får plats) och mastern.
class MixerView : public juce::Component, private VisualTimer
{
public:
    explicit MixerView (PluginBase& processor);
    void resized() override;

private:
    void timerCallback() override;

    static constexpr int stripWidth = 88; // så bred att SC och MAX alltid syns på knapparna; får de inte plats rullar man i sidled

    PluginBase& processor;
    int lastActive = -1, lastFxMask = -1;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    MasterStrip master; // fast till höger, rullar inte
};

// En rad för sidechainen (field "Duck") eller maximizern (field "Max"): kicken som en knapp, och instrumenten och effektkanalerna var och en i en
// meny med flerval (10 instrument och 11 effektkanaler får inte plats som knappar). Samma parametrar som SC- och MAX-knapparna i mixern.
class SourceToggles : public juce::Component, private juce::Timer
{
public:
    SourceToggles (PluginBase& processor, const juce::String& field, const juce::String& title);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showMenu (bool instruments);
    int onCount (bool instruments) const;
    void updateTexts();

    PluginBase& processor;
    juce::String field;
    int lastActive = -1, lastNamesVersion = -1, lastInstrumentsOn = -1, lastFxOn = -1;
    juce::String title;
    std::unique_ptr<juce::TextButton> kickButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> kickAttachment;
    juce::TextButton instrumentsButton, fxButton;
};
