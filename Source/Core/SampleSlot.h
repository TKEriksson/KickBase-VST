#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include "SampleData.h"
#include "DeferredRelease.h"

// En plats för en sampling: originalet sparas i projektet, en omsamplad kopia spelas av ljudtråden.
// Bara message thread och prepareToPlay rör originalet (därför låset, aldrig på ljudtråden); ljudtråden läser `active()`.
class SampleSlot
{
public:
    explicit SampleSlot (double maxSecondsToLoad = 12.0) : maxSeconds (maxSecondsToLoad) {}

    bool loadFile (const juce::File& file);                  // message thread
    void clear();
    void setOriginal (juce::AudioBuffer<float> mono, double rate, const juce::String& name);
    void prepare (double sampleRate);                        // bygger om kopian för samplingsfrekvensen

    const ClickSample* active() const { return activeSample.load(); }
    juce::String name() const;
    bool hasSample() const { return active() != nullptr; }

    // Skivor (slicer): markörerna hör till samplingen och sparas med den. Ändringen bygger om kopian (message thread).
    void setSlicing (int mode, float sensitivity, int gridCount);
    void setManualMarkers (std::vector<float> normalised);   // 0..1 av samplingens längd
    int sliceMode() const { const juce::ScopedLock scoped (lock); return mode; }
    float sliceSensitivity() const { const juce::ScopedLock scoped (lock); return sensitivity; }
    int sliceGridCount() const { const juce::ScopedLock scoped (lock); return gridCount; }
    std::vector<float> manualMarkers() const { const juce::ScopedLock scoped (lock); return markers; }

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
    int mode = SampleSlicing::grid;
    float sensitivity = 0.5f;
    int gridCount = 8;
    std::vector<float> markers;

    std::atomic<ClickSample*> activeSample { nullptr };
    std::unique_ptr<ClickSample> owned;
    DeferredRelease<ClickSample> retired; // tidigare samplingar: ljudtråden kan läsa en av dem ett block till, så de frigörs först efter en stund

public:
    size_t retiredCount() const { const juce::ScopedLock scoped (lock); return retired.size(); } // för testerna
};
