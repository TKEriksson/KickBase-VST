#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Lyssnar på en parameter men reagerar bara på ändringar som användaren gör i gränssnittet. juce::ParameterAttachment ropar sin callback vid
// varje värdeändring, även vid replaceState (laddning, undo) och värdeautomation. Gester (begin/endChangeGesture) skickas däremot bara av
// gränssnittets egna vägar (ComboBox-/Slider-attachments, Components.cpp, FmPresets), aldrig av replaceState eller värden. Därför räknas
// en ändring som användarens bara medan en gest pågår.
class UserChangeWatcher : private juce::AudioProcessorParameter::Listener
{
public:
    // onUserChange får parametern i dess egna enheter (för en meny: indexet).
    UserChangeWatcher (juce::RangedAudioParameter& parameterIn, std::function<void (float)> onUserChange)
        : parameter (parameterIn), callback (std::move (onUserChange))
    {
        parameter.addListener (this);
    }

    ~UserChangeWatcher() override { parameter.removeListener (this); }

    UserChangeWatcher (const UserChangeWatcher&) = delete;
    UserChangeWatcher& operator= (const UserChangeWatcher&) = delete;

private:
    void parameterValueChanged (int, float newValue) override
    {
        if (gestureDepth > 0 && callback != nullptr)
            callback (parameter.convertFrom0to1 (newValue));
    }

    void parameterGestureChanged (int, bool starting) override { gestureDepth = juce::jmax (0, gestureDepth + (starting ? 1 : -1)); }

    juce::RangedAudioParameter& parameter;
    std::function<void (float)> callback;
    int gestureDepth = 0;
};
