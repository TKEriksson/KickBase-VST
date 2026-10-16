#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "SampleData.h"

// En plats för en sampling: originalet sparas i projektet, en omsamplad kopia spelas av ljudtråden.
// Bara message thread och prepareToPlay rör originalet (därför låset, aldrig på ljudtråden); ljudtråden läser `active()`.
class SampleSlot
{
public:
    explicit SampleSlot (double maxSecondsToLoad = 2.0) : maxSeconds (maxSecondsToLoad) {}

    bool loadFile (const juce::File& file);                  // message thread
    void clear();
    void setOriginal (juce::AudioBuffer<float> mono, double rate, const juce::String& name);
    void prepare (double sampleRate);                        // bygger om kopian för samplingsfrekvensen

    const ClickSample* active() const { return activeSample.load(); }
    juce::String name() const;
    bool hasSample() const { return active() != nullptr; }

    // Sparas som ett barn i projektets (eller presetets) träd. `type` är barnets typ.
    juce::ValueTree toValueTree (const juce::Identifier& type) const;
    void fromValueTree (const juce::ValueTree& tree);

private:
    void rebuild();

    const double maxSeconds;
    double currentSampleRate = 44100.0;
    mutable juce::CriticalSection lock;
    juce::AudioBuffer<float> original;
    double originalRate = 0.0;
    juce::String originalName;

    std::atomic<ClickSample*> activeSample { nullptr };
    std::unique_ptr<ClickSample> owned;
    std::unique_ptr<ClickSample> retired; // föregående: ljudtråden kan läsa den ett block till
};
