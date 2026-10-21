#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace SampleSlicing
{
    constexpr int maxSlices = 128;
    constexpr double minSliceSeconds = 0.005; // kortare skivor ger bara klick
    enum Mode { grid = 0, automatic = 1, manual = 2 };
}

// En inladdad sampling (mono, omsamplad till aktuell samplingsfrekvens). Ägs av en SampleSlot; ljudtråden läser bara.
// Kicken spelar den som klick eller som hel kick, och varje baslager kan spela en egen som ljudkälla.
struct ClickSample
{
    juce::AudioBuffer<float> audio;
    juce::String name;
    std::vector<int> slices; // skivornas startsamples (stigande, första 0, max SampleSlicing::maxSlices); skivan slutar där nästa börjar
};
