#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter-ID:n samlade på ett ställe så att processor, röster och editor använder samma namn.
namespace ParamID
{
    inline constexpr auto waveform  = "waveform";
    inline constexpr auto attack    = "attack";
    inline constexpr auto decay     = "decay";
    inline constexpr auto sustain   = "sustain";
    inline constexpr auto release   = "release";
    inline constexpr auto cutoff    = "cutoff";
    inline constexpr auto resonance = "resonance";
    inline constexpr auto gain      = "gain";
}

enum class Waveform { sine, saw, square, triangle };

// Ett ögonblicksvärde av alla parametrar som rösterna behöver.
struct VoiceParams
{
    Waveform waveform = Waveform::saw;
    juce::ADSR::Parameters adsr;
    float cutoff = 2000.0f;
    float resonance = 0.7f;
};
