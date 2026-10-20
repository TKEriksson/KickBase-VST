#pragma once

#include "VisualTimer.h"

#include "../Core/PluginBase.h"
#include "Components.h"
#include "Cursors.h"
#include "ListenButton.h"

// Kickens vågform (renderad av själva kickmotorn) med envelopernas noder ovanpå, som i Kick 3.
// PITCH visar tonhöjdens svep (noterna märkta med ton och Hz), AMP volymen. Dra noder, dubbel-
// klicka för att lägga till, högerklicka för att ta bort och dra i romben mitt på ett segment
// för att böja det. Den sista noden bestämmer kickens längd.
class KickDisplay : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    explicit KickDisplay (PluginBase& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    NodeEnvelope& active() const { return amp ? processor.kickAmpEnv : processor.kickPitchEnv; }
    void setMode (bool showAmp);
    juce::int64 stateHash() const;
    void renderWaveform();

    juce::Rectangle<float> plotArea() const;
    float viewSeconds() const;
    float lengthScale() const;
    float displayMax() const { return amp ? 1.0f : 60.0f; }
    juce::Point<float> toScreen (float seconds, float value) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const;
    juce::Point<float> tensionHandle (size_t segment) const;
    int pointAt (juce::Point<float> p) const;
    int tensionHandleAt (juce::Point<float> p) const;
    juce::String describe (const NodeEnvelope::Point& point) const;
    void commit (std::vector<NodeEnvelope::Point> points);

    PluginBase& processor;
    bool amp = false;
    juce::MouseCursor speakerCursor = Cursors::speaker();
    bool hoverDisplay = false;                      // musen är över rutan: den glöder
    int lastTriggerCount = 0;
    double triggerWallTime = 0.0;                   // när senaste kicken märktes (ms), för blixten
    float markerSeconds = -1.0f;                    // var kicken är just nu (sekunder), -1 = tyst
    juce::TextButton pitchButton { "PITCH" }, ampButton { "AMP" };
    ListenButton listenButton; // högtalaren bredvid PITCH och AMP: spelar kicken

    // Vågformen som ritas (min/max per bildpunkt), omräknad när något ändrats.
    juce::AudioBuffer<float> waveform;
    double waveformSeconds = 0.1;
    juce::int64 lastHash = 0;
    int lastVersionPitch = -1, lastVersionAmp = -1;

    std::vector<NodeEnvelope::Point> editing; // kopia under drag
    int dragged = -1, draggedTension = -1, hovered = -1;
    float tensionStart = 0.0f, dragStartY = 0.0f;
    float fixedView = 0.0f; // visad längd under drag, så att bilden inte rör sig
};
