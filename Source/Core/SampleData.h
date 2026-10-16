#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// En inladdad sampling (mono, omsamplad till aktuell samplingsfrekvens). Ägs av en SampleSlot; ljudtråden läser bara.
// Kicken spelar den som klick eller som hel kick, och varje baslager kan spela en egen som ljudkälla.
struct ClickSample
{
    juce::AudioBuffer<float> audio;
    juce::String name;
};
