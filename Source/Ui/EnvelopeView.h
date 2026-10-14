#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Theme.h"

// Visar envelopes som kurvor med handtag man drar i direkt.
//
// Tidsaxeln är en kvadratrotsskala: de första millisekunderna (där kickens svep och klick
// finns) får mer plats än svansen. Varje handtag styr en parameter i x-led och/eller y-led.
class EnvelopeView : public juce::Component, private juce::Timer
{
public:
    struct Curve
    {
        std::function<float (double seconds)> valueAt; // 0..1
        juce::Colour colour;
        bool filled = false;
        juce::String name;
    };

    struct Handle
    {
        std::function<juce::Point<float>()> position; // x i sekunder, y i 0..1
        juce::RangedAudioParameter* xParam = nullptr;
        juce::RangedAudioParameter* yParam = nullptr;
        bool invertY = false; // uppåt minskar parametern (t.ex. kurvform)
        juce::Colour colour;
    };

    explicit EnvelopeView (double displaySecondsIn);

    // Körs i början av varje omritning, t.ex. för att läsa parametrarna en gång.
    std::function<void()> onBeforePaint;

    void addCurve (Curve curve)    { curves.push_back (std::move (curve)); }
    void addHandle (Handle handle) { handles.push_back (std::move (handle)); }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }

    juce::Rectangle<float> plotArea() const;
    float timeToX (double seconds) const;
    float valueToY (float value) const;
    juce::Point<float> handlePoint (const Handle& handle) const;
    int handleAt (juce::Point<float> point) const;
    juce::String describe (const Handle& handle) const;

    double displaySeconds;
    std::vector<Curve> curves;
    std::vector<Handle> handles;

    int hoveredHandle = -1, draggedHandle = -1;
    juce::Point<float> dragStart;
    float startX = 0.0f, startY = 0.0f; // parametrarnas normaliserade värden när draget började
};
